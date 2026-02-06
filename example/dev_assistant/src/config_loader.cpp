#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <optional>
#include <cstdio>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include "ilias/platform.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Global project configuration instance
ProjectConfig g_projectConfig;

// Helper function to execute shell commands
std::string executeCommand(const std::string& cmd, const std::string& workingDir) {
    std::string fullCmd = workingDir.empty() ? cmd : std::string("cd \"") + workingDir + std::string(" && ") + cmd;
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        return "";
    }
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

// Helper function to load JSON file
std::string loadJsonFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper function to find VSCode path
std::string findVSCodePath(const std::string& workspacePath) {
    std::string vscodePath = workspacePath + "/.vscode";
    if (std::filesystem::exists(vscodePath)) {
        return vscodePath;
    }
    return "";
}

// Load project configuration
bool loadProjectConfig(const std::string& configPath, std::string& errorMsg) {
    try {
        // If configPath is empty, try to auto-detect
        std::string actualConfigPath = configPath;
        std::string workspacePath = std::filesystem::current_path().string();
        
        if (actualConfigPath.empty()) {
            // Try to find config file in current directory
            std::vector<std::string> configFiles;
            configFiles.push_back("dev_assistant_config.json");
            configFiles.push_back(".dev_assistant.json");
            configFiles.push_back("dev-assistant-config.json");
            
            for (const auto& file : configFiles) {
                if (std::filesystem::exists(file)) {
                    actualConfigPath = file;
                    break;
                }
            }
            
            if (actualConfigPath.empty()) {
                errorMsg = "No configuration file found. Please provide config_path or create dev_assistant_config.json";
                return false;
            }
        }
        
        // Load the JSON file
        std::string jsonStr = loadJsonFile(actualConfigPath);
        if (jsonStr.empty()) {
            errorMsg = std::string("Failed to read or empty config file: ") + actualConfigPath;
            return false;
        }
        
        // Parse JSON using rapidjson
        rapidjson::Document doc;
        std::istringstream iss(jsonStr);
        rapidjson::IStreamWrapper isw(iss);
        if (doc.ParseStream(isw).HasParseError()) {
            errorMsg = std::string("JSON parse error at offset ") +
                      std::to_string(doc.GetErrorOffset()) + std::string(": ") +
                      std::to_string(static_cast<int>(doc.GetParseError()));
            return false;
        }
        
        // Parse project_name
        if (doc.HasMember("project_name") && doc["project_name"].IsString()) {
            g_projectConfig.project_name = doc["project_name"].GetString();
        } else {
            g_projectConfig.project_name = std::string("Project from ") + actualConfigPath;
        }
        g_projectConfig.workspace_path = workspacePath;
        
        // Parse docker configuration
        if (doc.HasMember("docker") && doc["docker"].IsObject()) {
            const auto& dockerObj = doc["docker"].GetObject();
            
            // Parse docker images
            if (dockerObj.HasMember("build_images") && dockerObj["build_images"].IsArray()) {
                const auto& imagesArr = dockerObj["build_images"].GetArray();
                for (const auto& imgVal : imagesArr) {
                    if (imgVal.IsObject()) {
                        const auto& imgObj = imgVal.GetObject();
                        ProjectConfig::DockerImageConfig image;
                        if (imgObj.HasMember("name") && imgObj["name"].IsString()) {
                            image.name = imgObj["name"].GetString();
                        }
                        if (imgObj.HasMember("dockerfile_path") && imgObj["dockerfile_path"].IsString()) {
                            image.dockerfile_path = imgObj["dockerfile_path"].GetString();
                        }
                        if (imgObj.HasMember("build_context") && imgObj["build_context"].IsString()) {
                            image.build_context = imgObj["build_context"].GetString();
                        }
                        if (imgObj.HasMember("build_args") && imgObj["build_args"].IsObject()) {
                            const auto& argsObj = imgObj["build_args"].GetObject();
                            for (auto it = argsObj.MemberBegin(); it != argsObj.MemberEnd(); ++it) {
                                std::string key = it->name.GetString();
                                std::string value = it->value.IsString() ? it->value.GetString() : "";
                                image.build_args[key] = value;
                            }
                        }
                        if (imgObj.HasMember("tags") && imgObj["tags"].IsArray()) {
                            const auto& tagsArr = imgObj["tags"].GetArray();
                            for (const auto& tagVal : tagsArr) {
                                if (tagVal.IsString()) {
                                    image.tags.push_back(tagVal.GetString());
                                }
                            }
                        }
                        g_projectConfig.docker_images.push_back(image);
                    }
                }
            }
            
            // Parse docker containers
            if (dockerObj.HasMember("containers") && dockerObj["containers"].IsArray()) {
                const auto& containersArr = dockerObj["containers"].GetArray();
                for (const auto& contVal : containersArr) {
                    if (contVal.IsObject()) {
                        const auto& contObj = contVal.GetObject();
                        ProjectConfig::DockerContainerConfig container;
                        if (contObj.HasMember("name") && contObj["name"].IsString()) {
                            container.name = contObj["name"].GetString();
                        }
                        if (contObj.HasMember("image") && contObj["image"].IsString()) {
                            container.image = contObj["image"].GetString();
                        }
                        if (contObj.HasMember("volumes") && contObj["volumes"].IsArray()) {
                            const auto& volsArr = contObj["volumes"].GetArray();
                            for (const auto& volVal : volsArr) {
                                if (volVal.IsString()) {
                                    container.volumes.push_back(volVal.GetString());
                                }
                            }
                        }
                        if (contObj.HasMember("ports") && contObj["ports"].IsArray()) {
                            const auto& portsArr = contObj["ports"].GetArray();
                            for (const auto& portVal : portsArr) {
                                if (portVal.IsString()) {
                                    container.ports.push_back(portVal.GetString());
                                }
                            }
                        }
                        if (contObj.HasMember("environment") && contObj["environment"].IsObject()) {
                            const auto& envObj = contObj["environment"].GetObject();
                            for (auto it = envObj.MemberBegin(); it != envObj.MemberEnd(); ++it) {
                                std::string key = it->name.GetString();
                                std::string value = it->value.IsString() ? it->value.GetString() : "";
                                container.environment[key] = value;
                            }
                        }
                        if (contObj.HasMember("detach") && contObj["detach"].IsBool()) {
                            container.detach = contObj["detach"].GetBool();
                        }
                        g_projectConfig.docker_containers.push_back(container);
                    }
                }
            }
        }
        
        // Parse vscode configuration
        if (doc.HasMember("vscode") && doc["vscode"].IsObject()) {
            const auto& vscodeObj = doc["vscode"].GetObject();
            
            // Parse sync targets
            if (vscodeObj.HasMember("sync_targets") && vscodeObj["sync_targets"].IsArray()) {
                const auto& syncArr = vscodeObj["sync_targets"].GetArray();
                for (const auto& syncVal : syncArr) {
                    if (syncVal.IsObject()) {
                        const auto& syncObj = syncVal.GetObject();
                        ProjectConfig::SyncTargetConfig sync;
                        if (syncObj.HasMember("name") && syncObj["name"].IsString()) {
                            sync.name = syncObj["name"].GetString();
                        }
                        if (syncObj.HasMember("source") && syncObj["source"].IsString()) {
                            sync.source = syncObj["source"].GetString();
                        }
                        if (syncObj.HasMember("destination") && syncObj["destination"].IsString()) {
                            sync.destination = syncObj["destination"].GetString();
                        }
                        if (syncObj.HasMember("host") && syncObj["host"].IsString()) {
                            sync.host = syncObj["host"].GetString();
                        }
                        if (syncObj.HasMember("user") && syncObj["user"].IsString()) {
                            sync.user = syncObj["user"].GetString();
                        }
                        if (syncObj.HasMember("exclude") && syncObj["exclude"].IsArray()) {
                            const auto& exclArr = syncObj["exclude"].GetArray();
                            for (const auto& exclVal : exclArr) {
                                if (exclVal.IsString()) {
                                    sync.exclude.push_back(exclVal.GetString());
                                }
                            }
                        }
                        g_projectConfig.sync_targets.push_back(sync);
                    }
                }
            }
        }
        
        // Parse debugging configuration
        if (doc.HasMember("debugging") && doc["debugging"].IsObject()) {
            const auto& debugObj = doc["debugging"].GetObject();
            
            // Parse gdb servers
            if (debugObj.HasMember("gdb_servers") && debugObj["gdb_servers"].IsArray()) {
                const auto& gdbArr = debugObj["gdb_servers"].GetArray();
                for (const auto& gdbVal : gdbArr) {
                    if (gdbVal.IsObject()) {
                        const auto& gdbObj = gdbVal.GetObject();
                        ProjectConfig::GdbConfig gdb;
                        if (gdbObj.HasMember("name") && gdbObj["name"].IsString()) {
                            gdb.name = gdbObj["name"].GetString();
                        }
                        if (gdbObj.HasMember("host") && gdbObj["host"].IsString()) {
                            gdb.host = gdbObj["host"].GetString();
                        }
                        if (gdbObj.HasMember("port") && gdbObj["port"].IsInt()) {
                            gdb.port = gdbObj["port"].GetInt();
                        }
                        if (gdbObj.HasMember("executable") && gdbObj["executable"].IsString()) {
                            gdb.executable = gdbObj["executable"].GetString();
                        }
                        if (gdbObj.HasMember("arguments") && gdbObj["arguments"].IsArray()) {
                            const auto& argsArr = gdbObj["arguments"].GetArray();
                            for (const auto& argVal : argsArr) {
                                if (argVal.IsString()) {
                                    gdb.arguments.push_back(argVal.GetString());
                                }
                            }
                        }
                        if (gdbObj.HasMember("working_directory") && gdbObj["working_directory"].IsString()) {
                            gdb.working_directory = gdbObj["working_directory"].GetString();
                        }
                        if (gdbObj.HasMember("source_paths") && gdbObj["source_paths"].IsArray()) {
                            const auto& srcArr = gdbObj["source_paths"].GetArray();
                            for (const auto& srcVal : srcArr) {
                                if (srcVal.IsString()) {
                                    gdb.source_paths.push_back(srcVal.GetString());
                                }
                            }
                        }
                        g_projectConfig.gdb_configs.push_back(gdb);
                    }
                }
            }
        }
        
        // Parse prompts
        if (doc.HasMember("prompts") && doc["prompts"].IsObject()) {
            const auto& promptsObj = doc["prompts"].GetObject();
            for (auto it = promptsObj.MemberBegin(); it != promptsObj.MemberEnd(); ++it) {
                std::string categoryName = it->name.GetString();
                ProjectConfig::PromptsConfig::Category category;
                if (it->value.IsObject()) {
                    const auto& catObj = it->value.GetObject();
                    for (auto catIt = catObj.MemberBegin(); catIt != catObj.MemberEnd(); ++catIt) {
                        std::string key = catIt->name.GetString();
                        std::string value = catIt->value.IsString() ? catIt->value.GetString() : "";
                        category.entries[key] = value;
                    }
                }
                g_projectConfig.prompts.categories[categoryName] = category;
            }
        }
        
        // Parse dictionary
        if (doc.HasMember("dictionary") && doc["dictionary"].IsObject()) {
            const auto& dictObj = doc["dictionary"].GetObject();
            for (auto it = dictObj.MemberBegin(); it != dictObj.MemberEnd(); ++it) {
                std::string categoryName = it->name.GetString();
                ProjectConfig::DictionaryConfig::Category category;
                if (it->value.IsObject()) {
                    const auto& catObj = it->value.GetObject();
                    for (auto catIt = catObj.MemberBegin(); catIt != catObj.MemberEnd(); ++catIt) {
                        std::string key = catIt->name.GetString();
                        std::string value = catIt->value.IsString() ? catIt->value.GetString() : "";
                        category.entries[key] = value;
                    }
                }
                g_projectConfig.dictionary.categories[categoryName] = category;
            }
        }
        
        // Parse workflows
        if (doc.HasMember("workflows") && doc["workflows"].IsObject()) {
            const auto& workflowsObj = doc["workflows"].GetObject();
            for (auto it = workflowsObj.MemberBegin(); it != workflowsObj.MemberEnd(); ++it) {
                std::string workflowName = it->name.GetString();
                ProjectConfig::WorkflowConfig workflow;
                workflow.name = workflowName;
                if (it->value.HasMember("description") && it->value["description"].IsString()) {
                    workflow.description = it->value["description"].GetString();
                }
                if (it->value.HasMember("steps") && it->value["steps"].IsArray()) {
                    const auto& stepsArr = it->value["steps"].GetArray();
                    for (const auto& stepVal : stepsArr) {
                        if (stepVal.IsString()) {
                            // Simple string format: "tool_name:args"
                            std::string stepStr = stepVal.GetString();
                            size_t colonPos = stepStr.find(':');
                            if (colonPos != std::string::npos) {
                                workflow.steps.push_back({.tool_name = stepStr.substr(0, colonPos), .args = stepStr.substr(colonPos + 1)});
                            } else {
                                workflow.steps.push_back({.tool_name = stepStr, .args = "{}"});
                            }
                        }
                    }
                }
                g_projectConfig.workflows.push_back(workflow);
            }
        }
        
        NEKO_LOG_INFO("dev-assistant", "Configuration loaded successfully. {} docker images, {} containers, {} sync targets, {} gdb configs, {} workflows",
                     g_projectConfig.docker_images.size(),
                     g_projectConfig.docker_containers.size(),
                     g_projectConfig.sync_targets.size(),
                     g_projectConfig.gdb_configs.size(),
                     g_projectConfig.workflows.size());
        
        return true;
    } catch (const std::exception& e) {
        errorMsg = std::string("Exception loading config: ") + std::string(e.what());
        return false;
    }
}
