# coro-cpp-mcp
Lightweight C++ Coroutines MCP (Model Context  Protocol)

## Overview
该项目为轻量级C++协程库，基于C++20协程实现。提供了基础的mcp-server的实现框架。
项目使用xmake构建，所有依赖均可全头。

## Usage
[xmake](https://github.com/xmake-io/xmake)
### install deps
```shell
xmake f -m debug -cvy
```

### build
build example/server 下面的示例
```shell
xmake build server-example
```

### run
运行example/server 下面的示例，
```shell
xmake run server-example
```

该示例基于stdio。可以手动输入json进行交互
```json
{"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"Cline","version":"3.13.1"}},"jsonrpc":"2.0","id":0}
{"method":"notifications/initialized","jsonrpc":"2.0"}
{"method": "tools/list", "jsonrpc": "2.0", "id": 1}
{"method": "tools/call", "params": {"name": "add", "arguments": {"a": 13, "b": 22}}, "jsonrpc": "2.0", "id": 2}
{"method": "tools/call", "params": {"name": "mult", "arguments": {"a": 15, "b": 42}}, "jsonrpc": "2.0", "id": 3}
{"method": "tools/call", "params": {"name": "div", "arguments": {"a": 16, "b": 22}}, "jsonrpc": "2.0", "id": 4}
{"method": "tools/call", "params": {"name": "div", "arguments": {"a": 1, "b": 0}}, "jsonrpc": "2.0", "id": 5}
```
也可以配置到cline的本地服务中，配置完成后即可让ai使用。

## Example
### server
以下是服务端简易的使用说明，完整的示例请参考`example/server`。
```c++
// 定义
struct TowParams { // 定义参数结构体
    double a;
    double b;
};
struct MCPTools {
    ToolFunction<double(TowParams), "add"> add{"sum of two numbers"}; // 定义add函数。
};
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    ILIAS_NAMESPACE::PlatformContext platform;
    McpServer<MCPTools> server(platform); // 创建服务,并自动注册MCPTools中的函数
    server.set_instructions("This is a test server");   // 设置说明(可选)
    server.setCapabilities(ResourcesCapability{.subscribe = {}, .list_changed = {}}); // 设置能力(可选)
    server->add.paramsDescription({{"a", "first number"}, {"b", "second number"}}); // 设置参数说明(可选)
    server.register_tool_function("mult", std::function([](Params p) { return p.a * p.b;})); // 通过函数注册函数

    auto logFile = (std::filesystem::current_path() / "log.txt").string();
    server.register_local_file_resource("log", logFile); // 注册本地文件资源

    // 注册静态资源
    server.register_resource({.uri         = "my_uri",
                              .name        = "my_name",
                              .description = "my github name",
                              .metadata    = std::nullopt,
                              .annotations = std::nullopt},
                             TextResourceContents{.text = "liuli-neko"});

    // 注册动态资源
    server.register_resource({.uri         = "my_uri2",
                              .name        = "my_name2",
                              .description = "my github name2",
                              .metadata    = std::nullopt,
                              .annotations = std::nullopt},
                             [](std::optional<ccmcp::Meta> meta) -> TextResourceContents {
                                 // you can use the meta to determine what to return
                                 return TextResourceContents{.text = "https://github.com/liuli-neko"};
                             });

    // 自定以jsonrpc回复
    server.json_rpc_server()->resourcesTemplatesList = [](auto) -> ListResourceTemplatesResult { 
        // 您可以在这里返回你自己的资源模板列表
        return {}; 
    };

    ilias_wait server.start<Stdio>("stdio://stdout-stdin"); // 启动服务
    ilias_wait server.wait(); // 等待服务结束
}
```

### client
以下是一个简单的客户端示例，完整的示例请参考`example/client/`。
```cpp
struct TowParams { // 定义参数结构体
    double a;
    double b;
};
struct MCPTools {
    ToolFunction<double(TowParams), "add"> add{"sum of two numbers"}; // 定义一个方法，参数为TowParams，返回值为double
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    ILIAS_NAMESPACE::PlatformContext platform;
    McpClient<MCPTools> client(platform); // 创建客户端并自动注册MCPTools里面定义的方法

    ilias_wait client.connect<Stdio>("stdio://stdout-stdin"); // 连接服务
    auto ret = ilias_wait client->add({.a = 5, .b = 2}); // 调用服务端的方法
    std::cout << "add: " << ret.value_or(-1) << std::endl; // 输出结果

    return 0;
}
```