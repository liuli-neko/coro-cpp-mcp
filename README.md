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
xmake build mcp-server
```

### run
运行example/server 下面的示例，
```shell
xmake run mcp-server
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