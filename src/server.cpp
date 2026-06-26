#include "ccmcp/server/server.hpp"

#include "ccmcp/server/system_info.hpp"

#include <nekoproto/serialization/types/binary_data.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>

CCMCP_BN

namespace {
auto to_lower(std::string_view str) -> std::string {
    std::string lower_str(str);
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower_str;
}

auto mime_type_map() -> const std::unordered_map<std::string, std::string>& {
    static const std::unordered_map<std::string, std::string> map = {
        {".txt", "text/plain"},
        {".md", "text/markdown"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".py", "text/x-python"},
        {".cpp", "text/x-c++"},
        {".hpp", "text/x-c++"},
        {".h", "text/x-c++"},
        {".c", "text/x-c"},
        {".java", "text/x-java-source"},
        {".rs", "text/rust"},
        {".toml", "application/toml"},
        {".yaml", "application/yaml"},
        {".yml", "application/yaml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"},
    };
    return map;
}

auto is_text_mime(std::string_view mime_type) -> bool {
    return mime_type.rfind("text/", 0) == 0 || mime_type == "application/json" ||
           mime_type == "application/javascript" || mime_type == "application/xml";
}
} // namespace

namespace detail {
auto createResourceContentsFromFile(const std::filesystem::path& path, const std::string& uri) -> ResourceContents {
    constexpr uintmax_t max_file_size = 16 * 1024 * 1024;

    try {
        if (!std::filesystem::exists(path)) {
            NEKO_LOG_WARN("mcp server", "Resource file not found: {}", path.string());
            return TextResourceContents{.uri = uri, .text = "Error: File not found.", .mimeType = "text/plain"};
        }

        const auto file_size = std::filesystem::file_size(path);
        if (file_size > max_file_size) {
            NEKO_LOG_WARN("mcp server", "Resource file exceeds size limit ({} > {} bytes): {}", file_size,
                          max_file_size, path.string());
            return TextResourceContents{
                .uri = uri, .text = "Error: File is too large to read.", .mimeType = "text/plain"};
        }

        const std::string extension = to_lower(path.extension().string());
        const auto& mime_types      = mime_type_map();
        const auto it               = mime_types.find(extension);
        const std::string mime_type = (it != mime_types.end()) ? it->second : "application/octet-stream";

        if (is_text_mime(mime_type)) {
            std::ifstream file(path);
            if (!file.is_open()) {
                return TextResourceContents{
                    .uri = uri, .text = "Error: Could not open file.", .mimeType = "text/plain"};
            }

            std::string text_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return TextResourceContents{.uri = uri, .text = std::move(text_content), .mimeType = mime_type};
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return TextResourceContents{.uri = uri, .text = "Error: Could not open file.", .mimeType = "text/plain"};
        }

        std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
        auto data = NEKO_NAMESPACE::Base64Covert::Encode(buffer);
        return BlobResourceContents{.uri = uri, .blob = std::string(data.begin(), data.end()), .mimeType = mime_type};
    } catch (const std::filesystem::filesystem_error& e) {
        NEKO_LOG_ERROR("mcp server", "Filesystem error reading resource {}: {}", path.string(), e.what());
        return TextResourceContents{.uri = uri, .text = "Error: Filesystem error.", .mimeType = "text/plain"};
    }
}
} // namespace detail

McpServer<void>::McpServer(IoContext& ctx) : mServer(ctx) { _register_rpc_methods(); }

auto McpServer<void>::setCapabilities(const ExperimentalCapabilities& capabilities) noexcept -> void {
    mCapabilities.experimental = capabilities;
}

auto McpServer<void>::setCapabilities(const LoggingCapability& capabilities) noexcept -> void {
    mCapabilities.logging = capabilities;
}

auto McpServer<void>::setCapabilities(const CompletionsCapability& capabilities) noexcept -> void {
    mCapabilities.completions = capabilities;
}

auto McpServer<void>::setCapabilities(const PromptsCapability& capabilities) noexcept -> void {
    mCapabilities.prompts = capabilities;
}

auto McpServer<void>::setCapabilities(const ResourcesCapability& capabilities) noexcept -> void {
    mCapabilities.resources = capabilities;
}

auto McpServer<void>::setCapabilities(const ToolsCapability& capabilities) noexcept -> void {
    mCapabilities.tools = capabilities;
}

void McpServer<void>::setInstructions(std::string_view instructions) noexcept { mInstructions = instructions; }

