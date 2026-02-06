#define NEKO_VERBOSE_LOGS
#include <ilias/platform.hpp>
#include <nekoproto/serialization/to_string.hpp>

#include "ccmcp/io/sse_stream.hpp"
#include "ccmcp/io/stdio_stream.hpp"
#include "ccmcp/model/model.hpp"
#include "ccmcp/server/server.hpp"

#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE
CCMCP_USE_NAMESPACE

// Tool handler forward declarations
std::string load_config(LoadConfigParams params);
std::string get_project_info(GetProjectInfoParams params);
std::string list_vscode_tasks(ListVSCodeTasksParams params);
std::string execute_vscode_task(ExecuteVSCodeTaskParams params);
std::string list_vscode_launches(ListVSCodeLaunchesParams params);
std::string start_vscode_debug(StartVSCodeDebugParams params);
std::string docker_build(DockerBuildParams params);
std::string docker_run(DockerRunParams params);
std::string sync_files(SyncFilesParams params);
std::string start_gdb(StartGdbParams params);
std::string read_file(ReadFileParams params);
std::string query_prompts(QueryPromptsParams params);
std::string query_dictionary(QueryDictionaryParams params);
std::string list_workflows(ListWorkflowsParams params);
std::string execute_workflow(ExecuteWorkflowParams params);

// Dev Assistant Tools
struct DevAssistantTools {
    ToolFunctionS<::load_config> load_config{
        "Load project configuration. If config_path is empty, auto-detect from current directory"};
    ToolFunctionS<::get_project_info> get_project_info{
        "Get current project information including VSCode configs and detected build systems"};
    ToolFunctionS<::list_vscode_tasks> list_vscode_tasks{"List all VSCode tasks defined in .vscode/tasks.json"};
    ToolFunctionS<::execute_vscode_task> execute_vscode_task{"Execute a VSCode task by name using VSCode CLI"};
    ToolFunctionS<::list_vscode_launches> list_vscode_launches{
        "List all VSCode debug configurations in .vscode/launch.json"};
    ToolFunctionS<::start_vscode_debug> start_vscode_debug{"Start VSCode debugging session using VSCode CLI"};
    ToolFunctionS<::docker_build> docker_build{"Build Docker image using configuration from project config"};
    ToolFunctionS<::docker_run> docker_run{"Run Docker container using configuration from project config"};
    ToolFunctionS<::sync_files> sync_files{
        "Sync files to remote target using rsync/scp with configuration from project config"};
    ToolFunctionS<::start_gdb> start_gdb{"Prepare remote GDB debugging using configuration from project config"};
    ToolFunctionS<::read_file> read_file{"Read file contents from filesystem"};
    ToolFunctionS<::query_prompts> query_prompts{
        "Query AI assistance prompts from config. Use to get pre-defined context and save tokens"};
    ToolFunctionS<::query_dictionary> query_dictionary{"Query dictionary for abbreviations and command definitions"};
    ToolFunctionS<::list_workflows> list_workflows{"List all available workflows for complex multi-step operations"};
    ToolFunctionS<::execute_workflow> execute_workflow{"Execute a predefined workflow (sequence of tool calls)"};
};

int ilias_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
#if defined(_WIN32)
    ::SetConsoleCP(65001);
    ::SetConsoleOutputCP(65001);
    std::setlocale(LC_ALL, ".utf-8");
#endif

    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_EXCLUDE("JsonSerializer");
    ILIAS_NAMESPACE::PlatformContext platform;
    McpServer<DevAssistantTools> server(platform);

    server.setInstructions("Development Assistant MCP Server\n\n"
                           "This server assists with C++ development workflow by:\n"
                           "- Automatically detecting project configuration and structure\n"
                           "- Reading and executing VSCode tasks from .vscode/tasks.json\n"
                           "- Reading and starting VSCode debug sessions from .vscode/launch.json\n"
                           "- Docker container management\n"
                           "- File synchronization to remote VMs\n"
                           "- Remote GDB debugging configuration\n"
                           "- Querying AI assistance prompts (saves tokens!)\n"
                           "- Querying dictionary for abbreviations and commands\n"
                           "- Executing predefined workflows\n\n"
                           "Key Features:\n"
                           "1. Auto-detects project directory (uses current working directory)\n"
                           "2. Reads VSCode configuration files directly\n"
                           "3. Executes VSCode tasks using 'code --run-task'\n"
                           "4. Starts VSCode debugging using 'code --launch'\n"
                           "5. All project-specific configurations in dev_assistant_config.json\n"
                           "6. Use query_prompts to get pre-defined context and save tokens\n"
                           "7. Use query_dictionary for quick lookups of abbreviations\n"
                           "8. Use list_workflows and execute_workflow for complex operations\n\n"
                           "Start with 'get_project_info' to see what's available in your project,\n"
                           "then use list_vscode_tasks, execute_vscode_task, query_prompts, etc.");

    server.setCapabilities(ToolsCapability{});

    // Set up parameter descriptions
    server->load_config.paramsDescription = {
        {"config_path", "Optional: Path to dev_assistant_config.json. If empty, auto-detects from current directory"}};
    server->get_project_info.paramsDescription = {
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->list_vscode_tasks.paramsDescription = {
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->execute_vscode_task.paramsDescription = {
        {"task_name", "Name of the VSCode task to execute (from tasks.json)"},
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->list_vscode_launches.paramsDescription = {
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->start_vscode_debug.paramsDescription = {
        {"launch_name", "Name of the launch configuration (from launch.json)"},
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->docker_build.paramsDescription = {
        {"config_name", "Name of Docker image configuration from dev_assistant_config.json"},
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->docker_run.paramsDescription = {
        {"container_name", "Name of Docker container configuration from dev_assistant_config.json"},
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->sync_files.paramsDescription = {
        {"target_name", "Name of sync target from dev_assistant_config.json"},
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->start_gdb.paramsDescription = {
        {"gdb_config_name", "Name of GDB configuration from dev_assistant_config.json"},
        {"workspace_path", "Optional: Workspace path. If empty, uses current working directory"}};
    server->read_file.paramsDescription     = {{"file_path", "Path to file to read"}};
    server->query_prompts.paramsDescription = {{"category", "Optional: Category name (global, environment, workflow)"},
                                               {"key", "Optional: Specific key within category"}};
    server->query_dictionary.paramsDescription = {{"category", "Optional: Category name (abbreviations, commands)"},
                                                  {"key", "Optional: Specific key within category"}};
    server->list_workflows.paramsDescription   = {
        {"include_descriptions", "Whether to include workflow descriptions (default: true)"}};
    server->execute_workflow.paramsDescription = {{"workflow_name", "Name of the workflow to execute"}};

    // Start server on stdio
#ifdef USE_STDIO_STREAM
    StdioStream stdio;
    co_await stdio.start();
    server.addTransport(std::move(stdio));
#else
    auto listener = co_await ILIAS_NAMESPACE::TcpListener::bind("127.0.0.1:8848");
    if (!listener) {
        co_return -1;
    }
    SseListener sse(std::move(*listener));
    server.setListener<SseListener>(std::move(sse));
#endif

    co_await server.wait();

    co_return 0;
}
