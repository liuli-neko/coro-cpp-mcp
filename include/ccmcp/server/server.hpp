#pragma once

#include "ccmcp/global/global.hpp"

#include <filesystem>
#include <fstream>
#include <ilias/sync/scope.hpp>
#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "ccmcp/model/jsonrpc_protocol.hpp"
#include "ccmcp/model/model.hpp"
#include "ccmcp/server/system_info.hpp"

CCMCP_BN
template <typename ToolFunctions>
class McpServer;

namespace detail {
struct RpcMethodWrapper {
    template <typename U>
    using IoTask = ILIAS_NAMESPACE::IoTask<U>;

    virtual ~RpcMethodWrapper() = default;

    virtual auto call(JsonSerializer::InputSerializer& in) -> IoTask<CallToolResult> = 0;
    auto operator()(JsonSerializer::InputSerializer& in) -> IoTask<CallToolResult> { return call(in); }
};
} // namespace detail

template <>
class McpServer<void> {
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
    using JsonRpcServer = NEKO_NAMESPACE::JsonRpcServer<T>;
    template <typename T>
    using Reflect            = NEKO_NAMESPACE::Reflect<T>;
    using JsonSerializer     = NEKO_NAMESPACE::JsonSerializer;
    using ScopedCancelHandle = ILIAS_NAMESPACE::ScopedCancelHandle;

    void _register_rpc_methods();
    auto _initialize(InitializeRequestParams) -> IoTask<InitializeResult>;
    auto _initialized(EmptyRequestParams) -> IoTask<void>;
    auto _tools_call(ToolCallRequestParams) -> IoTask<CallToolResult>;
    auto _tools_list(PaginatedRequest) -> IoTask<ToolsListResult>;
    auto _cancelled(CancelledNotificationParams) -> IoTask<void>;

public:
    using ResourceContentsT = std::variant<TextResourceContents, BlobResourceContents>;

public:
    McpServer(IoContext& ctx);

    void set_instructions(std::string_view instructions);
    virtual auto tools_list(const PaginatedRequest&) -> ToolsListResult;
    auto start(std::string_view url) -> Task<bool>;
    template <typename StreamType>
    auto start(std::string_view url) -> IoTask<IliasError>;
    auto close() -> void;
    auto is_running() -> bool { return mServer.isListening(); }
    auto wait() -> Task<void> { co_await mServer.wait(); }
    template <typename Ret, typename... Args>
    auto register_tool_function(std::string_view name, std::function<Ret(Args...)> func,
                                std::string_view description = "") -> bool;
    template <typename Ret, typename... Args>
    auto register_tool_function(std::string_view name, std::function<IoTask<Ret>(Args...)> func,
                                std::string_view description = "") -> bool;
    auto json_rpc_server() -> JsonRpcServer<detail::McpJsonRpcMethods>& { return mServer; };
    auto register_local_file_resource(std::string_view name, std::filesystem::path path, std::string_view uri = "",
                                      std::string_view description = "") -> bool;
    auto register_resource(Resource resource, ResourceContentsT) -> void;
    auto register_resource(Resource resource, std::function<ResourceContentsT(std::optional<Meta> meta)>) -> void;

protected:
    JsonRpcServer<detail::McpJsonRpcMethods> mServer;
    std::string mInstructions;
    TaskScope mScope; // 针对tools_call的任务的管理
    std::map<std::string_view, std::unique_ptr<detail::RpcMethodWrapper>> mHandlers;

    // for tools
    std::vector<std::function<Tool()>> mTools;

    // for resources
    std::map<std::string, std::function<ResourceContentsT(std::optional<Meta> meta)>> mResources;
    std::vector<Resource> mResourceList;
};

template <typename ToolFunctions>
class McpServer final : public McpServer<void> {
public:
    McpServer(IoContext& ctx) : McpServer<void>(ctx) { _register_tool_functions(); }

    auto* operator->() { return &mToolFunctions; }
    const auto* operator->() const { return &mToolFunctions; }
    ToolsListResult tools_list(const PaginatedRequest&);

private:
    void _register_tool_functions();

private:
    ToolFunctions mToolFunctions;
};

inline McpServer<void>::McpServer(IoContext& ctx) : mServer(ctx) { _register_rpc_methods(); }

inline void McpServer<void>::set_instructions(std::string_view instructions) { mInstructions = instructions; }

