#include <filesystem>
#include <fstream>
#include <sstream>

#include "ilias/platform.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Tool handler: Load config
std::string load_config(LoadConfigParams params) {
    std::string errorMsg;
    std::string configPath = params.config_path.has_value() ? params.config_path.value() : "";
    if (loadProjectConfig(configPath, errorMsg)) {
        std::string result = "Configuration loaded successfully.\nProject: ";
        result += g_projectConfig.project_name + std::string("\nWorkspace: ") + g_projectConfig.workspace_path;
        return result;
    } else {
        return errorMsg;
    }
}

// Tool handler: Get project info
std::string get_project_info(GetProjectInfoParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Getting project info for: {}", workspacePath);
        
        std::string result = std::string("=== Project Information ===\n\n");
        result += std::string("Workspace Path: ") + workspacePath + std::string("\n\n");
        
        // Check for build systems
        result += "Build Systems:\n";
        if (std::filesystem::exists(workspacePath + std::string("/CMakeLists.txt"))) {
            result += "  ✓ CMake detected\n";
        }
        if (std::filesystem::exists(workspacePath + std::string("/Makefile"))) {
            result += "  ✓ Make detected\n";
        }
        if (std::filesystem::exists(workspacePath + std::string("/xmake.lua"))) {
            result += "  ✓ XMake detected\n";
        }
        if (std::filesystem::exists(workspacePath + std::string("/package.json"))) {
            result += "  ✓ NPM/Node detected\n";
        }
        
        // Check for Docker
        result += "\nDocker:\n";
        if (std::filesystem::exists(workspacePath + std::string("/Dockerfile"))) {
            result += "  ✓ Dockerfile detected\n";
        }
        if (std::filesystem::exists(workspacePath + std::string("/docker-compose.yml")) ||
            std::filesystem::exists(workspacePath + std::string("/docker-compose.yaml"))) {
            result += "  ✓ Docker Compose detected\n";
        }
        
        // Check for VSCode
        result += "\nVSCode Configuration:\n";
        std::string vscodePath = workspacePath + std::string("/.vscode");
        if (std::filesystem::exists(vscodePath)) {
            result += "  ✓ .vscode directory found\n";
            
            if (std::filesystem::exists(vscodePath + std::string("/tasks.json"))) {
                result += "  ✓ tasks.json found\n";
            }
            if (std::filesystem::exists(vscodePath + std::string("/launch.json"))) {
                result += "  ✓ launch.json found\n";
            }
            if (std::filesystem::exists(vscodePath + std::string("/settings.json"))) {
                result += "  ✓ settings.json found\n";
            }
        } else {
            result += "  ✗ .vscode directory not found\n";
        }
        
        // Check for dev assistant config
        result += "\nDev Assistant Config:\n";
        std::vector<std::string> configFiles;
        configFiles.push_back("dev_assistant_config.json");
        configFiles.push_back(".dev_assistant.json");
        configFiles.push_back("dev-assistant-config.json");
        
        bool configFound = false;
        for (const auto& file : configFiles) {
            if (std::filesystem::exists(workspacePath + std::string("/") + file)) {
                result += std::string("  ✓ ") + file + std::string(" found\n");
                configFound = true;
            }
        }
        
        if (!configFound) {
            result += "  ✗ No config file found (create dev_assistant_config.json)\n";
        }
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in get_project_info: {}", e.what());
        return std::string("Error getting project info: ") + std::string(e.what());
    }
}
