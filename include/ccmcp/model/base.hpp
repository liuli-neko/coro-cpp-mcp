#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/types/types.hpp>

CCMCP_BN

enum class Role {
    User,
    Assistant,
};

using ProgressToken = std::variant<int, std::string>;

struct Meta {
    std::optional<ProgressToken> progressToken;

    NEKO_SERIALIZER(progressToken)
};

struct PaginatedRequest {
    std::optional<std::string> cursor;

    NEKO_SERIALIZER(cursor)
};

struct Annotations {
    std::optional<std::vector<Role>> audience;
    std::optional<float> priority;

    NEKO_SERIALIZER(audience, priority)
};

struct TextContent {
    std::string type = "text";
    std::string text;
    std::optional<Annotations> annotations;

    NEKO_SERIALIZER(type, text, annotations)
};

struct ImageContent {
    std::string type = "image";
    // The base64-encoded image data.
    std::string data;
    // The MIME type of the image. Different providers may support different image types.
    std::string mimeType;
    std::optional<Annotations> annotations;

    NEKO_SERIALIZER(type, data, mimeType, annotations)
};

struct SamplingMessage {
    Role role;
    std::variant<TextContent, ImageContent> content;

    NEKO_SERIALIZER(role, content)
};

struct BlobResourceContents {
    std::string blob;

    NEKO_SERIALIZER(blob)
};

struct ResourceMetadata {
    /// the type of the resource, e.g., "file", "url", etc.
    std::optional<std::string> type;
    /// the size of the resource, in bytes
    std::optional<int64_t> size;

    NEKO_SERIALIZER(type, size)
};

struct Resource {
    /// uri of the resource
    std::string uri;
    /// The name of the resource
    std::string name;
    /// The description of the resource
    std::optional<std::string> description;
    /// the metadata of the resource
    std::optional<ResourceMetadata> metadata;
    /// the annotations of the resource
    std::optional<Annotations> annotations;

    NEKO_SERIALIZER(uri, name, description, metadata, annotations)
};

struct ResourceTemplate {
    std::string uriTemplate;
    std::string name;
    std::optional<std::string> description;
    std::optional<ResourceMetadata> metadata;
    std::optional<Annotations> annotations;

    NEKO_SERIALIZER(uriTemplate, name, description, metadata, annotations)
};

struct ResourceContents {
    std::string uri;
    std::optional<std::string> mimeType;

    NEKO_SERIALIZER(uri, mimeType)
};

struct TextResourceContents {
    std::string text;

    NEKO_SERIALIZER(text)
};


CCMCP_EN