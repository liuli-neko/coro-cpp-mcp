#pragma once

#include "../global/global.hpp"

#include <ilias/fs/console.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/message_stream_wrapper.hpp>

NEKO_BEGIN_NAMESPACE

class StdioStream {
    using Console = ILIAS_NAMESPACE::Console;
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    using IliasError = ILIAS_NAMESPACE::Error;

public:
    StdioStream() {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void> {
        if (!mIn) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        auto ret = co_await (mIn.getline("\n") | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret) {
            std::swap(mJson, ret.value());
            NEKO_LOG_INFO("DatagramClient", "recv: {}", mJson);
            if (mJson.find('{') == std::string::npos && mJson.find('[') == std::string::npos &&
                mJson.find("exit") != std::string::npos) {
                NEKO_LOG_INFO("DatagramClient", "exit");
                co_return Unexpected<IliasError>(IliasError::Canceled);
            }
            buffer = std::vector<std::byte>{reinterpret_cast<std::byte*>(mJson.data()),
                                            reinterpret_cast<std::byte*>(mJson.data()) + mJson.size()};
            co_return {};
        } else {
            co_return Unexpected<IliasError>(ret.error());
        }
    }

    auto send(std::span<const std::byte> data) -> IoTask<void> {
        // TODO: USE BETTER WAY
        ::fwrite(reinterpret_cast<const char*>(data.data()), 1, data.size(), stdout);
        ::fputc('\n', stdout);
        ::fflush(stdout);
        co_return {};
    }

    auto close() -> void {
        if (mIn) {
            mIn.close();
        }
        if (mOut) {
            mOut.close();
        }
    }

    auto cancel() -> void {
        if (mIn) {
            mIn.cancel();
        }
        if (mOut) {
            mOut.cancel();
        }
    }

    auto start() -> IoTask<void> {
        if (auto ret = co_await Console::fromStdin(); ret) {
            mIn = std::move(ret.value());
        } else {
            co_return Unexpected(ret.error());
        }
        // ilias console out has bug in this version in mcp-server
        // if (auto ret = co_await Console::fromStdout(); ret) {
        //     mOut = std::move(ret.value());
        // } else {
        //     co_return Unexpected(ret.error());
        // }
        co_return {};
    }

private:
    Console mIn;
    Console mOut;
    std::string mJson;
};

NEKO_END_NAMESPACE