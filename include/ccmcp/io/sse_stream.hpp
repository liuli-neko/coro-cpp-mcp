#pragma once

#include "../global/global.hpp"

#include <ilias/http.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/datagram_wapper.hpp>

NEKO_BEGIN_NAMESPACE
struct SseClient {};
struct SseServer {};

static constexpr int SSE_DEFAULT_PORT                = 8080;
static constexpr std::string_view SSE_DEFAULT_HOST   = "localhost";
static constexpr std::string_view SSE_DEFAULT_SCHEME = "http";

template <>
class DatagramClient<SseClient, void> : public DatagramBase, public DatagramClientBase {
    using HttpSession = ILIAS_NAMESPACE::HttpSession;
    using Url         = ILIAS_NAMESPACE::Url;

public:
    DatagramClient() = default;
    DatagramClient(HttpSession&& mClient) : mClient(std::make_unique<HttpSession>(std::move(mClient))) {}

    auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> override {
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        while (mBuffer.empty()) {
            mEvent.set();
            if (auto ret = co_await mEvent; !ret) {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        }
        co_return mBuffer;
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> override {
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        ILIAS_NAMESPACE::HttpRequest req;
        req.setUrl(mUrl);
        req.setHeader("Content-Type", "text/mEvent-stream");
        req.setHeader("Cache-Control", "no-cache");
        req.setHeader("Connection", "keep-alive");

        auto ret = co_await (mClient->post(req, data) | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error::Unknown));
        }
        co_await reply(ret.value());

        co_return {};
    }

    auto close() -> void override {
        mClient.reset();
        mIsCancel = false;
    }

    auto cancel() -> void override { mIsCancel = true; }

    // url like : sse://127.0.0.1:8080
    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "sse://") && type == Type::Client;
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        mIsCancel = false;
        mClient   = std::make_unique<HttpSession>(co_await HttpSession::make());
        mUrl      = Url(mScheme + "://" + std::string(url.substr(6)));
        if (!mUrl.isValid()) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        co_return {};
    }

    auto isConnected() -> bool override { return mClient != nullptr; }

private:
    ILIAS_NAMESPACE::Task<void> reply(ILIAS_NAMESPACE::HttpReply& reply) {
        if (reply.statusCode() == 200) {
            if (auto ret = co_await reply.content(); ret) {
                mBuffer = std::move(*ret);
                mEvent.set();
            }
        } else {
            NEKO_LOG_INFO("sse", "got unexpected status code: {} - {}", reply.statusCode(), reply.status());
        }
    }

private:
    std::unique_ptr<HttpSession> mClient;
    ILIAS_NAMESPACE::Event mEvent;
    std::vector<std::byte> mBuffer;
    std::string mScheme = std::string(SSE_DEFAULT_SCHEME);
    Url mUrl;
    bool mIsCancel = false;
};

template <>
class DatagramClient<SseServer, void> : public DatagramBase, public DatagramServerBase {
    using HttpSession = ILIAS_NAMESPACE::HttpSession;

public:
    auto close() -> void override {
        if (listener) {
            listener.close();
        }
        mClient.reset();
        mIsCancel = false;
    }

    auto cancel() -> void override {
        if (listener) {
            listener.cancel();
        }
        mIsCancel = true;
    }

    auto isListening() -> bool override { return (bool)listener; }

    auto checkProtocol(Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "sse://") && type == Type::Server;
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        mIsCancel       = false;
        auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
        if (!ipendpoint) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        if (auto ret = co_await ILIAS_NAMESPACE::TcpListener::make(ipendpoint->family()); ret) {
            ret.value().setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
            if (auto ret1 = ret.value().bind(ipendpoint.value()); ret1) {
                listener = std::move(ret.value());
                co_return {};
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

    auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        if (!listener) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Canceled);
        }
        if (auto ret = co_await listener.accept(); ret) {
            mClient = std::make_unique<HttpSession>(std::move(ret.value().first));

            co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(
                new DatagramClient<ILIAS_NAMESPACE::TcpClient>(std::move(ret.value().first)),
                +[](DatagramClientBase* ptr) { delete ptr; });
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

private:
    ILIAS_NAMESPACE::TcpListener listener;
    std::unique_ptr<HttpSession> mClient;
    ILIAS_NAMESPACE::Event mEvent;
    std::vector<std::byte> mBuffer;
    std::string mSchema = std::string(SSE_DEFAULT_SCHEME);
    bool mIsCancel      = false;
};

NEKO_END_NAMESPACE