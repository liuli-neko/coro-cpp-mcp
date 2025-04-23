target("mcp-server")
    set_kind("binary")
    add_deps("coro-cpp-mcp")
    add_files("main.cpp")