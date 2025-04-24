#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

CCMCP_BN

using JsonValue                = NEKO_NAMESPACE::JsonSerializer::JsonValue;
using ExperimentalCapabilities = std::map<std::string, JsonValue>;

struct RootsCapabilities {
    bool list_changed;

    NEKO_SERIALIZER(list_changed)
};

struct PromptsCapability {
    std::optional<bool> list_changed;

    NEKO_SERIALIZER(list_changed)
};

struct ToolsCapability {
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
    std::optional<JsonValue> sampling;

    NEKO_SERIALIZER(experimental, roots, sampling)
};

struct ServerCapabilities {
    std::optional<ExperimentalCapabilities> experimental;
    std::optional<JsonValue> logging;
    std::optional<JsonValue> completions;
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

struct InitializeRequestParam {
    std::string protocolVersion;
    ClientCapabilities capabilities;
    Implementation clientInfo;

    NEKO_SERIALIZER(protocolVersion, capabilities, clientInfo)
};

struct InitializeResult {
    std::string protocolVersion;
    ServerCapabilities capabilities;
    Implementation serverInfo;
    std::optional<std::string> instructions;

    NEKO_SERIALIZER(protocolVersion, capabilities, serverInfo, instructions)
};

CCMCP_EN