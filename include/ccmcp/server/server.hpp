#pragma once

#include "ccmcp/global/global.hpp"

#include <ilias/sync/scope.hpp>
#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "ccmcp/model/jsonrpc_protocol.hpp"
#include "ccmcp/server/system_info.hpp"

CCMCP_BN

template <typename ToolFunctions>
class McpServer {
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
    using JsonRpcServer = NEKO_NAMESPACE::JsonRpcServer<T>;
    template <typename T>
    using Reflect            = NEKO_NAMESPACE::Reflect<T>;
    using JsonSerializer     = NEKO_NAMESPACE::JsonSerializer;
    using ScopedCancelHandle = ILIAS_NAMESPACE::ScopedCancelHandle;

    void _register_tool_functions();
    void _register_rpc_methods();
    auto _initialize(InitializeRequestParams) -> IoTask<InitializeResult>;
    auto _initialized(EmptyRequestParams) -> IoTask<void>;
    auto _tools_call(ToolCallRequestParams) -> IoTask<CallToolResult>;
    auto _tools_list(PaginatedRequest) -> IoTask<ToolsListResult>;
    auto _cancelled(CancelledNotificationParams) -> IoTask<void>;

public:
    McpServer(IoContext& ctx);

    void set_instructions(std::string_view instructions);
    ToolsListResult tools_list(const PaginatedRequest&);
    auto start(std::string_view url) -> Task<bool>;
    template <typename StreamType>
    auto start(std::string_view url) -> IoTask<IliasError>;
    auto close() -> void;
    auto wait() -> Task<void> { co_await mServer.wait(); }
    auto json_rpc_server() -> JsonRpcServer<detail::McpJsonRpcMethods>& { return mServer; };

    auto* operator->() { return &mToolFunctions; }

private:
    JsonRpcServer<detail::McpJsonRpcMethods> mServer;
    ToolFunctions mToolFunctions;
    std::string mInstructions;
    TaskScope mScope; // 针对tools_call的任务的管理
    std::map<std::string_view, std::function<IoTask<CallToolResult>(JsonSerializer::InputSerializer& in)>> mHandlers;
};

template <typename ToolFunctions>
McpServer<ToolFunctions>::McpServer(IoContext& ctx) : mServer(ctx) {
    _register_rpc_methods();
    _register_tool_functions();
}

template <typename ToolFunctions>
void McpServer<ToolFunctions>::set_instructions(std::string_view instructions) {
    mInstructions = instructions;
}

template <typename ToolFunctions>
void McpServer<ToolFunctions>::_register_rpc_methods() {
    mServer->initialize  = std::bind(&McpServer::_initialize, this, std::placeholders::_1);
    mServer->initialized = std::function<IoTask<void>(EmptyRequestParams)>(
        std::bind(&McpServer::_initialized, this, std::placeholders::_1));
    mServer->toolsList     = std::bind(&McpServer::_tools_list, this, std::placeholders::_1);
    mServer->toolsCall     = std::bind(&McpServer::_tools_call, this, std::placeholders::_1);
    mServer->resourcesList = [](auto) -> ListResourcesResult { return {}; }; // TODO: MUST IMPL 默认没有任何资源模板
    mServer->resourcesTemplatesList = [](auto) -> ListResourceTemplatesResult { return {}; }; // TODO: MUST IMPL
    mServer->resourcesRead          = [](auto) -> ReadResourceResult { return {}; };
    mServer->progress               = [](auto) -> void {};
    mServer->resourcesListChanged   = [](auto) -> void {};
    mServer->resourcesSubscribe     = [](auto) -> void {};
    mServer->resourcesUnsubscribe   = [](auto) -> void {};
    mServer->resourcesUpdated       = [](auto) -> void {};
    mServer->promptsList            = [](auto) -> void {};
    mServer->promptsGet             = [](auto) -> GetPromptResult { return {}; };
    mServer->promptsListChanged     = [](auto) -> void {};
    mServer->toolsListChanged       = [](auto) -> void {};
    mServer->loggingSetLevel        = [](auto) -> void {};
    mServer->loggingMessage         = [](auto) -> void {};
    mServer->createMessage          = [](auto) -> void {};
    mServer->completionComplete     = [](auto) -> CompleteResult { return {}; };
    mServer->rootsList              = [](auto) -> ListRootsResult { return {}; };
    mServer->rootsListChanged       = [](auto) -> void {};
    mServer->cancelled              = std::function<IoTask<void>(CancelledNotificationParams)>(
        std::bind(&McpServer::_cancelled, this, std::placeholders::_1));
}

namespace detail {
template <typename MethodT, typename ToolFunctions>
struct RpcMethodWrapper {
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;
    template <typename T>
    using Result = ILIAS_NAMESPACE::Result<T>;

