#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace ap {

// A blocking WebSocket client on WinHTTP. One thread (the owner) calls Connect,
// Receive and Close; Send and Interrupt may be called from any thread.
//
// Only the owner closes handles: a handle closed from another thread while the
// owner is about to use it could be reused by a new WinHttpOpen anywhere in the
// process, and the owner would then read from someone else's handle. The one
// exception is a peer that ignores the close: a Receive still blocked a while
// after Interrupt is certainly inside WinHTTP by then, so a watchdog closes its
// handle to wake it (WinHTTP keeps a handle alive through a call in progress).
class WebSocket {
public:
	WebSocket() = default;
	~WebSocket();
	WebSocket(const WebSocket&)            = delete;
	WebSocket& operator=(const WebSocket&) = delete;

	// Connects to host:port over wss (secure) or ws. Returns false and fills
	// error on failure. Owner only.
	bool Connect(const std::wstring& host, unsigned short port, bool secure, std::string& error);
	// Blocks until one whole text message arrives. Returns false when the
	// connection closes or fails, or after Interrupt. Owner only.
	bool Receive(std::string& message);
	bool Send(const std::string& message);
	// Any thread: starts closing the connection (a WebSocket close to the server),
	// so a Receive returns false soon and the owner closes the handles.
	void Interrupt();
	// Owner only: closes the connection and its handles.
	void Close();
	// Interrupts, then closes the handles from whichever thread calls it, even
	// with the owner blocked in Receive. Only for shutting the client down for
	// good, where waiting on the server isn't wanted.
	void Abort();

private:
	void Release();
	// Sends the WebSocket close once; handleMutex_ and sendMutex_ must be held.
	void ShutdownLocked();
	// Arms the watchdog for the Receive in progress, if any; handleMutex_ must be held.
	void ArmWatchdogLocked();
	void Watchdog();

	void*      session_ = nullptr;
	void*      connection_ = nullptr;
	void*      socket_ = nullptr;
	bool       interrupted_ = false;
	bool       shutdownSent_ = false;
	// The owner's Receive call in progress (numbered), for the watchdog.
	bool       receiving_ = false;
	uint64_t   receiveSerial_ = 0;
	// The watchdog: closes the socket if Receive call armedSerial_ is still blocked at abortAt_.
	std::thread                           watchdog_;
	std::condition_variable               watchdogWake_;
	bool                                  armed_ = false;
	bool                                  stopping_ = false;
	uint64_t                              armedSerial_ = 0;
	std::chrono::steady_clock::time_point abortAt_;
	std::mutex sendMutex_;
	std::mutex handleMutex_;
};

} // namespace ap
