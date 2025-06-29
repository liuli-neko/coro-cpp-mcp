#include <iostream>

#include <ilias/platform.hpp>
#include <nekoproto/serialization/to_string.hpp>

#include "ccmcp/client/client.hpp"
#include "ccmcp/io/stdio_stream.hpp"
#include "ccmcp/model/model.hpp"

NEKO_USE_NAMESPACE
CCMCP_USE_NAMESPACE

struct TowParams {
    double a;
    double b;
};
struct EchoParams {
    std::string message;
};

struct HelloParams {
    std::string name;
    std::string greeting;
};
// {"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"Cline","version":"3.13.1"}},"jsonrpc":"2.0","id":0}
// {"method":"notifications/initialized","jsonrpc":"2.0"}
// {"method": "tools/list", "jsonrpc": "2.0", "id": 1}
// {"method": "tools/call", "params": {"name": "add", "arguments": {"a": 13, "b": 22}}, "jsonrpc": "2.0", "id": 2}
// {"method": "tools/call", "params": {"name": "mult", "arguments": {"a": 15, "b": 42}}, "jsonrpc": "2.0", "id": 3}
// {"method": "tools/call", "params": {"name": "div", "arguments": {"a": 16, "b": 22}}, "jsonrpc": "2.0", "id": 4}
// {"method": "tools/call", "params": {"name": "div", "arguments": {"a": 1, "b": 0}}, "jsonrpc": "2.0", "id": 5}
struct MCPTools {
    ToolFunction<double(TowParams), "add"> add{"sum of two numbers"};
    ToolFunction<double(TowParams), "mult"> mult{"product of two numbers"};
    ToolFunction<double(TowParams), "div"> div{"quotient of two numbers"};
    ToolFunction<double(TowParams), "sub"> sub{"difference of two numbers"};
    ToolFunction<std::string(EchoParams), "echo"> echo{"echo string"};
    ToolFunction<std::string(HelloParams), "hello_unicode"> hello_unicode{
        "🌟 A tool that uses various Unicode characters in its description: "
        "á é í ó ú ñ 漢字 🎉"};
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
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
    McpClient<MCPTools> client(platform);

    // TODO: add server code here
    StdioStream stdio;
    ilias_wait stdio.start();
    client.setTransport(std::move(stdio));
    auto ret = ilias_wait client->add({.a = 5, .b = 2});
    std::cout << "add: " << ret.value_or(-1) << std::endl;

    return 0;
}