#pragma once

#include "ccmcp/global/global.hpp"

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
    using IliasError = ILIAS_NAMESPACE::IoError;
    template <typename T>
    using TaskGroup  = ILIAS_NAMESPACE::TaskGroup<T>;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Unexpected<T>;
    template <typename T>
    using JsonRpcServer = NEKO_NAMESPACE::JsonRpcServer<T>;
    template <typename T>
    using Result        = ILIAS_NAMESPACE::IoResult<T>;
    template <typename T>
    using Reflect            = NEKO_NAMESPACE::Reflect<T>;
    using JsonSerializer     = NEKO_NAMESPACE::JsonSerializer;
    using ScopedCancelHandle = ILIAS_NAMESPACE::StopHandle;

    void _register_rpc_methods();
    auto _initialize(InitializeRequestParams) noexcept -> IoTask<InitializeResult>;
    auto _initialized(EmptyRequestParams) noexcept -> IoTask<void>;
    auto _tools_call(ToolCallRequestParams) noexcept -> IoTask<CallToolResult>;
    auto _tools_list(PaginatedRequest) noexcept -> IoTask<ToolsListResult>;
    auto _cancelled(CancelledNotificationParams) noexcept -> IoTask<void>;

public:
    using ResourceContentsT = std::variant<TextResourceContents, BlobResourceContents>;

public:
    McpServer(IoContext& ctx);
    auto setCapabilities(const ExperimentalCapabilities& capabilities) noexcept -> void;
    auto setCapabilities(const LoggingCapability& capabilities) noexcept -> void;
    auto setCapabilities(const CompletionsCapability& capabilities) noexcept -> void;
    auto setCapabilities(const PromptsCapability& capabilities) noexcept -> void;
    auto setCapabilities(const ResourcesCapability& capabilities) noexcept -> void;
    auto setCapabilities(const ToolsCapability& capabilities) noexcept -> void;
    void setInstructions(std::string_view instructions) noexcept;
    virtual auto toolsList(const PaginatedRequest&) -> ToolsListResult;
    template <typename StreamType>
    auto addTransport(StreamType&& stream) -> void;
    template <typename ListenerType>
    auto setListener(ListenerType&& listener) -> void;
    auto close() -> void;
    auto wait() -> Task<void> { co_await mServer.wait(); }
    template <typename Ret, typename... Args>
    auto registerToolFunction(std::string_view name, std::function<Ret(Args...)> func,
                              std::string_view description = "") -> bool;
    template <typename Ret, typename... Args>
    auto registerToolFunction(std::string_view name, std::function<IoTask<Ret>(Args...)> func,
                              std::string_view description = "") -> bool;
    auto jsonRpcServer() -> JsonRpcServer<detail::McpJsonRpcMethods>& { return mServer; };
    auto registerLocalFileResource(std::string_view name, std::filesystem::path path, std::string_view uri = "",
                                   std::string_view description = "") -> bool;
    auto registerResource(Resource resource, ResourceContentsT) -> void;
    auto registerResource(Resource resource, std::function<ResourceContentsT(std::optional<Meta> meta)>) -> void;

protected:
    JsonRpcServer<detail::McpJsonRpcMethods> mServer;
    std::string mInstructions;
    std::map<std::string_view, std::unique_ptr<detail::RpcMethodWrapper>> mHandlers;

    // for tools
    std::vector<std::function<Tool()>> mTools;

    // for resources
    std::map<std::string, std::function<ResourceContentsT(std::optional<Meta> meta)>> mResources;
    std::vector<Resource> mResourceList;
    ServerCapabilities mCapabilities;
};

template <typename ToolFunctions>
class McpServer final : public McpServer<void> {
public:
    McpServer(IoContext& ctx) : McpServer<void>(ctx) { _register_tool_functions(); }

    auto* operator->() { return &mToolFunctions; }
    const auto* operator->() const { return &mToolFunctions; }
    auto toolsList(const PaginatedRequest&) -> ToolsListResult override;

private:
    void _register_tool_functions();

private:
    ToolFunctions mToolFunctions;
};

inline McpServer<void>::McpServer(IoContext& ctx) : mServer(ctx) { _register_rpc_methods(); }

inline auto McpServer<void>::setCapabilities(const ExperimentalCapabilities& capabilities) noexcept -> void {
    mCapabilities.experimental = capabilities;
}
inline auto McpServer<void>::setCapabilities(const LoggingCapability& capabilities) noexcept -> void {
    mCapabilities.logging = capabilities;
}
inline auto McpServer<void>::setCapabilities(const CompletionsCapability& capabilities) noexcept -> void {
    mCapabilities.completions = capabilities;
}
inline auto McpServer<void>::setCapabilities(const PromptsCapability& capabilities) noexcept -> void {
    mCapabilities.prompts = capabilities;
}
inline auto McpServer<void>::setCapabilities(const ResourcesCapability& capabilities) noexcept -> void {
    mCapabilities.resources = capabilities;
}
inline auto McpServer<void>::setCapabilities(const ToolsCapability& capabilities) noexcept -> void {
    mCapabilities.tools = capabilities;
}

