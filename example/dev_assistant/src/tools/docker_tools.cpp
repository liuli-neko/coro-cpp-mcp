#include <filesystem>
#include <expected>

#include <ilias/platform.hpp>
#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Tool handler: Docker build
std::string docker_build(DockerBuildParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Docker build requested: {}", params.config_name);
        
        // Find the docker image configuration
        const ProjectConfig::DockerImageConfig* imageConfig = nullptr;
        for (const auto& img : g_projectConfig.docker_images) {
            if (img.name == params.config_name) {
                imageConfig = &img;
                break;
            }
        }
        
        if (!imageConfig) {
            std::string result = std::string("Docker image configuration not found: ") + params.config_name + std::string("\n\n");
            result += "Available docker images:\n";
            for (const auto& img : g_projectConfig.docker_images) {
                result += std::string("  - ") + img.name + std::string("\n");
            }
            return result;
        }
        
        // Build docker build command
        std::string cmd = "docker build";
        
        // Add dockerfile path
        if (!imageConfig->dockerfile_path.empty()) {
            cmd += std::string(" -f ") + imageConfig->dockerfile_path;
        }
        
        // Add build context
        std::string buildContext = imageConfig->build_context.empty() ? "." : imageConfig->build_context;
        cmd += std::string(" -t ") + params.config_name + std::string(" ") + buildContext;
        
        // Add build args
        for (const auto& [key, value] : imageConfig->build_args) {
            cmd += std::string(" --build-arg ") + key + std::string("=") + value;
        }
        
        // Add tags
        for (const auto& tag : imageConfig->tags) {
            cmd += std::string(" -t ") + tag;
        }
        
        NEKO_LOG_INFO("dev-assistant", "Executing docker build: {}", cmd);
        
        // Execute command
        std::string output = executeCommand(cmd, workspacePath);
        
        std::string result = std::string("=== Docker Build ===\n\n");
        result += std::string("Image: ") + params.config_name + std::string("\n");
        result += std::string("Dockerfile: ") + imageConfig->dockerfile_path + std::string("\n");
        result += std::string("Context: ") + buildContext + std::string("\n\n");
        result += std::string("Command: ") + cmd + std::string("\n\n");
        result += std::string("Output:\n") + output;
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in docker_build: {}", e.what());
        return std::string("Error building Docker image: ") + std::string(e.what());
    }
}

// Tool handler: Docker run
std::string docker_run(DockerRunParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "Docker run requested: {}", params.container_name);
        
        // Find the docker container configuration
        const ProjectConfig::DockerContainerConfig* containerConfig = nullptr;
        for (const auto& cont : g_projectConfig.docker_containers) {
            if (cont.name == params.container_name) {
                containerConfig = &cont;
                break;
            }
        }
        
        if (!containerConfig) {
            std::string result = std::string("Docker container configuration not found: ") + params.container_name + std::string("\n\n");
            result += "Available containers:\n";
            for (const auto& cont : g_projectConfig.docker_containers) {
                result += std::string("  - ") + cont.name + std::string(" (image: ") + cont.image + std::string(")\n");
            }
            return result;
        }
        
        // Build docker run command
        std::string cmd = "docker run";
        
        // Add detach option
        if (containerConfig->detach) {
            cmd += " -d";
        }
        
        // Add name
        cmd += std::string(" --name ") + containerConfig->name;
        
        // Add volumes
        for (const auto& vol : containerConfig->volumes) {
            cmd += std::string(" -v ") + vol;
        }
        
        // Add ports
        for (const auto& port : containerConfig->ports) {
            cmd += std::string(" -p ") + port;
        }
        
        // Add environment variables
        for (const auto& [key, value] : containerConfig->environment) {
            cmd += std::string(" -e ") + key + std::string("=") + value;
        }
        
        // Add image
        cmd += std::string(" ") + containerConfig->image;
        
        NEKO_LOG_INFO("dev-assistant", "Executing docker run: {}", cmd);
        
        // Execute command
        std::string output = executeCommand(cmd, workspacePath);
        
        std::string result = std::string("=== Docker Run ===\n\n");
        result += std::string("Container: ") + params.container_name + std::string("\n");
        result += std::string("Image: ") + containerConfig->image + std::string("\n\n");
        result += std::string("Command: ") + cmd + std::string("\n\n");
        result += std::string("Output:\n") + output;
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in docker_run: {}", e.what());
        return std::string("Error running Docker container: ") + std::string(e.what());
    }
}
