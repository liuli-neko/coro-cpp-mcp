#pragma once

#include "ccmcp/global/global.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "ccmcp/model/capabilities.hpp"

CCMCP_BN

namespace detail {

NEKO_USE_NAMESPACE

struct McpJsonRpcMethods {
    RpcMethod<InitializeResult(InitializeRequestParam), "initialize"> initialize;
    RpcMethod<void(), "notifications/initialized"> initialized;
    RpcMethod<void(), "tools/list"> toolsList;
};
} // namespace detail

CCMCP_EN