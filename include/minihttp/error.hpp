#pragma once

#include <system_error>

namespace minihttp {

/**
 * @brief The Http Error Category
 * 
 */
class HttpErrorCategory final : public std::error_category {
public:

#if defined(_MSC_VER)
    // Extension for MSVC STL, use this to prevent the different dll has different address
    constexpr HttpErrorCategory() noexcept : std::error_category(0x114514 + 0x1919810 + 0x10086) {}
#else
    constexpr HttpErrorCategory() noexcept {};
#endif // _MSV_VER

    auto name() const noexcept -> const char* override;
    auto message(int code) const -> std::string override;
};

/**
 * @brief The Http Error
 * 
 */
enum class HttpError {
    InvalidHeader,
    InvalidLine, // The First line of the request is invalid
    InvalidState, // The current state of the HttpStream is invalid
    InvalidChunkFormat, // The chunk size string is invalid
};

inline auto HttpErrorCategory::name() const noexcept -> const char* {
    return "minihttp";
}

inline auto HttpErrorCategory::message(int code) const -> std::string {
    switch (HttpError(code)) {
        case HttpError::InvalidHeader: return "Invalid Header";
        case HttpError::InvalidLine: return "Invalid Line";
        case HttpError::InvalidState: return "Invalid State";
        case HttpError::InvalidChunkFormat: return "Invalid Chunk Format";
        default: return "Unknown Error";
    }
}

// Create a std::error_code from an Error
inline auto make_error_code(HttpError error) noexcept -> std::error_code {
    static constinit HttpErrorCategory category;
    return {static_cast<int>(error), category};
}

} // namespace minihttp

template <>
struct std::is_error_code_enum<minihttp::HttpError> : std::true_type {};