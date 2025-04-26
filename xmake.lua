-- 工程设置
set_project("coro-cpp-mcp")
set_version("0.0.1", {build = "$(buildversion)"})

set_configvar("LEGAL_COPYRIGHT", "Copyright (C) 2024 Btk-Project")
set_configvar("PROJECT_NAME", "coro-cpp-mcp")
set_version("0.2.1", {build = "%Y%m%d%H%M"})
set_configvar("LATEST_PROTOCOL_VERSION", "2024-11-05")

-- 全局设置
add_configfiles("include/ccmcp/global/config.h.in")
set_configdir("include/ccmcp/global")
set_warnings("allextra")
set_languages("cxx20", "c17")
set_exceptions("cxx")
set_encodings("utf-8")

-- 添加编译选项
add_rules("mode.release", "mode.debug", "mode.releasedbg", "mode.minsizerel")
add_rules("plugin.compile_commands.autoupdate", {lsp = "clangd", outputdir = ".vscode"})
set_policy("package.cmake_generator.ninja", true)

if is_plat("linux") then
    add_cxxflags("-fcoroutines")
    add_links("rt")
end

-- 编译设置
option("3rd_kind",     {showmenu = true, default = "shared", values = {"static", "shared"}})
option("buildversion", {showmenu = true, default = "0", type = "string"})
option("outputdir",    {showmenu = true, default = "bin", type = "string"})

-- 隐藏设置、隐藏目标、打包命令
includes("lua/hideoptions.lua")
includes("lua/hidetargets.lua")
includes("lua/pack.lua")

-- Some of the third-libraries use our own configurations
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")

-- headonly
add_requires("ilias")
add_requires("fmt", {version = "11.0.x", configs = {shared = is_config("3rd_kind", "shared"), header_only = false}})
-- normal libraries
add_requires("neko-proto-tools", {version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = true, enable_fmt = true, enable_communication = false}})


-- normal libraries' dependencies configurations
add_requireconfs("**.fmt", {override = true, version = "11.0.x", configs = {shared = is_config("3rd_kind", "shared"), header_only = false}})
add_requireconfs("**.neko-proto-tools", {override = true, version = "dev", configs = {shared = is_config("3rd_kind", "shared"), enable_rapidxml = false, enable_simdjson = false, enable_protocol = false, enable_rapidjson = true, enable_fmt = true, enable_communication = false}})
add_requireconfs("**.ilias", {override = true})
add_requireconfs("**.rapidjson", {override = true, configs = {header_only = true}})

if is_mode("debug") then
    add_defines("ILIAS_ENABLE_LOG", "ILIAS_USE_FMT", "ILIAS_TASK_TRACE")
end

option("custom_namespace")
    set_default("ccmcp")
    set_showmenu(true)
    set_description("Use custom namespace")
    set_category("advanced")
option_end()


target("coro-cpp-mcp")
    set_kind("headeronly")

    add_options("custom_namespace")

    add_headerfiles("include/(ccmcp/**.hpp)")
    set_configvar("CCMCP_NAMESPACE", "$(custom_namespace)")
    add_packages("ilias", "neko-proto-tools", "fmt", {public = true})
    add_includedirs("include", {public = true})
target_end()

includes("example/*/xmake.lua")
includes("test/xmake.lua")
