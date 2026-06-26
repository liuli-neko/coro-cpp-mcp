#pragma once

#include "ccmcp/global/global.hpp"

#include "ccmcp/model/jsonrpc_protocol.hpp"
#include "ccmcp/model/model.hpp"

#include <ilias/io/context.hpp>
#include <ilias/io/error.hpp>
#include <ilias/task/group.hpp>
#include <ilias/task/spawn.hpp>
#include <ilias/task/task.hpp>

#include <filesystem>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

CCMCP_BN
template <typename ToolFunctions>
class McpServer;

using ResourceContents = std::variant<TextResourceContents, BlobResourceContents>;
namespace detail {
struct RpcMethodWrapper {
    template <typename U>
    using IoTask = ILIAS_NAMESPACE::IoTask<U>;

    virtual ~RpcMethodWrapper() = default;

    virtual auto call(JsonSerializer::InputSerializer& in) -> IoTask<CallToolResult> = 0;
    auto operator()(JsonSerializer::InputSerializer& in) -> IoTask<CallToolResult> { return call(in); }
};

template <typename McpServerT>
class RegisterFunctionHelper {
public:
    RegisterFunctionHelper(McpServerT& server, std::string_view method_name)
        : mServer(server), mMethodName(method_name) {}
    RegisterFunctionHelper& setParameterDescription(std::string_view param, std::string_view description) {
        mParameters[param] = description;
        return *this;
    }
    RegisterFunctionHelper& setParameterDescription(const std::map<std::string_view, std::string>& parameters) {
        mParameters = parameters;
        return *this;
    }
    RegisterFunctionHelper& setDescription(std::string_view description) {
        mDescription = description;
        return *this;
    }
    template <typename FunctionT>

    void operator=(FunctionT&& func) {
        if constexpr (requires(FunctionT&& func) {
                          mServer.registerToolFunction(mMethodName, std::function(std::forward<FunctionT>(func)),
                                                       mDescription, mParameters);
                      }) {
            mServer.registerToolFunction(mMethodName, std::function(std::forward<FunctionT>(func)), mDescription,
                                         mParameters);
        } else {
            mServer.registerToolFunction(mMethodName, std::forward<FunctionT>(func), mDescription, mParameters);
        }
    }

private:
    McpServerT& mServer;
    std::string_view mMethodName;
    std::string_view mDescription;
    std::map<std::string_view, std::string> mParameters;
};

auto createResourceContentsFromFile(const std::filesystem::path& path, const std::string& uri) -> ResourceContents;
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
    using TaskGroup = ILIAS_NAMESPACE::TaskGroup<T>;
    template <typename T>
    using Unexpected = ILIAS_NAMESPACE::Err<T>;
    template <typename T>
    using JsonRpcServer = NEKO_NAMESPACE::JsonRpcServer<T>;
    template <typename T>
    using Result = ILIAS_NAMESPACE::IoResult<T>;
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
    auto close() -> void;
    auto wait() -> Task<void>;
    template <typename Ret, typename... Args>
    auto registerToolFunction(std::string_view name, std::function<Ret(Args...)> func,
                              std::string_view description                                     = "",
                              const std::map<std::string_view, std::string>& paramsDescription = {}) -> bool;
    template <typename Ret, typename... Args>
    auto registerToolFunction(std::string_view name, std::function<IoTask<Ret>(Args...)> func,
                              std::string_view description                                     = "",
                              const std::map<std::string_view, std::string>& paramsDescription = {}) -> bool;
    auto jsonRpcServer() -> JsonRpcServer<detail::McpJsonRpcMethods>&;
    auto registerLocalFileResource(std::string_view name, std::filesystem::path path, std::string_view uri = "",
                                   std::string_view description = "") -> bool;
    auto registerResource(Resource resource, ResourceContents) -> void;
    auto registerResource(Resource resource, std::function<ResourceContents(std::optional<Meta> meta)>) -> void;

protected:
    JsonRpcServer<detail::McpJsonRpcMethods> mServer;
    std::string mInstructions;
    std::map<std::string_view, std::unique_ptr<detail::RpcMethodWrapper>> mHandlers;

