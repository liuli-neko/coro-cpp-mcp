#pragma once

#include <minihttp/defines.hpp>
#include <minihttp/url.hpp>
#include <ilias/net.hpp>
#include <ilias/tls.hpp>
#include <ilias/io.hpp>
#include <variant>

namespace minihttp {

/**
 * @brief The url stream class, it takes a url and connect to it, and provide a stream to read from and write to
 * 
 */
class UrlStream final : public ilias::StreamMethod<UrlStream> {
public:
    UrlStream() = default;
    UrlStream(const UrlStream &) = delete;
    UrlStream(UrlStream &&) = default;
    ~UrlStream() = default;

    template <typename T> 
        requires (std::convertible_to<T, Url>)
    UrlStream(T &&value) : mUrl(std::forward<T>(value)) {}

    /**
     * @brief Do the connect to the url
     * 
     * @return IoTask<void> 
     */
    auto connect() -> IoTask<void> {
        if (!std::holds_alternative<std::monostate>(mStorage)) {
            co_return {}; // Already connected
        }
        auto scheme = mUrl.scheme();
        auto port = 0;
        auto tls = false;
        if (scheme == "http" || scheme == "ws") {
            port = 80;
        }
        else if (scheme == "https" || scheme == "wss") {
            if (!mTlsCtxt) {
                co_return Err(IoError::ProtocolNotSupported); // No tls context
            }
            port = 443;
            tls = true;
        }
        else {
            co_return Err(IoError::InvalidArgument);
        }
        // Then do the connect
        auto addr = co_await ilias::AddressInfo::fromHostname(mUrl.host(), std::to_string(port));
        if (!addr) {
            co_return Err(addr.error());
        }

        ilias::TcpClient client;
        std::error_code lastErr;
        for (auto endpoint : addr->endpoints()) {
            auto res = co_await ilias::TcpClient::connect(endpoint);
            if (!res) {
                lastErr = res.error();
                continue;
            }
            client = std::move(res.value());
            break;
        }
        if (!client) {
            co_return Err(lastErr);
        }
        if (!tls) { // Done
            mStorage = std::move(client);
            co_return {};
        }
        // We need to do the tls handshake
        ilias::TlsStream stream {*mTlsCtxt, std::move(client)};
        stream.setHostname(mUrl.host());
        if (auto res = co_await stream.handshake(); !res) {
            co_return Err(res.error());
        }
        mStorage = std::move(stream);
        co_return {};
    }

    // Readable
    auto read(MutableBuffer buffer) -> IoTask<size_t> {
        const auto visitor = Overloads {
            [&](std::monostate) -> IoTask<size_t> {
                co_return Err(IoError::SocketIsNotConnected);
            },
            [&](ilias::TcpClient &stream) -> IoTask<size_t> {
                return stream.read(buffer);
            },
            [&](ilias::TlsStream<ilias::TcpClient> &stream) -> IoTask<size_t> {
                return stream.read(buffer);  
            }
        };
        return std::visit(visitor, mStorage);
    }

    // Writable
    auto write(Buffer buffer) -> IoTask<size_t> {
        const auto visitor = Overloads {
            [&](std::monostate) -> IoTask<size_t> {
                co_return Err(IoError::SocketIsNotConnected);
            },
            [&](ilias::TcpClient &stream) -> IoTask<size_t> {
                return stream.write(buffer);
            },
            [&](ilias::TlsStream<ilias::TcpClient> &stream) -> IoTask<size_t> {
                return stream.write(buffer);
            }
        };
        return std::visit(visitor, mStorage);
    }

    auto flush() -> IoTask<void> {
        const auto visitor = Overloads {
            [&](std::monostate) -> IoTask<void> {
                co_return Err(IoError::SocketIsNotConnected);
            },
            [&](ilias::TcpClient &stream) -> IoTask<void> {
                return stream.flush();
            },
            [&](ilias::TlsStream<ilias::TcpClient> &stream) -> IoTask<void> {
                return stream.flush();
            }
        };
        return std::visit(visitor, mStorage);
    }

    auto shutdown() -> IoTask<void> {
        const auto visitor = Overloads {
            [](std::monostate) -> IoTask<void> {
                co_return Err(IoError::SocketIsNotConnected);
            },
            [&](ilias::TcpClient &stream) -> IoTask<void> {
                return stream.shutdown();
            },
            [&](ilias::TlsStream<ilias::TcpClient> &stream) -> IoTask<void> {
                return stream.shutdown();
            }
        };
        return std::visit(visitor, mStorage);
    }

    // Layered
    auto nextLayer() -> ilias::TcpClient & {
        const auto visitor = Overloads {
            [](std::monostate) -> ilias::TcpClient & {
                throw std::runtime_error("Socket is not connected");
            },
            [&](ilias::TcpClient &stream) -> ilias::TcpClient & {
                return stream;
            },
            [&](ilias::TlsStream<ilias::TcpClient> &stream) -> ilias::TcpClient & {
                return stream.nextLayer();
            },
        };
        return std::visit(visitor, mStorage);
    }

    // Config
    auto setTlsContext(ilias::TlsContext &context) -> void {
        mTlsCtxt = &context;
    }

    // Query
    auto url() const -> const Url & {
        return mUrl;
    }

    // Check if the stream is connected
    explicit operator bool() const noexcept {
        return !std::holds_alternative<std::monostate>(mStorage);
    }
private:
    std::variant<
        std::monostate, // Empty state
        ilias::TcpClient, // Http & ws
        ilias::TlsStream<ilias::TcpClient> // Https & wss
    > mStorage;

    Url mUrl;
    ilias::TlsContext *mTlsCtxt = nullptr;
};

} // namespace minihttp