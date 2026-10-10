#include "websocket.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>

namespace ap {

WebSocket::~WebSocket() {
	Close();
}

bool WebSocket::Connect(const std::wstring& host, unsigned short port, bool secure, std::string& error) {
	Close();
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
			socket = socket_;
		}
		if (socket == nullptr) {
			return false;
		}
		DWORD                           read = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE  type {};
		const DWORD result = WinHttpWebSocketReceive(socket, buffer, sizeof(buffer), &read, &type);
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
	std::lock_guard sendLock(sendMutex_);
	HINTERNET socket = nullptr;
	{
		std::lock_guard lock(handleMutex_);
		socket = socket_;
	}
	if (socket == nullptr) {
		return false;
	}
	return WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, const_cast<char*>(message.data()), static_cast<DWORD>(message.size())) == NO_ERROR;
}

void WebSocket::Close() {
	{
		std::lock_guard lock(handleMutex_);
		if (socket_ != nullptr) {
			// Closing the handle also wakes a thread blocked in Receive.
			WinHttpWebSocketShutdown(socket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
		}
	}
	Release();
}

void WebSocket::Release() {
	std::lock_guard lock(handleMutex_);
	for (void** handle : { &socket_, &connection_, &session_ }) {
		if (*handle != nullptr) {
			WinHttpCloseHandle(*handle);
			*handle = nullptr;
		}
	}
}

} // namespace ap
