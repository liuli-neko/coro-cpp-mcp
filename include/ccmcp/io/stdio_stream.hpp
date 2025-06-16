#pragma once

#include "../global/global.hpp"

#include <ilias/fs/console.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/datagram_wapper.hpp>

NEKO_BEGIN_NAMESPACE

template <>
class DatagramClient<ILIAS_NAMESPACE::Console, void>
    : public DatagramBase, public DatagramClientBase, public DatagramServerBase {
public:
    DatagramClient() {}

    auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> override {
        if (!in) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        auto ret = co_await (in.getline("\n") | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret) {
            std::swap(json, ret.value());
            NEKO_LOG_INFO("DatagramClient", "recv: {}", json);
            if (json.find('{') == std::string::npos && json.find('[') == std::string::npos &&
                json.find("exit") != std::string::npos) {
                NEKO_LOG_INFO("DatagramClient", "exit");
                mIsCancel = true;
                co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ILIAS_NAMESPACE::Error::Canceled);
            }
            co_return std::span<std::byte>{reinterpret_cast<std::byte*>(json.data()), json.size()};
        } else {
            co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> override {
        if (!out) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        // TODO: USE BETTER WAY
        std::string json{reinterpret_cast<const char*>(data.data()), data.size()};
        json.push_back('\n');
        ::fputs(json.c_str(), stdout);
        ::fflush(stdout);
        // auto ret = co_await (out.writeAll(ILIAS_NAMESPACE::makeBuffer(json)) | ILIAS_NAMESPACE::ignoreCancellation);
        // if (!ret) {
            // co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
        // }
        // NEKO_LOG_INFO("ccmcp", "Sent {}", std::string_view{reinterpret_cast<const char*>(data.data()), data.size()});
        co_return {};
    }

    auto close() -> void override {
        if (in) {
            in.close();
        }
        if (out) {
            out.close();
        }
        mIsCancel = false;
    }

    auto cancel() -> void override {
        mIsCancel = true;
        if (in) {
            in.cancel();
        }
        if (out) {
            out.cancel();
        }
    }

    // like stdio://stdout-stdin
    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 8) != "stdio://");
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        mIsCancel = false;
        auto type = url.substr(9);
        if (type.find("stdout")) {
            if (auto ret = co_await ILIAS_NAMESPACE::Console::fromStdout(); ret) {
                out = std::move(ret.value());
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        }
        if (type.find("stdin")) {
            if (auto ret = co_await ILIAS_NAMESPACE::Console::fromStdin(); ret) {
                in = std::move(ret.value());
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        }
        if (type.find("stderr")) {
            if (auto ret = co_await ILIAS_NAMESPACE::Console::fromStderr(); ret) {
                out = std::move(ret.value());
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        }
        co_return {};
    }

    auto isConnected() -> bool override { return (bool)in && (bool)out; }
    auto isListening() -> bool override { return (bool)in && (bool)out; }

    auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(this, [](DatagramClientBase*) {});
    }

    ILIAS_NAMESPACE::Console in;
    ILIAS_NAMESPACE::Console out;
    std::string json;
    bool mIsCancel = false;
};

NEKO_END_NAMESPACE