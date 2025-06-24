#pragma once

#include "ccmcp/global/global.hpp"

#include "ccmcp/model/base.hpp"

CCMCP_BN
using NEKO_NAMESPACE::JsonSchema;
using JsonValue                = NEKO_NAMESPACE::JsonSerializer::JsonValue;
using ExperimentalCapabilities = std::map<std::string, JsonValue>;
using CompletionsCapability = JsonValue;

struct RootsCapabilities {
    bool list_changed;

    NEKO_SERIALIZER(list_changed)
};

struct PromptsCapability {
    std::optional<bool> list_changed;

    NEKO_SERIALIZER(list_changed)
};

struct RootsCapability {
    std::optional<bool> listChanged;

    NEKO_SERIALIZER(listChanged)
};

struct SamplingCapability {
    std::optional<bool> listChanged;

    NEKO_SERIALIZER(listChanged)
};

struct ToolsCapability {
    std::optional<bool> list_changed;

    NEKO_SERIALIZER(list_changed)
};

struct LoggingCapability {
    std::optional<bool> list_changed;

    NEKO_SERIALIZER(list_changed)
};

struct ResourcesCapability {
    std::optional<bool> subscribe;
    std::optional<bool> list_changed;

    NEKO_SERIALIZER(subscribe, list_changed)
};

struct ClientCapabilities {
    std::optional<ExperimentalCapabilities> experimental;
    std::optional<RootsCapabilities> roots;
    std::optional<SamplingCapability> sampling;

    NEKO_SERIALIZER(experimental, roots, sampling)
};

struct ServerCapabilities {
    std::optional<ExperimentalCapabilities> experimental;
    std::optional<LoggingCapability> logging;
    std::optional<CompletionsCapability> completions;
    std::optional<PromptsCapability> prompts;
    std::optional<ResourcesCapability> resources;
    std::optional<ToolsCapability> tools;

    NEKO_SERIALIZER(experimental, logging, completions, prompts, resources, tools)
};

struct Implementation {
    std::string name;
    std::string version;

    NEKO_SERIALIZER(name, version)
};

struct ToolAnnotations {
    /// A human-readable title for the tool.
    std::optional<std::string> title;

    /// If true, the tool does not modify its environment.
    ///
    /// Default: false
    std::optional<bool> readOnlyHint;

    /// If true, the tool may perform destructive updates to its environment.
    /// If false, the tool performs only additive updates.
    ///
    /// (This property is meaningful only when `readOnlyHint == false`)
    ///
    /// Default: true
    /// A human-readable description of the tool's purpose.
    std::optional<bool> destructiveHint;

    /// If true, calling the tool repeatedly with the same arguments
    /// will have no additional effect on the its environment.
    ///
    /// (This property is meaningful only when `readOnlyHint == false`)
    ///
    /// Default: false.
    std::optional<bool> idempotentHint;

    /// If true, this tool may interact with an "open world" of external
    /// entities. If false, the tool's domain of interaction is closed.
    /// For example, the world of a web search tool is open, whereas that
    /// of a memory tool is not.
    ///
    /// Default: true
    std::optional<bool> openWorldHint;

    NEKO_SERIALIZER(title, readOnlyHint, destructiveHint, idempotentHint, openWorldHint)
};

struct Tool {
    /// The name of the tool
    std::string name;
    /// A description of what the tool does
    std::optional<std::string> description;
    /// A JSON Schema object defining the expected parameters for the tool
    std::shared_ptr<JsonSchema> inputSchema;
    /// Optional additional tool information.
    std::optional<ToolAnnotations> annotations;

    NEKO_SERIALIZER(name, description, inputSchema, annotations)
};
CCMCP_EN

NEKO_BEGIN_NAMESPACE

template <>
struct is_minimal_serializable<ccmcp::PaginatedRequest> : std::true_type {};

NEKO_END_NAMESPACE