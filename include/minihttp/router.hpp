#pragma once

#include <minihttp/detail/fn.hpp> // FnLike, FnTraits
#include <minihttp/headers.hpp> // Headers
#include <minihttp/status.hpp> // Status
#include <minihttp/method.hpp> // Method
#include <minihttp/url.hpp> // Url
#include <minihttp/h1.hpp> // Http1Connection
#include <ilias/task.hpp> // TcpListener
#include <ilias/net.hpp> // TcpListener
#include <ilias/io.hpp> // Stream, StreamView
#include <unordered_map>
#include <functional>
#include <ranges> // views::split
#include <string>
#include <span>

namespace minihttp::server {
namespace detail {

// For unordered_map
struct StringHash {
    using is_transparent = void;

    auto operator()(std::string_view str) const noexcept -> size_t {
        return std::hash<std::string_view>{}(str);
    }
};

} // namespace detail

using ilias::StreamView;
using ilias::Stream;

using UrlParams  = std::unordered_map<std::string, std::string, detail::StringHash, std::equal_to<> >;
using BodyStream = StreamView;

// Elements of the request or response
template <typename T>
struct Text {
    T text;
};

template <typename T>
struct Json {
    T json;
};

template <typename T>
struct Html {
    T html;
};

template <typename T>
struct Blob {
    T blob;
};

// Extract the data from the path
template <typename T>
struct Path {
    T path;
};

template <typename T>
struct Query {
    T query;
};

// SSE
struct SseEvent {
    std::string_view comment;
    std::string_view event; // The event name
    std::string_view data;
    std::optional<uint32_t> retry; 
};

struct Sse {
    Generator<SseEvent> stream;
};

// The core struct
struct BodyChunk {
    Buffer data;
    bool   flush = false;
};

struct Request {
    Method method;
    std::string_view path;
    std::string_view params;
    Headers   &headers;
    BodyStream content;
};

struct Response {
    Status status;
    Headers headers;
    IoGenerator<BodyChunk> content;
};

template <typename T>
struct Parser;

/**
 * @brief Check the type can be used to to a response
 * 
 * @tparam T 
 */
template <typename T>
concept ToResponse = requires(T t) {
    { toResponse(std::forward<T>(t)) } -> std::convertible_to<Response>;
};

template <typename T>
concept FromRequest = requires(T t) {
    { Parser<T>{}.parse(std::declval<Request &>()) } -> std::same_as<IoTask<T> >;
};

template <typename T>
concept Element = ToResponse<T> || FromRequest<T>;

// Helpers generator
inline auto makeGenerator(Buffer buffer) -> IoGenerator<BodyChunk> {
    co_yield BodyChunk { .data = buffer };
}

inline auto makeGenerator(std::string_view str) -> IoGenerator<BodyChunk> {
    return makeGenerator(ilias::makeBuffer(str));
}

inline auto makeGenerator(const char *str) -> IoGenerator<BodyChunk> {
    return makeGenerator(ilias::makeBuffer(std::string_view(str)));
}

inline auto makeGenerator(std::string str) -> IoGenerator<BodyChunk> {
    co_yield BodyChunk { .data = ilias::makeBuffer(str) };
}

inline auto makeGenerator(std::vector<std::byte> bytes) -> IoGenerator<BodyChunk> {
    co_yield BodyChunk { .data = ilias::makeBuffer(bytes) };
}

// For Handler concepts
namespace detail {
    // Utils for check the arguments of the handler
    template <typename ...Ts>
    inline constexpr bool AllFromRequest = false;

    template <typename ...Ts>
    inline constexpr bool AllFromRequest<std::tuple<Ts...> > = (FromRequest<Ts> && ...);

    // Check the handler return a response
    template <typename T>
    inline constexpr bool AsyncReturnResponse = false;

    template <typename T>
    inline constexpr bool AsyncReturnResponse<Task<T> > = ToResponse<T>;

    template <typename T>
    inline constexpr bool ReturnResponse = ToResponse<T> || AsyncReturnResponse<T>;
} // namespace detail

/**
 * @brief Check an object can be used as a router handler
 * 
 * @tparam Fn 
 */
template <typename Fn>
concept Handler = 
    FnLike<Fn> && 
    detail::ReturnResponse<typename FnTraits<Fn>::ReturnType> &&
    detail::AllFromRequest<typename FnTraits<Fn>::Arguments>
;

/**
 * @brief The router, map the path and method to a handler
 * 
 */
class Router {
public:
    Router(const Router &) = delete;
    Router(Router &&) = default; // Only allow move
    Router() = default;

    /**
     * @brief Add an handler for get method
     * 
     * @tparam Fn 
     * @param path 
     * @param handler 
     */
    template <Handler Fn>
    auto get(std::string_view path, Fn handler) && -> Router {
        return std::move(*this).route(Method::Get, path, handler);
    }

