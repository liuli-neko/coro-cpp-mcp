#include "ccmcp/io/sse_stream.hpp"

#include <ilias/net/tcp.hpp>
#include <ilias/sync/mpsc.hpp>
#include <ilias/task.hpp>
#include <minihttp/router.hpp>
#include <nekoproto/global/log.hpp>

#include <chrono>
#include <map>
#include <string>
#include <string_view>
#include <utility>

NEKO_BEGIN_NAMESPACE

struct SseServerStream::Impl {
    ilias::mpsc::Sender<std::string> output;
    ilias::mpsc::Receiver<std::string> input;
};

SseServerStream::SseServerStream() : mImpl(std::make_unique<Impl>()) {}
SseServerStream::SseServerStream(SseServerStream&&) noexcept = default;
SseServerStream::~SseServerStream()                          = default;

auto SseServerStream::operator=(SseServerStream&&) noexcept -> SseServerStream& = default;

auto SseServerStream::recv(std::vector<std::byte>& buffer) -> ilias::IoTask<void> {
    if (!mImpl) {
        co_return ilias::Err(ilias::IoError::Canceled);
    }

    auto in = co_await mImpl->input.recv();
    if (!in) {
        co_return ilias::Err(ilias::IoError::Canceled);
    }

    auto ptr = reinterpret_cast<const std::byte*>(in->data());
    buffer.assign(ptr, ptr + in->size());
    NEKO_LOG_INFO("sse", "received {} bytes: {}", buffer.size(),
                  std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()});
    co_return {};
}

auto SseServerStream::send(std::span<const std::byte> data) -> ilias::IoTask<void> {
    if (!mImpl) {
        co_return ilias::Err(ilias::IoError::Canceled);
    }

    auto output = std::string(reinterpret_cast<const char*>(data.data()), data.size());
    if (!co_await mImpl->output.send(std::move(output))) {
        co_return ilias::Err(ilias::IoError::Canceled);
    }
    co_return {};
}

auto SseServerStream::close() -> void {
    if (!mImpl) {
        return;
    }
    mImpl->output.close();
    mImpl->input.close();
}

auto SseServerStream::cancel() -> void {}

auto SseServerStream::start() -> ilias::IoTask<void> { co_return {}; }

struct SseListener::Impl {
    using SseEvent     = minihttp::server::SseEvent;
    using SseGenerator = ilias::Generator<SseEvent>;
    using Sse          = minihttp::server::Sse;
    using Query        = minihttp::server::Query<minihttp::server::UrlParams>;
    using Text         = minihttp::server::Text<std::string>;
    using Router       = minihttp::server::Router;

    explicit Impl(ilias::TcpListener listener) : listener(std::move(listener)) {}

    auto close() -> void {
        if (handle) {
            handle.stop();
            handle.wait();
            handle = nullptr;
        }
    }

    auto loop() -> ilias::Task<void> {
        auto router = Router()
                          .get("/sse", [this]() { return processIncoming(); })
                          .post("/message", [this](Query request, Text content) {
                              return processPost(std::move(request), std::move(content));
                          });
        co_await minihttp::server::serve(std::move(listener), std::move(router));
    }

    auto accept() -> ilias::IoTask<SseServerStream> {
        if (!handle) {
            auto [sender, receiver] = ilias::mpsc::channel<SseServerStream>();
            streamSender            = std::move(sender);
            streamReceiver          = std::move(receiver);
            handle                  = ilias::spawn(loop());
        }
        auto res = co_await streamReceiver.recv();
        if (!res) {
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        co_return std::move(*res);
    }

    auto processIncoming() -> ilias::Task<Sse> {
        auto [inSender, inReceiver]   = ilias::mpsc::channel<std::string>();
        auto [outSender, outReceiver] = ilias::mpsc::channel<std::string>();
        auto stream                   = SseServerStream{};
        stream.mImpl->input           = std::move(inReceiver);
        stream.mImpl->output          = std::move(outSender);
        co_await streamSender.send(std::move(stream));

        id += 1;
        sessions.insert({std::to_string(id), std::move(inSender)});
        co_return Sse(sseGenerator(std::move(outReceiver), id));
    }

    auto processPost(Query request, Text textContent) -> ilias::Task<Text> {
        auto& [params]  = request;
        auto& [content] = textContent;
        NEKO_LOG_INFO("sse", "Received POST /message?id={}, content size: {}", params["id"], content.size());
        if (!content.empty()) {
            NEKO_LOG_DEBUG("sse", "Content: {}", content);
        }

        auto it = sessions.find(params["id"]);
        if (it == sessions.end()) {
            NEKO_LOG_WARN("sse", "Session id '{}' not found. Available sessions: {}", params["id"], sessions.size());
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

    auto sseGenerator(ilias::mpsc::Receiver<std::string> input, size_t sessionId) -> SseGenerator {
        struct Guard {
            ~Guard() { self.sessions.erase(std::to_string(sessionId)); }

            Impl& self;
            size_t sessionId;
        } guard{*this, sessionId};

        co_yield SseEvent{
            .comment = {}, .event = "endpoint", .data = "/message?id=" + std::to_string(sessionId), .retry = {}};
        while (true) {
            auto [text, timeout] = co_await ilias::whenAny(input.recv(), ilias::sleep(keepAliveInterval));
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

    ilias::TcpListener listener;
    ilias::WaitHandle<void> handle;
    ilias::mpsc::Sender<SseServerStream> streamSender;
    ilias::mpsc::Receiver<SseServerStream> streamReceiver;
    std::chrono::milliseconds keepAliveInterval{15000};
    std::map<std::string, ilias::mpsc::Sender<std::string>> sessions;
    size_t id = 0;
};

SseListener::SseListener(ilias::TcpListener listener) : mImpl(std::make_unique<Impl>(std::move(listener))) {}
SseListener::SseListener(SseListener&&) noexcept = default;
SseListener::~SseListener()                      = default;

auto SseListener::operator=(SseListener&&) noexcept -> SseListener& = default;

auto SseListener::close() -> void {
    if (mImpl) {
        mImpl->close();
    }
}

auto SseListener::cancel() -> void {}

auto SseListener::accept() -> ilias::IoTask<SseServerStream> {
    if (!mImpl) {
        co_return ilias::Err(ilias::IoError::Canceled);
    }
    co_return co_await mImpl->accept();
}

NEKO_END_NAMESPACE
