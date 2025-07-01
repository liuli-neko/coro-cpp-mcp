#pragma once
#include "ccmcp/global/global.hpp"

#include "capabilities.hpp"

CCMCP_BN

NEKO_USE_NAMESPACE
namespace traits {
template <typename CallableT>
struct ToolFunctionTraits;
template <typename Ret, typename SerializableT>
    requires NEKO_NAMESPACE::detail::has_values_meta<SerializableT> &&
             NEKO_NAMESPACE::detail::has_names_meta<SerializableT>
struct ToolFunctionTraits<Ret(SerializableT)> {
    using ParamsT = SerializableT;
    using ReturnT = Ret;
#if __cpp_lib_move_only_function >= 202110L
    using FunctionT    = std::move_only_function<ILIAS_NAMESPACE::IoTask<ReturnT>(ParamsT)>;
    using FunctionTRaw = std::move_only_function<ReturnT(ParamsT)>;
#else
    using FunctionT    = std::function<ILIAS_NAMESPACE::IoTask<ReturnT>(ParamsT)>;
    using FunctionTRaw = std::function<ReturnT(ParamsT)>;
#endif
    auto operator()(ParamsT&& params) const { return function(std::forward<ParamsT>(params)); }

protected:
    mutable FunctionT function;
};
template <typename Ret, typename SerializableT>
    requires NEKO_NAMESPACE::detail::has_values_meta<SerializableT> &&
             NEKO_NAMESPACE::detail::has_names_meta<SerializableT>
struct ToolFunctionTraits<std::function<Ret(SerializableT)>> {
    using ParamsT = SerializableT;
    using ReturnT = Ret;
#if __cpp_lib_move_only_function >= 202110L
    using FunctionT    = std::move_only_function<ILIAS_NAMESPACE::IoTask<ReturnT>(ParamsT)>;
    using FunctionTRaw = std::move_only_function<ReturnT(ParamsT)>;
#else
    using FunctionT    = std::function<ILIAS_NAMESPACE::IoTask<ReturnT>(ParamsT)>;
    using FunctionTRaw = std::function<ReturnT(ParamsT)>;
#endif

    auto operator()(ParamsT&& params) const { return function(std::forward<ParamsT>(params)); }

protected:
    mutable FunctionT function;
};
template <typename Ret>
struct ToolFunctionTraits<Ret()> {
    using ParamsT = void;
    using ReturnT = Ret;
#if __cpp_lib_move_only_function >= 202110L
    using FunctionT    = std::move_only_function<ILIAS_NAMESPACE::IoTask<ReturnT>()>;
    using FunctionTRaw = std::move_only_function<ReturnT()>;
#else
    using FunctionT    = std::function<ILIAS_NAMESPACE::IoTask<ReturnT>()>;
    using FunctionTRaw = std::function<ReturnT()>;
#endif

    auto operator()() const { return function(); }

protected:
    mutable FunctionT function;
};
template <typename Ret>
struct ToolFunctionTraits<std::function<Ret()>> {
    using ParamsT = void;
    using ReturnT = Ret;
#if __cpp_lib_move_only_function >= 202110L
    using FunctionT    = std::move_only_function<ILIAS_NAMESPACE::IoTask<ReturnT>()>;
    using FunctionTRaw = std::move_only_function<ReturnT()>;
#else
    using FunctionT    = std::function<ILIAS_NAMESPACE::IoTask<ReturnT>()>;
    using FunctionTRaw = std::function<ReturnT()>;
#endif

    auto operator()() const { return function(); }

protected:
    mutable FunctionT function;
};
} // namespace traits

template <typename T>
struct DynamicToolFunction : traits::ToolFunctionTraits<T> {
    using TypeTraits = traits::ToolFunctionTraits<T>;
    using ParamsT    = TypeTraits::ParamsT;
    using ReturnT    = TypeTraits::ReturnT;
    using FunctionT  = TypeTraits::FunctionT;
    using FunctionTRaw  = TypeTraits::FunctionTRaw;

    DynamicToolFunction(std::string_view name, std::string_view description) : name(name), description(description) {}

    auto tool() -> Tool {
        Tool tl;
        tl.name        = std::string(name);
        tl.description = description;
        JsonSchema inputSchema;
        if constexpr (!std::is_void_v<ParamsT>) {
            inputSchema.properties = std::map<std::string, JsonSchema>();
            for (const auto& [pname, desc] : paramsDescription) {
                (*inputSchema.properties)[std::string(pname)].description = desc;
            }
            generate_schema<ParamsT>(ParamsT{}, inputSchema);
        } else {
            inputSchema.type       = "object";
            inputSchema.properties = std::map<std::string, JsonSchema>();
        }
        inputSchema.schema = std::nullopt; // why cline must be null for empty object.
        tl.inputSchema     = std::make_shared<JsonSchema>(std::move(inputSchema));
        return tl;
    }

    auto& operator=(FunctionTRaw func) {
        if constexpr (std::is_void_v<ParamsT>) {
            this->function = [func = std::move(func)]() mutable -> ILIAS_NAMESPACE::IoTask<ReturnT> {
                co_return func();
            };
        } else {
            this->function = [func = std::move(func)](ParamsT params) mutable -> ILIAS_NAMESPACE::IoTask<ReturnT> {
                co_return func(std::move(params));
            };
        }
        return *this;
    }
    auto& operator=(FunctionT func) {
        this->function = std::move(func);
        return *this;
    }
    operator bool() const { return this->function != nullptr; }

    std::string name;
    std::map<std::string_view, std::string> paramsDescription;
    std::string description;
};

template <typename T, NEKO_NAMESPACE::ConstexprString FuncName>
struct ToolFunction : DynamicToolFunction<T> {
    ToolFunction(std::string_view description) : DynamicToolFunction<T>(FuncName.view(), description) {}
    using DynamicToolFunction<T>::operator=;
    constexpr static std::string_view name = FuncName.view();
};

CCMCP_EN
