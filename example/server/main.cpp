#include <iostream>

#include "ccmcp/model/model.hpp"
#include "ccmcp/server/server.hpp"

NEKO_USE_NAMESPACE
CCMCP_USE_NAMESPACE

struct AddFuncParams {
    int a;
    int b;

    NEKO_SERIALIZER(a, b)
};

struct MCPTools {
    ToolFunction<int(AddFuncParams), "add"> add{"sum of two numbers", {"a number", "a number"}};
};

int main(int argc, char* argv[]) {

    McpServer<MCPTools> server;

    // TODO: add server code here

    return 0;
}