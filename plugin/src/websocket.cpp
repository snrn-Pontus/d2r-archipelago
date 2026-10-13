#include "websocket.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>

namespace ap {

namespace {

// How long a Receive may stay blocked after Interrupt before the watchdog closes its handle.
constexpr auto AbortAfter = std::chrono::seconds(3);

} // namespace

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

bool WebSocket::Receive(std::string& message) {
	message.clear();
	char buffer[16 * 1024];
	for (;;) {
		HINTERNET socket = nullptr;
		{
			std::lock_guard lock(handleMutex_);
			if (interrupted_) {
				return false;
			}
			// Only this thread closes the handle (or the watchdog, once this call is long under
			// way), so it stays valid through the call below.
			socket = socket_;
			if (socket == nullptr) {
				return false;
			}
			receiving_ = true;
			++receiveSerial_;
		}
		DWORD                           read = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE  type {};
		const DWORD result = WinHttpWebSocketReceive(socket, buffer, sizeof(buffer), &read, &type);
		{
			std::lock_guard lock(handleMutex_);
			receiving_ = false;
		}
		if (result != NO_ERROR || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
			return false;
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
