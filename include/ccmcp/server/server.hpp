#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "ccmcp/model/jsonrpc_protocol.hpp"
#include "ccmcp/server/system_info.hpp"

CCMCP_BN

template <typename ToolFunctions>
class McpServer {
    template <typename T>
    using JsonRpcServer = NEKO_NAMESPACE::JsonRpcServer<T>;

    void _register_tool_functions();
    void _register_rpc_methods();
    ILIAS_NAMESPACE::IoTask<detail::InitializeResult> _initialize(detail::InitializeRequestParams);
    ILIAS_NAMESPACE::IoTask<void> _initialized(detail::EmptyRequestParams);
    ILIAS_NAMESPACE::IoTask<detail::CallToolResult> _tools_call(detail::ToolCallRequestParams);
    ILIAS_NAMESPACE::IoTask<detail::ToolsListResult> _tools_list(PaginatedRequest);

public:
    McpServer(ILIAS_NAMESPACE::IoContext& ctx);

    void set_instructions(std::string_view instructions);
    detail::ToolsListResult tools_list(const PaginatedRequest&);
    auto start(std::string_view url) -> ILIAS_NAMESPACE::Task<bool>;
    template <typename StreamType>
    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<ILIAS_NAMESPACE::Error>;
    auto close() -> void;
    auto wait() -> ILIAS_NAMESPACE::Task<void> { co_await mServer.wait(); }

    auto* operator->() { return &mToolFunctions; }

private:
    JsonRpcServer<detail::McpJsonRpcMethods> mServer;
    ToolFunctions mToolFunctions;
    std::string mInstructions;
    std::map<std::string_view, std::function<ILIAS_NAMESPACE::Task<detail::CallToolResult>(
                                   NEKO_NAMESPACE::JsonSerializer::InputSerializer& in)>>
        mHandlers;
};

template <typename ToolFunctions>
McpServer<ToolFunctions>::McpServer(ILIAS_NAMESPACE::IoContext& ctx) : mServer(ctx) {
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
    mServer->initialized = std::function<ILIAS_NAMESPACE::IoTask<>(detail::EmptyRequestParams)>(
        std::bind(&McpServer::_initialized, this, std::placeholders::_1));
    mServer->toolsList = std::bind(&McpServer::_tools_list, this, std::placeholders::_1);
    mServer->toolsCall = std::bind(&McpServer::_tools_call, this, std::placeholders::_1);
}

template <typename ToolFunctions>
void McpServer<ToolFunctions>::_register_tool_functions() {
    NEKO_NAMESPACE::Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions, [this](auto& rpcMethodMetadata) {
        using MethodType = std::decay_t<decltype(rpcMethodMetadata)>;
        mHandlers[rpcMethodMetadata.name] =
            [this, &rpcMethodMetadata](
                NEKO_NAMESPACE::JsonSerializer::InputSerializer& in) -> ILIAS_NAMESPACE::Task<detail::CallToolResult> {
            typename MethodType::ParamsT params;
            using RetT = typename MethodType::ReturnT;
            if (in(params)) {
                const auto& respon = rpcMethodMetadata.function(params);
                detail::CallToolResult result;
                if constexpr (NEKO_NAMESPACE::detail::has_values_meta<RetT>) {
                    NEKO_NAMESPACE::Reflect<RetT>::forEachWithoutName(
                        respon, [&result](auto& field) { result.content.push_back(make_content(field)); });
                } else {
                    result.content.push_back(make_content(respon));
                }
                co_return result;
            }
            co_return detail::CallToolResult{.content = {}, .isError = true, .metadata = {}};
        };
    });
}

template <typename ToolFunctions>
ILIAS_NAMESPACE::IoTask<detail::InitializeResult>
McpServer<ToolFunctions>::_initialize(detail::InitializeRequestParams params) {
    NEKO_LOG_INFO("mcp server", "initialize protocol version: {}", params.protocolVersion);
    co_return detail::InitializeResult{.protocolVersion = LATEST_PROTOCOL_VERSION,
                                       .capabilities    = ServerCapabilities{},
                                       .serverInfo =
                                           Implementation{.name = CCMCP_PROJECT_NAME, .version = CCMCP_VERSION_STRING},
                                       .instructions = mInstructions};
}

template <typename ToolFunctions>
ILIAS_NAMESPACE::IoTask<void> McpServer<ToolFunctions>::_initialized(detail::EmptyRequestParams) {
    NEKO_LOG_INFO("mcp server", "initialized");
    co_return {};
}
template <typename ToolFunctions>
auto McpServer<ToolFunctions>::start(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
    return mServer.start(url);
}

template <typename ToolFunctions>
template <typename StreamType>
auto McpServer<ToolFunctions>::start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<ILIAS_NAMESPACE::Error> {
    return mServer.start<StreamType>(url);
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::close() -> void {
    mServer.close();
}

template <typename ToolFunctions>
detail::ToolsListResult McpServer<ToolFunctions>::tools_list([[maybe_unused]] const PaginatedRequest& params) {
    detail::ToolsListResult result;
    NEKO_NAMESPACE::Reflect<ToolFunctions>::forEachWithoutName(
        mToolFunctions, [&](auto& tool) { result.tools.push_back(tool.tool()); });
    return result;
}

template <typename ToolFunctions>
ILIAS_NAMESPACE::IoTask<detail::ToolsListResult> McpServer<ToolFunctions>::_tools_list(PaginatedRequest params) {
    co_return tools_list(params);
}

template <typename ToolFunctions>
ILIAS_NAMESPACE::IoTask<detail::CallToolResult>
McpServer<ToolFunctions>::_tools_call(detail::ToolCallRequestParams params) {
    auto time = std::chrono::high_resolution_clock::now();
    detail::CallToolResult result;
    if (auto item = mHandlers.find(params.name); item != mHandlers.end()) {
        NEKO_NAMESPACE::JsonSerializer::InputSerializer in(params.arguments);
        result = co_await item->second(in);
    } else {
        result = detail::CallToolResult{.content = {}, .isError = true, .metadata = {}};
    }
    ToolCallInfo info;
    info.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time).count();
    info.resourceUsage.memory = std::to_string(getCurrentRSS() / 1024) + "KB";
    result.metadata           = NEKO_NAMESPACE::from_object(info);
    co_return result;
}

CCMCP_EN