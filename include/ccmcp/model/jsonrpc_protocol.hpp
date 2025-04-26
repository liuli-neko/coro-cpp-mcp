#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "ccmcp/model/capabilities.hpp"

CCMCP_BN

namespace detail {
NEKO_USE_NAMESPACE

struct EmptyRequestParams {
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(_meta)
};

struct InitializeRequestParams {
    std::string protocolVersion;
    ClientCapabilities capabilities;
    Implementation clientInfo;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(protocolVersion, capabilities, clientInfo, _meta)
};

struct InitializeResult {
    std::string protocolVersion;
    ServerCapabilities capabilities;
    Implementation serverInfo;
    std::optional<std::string> instructions;

    NEKO_SERIALIZER(protocolVersion, capabilities, serverInfo, instructions)
};

struct ResourceListRequestParams {
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(_meta)
};

struct ToolsListResult {
    /// The list of tools
    std::vector<Tool> tools;

    NEKO_SERIALIZER(tools)
};

struct ToolCallRequestParams {
    /// The name of the tool to call
    std::string name;
    /// The parameters to pass to the tool
    JsonValue parameters;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(name, parameters, _meta)
};

struct ToolCallResult {
    /// The result of the tool call
    JsonValue content;
    bool isError;
    std::optional<JsonValue> metadata;

    NEKO_SERIALIZER(content, isError, metadata)
};

struct PingRequestParams {
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(_meta)
};

struct ProgressNotificationParams {
    ProgressToken progressToken;
    float progress;
    std::optional<float> total;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(progressToken, progress, total, _meta)
};

struct ListResourcesResult {
    std::vector<Resource> resources;
    std::optional<std::string> nextCursor;

    NEKO_SERIALIZER(resources, nextCursor)
};

struct ListResourceTemplatesResult {
    std::vector<ResourceTemplate> resources;
    std::optional<std::string> nextCursor;

    NEKO_SERIALIZER(resources, nextCursor)
};

struct ReadResourceRequestParams {
    std::string uri;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(uri, _meta)
};

struct ReadResourceResult {
    std::vector<std::variant<TextResourceContents, BlobResourceContents>> contents;

    NEKO_SERIALIZER(contents)
};

struct SubscribeRequestParams {
    std::string uri;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(uri, _meta)
};

struct UnsubscribeRequestParams {
    std::string uri;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(uri, _meta)
};

struct ResourceUpdatedNotificationParams {
    std::string uri;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(uri, _meta)
};

struct PromptArgument {
    std::string name;
    std::optional<std::string> description;
    std::optional<bool> required;

    NEKO_SERIALIZER(name, description, required)
};

struct Prompt {
    std::string name;
    std::optional<std::string> description;
    std::vector<PromptArgument> arguments;

    NEKO_SERIALIZER(name, description, arguments)
};

struct ListPromptsResult {
    std::vector<Prompt> prompts;
    std::optional<std::string> nextCursor;

    NEKO_SERIALIZER(prompts)
};

struct GetPromptRequestParams {
    std::string name;
    std::optional<std::map<std::string, std::string>> arguments;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(name, arguments, _meta)
};

struct McpJsonRpcMethods {
    RpcMethod<InitializeResult(InitializeRequestParams), "initialize"> initialize;
    RpcMethod<void(EmptyRequestParams), "notifications/initialized"> initialized;
    RpcMethod<ToolsListResult(PaginatedRequest), "tools/list"> toolsList;
    RpcMethod<ToolCallResult(ToolCallRequestParams), "tools/call"> toolsCall;
    RpcMethod<ListResourcesResult(EmptyRequestParams), "resources/list"> resourcesList;
    RpcMethod<ListResourceTemplatesResult(EmptyRequestParams), "resources/templates/list"> resourcesTemplatesList;
    RpcMethod<ReadResourceResult(ReadResourceRequestParams), "resources/read"> resourcesRead;
    RpcMethod<void(ProgressNotificationParams), "notifications/progress"> progress;
    RpcMethod<void(EmptyRequestParams), "notifications/resources/list_changed"> resourcesListChanged;
    RpcMethod<void(SubscribeRequestParams), "resources/subscribe"> resourcesSubscribe;
    RpcMethod<void(UnsubscribeRequestParams), "resources/unsubscribe"> resourcesUnsubscribe;
    RpcMethod<void(ResourceUpdatedNotificationParams), "notifications/resources/updated"> resourcesUpdated;
    RpcMethod<void(EmptyRequestParams), "prompts/list"> promptsList;
};
} // namespace detail

CCMCP_EN