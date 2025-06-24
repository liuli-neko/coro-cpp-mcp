#include <ilias/platform.hpp>
#include <nekoproto/serialization/to_string.hpp>

#include "ccmcp/io/stdio_stream.hpp"
#include "ccmcp/model/model.hpp"
#include "ccmcp/server/server.hpp"

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
// {"method": "tools/call", "params": {"name": "list_emoji_categories", "arguments": {}}, "jsonrpc": "2.0", "id": 5}
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
    McpServer<MCPTools> server(platform);
    server.setInstructions("This is a test server");
    server.setCapabilities(ToolsCapability{});
    server->add.paramsDescription = {{"a", "first number"}, {"b", "second number"}};
    server->add                   = [](TowParams params) { return params.a + params.b; };
    server->mult                  = [](TowParams params) { return params.a * params.b; };
    server->div                   = [](TowParams params) { return params.a / params.b; };
    server->sub                   = [](TowParams params) { return params.a - params.b; };
    server->echo                  = [](EchoParams params) { return params.message; };
    server->hello_unicode         = [](HelloParams params) { return params.greeting + " " + params.name; };
    server.registerToolFunction("list_emoji_categories", std::function([]() {
                                    std::vector<std::string> categories = {
                                        "😀 Smileys & Emotion", "👋 People & Body", "🐶 Animals & Nature",
                                        "🍎 Food & Drink",      "⚽ Activities",    "🌍 Travel & Places",
                                        "💡 Objects",           "❤️ Symbols",        "🚩 Flags"};
                                    return categories;
                                }),
                                "🎨 Tool that returns a list of emoji categories");

    // TODO: add server code here
    ilias_wait server.start<Stdio>("stdio://stdout-stdin");
    ilias_wait server.wait();

    return 0;
}