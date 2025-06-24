#pragma once

#include "ccmcp/global/global.hpp"

#include "ccmcp/model/capabilities.hpp"

CCMCP_BN
struct EmptyRequestParams {
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(_meta)
};

struct EmptyResult {
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
    JsonValue arguments;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(name, arguments, _meta)
};

struct CallToolResult {
    /// The result of the tool call
    std::vector<std::variant<TextContent, ImageContent, EmbeddedResource>> content;
    bool isError = false;
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

struct GetPromptResult {
    std::optional<std::string> description;
    std::vector<PromptMessage> messages;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(description, messages, _meta)
};

const std::vector<std::string> LoggingLevel = {"debug", "info",     "notice", "warning",
                                               "error", "critical", "alert",  "emergency"};

struct SetLevelRequestParams {
    std::string level;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(level, _meta)
};

struct LoggingMessageNotificationParams {
    std::string level;
    std::optional<std::string> logger;
    JsonValue data;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(level, logger, data, _meta)
};

const std::vector<std::string> IncludeContext = {"none", "thisServer", "allServers"};

struct ModelHint {
    std::optional<std::string> name;

    NEKO_SERIALIZER(name)
};

struct ModelPreferences {
    std::optional<std::vector<ModelHint>> hints;
    std::optional<float> costPriority;
    std::optional<float> speedPriority;
    std::optional<float> intelligencePriority;

    NEKO_SERIALIZER(hints, costPriority, speedPriority, intelligencePriority)
};

struct CreateMessageRequestParams {
    std::vector<SamplingMessage> messages;
    std::optional<ModelPreferences> modelPreferences;
    std::optional<std::string> systemPrompt;
    std::optional<std::string> includeContext;
    std::optional<float> temperature;
    int maxTokens;
    std::optional<std::vector<std::string>> stopSequences;
    std::optional<std::map<std::string, JsonValue>> metadata;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(messages, modelPreferences, systemPrompt, includeContext, temperature, maxTokens, stopSequences,
                    metadata, _meta)
};

struct CreateMessageResult {
    Role role;
    std::variant<TextContent, ImageContent> context;
    std::string model;
    std::optional<std::string> stopReason;

    NEKO_SERIALIZER(role, context, model, stopReason)
};

struct ResourceReference {
    std::string type = "ref/resource";
    std::string uri;

    NEKO_SERIALIZER(type, uri)
};

struct PromptReference {
    std::string type = "ref/prompt";
    std::string name;

    NEKO_SERIALIZER(type, name)
};

struct CompletionArgument {
    std::string name;
    std::string value;

    NEKO_SERIALIZER(name, value)
};

struct CompleteRequestParams {
    std::variant<ResourceReference, PromptReference> ref;
    CompletionArgument argument;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(ref, argument, _meta)
};

struct Completion {
    std::vector<std::string> values;
    std::optional<int> total;
    std::optional<bool> hasMore;

    NEKO_SERIALIZER(values, total, hasMore)
};

struct CompleteResult {
    Completion completion;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(completion, _meta)
};

struct Root {
    /*
    The URI identifying the root. This *must* start with file:// for now.
    This restriction may be relaxed in future versions of the protocol to allow
    other URI schemes.
    */
    std::string uri;
    std::optional<std::string> name;

    NEKO_SERIALIZER(uri, name)
};

struct ListRootsResult {
    std::vector<Root> roots;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(roots, _meta)
};

struct CancelledNotificationParams {
    std::variant<int, std::string> requestId;
    std::optional<std::string> reason;
    std::optional<Meta> _meta;

    NEKO_SERIALIZER(requestId, reason, _meta)
};
namespace detail {
NEKO_USE_NAMESPACE

struct McpJsonRpcMethods {
    RpcMethod<InitializeResult(InitializeRequestParams), "initialize"> initialize;
    RpcMethod<void(EmptyRequestParams), "notifications/initialized"> initialized;
    RpcMethod<ListResourcesResult(EmptyRequestParams), "resources/list"> resourcesList;
    RpcMethod<ListResourceTemplatesResult(EmptyRequestParams), "resources/templates/list"> resourcesTemplatesList;
    RpcMethod<ReadResourceResult(ReadResourceRequestParams), "resources/read"> resourcesRead;
    RpcMethod<void(ProgressNotificationParams), "notifications/progress"> progress;
    RpcMethod<void(EmptyRequestParams), "notifications/resources/list_changed"> resourcesListChanged;
    RpcMethod<void(SubscribeRequestParams), "resources/subscribe"> resourcesSubscribe;
    RpcMethod<void(UnsubscribeRequestParams), "resources/unsubscribe"> resourcesUnsubscribe;
    RpcMethod<void(ResourceUpdatedNotificationParams), "notifications/resources/updated"> resourcesUpdated;
    RpcMethod<void(EmptyRequestParams), "prompts/list"> promptsList;
    RpcMethod<GetPromptResult(GetPromptRequestParams), "prompts/get"> promptsGet;
    RpcMethod<void(EmptyRequestParams), "notifications/prompts/list_changed"> promptsListChanged;
    RpcMethod<ToolsListResult(PaginatedRequest), "tools/list"> toolsList;
    RpcMethod<CallToolResult(ToolCallRequestParams), "tools/call"> toolsCall;
    RpcMethod<void(EmptyRequestParams), "notifications/tools/list_changed"> toolsListChanged;
    RpcMethod<void(SetLevelRequestParams), "logging/setLevel"> loggingSetLevel;
    RpcMethod<void(LoggingMessageNotificationParams), "notifications/message"> loggingMessage;
    RpcMethod<void(CreateMessageRequestParams), "sampling/createMessage"> createMessage;
    RpcMethod<CompleteResult(CompleteRequestParams), "completion/complete"> completionComplete;
    RpcMethod<ListRootsResult(EmptyRequestParams), "roots/list"> rootsList;
    RpcMethod<void(EmptyRequestParams), "notifications/roots/list_changed"> rootsListChanged;
    RpcMethod<void(CancelledNotificationParams), "notifications/cancelled"> cancelled;
    RpcMethod<EmptyResult(EmptyRequestParams), "ping"> ping;
};
} // namespace detail

CCMCP_EN