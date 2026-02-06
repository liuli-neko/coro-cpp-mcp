#include <filesystem>
#include <fstream>
#include <sstream>

#include "ilias/platform.hpp"
#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"
#include "dev_assistant/common.hpp"
#include "dev_assistant/tools.hpp"

NEKO_USE_NAMESPACE

// Tool handler: Read file
std::string read_file(ReadFileParams params) {
    try {
        std::ifstream file(params.file_path);
        if (!file.is_open()) {
            return std::string("Failed to open file: ") + params.file_path;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (const std::exception& e) {
        return std::string("Error reading file: ") + std::string(e.what());
    }
}

// Tool handler: Query prompts from config
std::string query_prompts(QueryPromptsParams params) {
    try {
        NEKO_LOG_INFO("dev-assistant", "Querying prompts - category: {}, key: {}",
                     params.category.has_value() ? params.category.value() : "all",
                     params.key.has_value() ? params.key.value() : "all");
        
        std::string result = std::string("=== AI Assistance Prompts ===\n\n");
        
        if (!params.category.has_value() || params.category.value().empty()) {
            // List all categories
            result += "Available categories:\n";
            for (const auto& [catName, category] : g_projectConfig.prompts.categories) {
                result += std::string("  - ") + catName + std::string(" (") +
                         std::to_string(category.entries.size()) + std::string(" entries)\n");
            }
            result += "\nUse category parameter to get specific prompts, or both category and key for specific entry.";
            return result;
        }
        
        std::string categoryName = params.category.value();
        auto it = g_projectConfig.prompts.categories.find(categoryName);
        if (it == g_projectConfig.prompts.categories.end()) {
            return std::string("Category not found: ") + categoryName;
        }
        
        const auto& category = it->second;
        result += std::string("Category: ") + categoryName + std::string("\n\n");
        
        if (!params.key.has_value()) {
            // List all entries in category
            for (const auto& [key, prompt] : category.entries) {
                result += std::string("  [") + key + std::string("]\n");
                result += std::string("  ") + prompt + std::string("\n\n");
            }
        } else {
            // Get specific entry
            std::string key = params.key.value();
            auto entryIt = category.entries.find(key);
            if (entryIt == category.entries.end()) {
                return std::string("Key not found in category ") + categoryName + std::string(": ") + key;
            }
            result += std::string("Key: ") + key + std::string("\n\n");
            result += entryIt->second;
        }
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in query_prompts: {}", e.what());
        return std::string("Error querying prompts: ") + std::string(e.what());
    }
}

// Tool handler: Query dictionary
std::string query_dictionary(QueryDictionaryParams params) {
    try {
        NEKO_LOG_INFO("dev-assistant", "Querying dictionary - category: {}, key: {}",
                     params.category.has_value() ? params.category.value() : "all",
                     params.key.has_value() ? params.key.value() : "all");
        
        std::string result = std::string("=== Dictionary ===\n\n");
        
        if (!params.category.has_value() || params.category.value().empty()) {
            // List all categories
            result += "Available categories:\n";
            for (const auto& [catName, category] : g_projectConfig.dictionary.categories) {
                result += std::string("  - ") + catName + std::string(" (") +
                         std::to_string(category.entries.size()) + std::string(" entries)\n");
            }
            result += "\nUse category parameter to get specific entries, or both category and key for specific definition.";
            return result;
        }
        
        std::string categoryName = params.category.value();
        auto it = g_projectConfig.dictionary.categories.find(categoryName);
        if (it == g_projectConfig.dictionary.categories.end()) {
            return std::string("Category not found: ") + categoryName;
        }
        
        const auto& category = it->second;
        result += std::string("Category: ") + categoryName + std::string("\n\n");
        
        if (!params.key.has_value()) {
            // List all entries in category
            for (const auto& [key, definition] : category.entries) {
                result += std::string("  [") + key + std::string("] -> ") + definition + std::string("\n");
            }
        } else {
            // Get specific entry
            std::string key = params.key.value();
            auto entryIt = category.entries.find(key);
            if (entryIt == category.entries.end()) {
                return std::string("Key not found in category ") + categoryName + std::string(": ") + key;
            }
            result += std::string("[") + key + std::string("] -> ") + entryIt->second;
        }
        
        return result;
    } catch (const std::exception& e) {
        NEKO_LOG_ERROR("dev-assistant", "Error in query_dictionary: {}", e.what());
        return std::string("Error querying dictionary: ") + std::string(e.what());
    }
}