    MethodT& method;
    McpServer<ToolFunctions>* self;
    auto operator()(JsonSerializer::InputSerializer& in) const -> IoTask<CallToolResult> {
        using RetT = typename MethodT::ReturnT;
        CallToolResult result{.content = {}, .isError = true, .metadata = {}};
        Result<RetT> respon = ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Unknown);
        if constexpr (std::is_void_v<typename MethodT::ParamsT>) {
            respon = co_await method.function();
        } else {
            typename MethodT::ParamsT params;
            if (in(params)) {
                respon = co_await method.function(params);
            }
        }
        if (respon) {
            if constexpr (NEKO_NAMESPACE::detail::has_values_meta<RetT>) {
                Reflect<RetT>::forEachWithoutName(
                    respon.value(), [&result](auto& field) { result.content.push_back(make_content(field)); });
            }
            if constexpr (requires { std::begin(respon.value()); } && !(std::is_same_v<RetT, std::string>) &&
                          !(std::is_same_v<RetT, std::u8string>) && !(std::is_same_v<RetT, std::string_view>) &&
                          !(std::is_same_v<RetT, std::u8string_view>)) {
                for (auto& item : respon.value()) {
                    result.content.push_back(make_content(item));
                }
            } else {
                result.content.push_back(make_content(respon.value()));
            }
            result.isError = false;
        }
        co_return result;
    }
};
} // namespace detail

template <typename ToolFunctions>
void McpServer<ToolFunctions>::_register_tool_functions() {
    Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions, [this](auto& rpcMethodMetadata) {
        using MethodT                     = std::decay_t<decltype(rpcMethodMetadata)>;
        mHandlers[rpcMethodMetadata.name] = detail::RpcMethodWrapper<MethodT, ToolFunctions>(rpcMethodMetadata, this);
    });
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::_initialize(InitializeRequestParams params) -> IoTask<InitializeResult> {
    NEKO_LOG_INFO("mcp server", "initialize protocol version: {}", params.protocolVersion);
    co_return InitializeResult{.protocolVersion = LATEST_PROTOCOL_VERSION,
                               .capabilities    = ServerCapabilities{},
                               .serverInfo =
                                   Implementation{.name = CCMCP_PROJECT_NAME, .version = CCMCP_VERSION_STRING},
                               .instructions = mInstructions};
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::_initialized(EmptyRequestParams) -> IoTask<void> {
    NEKO_LOG_INFO("mcp server", "initialized");
    co_return {};
}
template <typename ToolFunctions>
auto McpServer<ToolFunctions>::start(std::string_view url) -> Task<bool> {
    return mServer.start(url);
}

template <typename ToolFunctions>
template <typename StreamType>
auto McpServer<ToolFunctions>::start(std::string_view url) -> IoTask<IliasError> {
    return mServer.start<StreamType>(url);
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::close() -> void {
    mServer.close();
}

template <typename ToolFunctions>
ToolsListResult McpServer<ToolFunctions>::tools_list([[maybe_unused]] const PaginatedRequest& params) {
    ToolsListResult result;
    Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions,
                                               [&](auto& tool) { result.tools.push_back(tool.tool()); });
    return result;
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::_tools_list(PaginatedRequest params) -> IoTask<ToolsListResult> {
    co_return tools_list(params);
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::_cancelled(CancelledNotificationParams params) -> IoTask<void> {
    NEKO_LOG_INFO("mcp server", "cancell {} beacuse {}",
                  params.requestId.index() == 0 ? std::to_string(std::get<0>(params.requestId))
                                                : std::get<1>(params.requestId),
                  params.reason.has_value() ? *params.reason : "unknown reason");
    switch (params.requestId.index()) {
    case 0:
        mServer.cancel(std::get<0>(params.requestId));
        break;
    case 1:
        mServer.cancel(std::get<1>(params.requestId));
        break;
    }
    co_return {};
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::_tools_call(ToolCallRequestParams params) -> IoTask<CallToolResult> {
    auto time = std::chrono::high_resolution_clock::now();
    CallToolResult result{.content = {}, .isError = true, .metadata = {}};
    if (auto item = mHandlers.find(params.name); item != mHandlers.end()) {
        JsonSerializer::InputSerializer in(params.arguments);
        if (auto ret = co_await mScope.spawn(item->second(in)); ret) {
            result = ret.value();
        } else {
            result.isError = true;
        }
    }
    ToolCallInfo info;
    info.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time).count();
    info.resourceUsage.memory = std::to_string(getCurrentRSS() / 1024) + "KB";
    result.metadata           = NEKO_NAMESPACE::to_json_value(info);
    co_return result;
}

CCMCP_EN