    template <Handler Fn>
    auto post(std::string_view path, Fn handler) && -> Router {
        return std::move(*this).route(Method::Post, path, handler);
    }

    /**
     * @brief Add an handler for the methid
     * 
     * @tparam Fn 
     * @param method 
     * @param path 
     * @param handler 
     * @return Router 
     */
    template <Handler Fn>
    auto route(Method method, std::string_view path, Fn handler) && -> Router;
private:
    using DynHandler = std::function<Task<Response> (Request)>;

    template <typename Tuple, size_t ...N>
    static auto parseRequest(Request &request, std::index_sequence<N...>);

    template <typename ...Args>
    static auto parseRequestImpl(Request &request) -> IoTask<std::tuple<Args...> >;

    auto findHandler(Method method, std::string_view path) -> DynHandler *;
    auto addHandler(Method method, std::string_view path, DynHandler handler) -> void;
    auto onRequest404(Request) -> Task<Response>;
    auto handle(StreamView stream) -> IoTask<void>;

    struct TrieNode {
        std::map<std::string, TrieNode, std::less<> > children;
        std::map<Method, DynHandler> handlers;
        bool isWildcard = false;
    };

    TrieNode mRoot; // The root of the trie (/)
friend auto serve(ilias::TcpListener, Router) -> IoTask<void>;
};

template <Handler Fn>
inline auto Router::route(Method method, std::string_view pathView, Fn rawHandler) && -> Router {
    // Check all arguments is FromRequest
    using Arguments = typename FnTraits<Fn>::Arguments;
    using ReturnType = typename FnTraits<Fn>::ReturnType;

    auto handler = [h = std::move(rawHandler)](Request request) -> Task<Response> {
        auto sequence = std::make_index_sequence<std::tuple_size_v<Arguments> >();
        auto args = co_await parseRequest<Arguments>(request, sequence);
        if (!args) { // Failed to parse the arguments
            co_return Response {
                .status = Status::BadRequest,
                .content = makeGenerator("<html>Bad Request</html>" + args.error().message()),
            };
        }

        // Invoke the handler 
        if constexpr (detail::AsyncReturnResponse<ReturnType>) {
            if constexpr (std::is_same_v<ReturnType, Task<void> >) {
                co_await std::apply(h, std::move(*args));
                co_return Response {};
            }
            else {
                co_return toResponse(
                    co_await std::apply(h, std::move(*args))
                );
            }
            // The handler is async
        }
        else {
            if constexpr (std::is_same_v<ReturnType, void>) {
                std::apply(h, std::move(*args));
            }
            else {
                co_return toResponse(
                    std::apply(h, std::move(*args))
                );
            }
            // The handler is sync
        }
    };

    addHandler(method, pathView, std::move(handler));
    return std::move(*this);
}

// Implmentation
template <typename Tuple, size_t ...N>
inline auto Router::parseRequest(Request &request, std::index_sequence<N...>) {
    if constexpr (sizeof...(N) == 0) { // No arguments, just return
        struct Awaiter {
            auto await_ready() { return true; }
            auto await_suspend(std::coroutine_handle<>) { return false; }
            auto await_resume() -> IoResult<std::tuple<> > { return {}; }
        };
        return Awaiter {};
    }
    else {
        return parseRequestImpl<std::tuple_element_t<N, Tuple>...>(request);
    }
}

template <typename ...Args>
inline auto Router::parseRequestImpl(Request &request) -> IoTask<std::tuple<Args...> > {
    // Parse the arguments
    try {
        auto unwrap = [](auto result) { // Used for avoid gcc bug error: insufficient contextual information to determine type :(
            return std::move(result).value();
        };
        co_return std::tuple<Args...> { // Because we can't check error and return in ... so use exception here
            unwrap(co_await Parser<Args>{}.parse(request))... // Just parse and unwrap the result
        };
    }
    catch (ilias::BadResultAccess<std::error_code> &e) { // Failed to parse the arguments
        co_return Err(e.error());
    }
}

inline auto Router::handle(StreamView connStream) -> IoTask<void> {
    auto defaultHandler = DynHandler { std::bind(&Router::onRequest404, this, std::placeholders::_1) };
    auto conn = Http1Connection { connStream };
    auto headers = Headers {};
    auto method = Method {};
    auto fullPath = std::string {};
    while (true) {
        auto stream = co_await conn.recvStream();
        if (!stream) {
            co_return Err(stream.error());
        }
        headers.clear();
        fullPath.clear();
        MINIHTTP_TRY(co_await stream->readRequest(method, fullPath, headers));

#if !defined(NDEBUG)
        ::fprintf(stderr, "[minihttp] %s: %s\n", toString(method).data(), fullPath.c_str());
#endif // NDEBUG

        // Process the path if it has params
        auto path = std::string_view { fullPath };
        auto params = std::string_view {};
        if (auto pos = path.find('?'); pos != std::string_view::npos) {
            params = path.substr(pos + 1);
            path = path.substr(0, pos);
        }
        else {
        }

        // Try find the handler
        auto handler = findHandler(method, path);
        if (!handler) {
            handler = &defaultHandler;
        }

        // Invoke it
        auto request = Request {
            .method = method,
            .path = path,
            .params = params,
            .headers = headers,
            .content = BodyStream { *stream },
        };
        auto response = co_await (*handler)(request);
        MINIHTTP_TRY(co_await stream->writeResponse(response.status, response.headers));
        if (response.content) { // If the response has content
            ilias_for_await(auto &chunk, response.content) { // Write all the content
                if (!chunk) {
                    co_return Err(chunk.error());
                }
                MINIHTTP_TRY(co_await stream->writeAll(chunk->data));
                if (chunk->flush) {
                    MINIHTTP_TRY(co_await stream->flush());
                }
            }
        }
        MINIHTTP_TRY(co_await stream->writeEnd());
        MINIHTTP_TRY(co_await stream->flush());
    }
}

inline auto Router::onRequest404(Request request) -> Task<Response> {
    co_return Response {
        .status = Status::NotFound,
        .content = makeGenerator("<html>404 Not Found</html>")
    };
}

inline auto Router::addHandler(Method method, std::string_view path, DynHandler handler) -> void {
    // Split into /
    // '/root' -> ["root"]
    // '/root/' -> ["root", ""]
    // '/' -> [""]
    if (!path.starts_with('/')) {
        MINIHTTP_THROW(std::invalid_argument("Invalid path: " + std::string(path) + ", Path should start with '/'"));
    }
    path.remove_prefix(1);
    auto curNode = &mRoot;
    auto emptyOnce = false; // Check duplicate '/'
    auto wildcard = false; // CHeck '*'
    auto buffer = std::string {};
    for (auto subRange : std::views::split(path, '/')) {
        auto subPath = std::string_view { subRange.begin(), subRange.end() };
        if (subPath.empty()) {
            if (emptyOnce) { // '/root//'
                MINIHTTP_THROW(std::invalid_argument("Invalid path: " + std::string(path) + ", Path can't have duplicate '/'"));
            }
            emptyOnce = true;
            continue;
        }
        if (subPath == "*") {
            wildcard = true;
            continue;
        }
        // Make the buffer
        buffer.assign(subPath);
        auto [it, insert] = curNode->children.insert_or_assign(buffer, TrieNode {});
        // Move it
        curNode = &it->second;
    }
    curNode->handlers.emplace(method, std::move(handler));
    curNode->isWildcard = wildcard;
}

inline auto Router::findHandler(Method method, std::string_view path) -> DynHandler * {
    if (!path.starts_with('/')) {
        return nullptr;
    }
    path.remove_prefix(1);
    auto curNode = &mRoot;
    auto emptyOnce = false;
    for (auto subRange : std::views::split(path, '/')) {
        auto subPath = std::string_view { subRange.begin(), subRange.end() };
        if (subPath.empty()) {
            if (emptyOnce) { // '/root//'
                return nullptr;
            }
            emptyOnce = true;
            continue;
        }
        if (curNode->isWildcard) {
            break;
        }
        auto it = curNode->children.find(subPath);
        if (it == curNode->children.end()) {
            return nullptr;
        }
        curNode = &it->second;
    }
    auto it = curNode->handlers.find(method);
    if (it != curNode->handlers.end()) {
        return &it->second;
    }
    return nullptr;
}

/**
 * @brief Begin serving the router
 * 
 * @param listener 
 * @param router 
 * @return IoTask<void> 
 */
inline auto serve(ilias::TcpListener listener, Router router) -> IoTask<void> {
    co_return co_await ilias::TaskScope::enter([&](auto &scope) -> IoTask<void> {
        while (true) {
            auto res = co_await listener.accept();
            if (!res) {
                continue; // Discard the error or quit ?
            }
            auto [stream, endpoint] = std::move(*res);

#if !defined(NDEBUG)
            ::fprintf(stderr, "[minihttp] conn accept %s\n", endpoint.toString().c_str());
#endif // NDEBUG

            scope.spawn([](auto stream, auto router) -> Task<void> {
                if (auto res = co_await router->handle(stream); !res) {
#if !defined(NDEBUG)
                    auto err = res.error();
                    ::fprintf(stderr, "[minihttp] conn close by %s => %s\n", err.category().name(), err.message().c_str());
#endif // NDEBUG
                }
            }(std::move(stream), &router));
        }
        co_return {};
    });
}

// Impl the FromRequest concept
template <>
struct Parser<Method> {
    auto parse(Request &request) -> IoTask<Method> {
        co_return request.method;
    }
};

template <>
struct Parser<Blob<std::vector<std::byte> > > {
    auto parse(Request &request) -> IoTask<Blob<std::vector<std::byte> > > {
        auto vec = std::vector<std::byte> {};
        MINIHTTP_TRY(co_await request.content.readToEnd(vec));
        co_return Blob { .blob = std::move(vec) };
    }
};

template <>
struct Parser<Blob<BodyStream> > {
    auto parse(Request &request) -> IoTask<Blob<BodyStream> > {
        co_return Blob { .blob = request.content };
    }
};

template <>
struct Parser<Text<std::string> > {
    auto parse(Request &request) -> IoTask<Text<std::string> > {
        auto str = std::string {};
        MINIHTTP_TRY(co_await request.content.readToEnd(str));
        co_return Text { .text = std::move(str) };
    }
};

template <typename T> requires (std::convertible_to<T, std::string_view>)
struct Parser<Path<T> > {
    auto parse(Request &request) -> IoTask<Path<T> > {
        co_return Path { T(request.path) };
    }
};

template <>
struct Parser<Query<UrlParams> > {
    auto parse(Request &request) -> IoTask<Query<UrlParams> > {
        // Split the params by '&'
        auto params = UrlParams {};
        for (auto subRange : std::views::split(request.params, '&')) {
            auto item = std::string_view { subRange.begin(), subRange.end() };
            auto pos = item.find('=');
            if (pos == std::string_view::npos) { // No '='
                params.emplace(Url::decodeComponent(item), std::string {});
                continue;
            }
            auto key = item.substr(0, pos);
            auto value = item.substr(pos + 1);
            params.emplace(Url::decodeComponent(key), Url::decodeComponent(value));
        }
        co_return Query { std::move(params) };
    }
};

// Impl the ToResponse concept for builtin elements
template <typename T>
inline auto toResponse(Text<T> text) -> Response {
    return Response {
        .status = Status::Ok,
        .headers = Headers {
            { "Content-Type", "text/plain" }
        },
        .content = makeGenerator(std::move(text.text))
    };
}

template <typename T>
inline auto toResponse(Html<T> html) -> Response {
    return Response {
        .status = Status::Ok,
        .headers = Headers {
            { "Content-Type", "text/html; charset=utf-8" }  
        },
        .content = makeGenerator(std::move(html.html))
    };
}

template <typename T>
inline auto toResponse(Blob<T> blob) -> Response {
    return Response {
        .status = Status::Ok,
        .headers = Headers {
            { "Content-Type", "application/octet-stream" }
        },
        .content = makeGenerator(std::move(blob.blob))
    };
}

inline auto toResponse(Sse sse) -> Response {
    auto subGenerator = [](Generator<SseEvent> gen) -> IoGenerator<BodyChunk> {
        using namespace std::literals;
        ilias_for_await(auto &e, gen) {
            // Write the event
            if (!e.comment.empty()) {
                auto comment = ": " + std::string(e.comment) + "\n";
                co_yield BodyChunk { .data = ilias::makeBuffer(comment)};
            }
            if (!e.event.empty()) {
                auto event = "event: " + std::string(e.event) + "\n";
                co_yield BodyChunk { .data = ilias::makeBuffer(event)};
            }
            if (!e.data.empty()) {
                auto data = "data: " + std::string(e.data) + "\n";
                co_yield BodyChunk { .data = ilias::makeBuffer(data)};
            }
            if (e.retry) {
                auto retry = "retry: " + std::to_string(*e.retry) + "\n";
                co_yield BodyChunk { .data = ilias::makeBuffer(retry)};
            }
            co_yield BodyChunk { .data = ilias::makeBuffer("\n"sv), .flush = true }; // End of event
        }
    };
    return Response {
        .status = Status::Ok,
        .headers = Headers {
            { "Content-Type", "text/event-stream" },
        },
        .content = subGenerator(std::move(sse.stream))
    };
}

// Some mixing types
template <ToResponse T>
inline auto toResponse(std::pair<Status, T> pair) -> Response {
    auto [status, content] = std::move(pair);
    auto response = toResponse(std::move(content));
    response.status = status;
    return response;
};

template <ToResponse T>
inline auto toResponse(std::pair<Headers, T> pair) -> Response {
    auto [headers, content] = std::move(pair);
    auto response = toResponse(std::move(content));
    for (auto &[key, value] : headers) {
        response.headers.append(key, value);
    }
    return response;
}

template <ToResponse T>
inline auto toResponse(std::tuple<Status, Headers, T> tuple) -> Response {
    auto [status, headers, content] = std::move(tuple);
    auto response = toResponse(std::move(content));
    response.status = status;
    for (auto &[key, value] : headers) {
        response.headers.append(key, value);
    }
    return response;
};


} // namespace minihttp