#include "websocket.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>

namespace ap {

namespace {

// How long a Receive may stay blocked after Interrupt before the watchdog closes its handle.
constexpr auto AbortAfter = std::chrono::seconds(3);

// The server's or the system's text on one log line: control characters become spaces.
auto OneLine(std::string_view text) -> std::string {
	std::string line;
	for (const char c : text) {
		line += static_cast<unsigned char>(c) < 0x20 ? ' ' : c;
	}
	const auto end = line.find_last_not_of(" .");
	line.erase(end == std::string::npos ? 0 : end + 1);
	return line;
}

// WinHTTP's own text for its errors (12000 and up), the system's for the rest: in English when
// Windows has it (the log is read in bug reports), else the user's language, as UTF-8.
auto ErrorText(DWORD error) -> std::string {
	wchar_t       buffer[256] {};
	const HMODULE winhttp = error >= WINHTTP_ERROR_BASE && error <= WINHTTP_ERROR_LAST ? GetModuleHandleW(L"winhttp.dll") : nullptr;
	const DWORD   flags = FORMAT_MESSAGE_IGNORE_INSERTS | (winhttp != nullptr ? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM);
	DWORD         length = FormatMessageW(flags, winhttp, error, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buffer, ARRAYSIZE(buffer), nullptr);
	if (length == 0) {
		length = FormatMessageW(flags, winhttp, error, 0, buffer, ARRAYSIZE(buffer), nullptr);
	}
	char utf8[512] {};
	const int size = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), utf8, sizeof(utf8), nullptr, nullptr);
	return std::string(utf8, size > 0 ? size : 0);
}

} // namespace

auto DescribeClose(unsigned short status, std::string_view reason) -> std::string {
	const char* meaning = nullptr;
	switch (status) {
	case 1000: meaning = "normal"; break;
	case 1001: meaning = "going away"; break;
	case 1002: meaning = "protocol error"; break;
	case 1003: meaning = "unsupported data"; break;
	case 1005: meaning = "no status"; break;
	case 1006: meaning = "abnormal"; break;
	case 1007: meaning = "invalid data"; break;
	case 1008: meaning = "policy violation"; break;
	case 1009: meaning = "message too big"; break;
	case 1010: meaning = "extension required"; break;
	case 1011: meaning = "server error"; break;
	case 1012: meaning = "service restart"; break;
	case 1013: meaning = "try again later"; break;
	}
	std::string text = "the server closed the connection (" + std::to_string(status);
	if (meaning != nullptr) {
		text += std::string(" ") + meaning;
	}
	if (const std::string line = OneLine(reason); !line.empty()) {
		text += ": " + line;
	}
	return text + ")";
}

auto DescribeReceiveError(unsigned long error, std::string_view text) -> std::string {
	std::string description = "receive failed (error " + std::to_string(error);
	if (const std::string line = OneLine(text); !line.empty()) {
		description += ": " + line;
	}
	return description + ")";
}

WebSocket::~WebSocket() {
	Close();
	{
		std::lock_guard lock(handleMutex_);
		stopping_ = true;
	}
	watchdogWake_.notify_all();
	if (watchdog_.joinable()) {
		watchdog_.join();
	}
}

bool WebSocket::Connect(const std::wstring& host, unsigned short port, bool secure, std::string& error) {
	Close();
	{
		// A new connection: an Interrupt meant for the last one doesn't carry over.
		std::lock_guard lock(handleMutex_);
		interrupted_ = false;
	}
	char buffer[128] {};
	auto fail = [&](const char* step) {
		std::snprintf(buffer, sizeof(buffer), "%s failed (error %lu)", step, GetLastError());
		error = buffer;
		Release();
		return false;
	};

	HINTERNET session = WinHttpOpen(L"d2rl-archipelago/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (session == nullptr) {
		return fail("WinHttpOpen");
	}
	WinHttpSetTimeouts(session, 10'000, 10'000, 10'000, 0);
	{
		std::lock_guard lock(handleMutex_);
		session_ = session;
	}
	HINTERNET connection = WinHttpConnect(session, host.c_str(), port, 0);
	if (connection == nullptr) {
		return fail("WinHttpConnect");
	}
	{
		std::lock_guard lock(handleMutex_);
		connection_ = connection;
	}
	HINTERNET request = WinHttpOpenRequest(connection, L"GET", L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
	if (request == nullptr) {
		return fail("WinHttpOpenRequest");
	}
	// The session's receive timeout is infinite (a quiet server is fine once connected), but a
	// server that stalls the upgrade must not hold the network thread: Interrupt can't reach a
	// handshake, so it gets a finite wait of its own.
	DWORD responseTimeout = 10'000;
	WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &responseTimeout, sizeof(responseTimeout));
	if (!WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)
		|| !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
		|| !WinHttpReceiveResponse(request, nullptr)) {
		WinHttpCloseHandle(request);
		return fail("WebSocket handshake");
	}
	HINTERNET socket = WinHttpWebSocketCompleteUpgrade(request, 0);
	WinHttpCloseHandle(request);
	if (socket == nullptr) {
		return fail("WebSocket upgrade");
	}
	std::lock_guard lock(handleMutex_);
	socket_ = socket;
	return true;
}

