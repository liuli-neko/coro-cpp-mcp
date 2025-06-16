#include "ccmcp/model/annotated.hpp"

#include <any>
#include <nekoproto/jsonrpc/jsonrpc.hpp>
#include <tuple>

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
};
} // namespace traits

template <typename T, typename NEKO_NAMESPACE::ConstexprString FuncName>
struct ToolFunction {
    using TypeTraits = traits::ToolFunctionTraits<T>;
    using ParamsT    = TypeTraits::ParamsT;
    using ReturnT    = TypeTraits::ReturnT;

    ToolFunction(std::string description) : description(description) {}

    auto tool() -> Tool {
        Tool tl;
        tl.name        = std::string(name);
        tl.description = description;
        JsonSchema inputSchema;
        inputSchema.properties = std::map<std::string, JsonSchema>();
        for (const auto& [pname, desc] : paramsDescription) {
            (*inputSchema.properties)[std::string(pname)].description = desc;
        }
        generate_schema<ParamsT>(ParamsT{}, inputSchema);
        tl.inputSchema = std::make_shared<JsonSchema>(std::move(inputSchema));
        return tl;
    }
    auto& operator=(std::function<ReturnT(ParamsT)> func) {
        function = func;
        return *this;
    }
    static constexpr auto name = FuncName.view();
    std::map<std::string_view, std::string> paramsDescription;
    std::string description;
    std::function<ReturnT(ParamsT)> function;
};

CCMCP_EN
