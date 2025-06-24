#include <ilias/platform.hpp>
#include "minihttp.hpp"

using namespace minihttp;

void ilias_main() 
try {
    auto router = Router()
        .route("/", get([]() -> Task<Html<const char *> > {
            co_return Html { "<html>Hello, world!</html>" };
        }))
        .route("/json", get([]() -> Task<Json<const char*> > {
            co_return Json { R"({"message": "Hello, world!"})" };
        }))
        .route("/post", post([]() -> Task<Json<const char*> > {
            co_return Json { R"({"message": "Hello, world!"})" };
        }))
        .route("/sse", get([]() -> Task<Sse> {
            co_return Sse { []() -> Generator<SseEvent > {
                while (true) {
                    SseEvent event {
                        .data = "Hello, world!",
                    };
                    co_yield event;
                    if (!co_await sleep(1s)) {
                        co_return;
                    }
                }
            }()};
        }));
    ;
    auto endpoint = IPEndpoint("0.0.0.0:25565");
    auto listener = (co_await TcpListener::make(endpoint.family())).value();
    listener.bind(endpoint).value();

    (co_await serve(std::move(listener), router)).value();
}
catch (...) {
    co_return;
}