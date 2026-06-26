#include <expected>

#include <ilias/platform.hpp>
#include <ilias/result.hpp>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Forward declarations for tool handlers (defined in other files)
std::string load_config(LoadConfigParams params);
std::string docker_build(DockerBuildParams params);
std::string docker_run(DockerRunParams params);
std::string sync_files(SyncFilesParams params);
std::string start_gdb(StartGdbParams params);
std::string execute_vscode_task(ExecuteVSCodeTaskParams params);
std::string start_vscode_debug(StartVSCodeDebugParams params);

// Tool handler: List workflows
std::string list_workflows(ListWorkflowsParams params) {
    try {
        NEKO_LOG_INFO("dev-assistant", "Listing workflows");
        
        std::string result = std::string("=== Available Workflows ===\n\n");
        
        if (g_projectConfig.workflows.empty()) {
            result += "No workflows configured in dev_assistant_config.json";
            return result;
        }
        
        for (const auto& workflow : g_projectConfig.workflows) {
            result += std::string("Workflow: ") + workflow.name + std::string("\n");
            if (params.include_descriptions) {
                result += std::string("  Description: ") + workflow.description + std::string("\n");
            }
            result += std::string("  Steps: ") + std::to_string(workflow.steps.size()) + std::string("\n");
            result += "\n";
        }
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in list_workflows: {}", e.what());
        return std::string("Error listing workflows: ") + std::string(e.what());
    }
}

// Tool handler: Execute workflow
std::string execute_workflow(ExecuteWorkflowParams params) {
    std::string workflowName = params.workflow_name;
    
    // Find workflow
    const ProjectConfig::WorkflowConfig* workflow = nullptr;
    for (const auto& wf : g_projectConfig.workflows) {
        if (wf.name == workflowName) {
            workflow = &wf;
            break;
        }
    }
    
    if (!workflow) {
        return std::string("Workflow not found: ") + workflowName + std::string("\n") +
               std::string("Use list_workflows to see available workflows.");
    }
    
    std::string result = std::string("=== Executing Workflow: ") + workflowName + std::string(" ===\n\n");
    result += std::string("Description: ") + workflow->description + std::string("\n\n");
    
    NEKO_LOG_INFO("dev-assistant", "Executing workflow: {}", workflowName);
    
    // Execute each step
    bool allSuccess = true;
    for (size_t i = 0; i < workflow->steps.size(); ++i) {
        const auto& step = workflow->steps[i];
        result += std::string("Step ") + std::to_string(i + 1) + std::string("/") +
                 std::to_string(workflow->steps.size()) + std::string(": ") +
                 step.tool_name + std::string("\n");
        result += std::string("  Args: ") + step.args + std::string("\n");
        
        std::string stepResult;
        bool stepSuccess = true;
        
        try {
            // Call the appropriate handler based on tool name
            if (step.tool_name == "load_config") {
                LoadConfigParams p;
                if (!step.args.empty() && step.args != "{}") {
                    p.config_path = step.args;
                }
                stepResult = load_config(p);
            } else if (step.tool_name == "docker_build") {
                DockerBuildParams p;
                p.config_name = step.args;
                stepResult = docker_build(p);
            } else if (step.tool_name == "docker_run") {
                DockerRunParams p;
                p.container_name = step.args;
                stepResult = docker_run(p);
            } else if (step.tool_name == "sync_files") {
                SyncFilesParams p;
                p.target_name = step.args;
                stepResult = sync_files(p);
            } else if (step.tool_name == "start_gdb") {
                StartGdbParams p;
                p.gdb_config_name = step.args;
                stepResult = start_gdb(p);
            } else if (step.tool_name == "execute_vscode_task") {
                ExecuteVSCodeTaskParams p;
                p.task_name = step.args;
                stepResult = execute_vscode_task(p);
            } else if (step.tool_name == "start_vscode_debug") {
                StartVSCodeDebugParams p;
                p.launch_name = step.args;
                stepResult = start_vscode_debug(p);
            } else {
                stepResult = std::string("Unknown tool: ") + step.tool_name;
                stepSuccess = false;
            }
            
            // Check if step failed
            if (stepResult.find("not found") != std::string::npos ||
                stepResult.find("failed") != std::string::npos ||
                stepResult.find("error") != std::string::npos) {
                stepSuccess = false;
            }
        } catch (const std::exception& e) {
            stepResult = std::string("Exception: ") + std::string(e.what());
            stepSuccess = false;
        }
        
        if (stepSuccess) {
            result += std::string("  Status: ✓ Success\n\n");
            result += std::string("  Output:\n") + stepResult + std::string("\n\n");
        } else {
            result += std::string("  Status: ✗ Failed\n\n");
            result += std::string("  Error:\n") + stepResult + std::string("\n\n");
            allSuccess = false;
            // Continue executing remaining steps
        }
    }
    
    if (allSuccess) {
        result += std::string("=== Workflow Completed Successfully ===\n");
    } else {
        result += std::string("=== Workflow Completed with Errors ===\n");
    }
    
    return result;
}
