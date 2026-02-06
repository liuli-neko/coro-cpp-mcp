set_project("dev-assistant-mcp")
set_version("1.0.0")
set_languages("c++20")

add_requires("neko-proto-tools", "ilias", "rapidjson")

target("dev-assistant-server")
    set_kind("binary")
    add_files("main.cpp", "src/*.cpp", "src/tools/*.cpp")
    add_includedirs("include", "../../include", {public = true})
    add_packages("neko-proto-tools", "ilias", "rapidjson")
    -- add_defines("USE_STDIO_STREAM")
    if is_plat("windows") then
        add_cxxflags("/utf-8")
    else
        add_cxxflags("-fPIC", "-pthread")
    end
