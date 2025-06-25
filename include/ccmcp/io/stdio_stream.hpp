#pragma once

#include "../global/global.hpp"

#include <ilias/fs/console.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/datagram_wapper.hpp>

NEKO_BEGIN_NAMESPACE

struct Stdio {};

template <>
class MessageStream<Stdio, void> : public IMessageStream, public IMessageStreamClient, public IMessageStreamServer {
    using Console = ILIAS_NAMESPACE::Console;
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    using IliasError = ILIAS_NAMESPACE::Error;

public:
    MessageStream() {}

    auto recv() -> IoTask<std::span<std::byte>> override {
        if (!mIn) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return Unexpected(IliasError::Canceled);
        }
        auto ret = co_await (mIn.getline("\n") | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret) {
            std::swap(mJson, ret.value());
            NEKO_LOG_INFO("DatagramClient", "recv: {}", mJson);
            if (mJson.find('{') == std::string::npos && mJson.find('[') == std::string::npos &&
                mJson.find("exit") != std::string::npos) {
                NEKO_LOG_INFO("DatagramClient", "exit");
                mIsCancel = true;
                co_return Unexpected<IliasError>(IliasError::Canceled);
            }
            co_return std::span<std::byte>{reinterpret_cast<std::byte*>(mJson.data()), mJson.size()};
        } else {
            co_return Unexpected<IliasError>(ret.error());
        }
    }

    auto send(std::span<const std::byte> data) -> IoTask<void> override {
        if (!mOut) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return Unexpected(IliasError::Canceled);
        }
        // TODO: USE BETTER WAY
        ::fwrite(reinterpret_cast<const char*>(data.data()), 1, data.size(), stdout);
        ::fputc('\n', stdout);
        ::fflush(stdout);
        // auto ret = co_await (mOut.writeAll(ILIAS_NAMESPACE::makeBuffer(mJson)) |
        // ILIAS_NAMESPACE::ignoreCancellation); if (!ret) { co_return
        // Unexpected<IliasError>(ret.error());
        // }
        // NEKO_LOG_INFO("ccmcp", "Sent {}", std::string_view{reinterpret_cast<const char*>(data.data()), data.size()});
        co_return {};
    }

    auto close() -> void override {
        if (mIn) {
            mIn.close();
        }
        if (mOut) {
            mOut.close();
        }
        mIsCancel = false;
    }

    auto cancel() -> void override {
        mIsCancel = true;
        if (mIn) {
            mIn.cancel();
        }
        if (mOut) {
            mOut.cancel();
        }
    }

    // like stdio://stdout-stdin
    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 8) != "stdio://");
    }

    auto start(std::string_view url) -> IoTask<void> override {
        mIsCancel = false;
        auto type = url.substr(9);
        if (type.find("stdin")) {
            if (auto ret = co_await Console::fromStdin(); ret) {
                mIn = std::move(ret.value());
            } else {
                co_return Unexpected(ret.error());
            }
        }
        if (type.find("stdout")) {
            if (auto ret = co_await Console::fromStdout(); ret) {
                mOut = std::move(ret.value());
            } else {
                co_return Unexpected(ret.error());
            }
        }
        if (type.find("stderr")) {
            if (auto ret = co_await Console::fromStderr(); ret) {
                mOut = std::move(ret.value());
            } else {
                co_return Unexpected(ret.error());
            }
        }
        co_return {};
    }

    auto isConnected() -> bool override { return (bool)mIn && (bool)mOut; }
    auto isListening() -> bool override { return (bool)mIn && (bool)mOut; }

    auto accept() -> IoTask<std::unique_ptr<IMessageStreamClient, void (*)(IMessageStreamClient*)>> override {
        co_return std::unique_ptr<IMessageStreamClient, void (*)(IMessageStreamClient*)>(this,
                                                                                         [](IMessageStreamClient*) {});
    }

private:
    Console mIn;
    Console mOut;
    std::string mJson;
    bool mIsCancel = false;
};

NEKO_END_NAMESPACE