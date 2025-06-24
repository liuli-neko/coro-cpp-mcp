#pragma once
#include <ilias/buffer.hpp>
#include <ilias/io/stream.hpp>
#include <ilias/sync.hpp>
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <functional>
#include <charconv>
#include <concepts>
#include <variant>
#include <string>
#include <map>

namespace minihttp {

namespace detail {
    
inline auto strcasecmp(std::string_view lhs, std::string_view rhs) noexcept {
    return std::lexicographical_compare_three_way(
        lhs.begin(), lhs.end(),
        rhs.begin(), rhs.end(),
        [](char lhs, char rhs) { return std::tolower(lhs) <=> std::tolower(rhs); }
    );
}

struct CaseCompare {
    using is_transparent = void;

    auto operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return strcasecmp(lhs, rhs) == std::strong_ordering::less;
    }
};

}

using namespace ilias;
using namespace std::literals;

using HeadersMap = std::map<std::string, std::string, detail::CaseCompare>;

struct Response {
    int code;
    HeadersMap headers;

    // Content
    Generator<Buffer> content; // Generator, for SSE
};

// GET / POST mark
template <typename T>
struct Get {
    T fn;
};

template <typename T>
struct Post {
    T fn;
};

// Response helpers
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

struct Request {
    std::string_view method;
    std::string_view path;
    HeadersMap headers;
    Buffer body;
};

template <typename T>
concept IntoResponse = requires(T t) {
    { toResponse(t) } ->  std::convertible_to<Response>;
};

class Router {
public:
    template <typename Callable> 
        // requires (IntoResponse<std::invoke_result_t<Callable> >) // The callable must return a a type that can be converted to a response
    auto route(std::string_view path, Get<Callable> cb) && -> Router {
        addRouteImpl("GET", path, std::move(cb.fn));
        return std::move(*this);
    }

    template <typename Callable>
    auto route(std::string_view path, Post<Callable> cb) && -> Router {
        addRouteImpl("POST", path, std::move(cb.fn));
        return std::move(*this);
    }
private:
    template <typename Callable>
    auto addRouteImpl(std::string_view method, std::string_view path, Callable callable) -> void {
        mRoutes[std::string(path)] = {method, [fn = std::move(callable)](auto req) -> Task<Response> {
            if constexpr (std::is_invocable_v<Callable, Request>) { // We can invoke the callable with a request
                co_return toResponse(co_await fn(std::move(req)));
            }
            // else if constexpr (std::is_invocable_v<Callable, HeadersMap>) { // The callable doesn't care about the request
            //     co_return toResponse(co_await fn(std::move(req.headers)));
            // }
            else if constexpr (std::is_invocable_v<Callable, Buffer>) { // The callable doesn't care about the request or headers
                co_return toResponse(co_await fn(std::move(req.body)));
            }
            else { // The callable doesn't care it
                co_return toResponse(co_await fn());
            }
        }};
    }

    std::map<
        std::string, 
        std::pair<std::string_view, std::function<Task<Response> (Request request)> >, // METHOD, CALLBACK
        detail::CaseCompare
    > mRoutes;

template <StreamClient T>
friend auto serveStream(T _stream, Router &rt) -> Task<void>;
};


// Helpers generator
inline auto makeGenerator(std::string_view str) -> Generator<Buffer> {
    co_yield makeBuffer(str);
}

inline auto makeGenerator(std::string str) -> Generator<Buffer> {
    co_yield makeBuffer(str);
}

inline auto makeGenerator(const char *str) -> Generator<Buffer> {
    return makeGenerator(std::string_view(str));
}

inline auto makeGenerator(Buffer buffer) -> Generator<Buffer> {
    co_yield buffer;
}

inline auto makeGenerator(std::vector<std::byte> bytes) -> Generator<Buffer> {
    co_yield makeBuffer(bytes);
}

inline auto makeSseGenerator(Generator<SseEvent> gen) -> Generator<Buffer> {
    ilias_foreach(auto e, gen) {
        // Write the event
        if (!e.comment.empty()) {
            auto comment = ": " + std::string(e.comment) + "\n";
            co_yield makeBuffer(comment);
        }
        if (!e.event.empty()) {
            auto event = "event: " + std::string(e.event) + "\n";
            co_yield makeBuffer(event);
        }
        if (!e.data.empty()) {
            auto data = "data: " + std::string(e.data) + "\n";
            co_yield makeBuffer(data);
        }
        if (e.retry) {
            auto retry = "retry: " + std::to_string(*e.retry) + "\n";
            co_yield makeBuffer(retry);
        }
        co_yield makeBuffer("\n"sv); // End of event
    }
}

// Casting...
inline auto toResponse(Response r) -> Response {
    return r;
}

template <typename T>
inline auto toResponse(Text<T> t) -> Response {
    return {
        .code = 200,
        .headers = { {"Content-Type", "text/plain; charset=utf-8"} },
        .content = makeGenerator(std::move(t.text))
    };
}

template <typename T>
inline auto toResponse(Html<T> h) -> Response {
    return {
        .code = 200,
        .headers = { {"Content-Type", "text/html; charset=utf-8"} },
        .content = makeGenerator(std::move(h.html))
    };
}

template <typename T>
inline auto toResponse(Json<T> j) -> Response {
    return {
        .code = 200,
        .headers = { {"Content-Type", "application/json; charset=utf-8"} },
        .content = makeGenerator(std::move(j.json))
    };
}

