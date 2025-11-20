#pragma once

#include "../global/global.hpp"

#include <ilias/fs/console.hpp>
#include <ilias/io/stream.hpp>
#include <ilias/task/task.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/message_stream_wrapper.hpp>

NEKO_BEGIN_NAMESPACE

class StdioStream {
    using Console = ILIAS_NAMESPACE::Console;
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;
    template <typename T>
    using Task = ILIAS_NAMESPACE::Task<T>;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    using IliasError = ILIAS_NAMESPACE::IoError;
    template <typename T>
    using BufReader = ILIAS_NAMESPACE::BufReader<T>;
    template <typename T>
    using BufWriter = ILIAS_NAMESPACE::BufWriter<T>;

public:
    StdioStream() {}

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void> {
        if (!mIn) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto ret = co_await (mIn.getline("\n") | ILIAS_NAMESPACE::unstoppable()); ret) {
            std::swap(mJson, ret.value());
            NEKO_LOG_INFO("DatagramClient", "recv: {}", mJson);
            if (mJson.find('{') == std::string::npos && mJson.find('[') == std::string::npos &&
                mJson.find("exit") != std::string::npos) {
                NEKO_LOG_INFO("DatagramClient", "exit");
                co_return Unexpected(IliasError::Canceled);
            }
            buffer = std::vector<std::byte>{reinterpret_cast<std::byte*>(mJson.data()),
                                            reinterpret_cast<std::byte*>(mJson.data()) + mJson.size()};
            co_return {};
        } else {
            co_return Unexpected(ret.error());
        }
    }

    auto send(std::span<const std::byte> data) -> IoTask<void> {
        if (data.empty()) {
            co_return {};
        }
        if (!mOut) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto ret = co_await (mOut.writeAll(data) | ILIAS_NAMESPACE::unstoppable()); !ret) {
            co_return Unexpected(ret.error());
        }
        std::byte end [1] = {std::byte{'\n'}};
        if (auto ret = co_await (mOut.writeAll(end) | ILIAS_NAMESPACE::unstoppable()); !ret) {
            co_return Unexpected(ret.error());
        }
        co_return co_await mOut.flush();
    }

    auto close() -> void {
        if (mIn) {
            auto console = std::exchange(mIn, {}).detach();
            console.close();
        }
        if (mOut) {
            auto console = std::exchange(mOut, {}).detach();
            console.close();
        }
    }

    auto cancel() -> void {
        if (mIn) {
            auto& console = mIn.nextLayer();
            console.cancel();
        }
        if (mOut) {
            auto& console = mOut.nextLayer();
            console.cancel();
        }
    }

    auto start() -> IoTask<void> {
        if (auto ret = co_await Console::fromStdin(); ret) {
            mIn = BufReader(std::move(ret.value()));
        } else {
            co_return Unexpected(ret.error());
        }
        if (auto ret = co_await Console::fromStdout(); ret) {
            mOut = std::move(ret.value());
        } else {
            co_return Unexpected(ret.error());
        }
        co_return {};
    }

private:
    BufReader<Console> mIn;
    BufWriter<Console> mOut;
    std::string mJson;
};

NEKO_END_NAMESPACE