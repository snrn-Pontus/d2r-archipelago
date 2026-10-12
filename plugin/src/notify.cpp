#include "notify.h"
#include "sounds.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string_view>
#include <vector>

namespace ap::notify {

namespace {

using Clock = std::chrono::steady_clock;

constexpr size_t MaxLines = 5;
constexpr auto   FadeIn   = std::chrono::milliseconds(200);
constexpr auto   FadeOut  = std::chrono::milliseconds(600);
constexpr BYTE   MaxAlpha = 235;
constexpr BYTE   StatusAlpha = 200;
// The status line follows the game window this often.
constexpr auto   StatusRefresh = std::chrono::milliseconds(250);

struct Line {
	std::wstring      text;
	Style             style;
	Clock::time_point shownAt;
	Clock::time_point endsAt;
};

// One of our windows: the rows it paints and the font it paints them in.
struct Box {
	HWND  window = nullptr;
	HFONT font = nullptr;
	int   fontHeight = 0;
	std::vector<std::pair<std::wstring, COLORREF>> rows;
	int   width = 0, height = 0;
};

std::mutex        gMutex;
std::deque<Line>  gWaiting; // queued, not on screen yet
std::vector<Line> gShowing; // on screen (UI thread only)
std::atomic<bool> gBusy;
// The status line, set from any thread.
std::wstring      gStatusText;
Corner            gStatusCorner = Corner::TopLeft;
bool              gStatusChanged = false;
std::atomic<bool> gStatusOn;
Clock::time_point gStatusPlaced;

// The banner sounds at the player's volume, in ap::sounds::All order; empty
// when off or missing. PlaySound reads them while they play, so they live on.
std::string gSounds[std::size(ap::sounds::All)];

HWND gHost = nullptr;
Box  gBanner, gStatus;
bool gFontLoaded = false;

auto Widen(const std::string& text) -> std::wstring {
	if (text.empty()) {
		return {};
	}
	const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring wide(static_cast<size_t>(std::max(size, 0)), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
	return wide;
}

auto Lifetime(Style style) -> Clock::duration {
	return style == Style::Important || style == Style::Trap ? std::chrono::milliseconds(6000) : std::chrono::milliseconds(4500);
}

auto TextColor(Style style) -> COLORREF {
	switch (style) {
	case Style::Important: return RGB(255, 215, 90);  // gold
	case Style::Trap:      return RGB(255, 90, 70);   // red
	case Style::Check:     return RGB(170, 170, 170); // grey
	default:               return RGB(235, 235, 235);
	}
}

// The sound for the most important of the lines just shown.
void PlayFor(const std::vector<Style>& shown) {
	// Important, Trap and Info match ap::sounds::All's order.
	const Style styles[] = { Style::Important, Style::Trap, Style::Info };
	for (size_t i = 0; i < std::size(styles); ++i) {
		if (std::ranges::find(shown, styles[i]) != shown.end()) {
			if (!gSounds[i].empty()) {
				PlaySoundW(reinterpret_cast<LPCWSTR>(gSounds[i].data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
			}
			return;
		}
	}
}

// The game's largest visible window, preferring one titled like the game.
struct Candidate {
	HWND      window = nullptr;
	long long area = 0;
	bool      titled = false;
};

BOOL CALLBACK FindHost(HWND window, LPARAM parameter) {
	auto* best = reinterpret_cast<Candidate*>(parameter);
	DWORD process = 0;
	GetWindowThreadProcessId(window, &process);
	if (process != GetCurrentProcessId() || window == gBanner.window || window == gStatus.window || !IsWindowVisible(window) || IsIconic(window)) {
		return TRUE;
	}
	RECT client {};
	if (!GetClientRect(window, &client)) {
		return TRUE;
	}
	const long long area = static_cast<long long>(client.right) * client.bottom;
	if (area < 640LL * 480LL) {
		return TRUE;
	}
	wchar_t title[256] {};
	GetWindowTextW(window, title, static_cast<int>(std::size(title)));
	const std::wstring_view text(title);
	const bool titled = text.find(L"Diablo") != std::wstring_view::npos || text.find(L"D2R") != std::wstring_view::npos;
	if ((titled && !best->titled) || (titled == best->titled && area > best->area)) {
		*best = { window, area, titled };
	}
	return TRUE;
}

// Exocet, the game's own font, from the game folder.
void LoadGameFont() {
	if (gFontLoaded) {
		return;
	}
	gFontLoaded = true;
	wchar_t path[MAX_PATH] {};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	const auto font = std::filesystem::path(path).parent_path() / L"Exocet.otf";
	AddFontResourceExW(font.c_str(), FR_PRIVATE, nullptr);
}

void UpdateFont(Box& box, int height) {
	if (box.font != nullptr && height == box.fontHeight) {
		return;
	}
	if (box.font != nullptr) {
		DeleteObject(box.font);
	}
	box.fontHeight = height;
	box.font = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		DEFAULT_PITCH, L"ExocetBlizzardMixedCapsOTMedium");
}

auto LineHeight(const Box& box) -> int {
	return box.fontHeight * 3 / 2;
}

LRESULT CALLBACK BoxProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_NCHITTEST) {
		return HTTRANSPARENT;
	}
	if (message == WM_MOUSEACTIVATE) {
		return MA_NOACTIVATE;
	}
	const auto* box = reinterpret_cast<const Box*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message != WM_PAINT || box == nullptr) {
		return DefWindowProcW(window, message, wParam, lParam);
	}
	PAINTSTRUCT paint;
	HDC        screen = BeginPaint(window, &paint);
	RECT       rect {};
	GetClientRect(window, &rect);
	HDC     dc     = CreateCompatibleDC(screen);
	HBITMAP bitmap = CreateCompatibleBitmap(screen, rect.right, rect.bottom);
	HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

