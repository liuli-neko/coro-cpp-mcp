#pragma once

#include "ccmcp/global/global.hpp"

#include "ccmcp/model/jsonrpc_protocol.hpp"

CCMCP_BN
template <typename ToolFunctions>
class McpClient;

template <>
class McpClient<void> {
protected:
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;
    template <typename T>
    using Task       = ILIAS_NAMESPACE::Task<T>;
    using IoContext  = ILIAS_NAMESPACE::IoContext;
    using IliasError = ILIAS_NAMESPACE::Error;
    using TaskScope  = ILIAS_NAMESPACE::TaskScope;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    template <typename T>
    using JsonRpcClient = NEKO_NAMESPACE::JsonRpcClient<T>;
    template <typename T>
    using Reflect            = NEKO_NAMESPACE::Reflect<T>;
    using JsonSerializer     = NEKO_NAMESPACE::JsonSerializer;
    using ScopedCancelHandle = ILIAS_NAMESPACE::ScopedCancelHandle;

    template <typename RetT>
    auto handlerCallResult(const std::vector<std::variant<TextContent, ImageContent, EmbeddedResource>>& contents)
        -> std::optional<RetT> {
        auto rc = [](std::variant<TextContent, ImageContent, EmbeddedResource> content, const auto& _t) {
            using T = std::decay_t<decltype(_t)>;
            switch (content.index()) {
            case 0:
                return recover_content<T>(std::get<0>(content));
            case 1:
                return recover_content<T>(std::get<1>(content));
            case 2:
            default:
                return std::optional<T>{std::nullopt};
            }
        };
        RetT respon;
        if constexpr (NEKO_NAMESPACE::detail::has_values_meta<RetT>) {
            int idx = 0;
            Reflect<RetT>::forEachWithoutName(respon, [&contents, &idx, &rc](auto& field) {
                if (auto val = rc(contents[idx++], std::decay_t<decltype(field)>{}); val) {
                    field = *val;
                }
            });
            return respon;
        }
        if constexpr (requires { std::begin(respon); } && !(std::is_same_v<RetT, std::string>) &&
                      !(std::is_same_v<RetT, std::u8string>) && !(std::is_same_v<RetT, std::string_view>) &&
                      !(std::is_same_v<RetT, std::u8string_view>)) {
            for (auto& content : contents) {
                if (auto val = rc(content, std::decay_t<decltype(respon[0])>{}); val) {
                    respon.push_back(*val);
                }
            }
            return respon;
        } else {
            if (contents.empty()) {
                return std::nullopt;
            }
            if (auto val = rc(contents[0], std::decay_t<decltype(respon)>{}); val) {
                return *val;
            }
        }
        return std::nullopt;
    }

public:
    McpClient(IoContext& ctxt) : mClient(ctxt) {}
    virtual ~McpClient() = default;
    auto setCapabilities(const ExperimentalCapabilities& capabilities) -> void {
        mCapabilities.experimental = capabilities;
    }
    auto setCapabilities(const RootsCapabilities& capabilities) -> void { mCapabilities.roots = capabilities; }
    auto setCapabilities(const SamplingCapability& capabilities) -> void { mCapabilities.sampling = capabilities; }
    auto makeInitializeRequestParams() {
        return InitializeRequestParams{.protocolVersion = LATEST_PROTOCOL_VERSION,
                                       .capabilities    = mCapabilities,
                                       .clientInfo =
                                           Implementation{.name = CCMCP_PROJECT_NAME, .version = CCMCP_VERSION_STRING},
                                       ._meta = {}};
    }

    template <typename StreamType>
    auto setTransport(StreamType&& transport) -> void {
        return mClient.setTransport(std::forward<StreamType>(transport));
    }
    auto close() -> void { mClient.close(); }
    auto isConnected() const -> bool { return mClient.isConnected(); }
    template <typename RetT, typename Arg>
        requires std::is_class_v<Arg> && (!std::is_empty_v<Arg>)
    auto callRemote(std::string_view name, Arg arg) -> ILIAS_NAMESPACE::IoTask<RetT> {
        auto ret = co_await mClient->toolsCall(
            ToolCallRequestParams{.name = std::string(name), .arguments = NEKO_NAMESPACE::to_json_value(arg)});
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        CallToolResult result = ret.value();
        if (result.isError) {
            co_return Unexpected(IliasError::Unknown);
        }
        if (auto ret1 = handlerCallResult<RetT>(result.content); ret1) {
            co_return *ret1;
        } else {
            co_return Unexpected(IliasError::Unknown);
        }
    }
    template <typename Arg>
        requires std::is_class_v<Arg> && (!std::is_empty_v<Arg>)
    auto notifyRemote(std::string_view name, Arg arg) -> ILIAS_NAMESPACE::IoTask<void> {
        co_return mClient->toolsCall.notification(
            ToolCallRequestParams{.name = std::string(name), .arguments = NEKO_NAMESPACE::to_json_value(arg)});
    }
    template <typename RetT>
    auto callRemote(std::string_view name) -> ILIAS_NAMESPACE::IoTask<RetT> {
        auto ret = co_await mClient->toolsCall(ToolCallRequestParams{.name = std::string(name)});
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        CallToolResult result = ret.value();
        if (result.isError) {
            co_return Unexpected(IliasError::Unknown);
        }
        if (auto ret = handlerCallResult<RetT>(result.content); ret) {
            co_return *ret;
        } else {
            co_return Unexpected(IliasError::Unknown);
        }
    }

    auto notifyRemote(std::string_view name) -> ILIAS_NAMESPACE::IoTask<void> {
        return mClient->toolsCall.notification(ToolCallRequestParams{.name = std::string(name)});
    }

private:
    JsonRpcClient<detail::McpJsonRpcMethods> mClient;
    ClientCapabilities mCapabilities;
};

template <typename ToolFunctions>
class McpClient final : public McpClient<void> {
public:
    McpClient(IoContext& ctx) : McpClient<void>(ctx) { _register_tool_functions(); }

    const auto* operator->() const { return &mToolFunctions; }

private:
    void _register_tool_functions();

private:
    ToolFunctions mToolFunctions;
};

template <typename ToolFunctions>
void McpClient<ToolFunctions>::_register_tool_functions() {
    if constexpr (std::is_empty_v<ToolFunctions> || std::is_void_v<ToolFunctions>) {
        static_assert(!std::is_empty_v<ToolFunctions>, "ToolFunctions must be a non-empty class or struct");
    } else {
        Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions, [this](auto& rpcMethodMetadata) {
            using MethodT = std::decay_t<decltype(rpcMethodMetadata)>;
            if constexpr (std::is_void_v<typename MethodT::ParamsT>) {
                rpcMethodMetadata = std::bind(&McpClient<ToolFunctions>::callRemote<typename MethodT::ReturnT>, this,
                                              std::string(rpcMethodMetadata.name));
            } else {
                rpcMethodMetadata = std::bind(
                    &McpClient<ToolFunctions>::callRemote<typename MethodT::ReturnT, typename MethodT::ParamsT>, this,
                    std::string(rpcMethodMetadata.name), std::placeholders::_1);
            }
        });
    }
}

CCMCP_EN