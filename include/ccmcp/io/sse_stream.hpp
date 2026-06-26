#pragma once

#include "../global/global.hpp"

#include <ilias/io/error.hpp>
#include <ilias/task/task.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace ILIAS_NAMESPACE {
class TcpListener;
}

NEKO_BEGIN_NAMESPACE

class SseServerStream {
public:
    SseServerStream(SseServerStream&&) noexcept;
    ~SseServerStream();

    auto recv(std::vector<std::byte>& buffer) -> ilias::IoTask<void>;
    auto send(std::span<const std::byte> data) -> ilias::IoTask<void>;
    auto close() -> void;
    auto start() -> ilias::IoTask<void>;
    auto shutdown() -> ilias::IoTask<void>;
    auto flush() -> ilias::IoTask<void>;

    auto operator=(SseServerStream&&) noexcept -> SseServerStream&;

private:
    SseServerStream();

    struct Impl;
    std::unique_ptr<Impl> mImpl;
    friend class SseListener;
};

class SseListener {
public:
    explicit SseListener(ilias::TcpListener listener);
    SseListener(SseListener&&) noexcept;
    SseListener(const SseListener&) = delete;
    ~SseListener();

    auto operator=(SseListener&&) noexcept -> SseListener&;
    auto operator=(const SseListener&) -> SseListener& = delete;

    auto close() -> void;
    auto cancel() -> void;
    auto accept() -> ilias::IoTask<SseServerStream>;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

NEKO_END_NAMESPACE
