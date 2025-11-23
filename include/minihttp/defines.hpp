#pragma once

#include <ilias/io.hpp>
#include <ilias/task.hpp>
#include <ilias/buffer.hpp>
#include <ilias/defines.hpp>

#if !defined(_MINIHTTP_SOURCE_)
    #define MINIHTTP_API ILIAS_IMPORT
#else
    #define MINIHTTP_API ILIAS_EXPORT
#endif // _MINIHTTP_SOURCE_

#define MINIHTTP_THROW(...) \
    throw __VA_ARGS__
#define MINIHTTP_TRY(...) \
    if (auto res = __VA_ARGS__; !res) co_return Err(res.error()) 

namespace minihttp {
    using ilias::IoGenerator;
    using ilias::IoResult;
    using ilias::IoError;
    using ilias::IoTask;
    using ilias::Task;
    using ilias::Generator;
    using ilias::MutableBuffer;
    using ilias::Buffer;
    using ilias::StreamView;
    using ilias::Stream;
    using ilias::Err;

    template<class... Ts>
    struct Overloads : public Ts... { using Ts::operator()...; };
} // namespace minihttp