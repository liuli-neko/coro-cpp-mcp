#pragma once

#include <string>
#include <optional>

// Tool parameter structures
struct LoadConfigParams {
    std::optional<std::string> config_path;
};

struct GetProjectInfoParams {
    std::optional<std::string> workspace_path;
};

struct ListVSCodeTasksParams {
    std::optional<std::string> workspace_path;
};

struct ExecuteVSCodeTaskParams {
    std::string task_name;
    std::optional<std::string> workspace_path;
};

struct ListVSCodeLaunchesParams {
    std::optional<std::string> workspace_path;
};

struct StartVSCodeDebugParams {
    std::string launch_name;
    std::optional<std::string> workspace_path;
};

struct ReadFileParams {
    std::string file_path;
};

struct DockerBuildParams {
    std::string config_name;
    std::optional<std::string> workspace_path;
};

struct DockerRunParams {
    std::string container_name;
    std::optional<std::string> workspace_path;
};

struct SyncFilesParams {
    std::string target_name;
    std::optional<std::string> workspace_path;
};

struct StartGdbParams {
    std::string gdb_config_name;
    std::optional<std::string> workspace_path;
};

struct QueryPromptsParams {
    std::optional<std::string> category;
    std::optional<std::string> key;
};

struct QueryDictionaryParams {
    std::optional<std::string> category;
    std::optional<std::string> key;
};

struct ListWorkflowsParams {
    bool include_descriptions = true;
};

struct ExecuteWorkflowParams {
    std::string workflow_name;
};