	HBRUSH background = CreateSolidBrush(RGB(14, 12, 10));
	FillRect(dc, &rect, background);
	DeleteObject(background);
	// A thin gold frame, like D2R's panels.
	HBRUSH frame = CreateSolidBrush(RGB(120, 96, 50));
	FrameRect(dc, &rect, frame);
	DeleteObject(frame);

	HGDIOBJ oldFont = SelectObject(dc, box->font);
	SetBkMode(dc, TRANSPARENT);
	int y = (rect.bottom - static_cast<int>(box->rows.size()) * LineHeight(*box)) / 2;
	for (const auto& [text, color] : box->rows) {
		SetTextColor(dc, color);
		RECT row { 0, y, rect.right, y + LineHeight(*box) };
		DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &row, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		y += LineHeight(*box);
	}
	SelectObject(dc, oldFont);

	BitBlt(screen, 0, 0, rect.right, rect.bottom, dc, 0, 0, SRCCOPY);
	SelectObject(dc, oldBitmap);
	DeleteObject(bitmap);
	DeleteDC(dc);
	EndPaint(window, &paint);
	return 0;
}

void DestroyBox(Box& box) {
	if (box.window != nullptr) {
		DestroyWindow(box.window);
		box.window = nullptr;
	}
	box.width = box.height = 0;
}

// Finds the game window and creates our two windows over it.
auto CreateWindows() -> bool {
	if (gHost != nullptr && IsWindow(gHost) && gBanner.window != nullptr && gStatus.window != nullptr) {
		return true;
	}
	Shutdown();
	Candidate best;
	EnumWindows(FindHost, reinterpret_cast<LPARAM>(&best));
	gHost = best.window;
	if (gHost == nullptr) {
		return false;
	}
	static const ATOM windowClass = [] {
		const WNDCLASSW description {
			.lpfnWndProc   = BoxProc,
			.hInstance     = GetModuleHandleW(nullptr),
			.lpszClassName = L"ArchipelagoBanner",
		};
		return RegisterClassW(&description);
	}();
	if (windowClass == 0) {
		return false;
	}
	LoadGameFont();
	// Owned by the game window, so they stay above it and minimize with it.
	for (Box* box : { &gBanner, &gStatus }) {
		box->window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"ArchipelagoBanner", L"", WS_POPUP, 0, 0, 1, 1,
			gHost, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (box->window == nullptr) {
			Shutdown();
			return false;
		}
		SetWindowLongPtrW(box->window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(box));
	}
	return true;
}

// The size a box needs for its rows.
auto Measure(Box& box) -> SIZE {
	HDC dc = GetDC(box.window);
	HGDIOBJ oldFont = SelectObject(dc, box.font);
	int widest = 0;
	for (const auto& [text, color] : box.rows) {
		SIZE size {};
		GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
		widest = std::max(widest, static_cast<int>(size.cx));
	}
	SelectObject(dc, oldFont);
	ReleaseDC(box.window, dc);
	return { widest + box.fontHeight * 2, static_cast<int>(box.rows.size()) * LineHeight(box) + box.fontHeight };
}

// Places and shows the box; repaints it when its rows or size changed.
void Place(Box& box, int x, int y, SIZE size, bool changed, BYTE alpha) {
	SetLayeredWindowAttributes(box.window, 0, alpha, LWA_ALPHA);
	SetWindowPos(box.window, HWND_TOP, x, y, size.cx, size.cy, SWP_NOACTIVATE | SWP_SHOWWINDOW);
	if (changed || size.cx != box.width || size.cy != box.height) {
		box.width = size.cx;
		box.height = size.cy;
		InvalidateRect(box.window, nullptr, FALSE);
	}
}

void TickBanner(const RECT& client, POINT origin, Clock::time_point now) {
	const size_t before = gShowing.size();
	std::erase_if(gShowing, [&](const Line& line) { return now >= line.endsAt; });
	bool changed = gShowing.size() != before;
	std::vector<Style> shown;
	{
		std::lock_guard lock(gMutex);
		while (gShowing.size() < MaxLines && !gWaiting.empty()) {
			Line line = std::move(gWaiting.front());
			gWaiting.pop_front();
			line.shownAt = now;
			line.endsAt = now + Lifetime(line.style);
			shown.push_back(line.style);
			gShowing.push_back(std::move(line));
			changed = true;
		}
		if (gShowing.empty() && gWaiting.empty()) {
			gBusy = false;
		}
	}
	PlayFor(shown);
	if (gShowing.empty()) {
		if (IsWindowVisible(gBanner.window)) {
			ShowWindow(gBanner.window, SW_HIDE);
		}
		return;
	}
	UpdateFont(gBanner, std::max(14, static_cast<int>(client.bottom) / 40));
	gBanner.rows.clear();
	for (const auto& line : gShowing) {
		gBanner.rows.emplace_back(line.text, TextColor(line.style));
	}
	// Fade in with the first line, out with the last.
	Clock::time_point first = gShowing.front().shownAt, last = gShowing.front().endsAt;
	for (const auto& line : gShowing) {
		first = std::min(first, line.shownAt);
		last = std::max(last, line.endsAt);
	}
	const float in = std::min(1.0F, std::chrono::duration<float>(now - first) / std::chrono::duration<float>(FadeIn));
	const float out = std::min(1.0F, std::chrono::duration<float>(last - now) / std::chrono::duration<float>(FadeOut));
	const SIZE size = Measure(gBanner);
	Place(gBanner, origin.x + (client.right - size.cx) / 2, origin.y + client.bottom / 9, size, changed,
		static_cast<BYTE>(MaxAlpha * std::clamp(std::min(in, out), 0.0F, 1.0F)));
}

void TickStatus(const RECT& client, POINT origin, Clock::time_point now) {
	std::wstring text;
	Corner corner = Corner::TopLeft;
	bool changed = false;
	{
		std::lock_guard lock(gMutex);
		text = gStatusText;
		corner = gStatusCorner;
		changed = gStatusChanged;
		gStatusChanged = false;
	}
	if (text.empty()) {
		if (IsWindowVisible(gStatus.window)) {
			ShowWindow(gStatus.window, SW_HIDE);
		}
		return;
	}
	if (!changed && now - gStatusPlaced < StatusRefresh && IsWindowVisible(gStatus.window)) {
		return;
	}
	gStatusPlaced = now;
	UpdateFont(gStatus, std::max(12, static_cast<int>(client.bottom) / 64));
	gStatus.rows = { { text, RGB(199, 179, 119) } };
	const SIZE size = Measure(gStatus);
	const int margin = std::max(4, static_cast<int>(client.bottom) / 90);
	const bool right = corner == Corner::TopRight || corner == Corner::BottomRight;
	const bool bottom = corner == Corner::BottomLeft || corner == Corner::BottomRight;
	int x = right ? client.right - size.cx - margin : margin;
	if (corner == Corner::Top) {
		x = (client.right - size.cx) / 2;
	}
	const int y = bottom ? client.bottom - size.cy - margin : margin;
	Place(gStatus, origin.x + x, origin.y + y, size, changed, StatusAlpha);
}

} // namespace

