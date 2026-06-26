#pragma once

#include "../global/global.hpp"

#include <ilias/io/error.hpp>
#include <ilias/task/task.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

NEKO_BEGIN_NAMESPACE

class StdioStream {
    template <typename T>
    using IoTask = ILIAS_NAMESPACE::IoTask<T>;

public:
    StdioStream();
    StdioStream(StdioStream&&) noexcept;
    StdioStream(const StdioStream&) = delete;
    ~StdioStream();

    auto operator=(StdioStream&&) noexcept -> StdioStream&;
    auto operator=(const StdioStream&) -> StdioStream& = delete;

    auto recv(std::vector<std::byte>& buffer) -> IoTask<void>;
    auto send(std::span<const std::byte> data) -> IoTask<void>;
    auto close() -> void;
    auto start() -> IoTask<void>;
    auto shutdown() -> IoTask<void>;
    auto flush() -> IoTask<void>;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

NEKO_END_NAMESPACE