inline void McpServer<void>::_register_rpc_methods() {
    mServer->initialize  = std::bind(&McpServer::_initialize, this, std::placeholders::_1);
    mServer->initialized = std::function<IoTask<void>(EmptyRequestParams)>(
        std::bind(&McpServer::_initialized, this, std::placeholders::_1));
    mServer->toolsList     = std::bind(&McpServer::_tools_list, this, std::placeholders::_1);
    mServer->toolsCall     = std::bind(&McpServer::_tools_call, this, std::placeholders::_1);
    mServer->resourcesList = [this](EmptyRequestParams) -> ListResourcesResult {
        return ListResourcesResult{.resources = mResourceList, .nextCursor = std::nullopt};
    }; // TODO: MUST IMPL 默认没有任何资源模板
    mServer->resourcesTemplatesList = [](EmptyRequestParams) -> ListResourceTemplatesResult {
        return {};
    }; // TODO: MUST IMPL
    mServer->resourcesRead = [this](ReadResourceRequestParams request) -> ReadResourceResult {
        ReadResourceResult result;
        NEKO_LOG_INFO("mcp server", "read resource {}", request.uri);
        if (auto it = mResources.find(request.uri); it != mResources.end()) {
            result.contents.push_back(it->second(request._meta));
        }
        return result;
    };
    mServer->progress             = [](ProgressNotificationParams) -> void {};
    mServer->resourcesListChanged = [](EmptyRequestParams) -> void {};
    mServer->resourcesSubscribe   = [](SubscribeRequestParams) -> void {};
    mServer->resourcesUnsubscribe = [](UnsubscribeRequestParams) -> void {};
    mServer->resourcesUpdated     = [](ResourceUpdatedNotificationParams) -> void {};
    mServer->promptsList          = [](EmptyRequestParams) -> void {};
    mServer->promptsGet           = [](GetPromptRequestParams) -> GetPromptResult { return {}; };
    mServer->promptsListChanged   = [](EmptyRequestParams) -> void {};
    mServer->toolsListChanged     = [](EmptyRequestParams) -> void {};
    mServer->loggingSetLevel      = [](SetLevelRequestParams) -> void {};
    mServer->loggingMessage       = [](LoggingMessageNotificationParams) -> void {};
    mServer->createMessage        = [](CreateMessageRequestParams) -> void {};
    mServer->completionComplete   = [](CompleteRequestParams) -> CompleteResult { return {}; };
    mServer->rootsList            = [](EmptyRequestParams) -> ListRootsResult { return {}; };
    mServer->rootsListChanged     = [](EmptyRequestParams) -> void {};
    mServer->cancelled            = std::function<IoTask<void>(CancelledNotificationParams)>(
        std::bind(&McpServer::_cancelled, this, std::placeholders::_1));
}

namespace detail {
template <typename T>
struct RpcMethodType {
    using MethodT = T;
};

template <typename T>
struct RpcMethodType<std::shared_ptr<T>> {
    using MethodT = T;
};
template <typename T>
struct RpcMethodType<std::unique_ptr<T>> {
    using MethodT = T;
};

template <typename T>
struct RpcMethodType<T*> {
    using MethodT = T;
};

template <typename T, typename ToolFunctions>
struct RpcMethodWrapperImpl : public RpcMethodWrapper {
    template <typename U>
    using IoTask = ILIAS_NAMESPACE::IoTask<U>;
    template <typename U>
    using Result  = ILIAS_NAMESPACE::Result<U>;
    using MethodT = std::decay_t<typename RpcMethodType<T>::MethodT>;
    RpcMethodWrapperImpl(T method, McpServer<ToolFunctions>* self) : method(std::move(method)), self(self) {}