    // for tools
    std::vector<std::function<Tool()>> mTools;

    // for resources
    std::map<std::string, std::function<ResourceContents(std::optional<Meta> meta)>> mResources;
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
    auto registerToolFunction(std::string_view name) -> detail::RegisterFunctionHelper<McpServer>;
    using McpServer<void>::registerToolFunction;

private:
    void _register_tool_functions();

private:
    ToolFunctions mToolFunctions;
};

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
        Result<RetT> respon = ILIAS_NAMESPACE::Err(ILIAS_NAMESPACE::IoError::Unknown);
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
                Reflect<RetT>::forEach(respon.value(),
                                       [&result](auto& field) { result.content.push_back(make_content(field)); });
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
        Reflect<ToolFunctions>::forEach(mToolFunctions, [this](auto& rpcMethodMetadata) {
            using MethodT = std::decay_t<decltype(rpcMethodMetadata)>;
            mHandlers[rpcMethodMetadata.name] =
                std::make_unique<detail::RpcMethodWrapperImpl<MethodT*, ToolFunctions>>(&rpcMethodMetadata, this);
        });
    }
}

template <typename StreamType>
inline auto McpServer<void>::addTransport(StreamType&& stream) -> void {
    mServer.addEndpoint(std::forward<StreamType>(stream));
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::toolsList(const PaginatedRequest& params) -> ToolsListResult {
    ToolsListResult result = McpServer<void>::toolsList(params);
    if constexpr (std::is_empty_v<ToolFunctions> || std::is_void_v<ToolFunctions>) {
        return result;
    } else {
        Reflect<ToolFunctions>::forEach(mToolFunctions, [&](auto& tool) { result.tools.push_back(tool.tool()); });
        return result;
    }
}

template <typename ToolFunctions>
auto McpServer<ToolFunctions>::registerToolFunction(std::string_view name)
    -> detail::RegisterFunctionHelper<McpServer> {
    return detail::RegisterFunctionHelper<McpServer>(*this, name);
}

template <typename Ret, typename... Args>
auto McpServer<void>::registerToolFunction(std::string_view name, std::function<Ret(Args...)> func,
                                           std::string_view description,
                                           const std::map<std::string_view, std::string>& paramsDescription) -> bool {
    if (mHandlers.find(name) != mHandlers.end()) {
        return false;
    }
    using MethodT = DynamicToolFunction<std::function<Ret(Args...)>>;
    MethodT rpcMethodMetadata(name, description);
    rpcMethodMetadata                   = func;
    rpcMethodMetadata.paramsDescription = paramsDescription;
    auto wrapper = std::make_unique<detail::RpcMethodWrapperImpl<std::unique_ptr<MethodT>, void>>(
        std::make_unique<MethodT>(std::move(rpcMethodMetadata)), this);
    mTools.push_back(std::function([method = wrapper->method.get()]() { return method->tool(); }));
    mHandlers[name] = std::move(wrapper);
    return true;
}

template <typename Ret, typename... Args>
auto McpServer<void>::registerToolFunction(std::string_view name, std::function<IoTask<Ret>(Args...)> func,
                                           std::string_view description,
                                           const std::map<std::string_view, std::string>& paramsDescription) -> bool {
    if (mHandlers.find(name) != mHandlers.end()) {
        return false;
    }
    using MethodT = DynamicToolFunction<std::function<Ret(Args...)>>;
    MethodT rpcMethodMetadata(name, description);
    rpcMethodMetadata                   = func;
    rpcMethodMetadata.paramsDescription = paramsDescription;
    auto wrapper = std::make_unique<detail::RpcMethodWrapperImpl<std::unique_ptr<MethodT>, void>>(
        std::make_unique<MethodT>(std::move(rpcMethodMetadata)), this);
    mTools.push_back(std::function([method = wrapper->method.get()]() { return method->tool(); }));
    mHandlers[name] = std::move(wrapper);
    return true;
}

CCMCP_EN
