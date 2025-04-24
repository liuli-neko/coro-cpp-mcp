#include <iostream>

#include "ccmcp/model/model.hpp"
#include "ccmcp/server/server.hpp"

NEKO_USE_NAMESPACE
CCMCP_USE_NAMESPACE

struct MCPTools {
    ToolFunction<int(int, int), "add", "a", "b"> add{{"a number", "a number"}};
};

int main(int argc, char* argv[]) {
    
    McpServer<MCPTools> server;

    // TODO: add server code here

    return 0;
}