void McpServer<void>::_register_rpc_methods() {
    mServer->initialize  = std::bind(&McpServer::_initialize, this, std::placeholders::_1);
    mServer->initialized = std::function<IoTask<void>(EmptyRequestParams)>(
        std::bind(&McpServer::_initialized, this, std::placeholders::_1));
    mServer->toolsList     = std::bind(&McpServer::_tools_list, this, std::placeholders::_1);
    mServer->toolsCall     = std::bind(&McpServer::_tools_call, this, std::placeholders::_1);
    mServer->resourcesList = [this](EmptyRequestParams) -> ListResourcesResult {
        return ListResourcesResult{.resources = mResourceList, .nextCursor = std::nullopt};
    };
    mServer->resourcesTemplatesList = [](EmptyRequestParams) -> ListResourceTemplatesResult {
        return ListResourceTemplatesResult{.resourceTemplates = {}, .nextCursor = std::nullopt};
    };
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
    mServer->promptsList          = [](EmptyRequestParams) -> ListPromptsResult {
        return ListPromptsResult{.prompts = {}, .nextCursor = std::nullopt};
    };
    mServer->promptsGet = [](GetPromptRequestParams) -> GetPromptResult {
        return GetPromptResult{.description = "default", .messages = {}, ._meta = std::nullopt};
    };
    mServer->promptsListChanged = [](EmptyRequestParams) -> void {};
    mServer->toolsListChanged   = [](EmptyRequestParams) -> void {};
    mServer->loggingSetLevel    = [](SetLevelRequestParams) -> void {};
    mServer->loggingMessage     = [](LoggingMessageNotificationParams) -> void {};
    mServer->createMessage      = [](CreateMessageRequestParams) -> void {};
    mServer->completionComplete = [](CompleteRequestParams) -> CompleteResult {
        return CompleteResult{.completion = {.values = {}, .total = 0, .hasMore = false}, ._meta = std::nullopt};
    };
    mServer->rootsList = [](EmptyRequestParams) -> ListRootsResult {
        return ListRootsResult{.roots = {}, ._meta = std::nullopt};
    };
    mServer->rootsListChanged = [](EmptyRequestParams) -> void {};
    mServer->cancelled        = std::function<IoTask<void>(CancelledNotificationParams)>(
        std::bind(&McpServer::_cancelled, this, std::placeholders::_1));
}

auto McpServer<void>::_initialize(InitializeRequestParams params) noexcept -> IoTask<InitializeResult> {
    NEKO_LOG_INFO("mcp server", "initialize protocol version: {}", params.protocolVersion);
    co_return InitializeResult{.protocolVersion = LATEST_PROTOCOL_VERSION,
                               .capabilities    = mCapabilities,
                               .serverInfo =
                                   Implementation{.name = CCMCP_PROJECT_NAME, .version = CCMCP_VERSION_STRING},
                               .instructions = mInstructions};
}

auto McpServer<void>::_initialized(EmptyRequestParams) noexcept -> IoTask<void> {
    NEKO_LOG_INFO("mcp server", "initialized");
    co_return {};
}

auto McpServer<void>::close() -> void { mServer.close(); }

auto McpServer<void>::wait() -> Task<void> { co_await mServer.wait(); }

auto McpServer<void>::jsonRpcServer() -> JsonRpcServer<detail::McpJsonRpcMethods>& { return mServer; }

auto McpServer<void>::toolsList([[maybe_unused]] const PaginatedRequest& params) -> ToolsListResult {
    ToolsListResult result;
    for (const auto& tool : mTools) {
        result.tools.push_back(tool());
    }
    return result;
}

auto McpServer<void>::_tools_list(PaginatedRequest params) noexcept -> IoTask<ToolsListResult> {
    co_return toolsList(params);
}

auto McpServer<void>::_cancelled(CancelledNotificationParams params) noexcept -> IoTask<void> {
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

auto McpServer<void>::_tools_call(ToolCallRequestParams params) noexcept -> IoTask<CallToolResult> {
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
        info.error = "Tool " + params.name + " not found";
    }
    info.executionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time).count();
    info.resourceUsage.memory = std::to_string(getCurrentRSS() / 1024) + "KB";
    result.metadata           = NEKO_NAMESPACE::to_json_value(info);
    co_return result;
}

auto McpServer<void>::registerLocalFileResource(std::string_view name, std::filesystem::path path, std::string_view uri,
                                                std::string_view description) -> bool {
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
    registerResource(resource, [path = std::move(path), uri = resource.uri](std::optional<Meta>) -> ResourceContents {
        return detail::createResourceContentsFromFile(path, uri);
    });
    return true;
}

auto McpServer<void>::registerResource(Resource resource, ResourceContents contents) -> void {
    registerResource(std::move(resource),
                     [contents = std::move(contents)](std::optional<Meta>) -> ResourceContents { return contents; });
}

auto McpServer<void>::registerResource(Resource resource, std::function<ResourceContents(std::optional<Meta>)> contents)
    -> void {
    mResources[resource.uri] = std::move(contents);
    mResourceList.push_back(std::move(resource));
}

CCMCP_EN
