#include <filesystem>
#include <expected>

#include <ilias/platform.hpp>
#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Tool handler: Sync files
std::string sync_files(SyncFilesParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "File sync requested: {}", params.target_name);
        
        // Find the sync target configuration
        const ProjectConfig::SyncTargetConfig* syncConfig = nullptr;
        for (const auto& sync : g_projectConfig.sync_targets) {
            if (sync.name == params.target_name) {
                syncConfig = &sync;
                break;
            }
        }
        
        if (!syncConfig) {
            std::string result = std::string("Sync target configuration not found: ") + params.target_name + std::string("\n\n");
            result += "Available sync targets:\n";
            for (const auto& sync : g_projectConfig.sync_targets) {
                result += std::string("  - ") + sync.name + std::string(" (") + sync.host + std::string(")\n");
            }
            return result;
        }
        
        // Build rsync command
        std::string cmd = "rsync -avz --progress";
        
        // Add exclude patterns
        for (const auto& excl : syncConfig->exclude) {
            cmd += std::string(" --exclude '") + excl + std::string("'");
        }
        
        // Add source and destination
        std::string source = syncConfig->source;
        if (source.empty()) {
            source = "./";
        }
        
        cmd += std::string(" ") + source;
        cmd += std::string(" ") + syncConfig->user + std::string("@") + syncConfig->host + std::string(":") + syncConfig->destination;
        
        NEKO_LOG_INFO("dev-assistant", "Executing rsync: {}", cmd);
        
        // Execute command
        std::string output = executeCommand(cmd, workspacePath);
        
        std::string result = std::string("=== File Sync ===\n\n");
        result += std::string("Target: ") + params.target_name + std::string("\n");
        result += std::string("Host: ") + syncConfig->host + std::string("\n");
        result += std::string("Source: ") + source + std::string("\n");
        result += std::string("Destination: ") + syncConfig->destination + std::string("\n\n");
        result += std::string("Command: ") + cmd + std::string("\n\n");
        result += std::string("Output:\n") + output;
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in sync_files: {}", e.what());
        return std::string("Error syncing files: ") + std::string(e.what());
    }
}

// Tool handler: Start GDB
std::string start_gdb(StartGdbParams params) {
    try {
        std::string workspacePath = !params.workspace_path.has_value()
            ? std::filesystem::current_path().string()
            : params.workspace_path.value();
        
        NEKO_LOG_INFO("dev-assistant", "GDB start requested: {}", params.gdb_config_name);
        
        // Find the GDB configuration
        const ProjectConfig::GdbConfig* gdbConfig = nullptr;
        for (const auto& gdb : g_projectConfig.gdb_configs) {
            if (gdb.name == params.gdb_config_name) {
                gdbConfig = &gdb;
                break;
            }
        }
        
        if (!gdbConfig) {
            std::string result = std::string("GDB configuration not found: ") + params.gdb_config_name + std::string("\n\n");
            result += "Available GDB configurations:\n";
            for (const auto& gdb : g_projectConfig.gdb_configs) {
                result += std::string("  - ") + gdb.name + std::string(" (") + gdb.host + std::string(":") + std::to_string(gdb.port) + std::string(")\n");
            }
            return result;
        }
        
        std::string result = std::string("=== Remote GDB Debugging ===\n\n");
        result += std::string("Configuration: ") + params.gdb_config_name + std::string("\n");
        result += std::string("Host: ") + gdbConfig->host + std::string("\n");
        result += std::string("Port: ") + std::to_string(gdbConfig->port) + std::string("\n");
        result += std::string("Executable: ") + gdbConfig->executable + std::string("\n");
        result += std::string("Working Directory: ") + gdbConfig->working_directory + std::string("\n\n");
        
        // Build GDB command
        std::string gdbCmd = "gdb";
        
        // Add source paths
        for (const auto& srcPath : gdbConfig->source_paths) {
            gdbCmd += std::string(" -d '") + srcPath + std::string("'");
        }
        
        // Add target remote
        gdbCmd += std::string(" -ex 'target remote ") + gdbConfig->host + std::string(":") + std::to_string(gdbConfig->port) + std::string("'");
        
        // Add file
        gdbCmd += std::string(" -ex 'file ") + gdbConfig->executable + std::string("'");
        
        // Add arguments
        if (!gdbConfig->arguments.empty()) {
            std::string args;
            for (const auto& arg : gdbConfig->arguments) {
                args += arg + std::string(" ");
            }
            gdbCmd += std::string(" -ex 'set args ") + args + std::string("'");
        }
        
        result += std::string("GDB Command:\n") + gdbCmd + std::string("\n\n");
        
        // Provide instructions for starting remote debugging
        result += std::string("=== Instructions ===\n\n");
        result += std::string("1. On the remote machine, start gdbserver:\n");
        result += std::string("   gdbserver --multi ") + gdbConfig->host + std::string(":") + std::to_string(gdbConfig->port) + std::string("\n\n");
        result += std::string("2. Or start with your application:\n");
        result += std::string("   gdbserver ") + gdbConfig->host + std::string(":") + std::to_string(gdbConfig->port) + std::string(" ") + gdbConfig->executable + std::string("\n\n");
        result += std::string("3. Then run the GDB command above on your local machine.\n\n");
        result += std::string("Alternatively, use VSCode's built-in debugger with 'start_vscode_debug'\n");
        result += std::string("for a better debugging experience.");
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in start_gdb: {}", e.what());
        return std::string("Error starting GDB: ") + std::string(e.what());
    }
}
