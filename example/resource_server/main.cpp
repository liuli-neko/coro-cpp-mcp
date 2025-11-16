#include <iostream>

#include <ilias/platform.hpp>
#include <nekoproto/serialization/to_string.hpp>

#include "ccmcp/io/stdio_stream.hpp"
#include "ccmcp/server/server.hpp"

NEKO_USE_NAMESPACE
CCMCP_USE_NAMESPACE

int ilias_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
#if defined(_WIN32)
    ::SetConsoleCP(65001);
    ::SetConsoleOutputCP(65001);
    std::setlocale(LC_ALL, ".utf-8");

#if 0
    // For debugging...
    while (!::IsDebuggerPresent()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif

#endif

    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);
    ILIAS_NAMESPACE::PlatformContext platform;
    McpServer<void> server(platform);
    server.setInstructions("This is a test resource server");
    server.setCapabilities(ResourcesCapability{.subscribe = {}, .list_changed = {}});
    // add a local file resource
    auto logFile = ("E:\\workplace\\coro-cpp-mcp\\build\\bin\\log.txt");
    server.registerLocalFileResource("log", logFile);
    // add a custom static resource
    server.registerResource({.uri         = "my_uri",
                             .name        = "my_name",
                             .description = "my github name",
                             .metadata    = std::nullopt,
                             .annotations = std::nullopt},
                            TextResourceContents{.uri = "my_uri", .text = "liuli-neko"});
    // add a custom dynamic resource
    server.registerResource({.uri         = "my_uri2",
                             .name        = "my_name2",
                             .description = "my github name2",
                             .metadata    = std::nullopt,
                             .annotations = std::nullopt},
                            []([[maybe_unused]] std::optional<ccmcp::Meta> meta) {
                                // you can use the meta to determine what to return
                                return TextResourceContents{.uri = "my_uri2", .text = "https://github.com/liuli-neko"};
                            });

    // TODO: add server code here
    StdioStream stdio;
    co_await stdio.start();
    server.addTransport(std::move(stdio));
    co_await server.wait();

    co_return 0;
}