#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>

// Forward declarations
namespace NEKO_NS {
    // NEKO_USE_NAMESPACE will be defined in ilias/platform.hpp
}

// Helper function to execute shell commands
std::string executeCommand(const std::string& cmd, const std::string& workingDir = "");

// Helper function to load JSON file
std::string loadJsonFile(const std::string& path);

// Helper function to find VSCode path
std::string findVSCodePath(const std::string& workspacePath);

// Load project configuration
bool loadProjectConfig(const std::string& configPath, std::string& errorMsg);

// Project configuration structure
struct ProjectConfig {
    std::string project_name;
    std::string workspace_path;
    
    // Prompts for AI assistance - helps save tokens by providing pre-defined context
    struct PromptsConfig {
        struct Category {
            std::map<std::string, std::string> entries;
        };
        std::map<std::string, Category> categories;
    };
    
    // Dictionary for quick lookup of abbreviations and commands
    struct DictionaryConfig {
        struct Category {
            std::map<std::string, std::string> entries;
        };
        std::map<std::string, Category> categories;
    };
    
    // Workflow definitions for complex multi-step operations
    struct WorkflowConfig {
        std::string name;
        std::string description;
        struct WorkflowStep {
            std::string tool_name;
            std::string args;  // JSON string
        };
        std::vector<WorkflowStep> steps;
    };
    
    // Docker configuration
    struct DockerImageConfig {
        std::string name;
        std::string dockerfile_path;
        std::string build_context;
        std::map<std::string, std::string> build_args;
        std::vector<std::string> tags;
    };
    
    struct DockerContainerConfig {
        std::string name;
        std::string image;
        std::vector<std::string> volumes;
        std::vector<std::string> ports;
        std::map<std::string, std::string> environment;
        bool detach = true;
    };
    
    struct SyncTargetConfig {
        std::string name;
        std::string source;
        std::string destination;
        std::string host;
        std::string user;
        std::vector<std::string> exclude;
    };
    
    struct GdbConfig {
        std::string name;
        std::string host;
        int port;
        std::string executable;
        std::vector<std::string> arguments;
        std::string working_directory;
        std::vector<std::string> source_paths;
    };
    
    std::vector<DockerImageConfig> docker_images;
    std::vector<DockerContainerConfig> docker_containers;
    std::vector<SyncTargetConfig> sync_targets;
    std::vector<GdbConfig> gdb_configs;
    PromptsConfig prompts;
    DictionaryConfig dictionary;
    std::vector<WorkflowConfig> workflows;
};

// Global project configuration instance
extern ProjectConfig g_projectConfig;
