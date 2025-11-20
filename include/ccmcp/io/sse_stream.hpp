#pragma once

#include "../global/global.hpp"

#include <ilias/io/stream.hpp>
#include <ilias/task/task.hpp>
#include <ilias/sync/mpsc.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/message_stream_wrapper.hpp>
#include "./detail/minihttp.hpp"

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

    // auto start() -> ilias::IoTask<void> {
    //     if (!mListener) {
    //         co_return ilias::Err(JsonRpcError::ClientNotInit);
    //     }
    //     mHandle = ilias::spawn(loop());
    //     co_return {};
    // }

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
    auto loop() -> ilias::Task<void> {
        auto router = minihttp::Router()
            .route("/sse", minihttp::get([this]() -> ilias::Task<minihttp::Sse> {
                return processIncoming();
            }))
            .route("/message", minihttp::post([this](minihttp::Request request) -> ilias::Task<minihttp::Text<std::string> > {
                return processPost(std::move(request));
            }))
        ;
        co_await minihttp::serve(std::move(mListener), router);
    }

    auto processIncoming() -> ilias::Task<minihttp::Sse> {
        auto [inSender, inReceiver] = ilias::mpsc::channel<std::string>();
        auto [outSender, outReceiver] = ilias::mpsc::channel<std::string>();
        auto stream = SseServerStream {};
        stream.mInput = std::move(inReceiver);
        stream.mOutput = std::move(outSender);
        co_await mSender.send(std::move(stream));
        

        mId += 1;
        mSessions.insert({std::to_string(mId), std::move(inSender)});
        co_return minihttp::Sse(sseGenerator(std::move(outReceiver), mId));
    }

    auto processPost(minihttp::Request request) -> ilias::Task<minihttp::Text<std::string> > {
        co_return {};
    }

    auto sseGenerator(ilias::mpsc::Receiver<std::string> input, size_t id) -> ilias::Generator<minihttp::SseEvent> {
        co_yield minihttp::SseEvent { .event = "endpoint", .data = "/message?id=" + std::to_string(id)};
        while (true) {
            auto text = co_await input.recv();
            if (!text) {
                co_return;
            }
            co_yield minihttp::SseEvent { .event = "message", .data = *text};
        }
    }

    ilias::TcpListener      mListener;
    ilias::WaitHandle<void> mHandle;
    ilias::mpsc::Sender<SseServerStream> mSender;
    ilias::mpsc::Receiver<SseServerStream> mReceiver;

    // TODO: use isolated endpoint to receive the client post
    std::map<std::string, ilias::mpsc::Sender<std::string> > mSessions; // endpoint -> sender
    size_t mId = 0;
};

NEKO_END_NAMESPACE