void SetSounds(const std::wstring& folder, bool enabled, int volume) {
	PlaySoundW(nullptr, nullptr, 0); // stop one playing from a buffer about to change
	for (size_t i = 0; i < std::size(ap::sounds::All); ++i) {
		gSounds[i].clear();
		if (!enabled || volume <= 0) {
			continue;
		}
		std::ifstream in(std::filesystem::path(folder) / ap::sounds::All[i].file, std::ios::binary);
		gSounds[i].assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		if (volume != 100) {
			ap::sounds::ScaleWav(gSounds[i], volume);
		}
	}
}

void Show(const std::string& text, Style style) {
	std::lock_guard lock(gMutex);
	gWaiting.push_back({ Widen(text), style, {}, {} });
	gBusy = true;
}

void SetStatus(const std::string& text, Corner corner) {
	std::lock_guard lock(gMutex);
	std::wstring wide = Widen(text);
	if (wide != gStatusText || corner != gStatusCorner) {
		gStatusText = std::move(wide);
		gStatusCorner = corner;
		gStatusChanged = true;
		gStatusOn = true; // one more tick hides it when empty
	}
}

auto Busy() -> bool {
	return gBusy || gStatusOn;
}

void Tick() {
	const auto now = Clock::now();
	{
		std::lock_guard lock(gMutex);
		if (gStatusText.empty() && !gStatusChanged) {
			gStatusOn = false;
		}
	}
	if (!CreateWindows()) {
		return;
	}
	if (IsIconic(gHost)) {
		ShowWindow(gBanner.window, SW_HIDE);
		ShowWindow(gStatus.window, SW_HIDE);
		return;
	}
	RECT client {};
	GetClientRect(gHost, &client);
	POINT origin {};
	ClientToScreen(gHost, &origin);
	TickBanner(client, origin, now);
	TickStatus(client, origin, now);
}

void Shutdown() {
	DestroyBox(gBanner);
	DestroyBox(gStatus);
	gHost = nullptr;
}

} // namespace ap::notify
