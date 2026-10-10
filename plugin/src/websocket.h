#pragma once

#include <mutex>
#include <string>

namespace ap {

// A blocking WebSocket client on WinHTTP. One thread calls Receive; Send and
// Close may be called from any thread.
class WebSocket {
public:
	WebSocket() = default;
	~WebSocket();
	WebSocket(const WebSocket&)            = delete;
	WebSocket& operator=(const WebSocket&) = delete;

	// Connects to host:port over wss (secure) or ws. Returns false and fills
	// error on failure.
	bool Connect(const std::wstring& host, unsigned short port, bool secure, std::string& error);
	// Blocks until one whole text message arrives. Returns false when the
	// connection closes or fails.
	bool Receive(std::string& message);
	bool Send(const std::string& message);
	void Close();

private:
	void Release();

	void*      session_ = nullptr;
	void*      connection_ = nullptr;
	void*      socket_ = nullptr;
	std::mutex sendMutex_;
	std::mutex handleMutex_;
};

} // namespace ap
