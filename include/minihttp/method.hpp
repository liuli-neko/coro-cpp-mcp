#pragma once
#include <stdexcept>
#include <optional>
#include <string>

namespace minihttp {
    
enum class Method {
    Get,
    Post,
    Put,
    Delete,
    Head,
    Options,
    Patch,
};

constexpr auto toString(Method method) -> std::string_view {
    switch (method) {
        case Method::Get: return "GET";
        case Method::Post: return "POST";
        case Method::Put: return "PUT";
        case Method::Delete: return "DELETE";
        case Method::Head: return "HEAD";
        case Method::Options: return "OPTIONS";
        case Method::Patch: return "PATCH";
        default: throw std::runtime_error("Unknown method");
    }
}

constexpr auto stringToMethod(std::string_view str) -> std::optional<Method> {
    if (str == "GET") return Method::Get;
    if (str == "POST") return Method::Post;
    if (str == "PUT") return Method::Put;
    if (str == "DELETE") return Method::Delete;
    if (str == "HEAD") return Method::Head;
    if (str == "OPTIONS") return Method::Options;
    if (str == "PATCH") return Method::Patch;
    return std::nullopt;
}

} // namespace minihttp