    T method;
    McpServer<ToolFunctions>* self;
    auto call(JsonSerializer::InputSerializer& in) -> IoTask<CallToolResult> override {
        if (!(*method)) {
            co_return CallToolResult{.content = {}, .isError = true, .metadata = {}};
        }
        using RetT = typename MethodT::ReturnT;
        CallToolResult result{.content = {}, .isError = true, .metadata = {}};
        Result<RetT> respon = ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Unknown);
        if constexpr (std::is_void_v<typename MethodT::ParamsT>) {
            respon = co_await (*method)();
        } else {
            typename MethodT::ParamsT params;
            if (in(params)) {
                respon = co_await (*method)(std::move(params));
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
    if constexpr (std::is_empty_v<ToolFunctions> || std::is_void_v<ToolFunctions>) {
        static_assert(!std::is_empty_v<ToolFunctions>, "ToolFunctions must be a non-empty class or struct");
    } else {
        Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions, [this](auto& rpcMethodMetadata) {
            using MethodT = std::decay_t<decltype(rpcMethodMetadata)>;
            mHandlers[rpcMethodMetadata.name] =
                std::make_unique<detail::RpcMethodWrapperImpl<MethodT*, ToolFunctions>>(&rpcMethodMetadata, this);
        });
    }
}

inline auto McpServer<void>::_initialize(InitializeRequestParams params) -> IoTask<InitializeResult> {
    NEKO_LOG_INFO("mcp server", "initialize protocol version: {}", params.protocolVersion);
    co_return InitializeResult{.protocolVersion = LATEST_PROTOCOL_VERSION,
                               .capabilities    = ServerCapabilities{},
                               .serverInfo =
                                   Implementation{.name = CCMCP_PROJECT_NAME, .version = CCMCP_VERSION_STRING},
                               .instructions = mInstructions};
}

inline auto McpServer<void>::_initialized(EmptyRequestParams) -> IoTask<void> {
    NEKO_LOG_INFO("mcp server", "initialized");
    co_return {};
}

inline auto McpServer<void>::start(std::string_view url) -> Task<bool> { return mServer.start(url); }

template <typename StreamType>
auto McpServer<void>::start(std::string_view url) -> IoTask<IliasError> {
    return mServer.start<StreamType>(url);
}

inline auto McpServer<void>::close() -> void { mServer.close(); }

inline ToolsListResult McpServer<void>::tools_list([[maybe_unused]] const PaginatedRequest& params) {
    ToolsListResult result;
    for (const auto& tool : mTools) {
        result.tools.push_back(tool());
    }
    return result;
}

template <typename ToolFunctions>
ToolsListResult McpServer<ToolFunctions>::tools_list(const PaginatedRequest& params) {
    ToolsListResult result = McpServer<void>::tools_list(params);
    if constexpr (std::is_empty_v<ToolFunctions> || std::is_void_v<ToolFunctions>) {
        return result;
    } else {
        Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions,
                                                   [&](auto& tool) { result.tools.push_back(tool.tool()); });
        return result;
    }
}

inline auto McpServer<void>::_tools_list(PaginatedRequest params) -> IoTask<ToolsListResult> {
    co_return tools_list(params);
}

inline auto McpServer<void>::_cancelled(CancelledNotificationParams params) -> IoTask<void> {
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

inline auto McpServer<void>::_tools_call(ToolCallRequestParams params) -> IoTask<CallToolResult> {
    auto time = std::chrono::high_resolution_clock::now();
    CallToolResult result{.content = {}, .isError = true, .metadata = {}};
    ToolCallInfo info;
    if (auto item = mHandlers.find(params.name); item != mHandlers.end()) {
        JsonSerializer::InputSerializer in(params.arguments);
        if (auto ret = co_await mScope.spawn((*item->second)(in)); ret) {
            result = ret.value();
        } else {
            result.isError = true;
        }
    } else {
        info.error = "Tool not found";
    }
    info.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time).count();
    info.resourceUsage.memory = std::to_string(getCurrentRSS() / 1024) + "KB";
    result.metadata           = NEKO_NAMESPACE::to_json_value(info);
    co_return result;
}

inline auto McpServer<void>::register_local_file_resource(std::string_view name, std::filesystem::path path,
                                                          std::string_view uri, std::string_view description) -> bool {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    Resource resource;
    resource.name = std::string(name);
    resource.uri  = uri.empty() ? "file://" + path.string() : std::string(uri);
    if (!description.empty()) {
        resource.description = std::string(description);
    }
    resource.metadata = ResourceMetadata{.type = "file", .size = std::filesystem::file_size(path)};
    register_resource(resource, [path = path, uri = resource.uri](std::optional<Meta>) -> ResourceContentsT {
        auto file = std::ifstream(path, std::ios::binary);
        BlobResourceContents contents;
        contents.uri = uri;
        if (file.is_open()) {
            auto data = Base64Covert::Encode({std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()});
            contents.blob = std::string(data.begin(), data.end());
        }
        return contents;
    });
    return true;
}

inline auto McpServer<void>::register_resource(Resource resource, ResourceContentsT contents) -> void {
    register_resource(resource,
                      [contents = std::move(contents)](std::optional<Meta>) -> ResourceContentsT { return contents; });
}

inline auto McpServer<void>::register_resource(Resource resource,
                                               std::function<ResourceContentsT(std::optional<Meta>)> contents) -> void {
    mResources[resource.uri] = contents;
    mResourceList.push_back(resource);
}

template <typename Ret, typename... Args>
auto McpServer<void>::register_tool_function(std::string_view name, std::function<Ret(Args...)> func,
                                             std::string_view description) -> bool {
    if (mHandlers.find(name) != mHandlers.end()) {
        return false;
    }
    using MethodT = DynamicToolFunction<std::function<Ret(Args...)>>;
    MethodT rpcMethodMetadata(name, description);
    rpcMethodMetadata = func;
    auto wrapper      = std::make_unique<detail::RpcMethodWrapperImpl<std::unique_ptr<MethodT>, void>>(
        std::make_unique<MethodT>(std::move(rpcMethodMetadata)), this);
    mTools.push_back(std::function([method = wrapper->method.get()]() { return method->tool(); }));
    mHandlers[name] = std::move(wrapper);
    return true;
}

template <typename Ret, typename... Args>
auto McpServer<void>::register_tool_function(std::string_view name, std::function<IoTask<Ret>(Args...)> func,
                                             std::string_view description) -> bool {
    if (mHandlers.find(name) != mHandlers.end()) {
        return false;
    }
    using MethodT = DynamicToolFunction<std::function<Ret(Args...)>>;
    MethodT rpcMethodMetadata(name, description);
    rpcMethodMetadata = func;
    auto wrapper      = std::make_unique<detail::RpcMethodWrapperImpl<std::unique_ptr<MethodT>, void>>(
        std::make_unique<MethodT>(std::move(rpcMethodMetadata)), this);
    mTools.push_back(std::function([method = &wrapper->method.get()]() { return method->tool(); }));
    mHandlers[name] = std::move(wrapper);
    return true;
}

CCMCP_EN