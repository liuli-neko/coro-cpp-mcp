#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "ccmcp/model/jsonrpc_protocol.hpp"

CCMCP_BN

template <typename ToolFunctions>
class McpServer {
    template <typename T>
    using JsonRpcServer = NEKO_NAMESPACE::JsonRpcServer<T>;

public:
    McpServer();

private:
    JsonRpcServer<detail::McpJsonRpcMethods> mServer;
};

CCMCP_EN