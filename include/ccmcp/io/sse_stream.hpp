#pragma once

#include "../global/global.hpp"

#include <ilias/io/stream.hpp>
#include <ilias/task/task.hpp>
#include <ilias/sync/mpsc.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/message_stream_wrapper.hpp>
#include <minihttp/router.hpp>

NEKO_BEGIN_NAMESPACE

class SseServerStream {
public:
    SseServerStream(SseServerStream &&) = default;
    ~SseServerStream() = default;

    auto recv(std::vector<std::byte> &buffer) -> ilias::IoTask<void> {
        auto in = co_await mInput.recv();
        if (!in) { // EOF
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        auto ptr = reinterpret_cast<const std::byte *>(in->data());
        buffer.assign(ptr, ptr + in->size());
        co_return {};
    }

    auto send(std::span<const std::byte> data) -> ilias::IoTask<void> {
        auto output = std::string(reinterpret_cast<const char *>(data.data()), data.size());
        if (!co_await mOutput.send(std::move(output))) {
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        co_return {};
    }

    auto close() -> void {
        mOutput.close();
        mInput.close();
    }

    auto cancel() -> void {

    }

    auto start() -> ilias::IoTask<void> {
        co_return {};
    }

    auto operator =(SseServerStream &&) -> SseServerStream & = default;
private:
    SseServerStream() = default;

    ilias::mpsc::Sender<std::string>   mOutput;
    ilias::mpsc::Receiver<std::string> mInput;
friend class SseListener;
};

class SseListener {
public:
    SseListener(ilias::TcpListener listener) : mListener(std::move(listener)) {

    }

    auto close() -> void {
        if (mHandle) {
            mHandle.stop();
            mHandle.wait();
            mHandle = nullptr;
        }
    }

    auto cancel() -> void {
        
    }

    auto accept() -> ilias::IoTask<SseServerStream> {
        if (!mHandle) {
            auto [sender, receiver] = ilias::mpsc::channel<SseServerStream>();
            mSender = std::move(sender);
            mReceiver = std::move(receiver);
            mHandle = ilias::spawn(loop());
        }
        auto res = co_await mReceiver.recv();
        if (!res) {
            co_return ilias::Err(ilias::IoError::Canceled);
        }
        co_return std::move(*res);
    }
private:
    using SseEvent = minihttp::server::SseEvent;
    using SseGenerator = ilias::Generator<SseEvent>;
    using Sse = minihttp::server::Sse;
    using Query = minihttp::server::Query<minihttp::server::UrlParams>;
    using Text = minihttp::server::Text<std::string>;
    using Router = minihttp::server::Router;

    auto loop() -> ilias::Task<void> {
        auto router = Router()
            .get("/sse", [this]() {
                return processIncoming();
            })
            .post("/message", [this](Query request, Text content) {
                return processPost(std::move(request), std::move(content));
            })
        ;
        co_await minihttp::server::serve(std::move(mListener), std::move(router));
    }

    auto processIncoming() -> ilias::Task<Sse> {
        auto [inSender, inReceiver] = ilias::mpsc::channel<std::string>();
        auto [outSender, outReceiver] = ilias::mpsc::channel<std::string>();
        auto stream = SseServerStream {};
        stream.mInput = std::move(inReceiver);
        stream.mOutput = std::move(outSender);
        co_await mSender.send(std::move(stream));

        // Register the session
        mId += 1;
        mSessions.insert({std::to_string(mId), std::move(inSender)});
        co_return Sse(sseGenerator(std::move(outReceiver), mId));
    }

    auto processPost(Query request, Text textContent) -> ilias::Task<Text> {
        auto &[params] = request;
        auto &[content] = textContent;
        auto it = mSessions.find(params["id"]);
        if (it == mSessions.end()) {
            co_return Text("Invalid id");
        }
        auto &[_, sender] = *it;
        co_await sender.send(std::move(content));
        co_return {};
    }

    auto sseGenerator(ilias::mpsc::Receiver<std::string> input, size_t id) -> SseGenerator {
        // FIXME: possible leak on here, need fix in minihttp
        struct Guard {
            ~Guard() {
                self.mSessions.erase(std::to_string(id));
            }

            SseListener &self;
            size_t id;
        } guard {*this, id};

        co_yield SseEvent { .event = "endpoint", .data = "/message?id=" + std::to_string(id)};
        while (true) {
            auto text = co_await input.recv();
            if (!text) {
                co_return;
            }
            co_yield SseEvent { .event = "message", .data = *text};
        }
    }

    ilias::TcpListener      mListener;
    ilias::WaitHandle<void> mHandle;
    ilias::mpsc::Sender<SseServerStream> mSender;
    ilias::mpsc::Receiver<SseServerStream> mReceiver;

    std::map<std::string, ilias::mpsc::Sender<std::string> > mSessions; // endpoint -> sender
    size_t mId = 0;
};

NEKO_END_NAMESPACE