#include <filesystem>

#include "ilias/platform.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Tool handler: List VSCode tasks
std::string list_vscode_tasks(ListVSCodeTasksParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Listing VSCode tasks for: {}", workspacePath);
        
        std::string tasksPath = workspacePath + std::string("/.vscode/tasks.json");
        
        if (!std::filesystem::exists(tasksPath)) {
            return std::string("No VSCode tasks file found at: ") + tasksPath;
        }
        
        std::string jsonContent = loadJsonFile(tasksPath);
        if (jsonContent.empty()) {
            return "Failed to read tasks.json";
        }
        
        std::string result = std::string("=== VSCode Tasks ===\n\n");
        result += std::string("Location: ") + tasksPath + std::string("\n\n");
        result += "Content:\n" + jsonContent + std::string("\n\n");
        result += "To execute a task, use: execute_vscode_task with task name";
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in list_vscode_tasks: {}", e.what());
        return std::string("Error listing VSCode tasks: ") + std::string(e.what());
    }
}

// Tool handler: Execute VSCode task
std::string execute_vscode_task(ExecuteVSCodeTaskParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Executing VSCode task: {} in {}", params.task_name, workspacePath);
        
        // Use VSCode CLI to run task
        std::string cmd = std::string("code --run-task \"") + params.task_name + std::string("\"");
        
        if (workspacePath != std::filesystem::current_path().string()) {
            cmd += std::string(" \"") + workspacePath + std::string("\"");
        }
        
        std::string output = executeCommand(cmd);
        
        if (output.empty() || output.find(std::string("not found")) != std::string::npos) {
            return std::string("Note: VSCode CLI may not be available or task not found.\n") +
                   std::string("Command executed: ") + cmd + std::string("\n") +
                   std::string("Make sure 'code' command is in PATH and VSCode is running.\n") +
                   std::string("Alternative: The task command is available in .vscode/tasks.json");
        }
        
        return std::string("Task execution initiated: ") + params.task_name + std::string("\n") + output;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in execute_vscode_task: {}", e.what());
        return std::string("Error executing VSCode task: ") + std::string(e.what());
    }
}

// Tool handler: List VSCode launches
std::string list_vscode_launches(ListVSCodeLaunchesParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Listing VSCode launches for: {}", workspacePath);
        
        std::string launchPath = workspacePath + std::string("/.vscode/launch.json");
        
        if (!std::filesystem::exists(launchPath)) {
            return std::string("No VSCode launch file found at: ") + launchPath;
        }
        
        std::string jsonContent = loadJsonFile(launchPath);
        if (jsonContent.empty()) {
            return "Failed to read launch.json";
        }
        
        std::string result = std::string("=== VSCode Debug Configurations ===\n\n");
        result += std::string("Location: ") + launchPath + std::string("\n\n");
        result += "Content:\n" + jsonContent + std::string("\n\n");
        result += "To start debugging, use: start_vscode_debug with launch configuration name";
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in list_vscode_launches: {}", e.what());
        return std::string("Error listing VSCode launches: ") + std::string(e.what());
    }
}

// Tool handler: Start VSCode debug
std::string start_vscode_debug(StartVSCodeDebugParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Starting VSCode debug session: {} in {}", params.launch_name, workspacePath);
        
        // Use VSCode CLI to start debugging
        std::string cmd = std::string("code --launch \"") + params.launch_name + std::string("\"");
        
        if (workspacePath != std::filesystem::current_path().string()) {
            cmd += std::string(" \"") + workspacePath + std::string("\"");
        }
        
        std::string output = executeCommand(cmd);
        
        if (output.empty() || output.find(std::string("not found")) != std::string::npos) {
            return std::string("Note: VSCode CLI may not be available or launch config not found.\n") +
                   std::string("Command executed: ") + cmd + std::string("\n") +
                   std::string("Make sure 'code' command is in PATH and VSCode is running.\n") +
                   std::string("Alternative: The launch configuration is in .vscode/launch.json");
        }
        
        return std::string("Debug session started: ") + params.launch_name + std::string("\n") + output;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in start_vscode_debug: {}", e.what());
        return std::string("Error starting VSCode debug: ") + std::string(e.what());
    }
}
