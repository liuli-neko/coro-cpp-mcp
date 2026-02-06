#pragma once

#include "../global/global.hpp"

#include <chrono>
#include <ilias/io/stream.hpp>
#include <ilias/sync/mpsc.hpp>
#include <ilias/task/task.hpp>
#include <minihttp/router.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/message_stream_wrapper.hpp>

NEKO_BEGIN_NAMESPACE

class SseServerStream {
public:
    SseServerStream(SseServerStream&&) = default;
    ~SseServerStream()                 = default;

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<void> {
        auto in = co_await mInput.recv();
        if (!in) { // EOF
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        auto ptr = reinterpret_cast<const std::byte*>(in->data());
        buffer.assign(ptr, ptr + in->size());
        NEKO_LOG_INFO("sse", "received {} bytes: {}", buffer.size(),
                      std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()});
        co_return {};
    }

    auto send(std::span<const std::byte> data) -> ilias::IoTask<void> {
        auto output = std::string(reinterpret_cast<const char*>(data.data()), data.size());
        if (!co_await mOutput.send(std::move(output))) {
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        co_return {};
    }

    auto close() -> void {
        mOutput.close();
        mInput.close();
    }

    auto cancel() -> void {}

    auto start() -> ilias::IoTask<void> { co_return {}; }

    auto operator=(SseServerStream&&) -> SseServerStream& = default;

private:
    SseServerStream() = default;

    ilias::mpsc::Sender<std::string> mOutput;
    ilias::mpsc::Receiver<std::string> mInput;
    friend class SseListener;
};

class SseListener {
public:
    SseListener(ilias::TcpListener listener) : mListener(std::move(listener)) {}

    auto close() -> void {
        if (mHandle) {
            mHandle.stop();
            mHandle.wait();
            mHandle = nullptr;
        }
    }

    auto cancel() -> void {}

    auto accept() -> ilias::IoTask<SseServerStream> {
        if (!mHandle) {
            auto [sender, receiver] = ilias::mpsc::channel<SseServerStream>();
            mSender                 = std::move(sender);
            mReceiver               = std::move(receiver);
            mHandle                 = ilias::spawn(loop());
        }
        auto res = co_await mReceiver.recv();
        if (!res) {
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        co_return std::move(*res);
    }

private:
    using SseEvent     = minihttp::server::SseEvent;
    using SseGenerator = ilias::Generator<SseEvent>;
    using Sse          = minihttp::server::Sse;
    using Query        = minihttp::server::Query<minihttp::server::UrlParams>;
    using Text         = minihttp::server::Text<std::string>;
    using Router       = minihttp::server::Router;

    auto loop() -> ilias::Task<void> {
        auto router = Router()
                          .get("/sse", [this]() { return processIncoming(); })
                          .post("/message", [this](Query request, Text content) {
                              return processPost(std::move(request), std::move(content));
                          });
        co_await minihttp::server::serve(std::move(mListener), std::move(router));
    }

    auto processIncoming() -> ilias::Task<Sse> {
        auto [inSender, inReceiver]   = ilias::mpsc::channel<std::string>();
        auto [outSender, outReceiver] = ilias::mpsc::channel<std::string>();
        auto stream                   = SseServerStream{};
        stream.mInput                 = std::move(inReceiver);
        stream.mOutput                = std::move(outSender);
        co_await mSender.send(std::move(stream));

        // Register the session
        mId += 1;
        mSessions.insert({std::to_string(mId), std::move(inSender)});
        co_return Sse(sseGenerator(std::move(outReceiver), mId));
    }

    auto processPost(Query request, Text textContent) -> ilias::Task<Text> {
        auto& [params]  = request;
        auto& [content] = textContent;
        NEKO_LOG_INFO("sse", "Received POST /message?id={}, content size: {}", params["id"], content.size());
        if (!content.empty()) {
            NEKO_LOG_DEBUG("sse", "Content: {}", content);
        }
        auto it = mSessions.find(params["id"]);
        if (it == mSessions.end()) {
            NEKO_LOG_WARN("sse", "Session id '{}' not found. Available sessions: {}", params["id"], mSessions.size());
            co_return Text("Invalid id");
        }
        auto& [_, sender] = *it;
        NEKO_LOG_DEBUG("sse", "Sending content to session {}", params["id"]);
        if (!co_await sender.send(std::move(content))) {
            NEKO_LOG_ERROR("sse", "Failed to send content to session {}", params["id"]);
            co_return Text("Failed to send");
        }
        NEKO_LOG_DEBUG("sse", "Content sent successfully to session {}", params["id"]);
        co_return Text("OK");
    }

    auto sseGenerator(ilias::mpsc::Receiver<std::string> input, size_t id) -> SseGenerator {
        // FIXME: possible leak on here, need fix in minihttp
        struct Guard {
            ~Guard() { self.mSessions.erase(std::to_string(id)); }

            SseListener& self;
            size_t id;
        } guard{*this, id};

        co_yield SseEvent{.comment = {}, .event = "endpoint", .data = "/message?id=" + std::to_string(id), .retry = {}};
        while (true) {
            auto [text, timeout] = co_await ilias::whenAny(input.recv(), ilias::sleep(mKeepAliveInterval));
            if (text && *text) {
                if (*text) {
                    co_yield SseEvent{.comment = {}, .event = "message", .data = text->value(), .retry = {}};
                } else {
                    co_return;
                }
            } else {
                co_yield SseEvent{.comment = "keep-alive", .event = {}, .data = {}, .retry = {}};
            }
        }
    }

    ilias::TcpListener mListener;
    ilias::WaitHandle<void> mHandle;
    ilias::mpsc::Sender<SseServerStream> mSender;
    ilias::mpsc::Receiver<SseServerStream> mReceiver;
    std::chrono::milliseconds mKeepAliveInterval{15000};               // 15 seconds
    std::map<std::string, ilias::mpsc::Sender<std::string>> mSessions; // endpoint -> sender
    size_t mId = 0;
};

NEKO_END_NAMESPACE