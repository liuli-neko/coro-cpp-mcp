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
    using IliasError  = ILIAS_NAMESPACE::Error;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    using Event      = ILIAS_NAMESPACE::Event;
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;
    template <typename T>
    using Task        = ILIAS_NAMESPACE::Task<T>;
    using HttpRequest = ILIAS_NAMESPACE::HttpRequest;
    using HttpReply   = ILIAS_NAMESPACE::HttpReply;

public:
    DatagramClient() = default;
    DatagramClient(HttpSession&& mClient) : mClient(std::make_unique<HttpSession>(std::move(mClient))) {}

    auto recv() -> IoTask<std::span<std::byte>> override {
        if (mIsCancel) {
            co_return Unexpected(IliasError::Canceled);
        }
        while (mBuffer.empty()) {
            mEvent.set();
            if (auto ret = co_await mEvent; !ret) {
                co_return Unexpected(ret.error());
            }
        }
        co_return mBuffer;
    }

    auto send(std::span<const std::byte> data) -> IoTask<void> override {
        if (mIsCancel) {
            co_return Unexpected(IliasError::Canceled);
        }
        HttpRequest req;
        req.setUrl(mUrl);
        req.setHeader("Content-Type", "text/mEvent-stream");
        req.setHeader("Cache-Control", "no-cache");
        req.setHeader("Connection", "keep-alive");

        auto ret = co_await (mClient->post(req, data) | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret) {
            co_return Unexpected(ret.error_or(IliasError::Unknown));
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

    auto start(std::string_view url) -> IoTask<void> override {
        mIsCancel = false;
        mClient   = std::make_unique<HttpSession>(co_await HttpSession::make());
        mUrl      = Url(mScheme + "://" + std::string(url.substr(6)));
        if (!mUrl.isValid()) {
            co_return Unexpected(IliasError::InvalidArgument);
        }
        co_return {};
    }

    auto isConnected() -> bool override { return mClient != nullptr; }

private:
    Task<void> reply(HttpReply& reply) {
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
    Event mEvent;
    std::vector<std::byte> mBuffer;
    std::string mScheme = std::string(SSE_DEFAULT_SCHEME);
    Url mUrl;
    bool mIsCancel = false;
};

template <>
class DatagramClient<SseServer, void> : public DatagramBase, public DatagramServerBase {
    using IliasError = ILIAS_NAMESPACE::Error;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    using Event      = ILIAS_NAMESPACE::Event;
    template <typename T>
    using IoTask       = ILIAS_NAMESPACE::IoTask<T>;
    using HttpSession  = ILIAS_NAMESPACE::HttpSession;
    using TcpListener  = ILIAS_NAMESPACE::TcpListener;
    using TcpClient    = ILIAS_NAMESPACE::TcpClient;
    using IPEndpoint   = ILIAS_NAMESPACE::IPEndpoint;
    using Url          = ILIAS_NAMESPACE::Url;
    using Reply        = ILIAS_NAMESPACE::HttpReply;
    using Request      = ILIAS_NAMESPACE::HttpRequest;
    using ReuseAddress = ILIAS_NAMESPACE::sockopt::ReuseAddress;

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

    auto start(std::string_view url) -> IoTask<void> override {
        mIsCancel       = false;
        auto ipendpoint = IPEndpoint::fromString(url.substr(6));
        if (!ipendpoint) {
            co_return Unexpected(IliasError::InvalidArgument);
        }
        if (auto ret = co_await TcpListener::make(ipendpoint->family()); ret) {
            ret.value().setOption(ReuseAddress(1));
            if (auto ret1 = ret.value().bind(ipendpoint.value()); ret1) {
                listener = std::move(ret.value());
                co_return {};
            } else {
                co_return Unexpected(ret1.error());
            }
        } else {
            co_return Unexpected(ret.error());
        }
    }

    auto accept() -> IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        if (!listener) {
            co_return Unexpected(JsonRpcError::ClientNotInit);
        }
        if (mIsCancel) {
            co_return Unexpected(IliasError::Canceled);
        }
        if (auto ret = co_await listener.accept(); ret) {
            mClient = std::make_unique<HttpSession>(std::move(ret.value().first));

            co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(
                new DatagramClient<TcpClient>(std::move(ret.value().first)),
                +[](DatagramClientBase* ptr) { delete ptr; });
        } else {
            co_return Unexpected(ret.error());
        }
    }

private:
    TcpListener listener;
    std::unique_ptr<HttpSession> mClient;
    Event mEvent;
    std::vector<std::byte> mBuffer;
    std::string mSchema = std::string(SSE_DEFAULT_SCHEME);
    bool mIsCancel      = false;
};

NEKO_END_NAMESPACE