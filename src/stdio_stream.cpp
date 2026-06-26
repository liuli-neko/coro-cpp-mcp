#include "ccmcp/io/stdio_stream.hpp"

#include <ilias/fs.hpp>
#include <ilias/io.hpp>
#include <ilias/io/stream.hpp>
#include <ilias/task/utils.hpp>
#include <nekoproto/global/log.hpp>
#include <nekoproto/jsonrpc/jsonrpc_error.hpp>

#include <string>
#include <utility>

NEKO_BEGIN_NAMESPACE

struct StdioStream::Impl {
    using Stdin  = ILIAS_NAMESPACE::Stdin;
    using Stdout = ILIAS_NAMESPACE::Stdout;
    template <typename T>
    using BufReader = ILIAS_NAMESPACE::BufReader<T>;
    template <typename T>
    using BufWriter = ILIAS_NAMESPACE::BufWriter<T>;

    BufReader<Stdin> in   = {Stdin{}};
    BufWriter<Stdout> out = {Stdout{}};
    std::string json;
};

StdioStream::StdioStream() : mImpl(std::make_unique<Impl>()) {}
StdioStream::StdioStream(StdioStream&&) noexcept = default;
StdioStream::~StdioStream()                      = default;

auto StdioStream::operator=(StdioStream&&) noexcept -> StdioStream& = default;

auto StdioStream::recv(std::vector<std::byte>& buffer) -> IoTask<void> {
    if (!mImpl || !mImpl->in) {
        NEKO_LOG_ERROR("DatagramClient", "recv: client not init");
        co_return ILIAS_NAMESPACE::Err(JsonRpcError::ClientNotInit);
    }
    if (auto ret = co_await (mImpl->in.getline("\n")); ret) {
        std::swap(mImpl->json, ret.value());
        NEKO_LOG_INFO("DatagramClient", "recv: {}", mImpl->json);
        if (mImpl->json.find('{') == std::string::npos && mImpl->json.find('[') == std::string::npos &&
            mImpl->json.find("exit") != std::string::npos) {
            NEKO_LOG_INFO("DatagramClient", "exit");
            co_return ILIAS_NAMESPACE::Err(ILIAS_NAMESPACE::IoError::Canceled);
        }
        buffer = std::vector<std::byte>{reinterpret_cast<std::byte*>(mImpl->json.data()),
                                        reinterpret_cast<std::byte*>(mImpl->json.data()) + mImpl->json.size()};
        co_return {};
    } else {
        NEKO_LOG_ERROR("DatagramClient", "recv: {}", ret.error().message());
        co_return ILIAS_NAMESPACE::Err(ret.error());
    }
}

auto StdioStream::send(std::span<const std::byte> data) -> IoTask<void> {
    if (data.empty()) {
        co_return {};
    }
    if (!mImpl || !mImpl->out) {
        co_return ILIAS_NAMESPACE::Err(JsonRpcError::ClientNotInit);
    }
    if (auto ret = co_await (mImpl->out.writeAll(data) | ILIAS_NAMESPACE::unstoppable()); !ret) {
        co_return ILIAS_NAMESPACE::Err(ret.error());
    }
    std::byte end[1] = {std::byte{'\n'}};
    if (auto ret = co_await (mImpl->out.writeAll(end) | ILIAS_NAMESPACE::unstoppable()); !ret) {
        co_return ILIAS_NAMESPACE::Err(ret.error());
    }
    co_return co_await mImpl->out.flush();
}

auto StdioStream::close() -> void {
    if (!mImpl) {
        return;
    }
    if (mImpl->in) {
        (void)std::exchange(mImpl->in, {}).detach();
    }
    if (mImpl->out) {
        (void)std::exchange(mImpl->out, {}).detach();
    }
}

auto StdioStream::start() -> IoTask<void> {
    NEKO_LOG_INFO("DatagramClient", "start ");
    if (!mImpl || !mImpl->in) {
        NEKO_LOG_ERROR("DatagramClient", "failed to open stdin");
        co_return ILIAS_NAMESPACE::Err(ILIAS_NAMESPACE::IoError::BadFileDescriptor);
    }
    if (!mImpl->out) {
        NEKO_LOG_ERROR("DatagramClient", "failed to open stdout");
        co_return ILIAS_NAMESPACE::Err(ILIAS_NAMESPACE::IoError::BadFileDescriptor);
    }
    co_return {};
}

auto StdioStream::shutdown() -> IoTask<void> {
    NEKO_LOG_INFO("DatagramClient", "shutdown ");
    if (!mImpl || !mImpl->out) {
        co_return ILIAS_NAMESPACE::Err(ILIAS_NAMESPACE::IoError::BadFileDescriptor);
    }
    co_return co_await mImpl->out.shutdown();
}

auto StdioStream::flush() -> IoTask<void> {
    NEKO_LOG_INFO("DatagramClient", "flush ");
    if (!mImpl || !mImpl->out) {
        co_return ILIAS_NAMESPACE::Err(ILIAS_NAMESPACE::IoError::BadFileDescriptor);
    }
    co_return co_await mImpl->out.flush();
}

NEKO_END_NAMESPACE
