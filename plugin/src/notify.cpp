#include "notify.h"
#include "sounds.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
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

struct Line {
	std::wstring      text;
	Style             style;
	Clock::time_point shownAt;
	Clock::time_point endsAt;
};

std::mutex        gMutex;
std::deque<Line>  gWaiting; // queued, not on screen yet
std::vector<Line> gShowing; // on screen (UI thread only)
std::atomic<bool> gBusy;

std::wstring gSoundFolder;
bool         gSoundsOn = false;

// The sound for the most important of the lines just shown.
void PlayFor(const std::vector<Style>& shown) {
	if (!gSoundsOn || gSoundFolder.empty()) {
		return;
	}
	const ap::sounds::Sound* sound = nullptr;
	for (const Style style : { Style::Important, Style::Trap, Style::Info }) {
		if (sound == nullptr && std::ranges::find(shown, style) != shown.end()) {
			sound = style == Style::Important ? &ap::sounds::Important : style == Style::Trap ? &ap::sounds::Trap : &ap::sounds::Item;
		}
	}
	if (sound != nullptr) {
		const std::wstring path = gSoundFolder + L"\\" + sound->file;
		PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
	}
}

HWND  gHost   = nullptr;
HWND  gWindow = nullptr;
HFONT gFont   = nullptr;
int   gFontHeight = 0;
bool  gFontLoaded = false;

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
	if (process != GetCurrentProcessId() || window == gWindow || !IsWindowVisible(window) || IsIconic(window)) {
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

void UpdateFont(int height) {
	if (gFont != nullptr && height == gFontHeight) {
		return;
	}
	if (gFont != nullptr) {
		DeleteObject(gFont);
	}
	gFontHeight = height;
	gFont = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		DEFAULT_PITCH, L"ExocetBlizzardMixedCapsOTMedium");
}

LRESULT CALLBACK BannerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_NCHITTEST) {
		return HTTRANSPARENT;
	}
	if (message == WM_MOUSEACTIVATE) {
		return MA_NOACTIVATE;
	}
	if (message != WM_PAINT) {
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

	HGDIOBJ oldFont = SelectObject(dc, gFont);
	SetBkMode(dc, TRANSPARENT);
	const int lineHeight = gFontHeight * 3 / 2;
	int y = gFontHeight / 2;
	for (const auto& line : gShowing) {
		SetTextColor(dc, TextColor(line.style));
		RECT row { 0, y, rect.right, y + lineHeight };
		DrawTextW(dc, line.text.c_str(), static_cast<int>(line.text.size()), &row, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		y += lineHeight;
	}
	SelectObject(dc, oldFont);

	BitBlt(screen, 0, 0, rect.right, rect.bottom, dc, 0, 0, SRCCOPY);
	SelectObject(dc, oldBitmap);
	DeleteObject(bitmap);
	DeleteDC(dc);
	EndPaint(window, &paint);
	return 0;
}

auto CreateBanner() -> bool {
	if (gWindow != nullptr && IsWindow(gWindow) && gHost != nullptr && IsWindow(gHost)) {
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
			.lpfnWndProc   = BannerProc,
			.hInstance     = GetModuleHandleW(nullptr),
			.lpszClassName = L"ArchipelagoBanner",
		};
		return RegisterClassW(&description);
	}();
	if (windowClass == 0) {
		return false;
	}
	LoadGameFont();
	// Owned by the game window, so it stays above it and minimizes with it.
	gWindow = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"ArchipelagoBanner", L"", WS_POPUP, 0, 0, 1, 1, gHost,
		nullptr, GetModuleHandleW(nullptr), nullptr);
	return gWindow != nullptr;
}

} // namespace

void SetSounds(const std::wstring& folder, bool enabled) {
	gSoundFolder = folder;
	gSoundsOn = enabled;
}

void Show(const std::string& text, Style style) {
	std::lock_guard lock(gMutex);
	gWaiting.push_back({ Widen(text), style, {}, {} });
	gBusy = true;
}

auto Busy() -> bool {
	return gBusy;
}

void Tick() {
	const auto now = Clock::now();
	bool changed = false;
	// Lines that are done leave; waiting ones take their place.
	const size_t before = gShowing.size();
	std::erase_if(gShowing, [&](const Line& line) { return now >= line.endsAt; });
	changed = gShowing.size() != before;
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
		if (gWindow != nullptr && IsWindowVisible(gWindow)) {
			ShowWindow(gWindow, SW_HIDE);
		}
		return;
	}
	if (!CreateBanner()) {
		return;
	}
	if (IsIconic(gHost)) {
		ShowWindow(gWindow, SW_HIDE);
		return;
	}
	RECT client {};
	GetClientRect(gHost, &client);
	POINT origin {};
	ClientToScreen(gHost, &origin);
	const int clientWidth = client.right, clientHeight = client.bottom;
	UpdateFont(std::max(14, clientHeight / 40));

	// Size the box to the widest line.
	HDC dc = GetDC(gWindow);
	HGDIOBJ oldFont = SelectObject(dc, gFont);
	int widest = 0;
	for (const auto& line : gShowing) {
		SIZE size {};
		GetTextExtentPoint32W(dc, line.text.c_str(), static_cast<int>(line.text.size()), &size);
		widest = std::max(widest, static_cast<int>(size.cx));
	}
	SelectObject(dc, oldFont);
	ReleaseDC(gWindow, dc);
	const int width = std::min(clientWidth, widest + gFontHeight * 2);
	const int height = static_cast<int>(gShowing.size()) * (gFontHeight * 3 / 2) + gFontHeight;
	const int x = origin.x + (clientWidth - width) / 2;
	const int y = origin.y + clientHeight / 9;

	// Fade in with the first line, out with the last.
	Clock::time_point first = gShowing.front().shownAt, last = gShowing.front().endsAt;
	for (const auto& line : gShowing) {
		first = std::min(first, line.shownAt);
		last = std::max(last, line.endsAt);
	}
	const float in = std::min(1.0F, std::chrono::duration<float>(now - first) / std::chrono::duration<float>(FadeIn));
	const float out = std::min(1.0F, std::chrono::duration<float>(last - now) / std::chrono::duration<float>(FadeOut));
	SetLayeredWindowAttributes(gWindow, 0, static_cast<BYTE>(MaxAlpha * std::clamp(std::min(in, out), 0.0F, 1.0F)), LWA_ALPHA);

	SetWindowPos(gWindow, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
	static int lastWidth = 0, lastHeight = 0;
	if (changed || width != lastWidth || height != lastHeight) {
		lastWidth = width;
		lastHeight = height;
		InvalidateRect(gWindow, nullptr, FALSE);
	}
}

void Shutdown() {
	if (gWindow != nullptr) {
		DestroyWindow(gWindow);
		gWindow = nullptr;
	}
	gHost = nullptr;
}

} // namespace ap::notify
