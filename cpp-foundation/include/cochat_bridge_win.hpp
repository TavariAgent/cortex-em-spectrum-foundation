#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <iostream>

namespace cortex {

// Line-delimited JSON over a named pipe. Minimal, single-writer, multi-client.
class CoChatBridgeWin {
public:
    explicit CoChatBridgeWin(const std::string& pipe_name = R"(\\.\pipe\CortexCoChat)")
        : pipeName(pipe_name), run(true) {
#ifdef _WIN32
        broadcaster = std::thread([this]{ this->server_loop(); });
#endif
    }

    // Enqueue a JSON message to broadcast
    void send_json(const std::string& json_line) {
        {
            std::lock_guard<std::mutex> lk(qm);
            q.push(json_line + "\n");
        }
        cv.notify_one();
    }

    ~CoChatBridgeWin() {
        run = false;
        cv.notify_all();
#ifdef _WIN32
        if (broadcaster.joinable()) broadcaster.join();
#endif
    }

private:
#ifdef _WIN32
    struct Client { HANDLE h{INVALID_HANDLE_VALUE}; };
    std::vector<Client> clients;
    std::mutex cm;

    void server_loop() {
        while (run) {
            // Create an instance and accept a client (overlapped not needed for simplicity)
            HANDLE h = CreateNamedPipeA(
                pipeName.c_str(),
                PIPE_ACCESS_OUTBOUND,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                PIPE_UNLIMITED_INSTANCES,
                1<<16, 1<<16, 0, nullptr
            );
            if (h == INVALID_HANDLE_VALUE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            BOOL ok = ConnectNamedPipe(h, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (!ok) { CloseHandle(h); continue; }

            {
                std::lock_guard<std::mutex> lk(cm);
                clients.push_back(Client{h});
            }

            // Kick a sender thread for this client
            std::thread([this, h]{
                client_sender(h);
            }).detach();
        }
    }

    void client_sender(HANDLE h) {
        while (run) {
            std::string msg;
            {
                std::unique_lock<std::mutex> lk(qm);
                cv.wait(lk, [&]{ return !q.empty() || !run; });
                if (!run) break;
                msg = std::move(q.front());
                q.pop();
            }
            DWORD written = 0;
            if (!WriteFile(h, msg.data(), (DWORD)msg.size(), &written, nullptr)) {
                break; // client disconnected
            }
        }
        // Remove client
        std::lock_guard<std::mutex> lk(cm);
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it->h == h) { CloseHandle(h); clients.erase(it); break; }
        }
    }
#endif

    std::string pipeName;
    std::atomic<bool> run;
    std::thread broadcaster;

    // queue
    std::mutex qm; std::condition_variable cv; std::queue<std::string> q;
};

} // namespace cortex