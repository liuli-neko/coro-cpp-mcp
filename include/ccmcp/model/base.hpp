#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/types/types.hpp>

CCMCP_BN

enum class Role {
    User,
    Assistant,
};

struct RawTextContent {
    std::string text;

    NEKO_SERIALIZER(text)
};

struct RawAudioContent {
    std::string data;
    std::string mimeType;

    NEKO_SERIALIZER(data, mimeType)
};

struct RawImageContent {
    std::string data;
    std::string mimeType;

    NEKO_SERIALIZER(data, mimeType)
};

struct TextResourceContents {
    std::string uri;
    std::optional<std::string> mimeType;
    std::string text;

    NEKO_SERIALIZER(uri, mimeType, text)
};

struct BlobResourceContents {
    std::string uri;
    std::optional<std::string> mimeType;
    std::string blob;

    NEKO_SERIALIZER(uri, mimeType, blob)
};

struct ResourceContents : public std::variant<TextResourceContents, BlobResourceContents> {};

CCMCP_EN