inline auto toResponse(Sse sse) -> Response {
    return {
        .code = 200,
        .headers = { {"Content-Type", "text/event-stream; charset=utf-8"} },
        .content = makeSseGenerator(std::move(sse.stream))
    };
}


template <typename T>
inline auto get(T fn) -> Get<T> {
    return {std::move(fn)};
}

template <typename T>
inline auto post(T fn) -> Post<T> {
    return {std::move(fn)};
}

template <StreamClient T>
auto serveStream(T _stream, Router &rt) -> Task<void> 
try {
    auto splitQuery = [](std::string_view query) -> std::tuple<std::string_view, std::string_view, std::string_view> {
        auto pos = query.find(' ');
        if (pos == std::string_view::npos) {
            return {};
        }
        auto method = query.substr(0, pos);
        query = query.substr(pos + 1);
        pos = query.find(' ');
        if (pos == std::string_view::npos) {
            return {};
        }
        auto path = query.substr(0, pos);
        query = query.substr(pos + 1);
        return {method, path, query};
    };
    auto status2str = [](int code) -> std::string_view {
        switch (code) {
            case 200: return "OK";
            case 404: return "Not Found";
            case 405: return "Internal Server Error";
            case 500: return "Method Not Allowed";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            default: return "Unknown";
        }
    };
    auto stream = BufferedStream<T> {std::move(_stream)};
    while (true) {
        auto query = co_await stream.getline("\r\n");
        if (!query) {
            co_return;
        }
        // Separate into parts
        auto [method, path, version] = splitQuery(*query); 
        if (method.empty() || path.empty() || version.empty()) {
            co_return;
        }

        // Get headers
        HeadersMap headers;
        while (true) {
            auto line = co_await stream.getline("\r\n");
            if (!line) {
                co_return;
            }
            if (line->empty()) {
                break;
            }
            auto pos = line->find(": ");
            if (pos == std::string_view::npos) {
                co_return;
            }
            auto key = line->substr(0, pos);
            auto value = line->substr(pos + 2);
            headers.emplace(std::string(key), std::string(value));
        }

        // Get the content if there is any
        std::vector<std::byte> content;
        if (headers.contains("Content-Length")) {
            auto len = std::stoi(headers["Content-Length"]);
            content.resize(len);
            if (!co_await stream.readAll(content)) {
                co_return;
            }
        }

        // Route the request
        auto response = co_await [&]() -> Task<Response> {
            auto it = rt.mRoutes.find(path);
            if (it == rt.mRoutes.end()) {
                co_return Response {
                    .code = 404,
                    .headers = {}, // DEFAULT
                    .content = makeGenerator(std::string_view("URL Not Found"))
                };
            }
            auto &[_, item] = *it;
            auto &[accept_method, fn] = item;
            if (accept_method != method) {
                co_return Response {
                    .code = 405,
                    .headers = {}, // DEFAULT
                    .content = makeGenerator(std::string_view("Method Not Allowed"))
                };
            }
            co_return co_await fn(Request {
                .method = method,
                .path = path,
                .headers = std::move(headers),
                .body = content,
            });
        }();

        // Send the response
        // HTTP/1.1 code OK
        char buffer[500] {0};
        auto n = ::snprintf(buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\n", response.code, status2str(response.code).data());
        if (!co_await stream.writeAll(makeBuffer(std::string_view(buffer, n)))) {
            co_return;
        }

        // Then headers
        for (auto &[key, value] : response.headers) {
            n = ::snprintf(buffer, sizeof(buffer), "%s: %s\r\n", key.c_str(), value.c_str());
            if (!co_await stream.writeAll(makeBuffer(std::string_view(buffer, n)))) {
                co_return;
            }
        }

        // We always use chunked transfer encoding, it's easier to implement the server side :)
        if (!co_await stream.writeAll(makeBuffer("Transfer-Encoding: chunked\r\n\r\n"sv))) {
            co_return;
        }

        ilias_foreach(Buffer chunk, response.content) {
            // size\r\n data\r\n
            char sizebuf[32];
            auto [ptr, ec] = std::to_chars(sizebuf, sizebuf + sizeof(sizebuf), chunk.size(), 16);
            if (ec != std::errc()) {
                co_return;
            }
            *(ptr) = '\r';
            *(ptr + 1) = '\n';
            std::string_view size(sizebuf, ptr - sizebuf + 2);

            if (!co_await stream.writeAll(makeBuffer(size))) {
                co_return;
            }

            if (!co_await stream.writeAll(chunk)) {
                co_return;
            }

            if (!co_await stream.writeAll(makeBuffer("\r\n"sv))) {
                co_return;
            }
        }

        // End chunked transfer encoding
        if (!co_await stream.writeAll(makeBuffer("0\r\n\r\n"sv))) {
            co_return;
        }
    }
    co_return;
}
catch (...) {
    co_return;
}

template <typename T>
auto serve(T listener, Router rt) -> IoTask<void> {
    auto scope = co_await TaskScope::make();
    while (true) {
        auto req = co_await listener.accept();
        if (!req) {
            co_return unexpected(req.error());
        }
        auto &[stream, addr] = *req;
        scope.spawn(serveStream(std::move(stream), rt));
    }
}


}