bool WebSocket::Receive(std::string& message, std::string& ended) {
	message.clear();
	ended.clear();
	const auto end = [&](std::string why) {
		ended = std::move(why);
		return false;
	};
	char buffer[16 * 1024];
	for (;;) {
		HINTERNET socket = nullptr;
		{
			std::lock_guard lock(handleMutex_);
			if (interrupted_) {
				return end("closed by the plugin");
			}
			// Only this thread closes the handle (or the watchdog, once this call is long under
			// way), so it stays valid through the call below.
			socket = socket_;
			if (socket == nullptr) {
				return end("not connected");
			}
			receiving_ = true;
			++receiveSerial_;
		}
		DWORD                           read = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE  type {};
		const DWORD result = WinHttpWebSocketReceive(socket, buffer, sizeof(buffer), &read, &type);
		const bool closed = type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE;
		USHORT     status = 0;
		char       reason[WINHTTP_WEB_SOCKET_MAX_CLOSE_REASON_LENGTH] {};
		DWORD      length = 0;
		DWORD      queried = ERROR_INVALID_HANDLE;
		bool       interrupted = false;
		{
			std::lock_guard lock(handleMutex_);
			receiving_ = false;
			interrupted = interrupted_;
			// Under the lock: only an Interrupt first (Abort) lets another thread close the handle.
			if (result == NO_ERROR && closed && !interrupted && socket_ == socket) {
				queried = WinHttpWebSocketQueryCloseStatus(socket, &status, reason, sizeof(reason), &length);
			}
		}
		// After Interrupt, a close or an error (the watchdog cancelling the call) is ours.
		if ((result != NO_ERROR || closed) && interrupted) {
			return end("closed by the plugin");
		}
		if (result != NO_ERROR) {
			return end(DescribeReceiveError(result, ErrorText(result)));
		}
		if (closed) {
			return end(queried == NO_ERROR ? DescribeClose(status, std::string_view(reason, length)) : "the server closed the connection");
		}
		message.append(buffer, read);
		if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
			return true;
		}
	}
}

bool WebSocket::Send(const std::string& message) {
	// Held through the send: the owner's Release waits for it, so the handle stays valid.
	std::lock_guard sendLock(sendMutex_);
	HINTERNET socket = nullptr;
	{
		std::lock_guard lock(handleMutex_);
		if (interrupted_) {
			return false;
		}
		socket = socket_;
	}
	if (socket == nullptr) {
		return false;
	}
	const bool sent = WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, const_cast<char*>(message.data()), static_cast<DWORD>(message.size())) == NO_ERROR;
	// An Interrupt during the send left the close to us (one send at a time).
	std::lock_guard lock(handleMutex_);
	if (interrupted_) {
		ShutdownLocked();
	}
	return sent;
}

void WebSocket::Interrupt() {
	// Never wait on a send in progress here (it may be another thread's, stuck on a dead
	// network): Send sees the flag when it's done and sends the close itself.
	std::unique_lock sendLock(sendMutex_, std::try_to_lock);
	std::lock_guard lock(handleMutex_);
	interrupted_ = true;
	if (sendLock.owns_lock()) {
		// The server answers the close, which ends the owner's Receive.
		ShutdownLocked();
	}
	// And if it doesn't answer, the watchdog ends it.
	ArmWatchdogLocked();
}

void WebSocket::ArmWatchdogLocked() {
	if (!receiving_) {
		return;
	}
	armed_ = true;
	armedSerial_ = receiveSerial_;
	abortAt_ = std::chrono::steady_clock::now() + AbortAfter;
	if (!watchdog_.joinable()) {
		watchdog_ = std::thread([this] { Watchdog(); });
	}
	watchdogWake_.notify_all();
}

void WebSocket::Watchdog() {
	std::unique_lock lock(handleMutex_);
	while (!stopping_) {
		if (!armed_) {
			watchdogWake_.wait(lock);
			continue;
		}
		if (std::chrono::steady_clock::now() < abortAt_) {
			watchdogWake_.wait_until(lock, abortAt_);
			continue;
		}
		// The Receive it was armed for has ended (or the owner closed the socket): nothing to do.
		if (!receiving_ || receiveSerial_ != armedSerial_ || socket_ == nullptr) {
			armed_ = false;
			continue;
		}
		// Not while a send uses the handle; it has a timeout, so try again shortly.
		std::unique_lock sendLock(sendMutex_, std::try_to_lock);
		if (!sendLock.owns_lock()) {
			abortAt_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
			continue;
		}
		// Seconds into the call, the owner is inside WinHttpWebSocketReceive, which this cancels.
		WinHttpCloseHandle(socket_);
		socket_ = nullptr;
		armed_ = false;
	}
}

void WebSocket::ShutdownLocked() {
	if (socket_ != nullptr && !shutdownSent_) {
		shutdownSent_ = true;
		WinHttpWebSocketShutdown(socket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
	}
}

void WebSocket::Close() {
	{
		std::lock_guard sendLock(sendMutex_);
		std::lock_guard lock(handleMutex_);
		interrupted_ = true;
		ShutdownLocked();
	}
	Release();
}

void WebSocket::Abort() {
	Interrupt();
	// Closing the handle also wakes a thread blocked in Receive.
	Release();
}

void WebSocket::Release() {
	std::lock_guard sendLock(sendMutex_);
	std::lock_guard lock(handleMutex_);
	for (void** handle : { &socket_, &connection_, &session_ }) {
		if (*handle != nullptr) {
			WinHttpCloseHandle(*handle);
			*handle = nullptr;
		}
	}
	shutdownSent_ = false;
}

} // namespace ap
