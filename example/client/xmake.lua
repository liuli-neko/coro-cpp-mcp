target("client-example")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_deps("coro-cpp-mcp")
    add_files("main.cpp")