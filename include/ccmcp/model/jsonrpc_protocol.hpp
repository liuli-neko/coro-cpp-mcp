#pragma once

#include "ccmcp/global/global.hpp"

#include "ccmcp/model/capabilities.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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
    std::optional<std::string> nextCursor; // 新增

    NEKO_SERIALIZER(tools, nextCursor)
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
    std::vector<ResourceTemplate> resourceTemplates;
    std::optional<std::string> nextCursor;

    NEKO_SERIALIZER(resourceTemplates, nextCursor)
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

    NEKO_SERIALIZER(prompts, nextCursor)
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
    std::variant<TextContent, ImageContent> content;
    std::string model;
    std::optional<std::string> stopReason;

    NEKO_SERIALIZER(role, content, model, stopReason)
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
    RpcMethodSpec<InitializeResult(InitializeRequestParams), rpc_name<"initialize">,
                  rpc_args<"initialize_request_params">, rpc_desc<"Initialize the connection">>
        initialize;
    RpcMethodSpec<void(EmptyRequestParams), rpc_name<"notifications/initialized">,
                  rpc_desc<"Notification that the client has initialized">, rpc_notification>
        initialized;
    RpcMethodSpec<ListResourcesResult(EmptyRequestParams), rpc_name<"resources/list">, rpc_desc<"List resources">>
        resourcesList;
    RpcMethodSpec<ListResourceTemplatesResult(EmptyRequestParams), rpc_name<"resources/templates/list">,
                  rpc_desc<"List resource templates">>
        resourcesTemplatesList;
    RpcMethodSpec<ReadResourceResult(ReadResourceRequestParams), rpc_name<"resources/read">,
                  rpc_args<"read_resource_request_params">, rpc_desc<"Read a resource">>
        resourcesRead;
    RpcMethodSpec<void(ProgressNotificationParams), rpc_name<"notifications/progress">,
                  rpc_args<"progress_notification_params">, rpc_desc<"Progress notification">, rpc_notification>
        progress;
    RpcMethodSpec<void(EmptyRequestParams), rpc_name<"notifications/resources/list_changed">,
                  rpc_desc<"Notification that the list of resources has changed">, rpc_notification>
        resourcesListChanged;
    RpcMethodSpec<void(SubscribeRequestParams), rpc_name<"resources/subscribe">, rpc_desc<"Subscribe to resources">,
                  rpc_args<"subscribe_request_params">>
        resourcesSubscribe;
    RpcMethodSpec<void(UnsubscribeRequestParams), rpc_name<"resources/unsubscribe">,
                  rpc_desc<"Unsubscribe from resources">, rpc_args<"unsubscribe_request_params">>
        resourcesUnsubscribe;
    RpcMethodSpec<void(ResourceUpdatedNotificationParams), rpc_name<"notifications/resources/updated">,
                  rpc_args<"resource_updated_notification_params">,
                  rpc_desc<"Notification that a resource has been updated">>
        resourcesUpdated;
    RpcMethodSpec<ListPromptsResult(EmptyRequestParams), rpc_name<"prompts/list">, rpc_desc<"Get the list of prompts">>
        promptsList;
    RpcMethodSpec<GetPromptResult(GetPromptRequestParams), rpc_name<"prompts/get">, rpc_desc<"Get a prompt">>
        promptsGet;
    RpcMethodSpec<void(EmptyRequestParams), rpc_name<"notifications/prompts/list_changed">,
                  rpc_desc<"list of prompts has changed">, rpc_notification>
        promptsListChanged;
    RpcMethodSpec<ToolsListResult(PaginatedRequest), rpc_name<"tools/list">,
                  rpc_desc<R"(Get the list of tools functioning on the server)">, rpc_args<"paginated_request">>
        toolsList;
    RpcMethodSpec<CallToolResult(ToolCallRequestParams), rpc_name<"tools/call">, rpc_desc<"Call a tool">,
                  rpc_args<"tool_call_request_params">>
        toolsCall;
    RpcMethodSpec<void(EmptyRequestParams), rpc_name<"notifications/tools/list_changed">,
                  rpc_desc<"tools list has changed">, rpc_notification>
        toolsListChanged;
    RpcMethodSpec<void(SetLevelRequestParams), rpc_name<"logging/setLevel">, rpc_desc<"Set logging level">>
        loggingSetLevel;
    RpcMethodSpec<void(LoggingMessageNotificationParams), rpc_name<"notifications/message">, rpc_desc<"Log message">>
        loggingMessage;
    RpcMethodSpec<void(CreateMessageRequestParams), rpc_name<"sampling/createMessage">, rpc_desc<"Create message">>
        createMessage;
    RpcMethodSpec<CompleteResult(CompleteRequestParams), rpc_name<"completion/complete">, rpc_desc<"Complete">>
        completionComplete;
    RpcMethodSpec<ListRootsResult(EmptyRequestParams), rpc_name<"roots/list">, rpc_desc<"List roots">> rootsList;
    RpcMethodSpec<void(EmptyRequestParams), rpc_name<"notifications/roots/list_changed">,
                  rpc_desc<"Roots list has changed">>
        rootsListChanged;
    RpcMethodSpec<void(CancelledNotificationParams), rpc_name<"notifications/cancelled">, rpc_desc<"Cancelled">>
        cancelled;
    RpcMethodSpec<EmptyResult(EmptyRequestParams), rpc_name<"ping">, rpc_desc<"Ping">> ping;
};
} // namespace detail

CCMCP_EN