inline void McpServer<void>::setInstructions(std::string_view instructions) noexcept { mInstructions = instructions; }

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
    mServer->ping                 = [](EmptyRequestParams) -> EmptyResult { return {}; };
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
    using Result  = ILIAS_NAMESPACE::IoResult<U>;
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
        Result<RetT> respon = ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::IoError::Unknown);
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

inline auto McpServer<void>::_initialize(InitializeRequestParams params) noexcept -> IoTask<InitializeResult> {
    NEKO_LOG_INFO("mcp server", "initialize protocol version: {}", params.protocolVersion);
    co_return InitializeResult{.protocolVersion = LATEST_PROTOCOL_VERSION,
                               .capabilities    = mCapabilities,
                               .serverInfo =
                                   Implementation{.name = CCMCP_PROJECT_NAME, .version = CCMCP_VERSION_STRING},
                               .instructions = mInstructions};
}

inline auto McpServer<void>::_initialized(EmptyRequestParams) noexcept -> IoTask<void> {
    NEKO_LOG_INFO("mcp server", "initialized");
    co_return {};
}

template <typename StreamType>
inline auto McpServer<void>::addTransport(StreamType&& stream) -> void {
    mServer.addTransport(std::forward<StreamType>(stream));
}

template <typename ListenerType>
inline auto McpServer<void>::setListener(ListenerType&& listener) -> void {
    mServer.setListener(std::forward<ListenerType>(listener));
}

inline auto McpServer<void>::close() -> void { mServer.close(); }

inline ToolsListResult McpServer<void>::toolsList([[maybe_unused]] const PaginatedRequest& params) {
    ToolsListResult result;
    for (const auto& tool : mTools) {
        result.tools.push_back(tool());
    }
    return result;
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::toolsList(const PaginatedRequest& params) -> ToolsListResult {
    ToolsListResult result = McpServer<void>::toolsList(params);
    if constexpr (std::is_empty_v<ToolFunctions> || std::is_void_v<ToolFunctions>) {
        return result;
    } else {
        Reflect<ToolFunctions>::forEachWithoutName(mToolFunctions,
                                                   [&](auto& tool) { result.tools.push_back(tool.tool()); });
        return result;
    }
}

inline auto McpServer<void>::_tools_list(PaginatedRequest params) noexcept -> IoTask<ToolsListResult> {
    co_return toolsList(params);
}

inline auto McpServer<void>::_cancelled(CancelledNotificationParams params) noexcept -> IoTask<void> {
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

inline auto McpServer<void>::_tools_call(ToolCallRequestParams params) noexcept -> IoTask<CallToolResult> {
    auto time = std::chrono::high_resolution_clock::now();
    CallToolResult result{.content = {}, .isError = true, .metadata = {}};
    ToolCallInfo info;
    if (auto item = mHandlers.find(params.name); item != mHandlers.end()) {
        JsonSerializer::InputSerializer in(params.arguments);
        if (auto ret = co_await (*item->second)(in); ret) {
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

inline auto McpServer<void>::registerLocalFileResource(std::string_view name, std::filesystem::path path,
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
    registerResource(resource, [path = path, uri = resource.uri](std::optional<Meta>) -> ResourceContentsT {
        auto file = std::ifstream(path, std::ios::binary);
        BlobResourceContents contents;
        contents.uri      = uri;
        contents.mimeType = "application/octet-stream";
        if (file.is_open()) {
            auto data = Base64Covert::Encode({std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()});
            contents.blob = std::string(data.begin(), data.end());
        }
        return contents;
    });
    return true;
}

inline auto McpServer<void>::registerResource(Resource resource, ResourceContentsT contents) -> void {
    registerResource(resource,
                     [contents = std::move(contents)](std::optional<Meta>) -> ResourceContentsT { return contents; });
}

inline auto McpServer<void>::registerResource(Resource resource,
                                              std::function<ResourceContentsT(std::optional<Meta>)> contents) -> void {
    mResources[resource.uri] = contents;
    mResourceList.push_back(resource);
}

template <typename Ret, typename... Args>
auto McpServer<void>::registerToolFunction(std::string_view name, std::function<Ret(Args...)> func,
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
auto McpServer<void>::registerToolFunction(std::string_view name, std::function<IoTask<Ret>(Args...)> func,
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