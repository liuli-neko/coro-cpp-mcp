#include "ccmcp/model/annotated.hpp"

#include <nekoproto/jsonrpc/jsonrpc.hpp>

CCMCP_BN

NEKO_USE_NAMESPACE
using NEKO_NAMESPACE::detail::JsonRpcRequest2;

template <typename T, traits::ConstexprString... ArgNames>
struct Request : public JsonRpcRequest2<T, ArgNames...> {
    using rpcjson_request = JsonRpcRequest2<T, ArgNames...>;
    using ParamsTupleType = decltype(std::tuple_cat(
        std::declval<std::conditional_t<traits::is_optional<typename rpcjson_request::ParamsTupleType>::value,
                                        std::tuple<>, typename rpcjson_request::ParamsTupleType>>(),
        std::declval<std::tuple<std::optional<JsonSerializer::JsonValue>>>()));

    template <typename SerializerT>
    bool serialize(SerializerT& serializer) const NEKO_NOEXCEPT {
        return this->JsonRpcRequest2<T, ArgNames...>::serialize(serializer);
    }

    template <typename SerializerT>
    bool serialize(SerializerT& serializer) NEKO_NOEXCEPT {
        return this->JsonRpcRequest2<T, ArgNames...>::serialize(serializer);
    }
};

template <typename T, NEKO_NAMESPACE::traits::ConstexprString... ArgNames>
struct ToolFunction : public RpcMethod<T, ArgNames...> {
    using RpcMethod    = RpcMethod<T, ArgNames...>;
    using MethodType   = RpcMethod::MethodType;
    using MethodTraits = RpcMethod::MethodTraits;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::ParamsTupleType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using typename MethodTraits::ReturnType;
    using RequestType  = RpcMethod::RequestType;
    using ResponseType = RpcMethod::ResponseType;

    using RpcMethod::operator();

    static_assert(sizeof...(ArgNames) - 1 == MethodTraits::NumParams, "ToolFunction must have at least one argument.");
    ToolFunction(std::vector<std::string> paramsDescription) : RpcMethod() {
        NEKO_ASSERT(paramsDescription.size() == sizeof...(ArgNames) - 1, "mcp",
                    "ToolFunction paramsDescription size error!");
        std::array<std::string, sizeof...(ArgNames)> argNames = {ArgNames...};
        [this, &argNames, &paramsDescription]<std::size_t... Is>(std::index_sequence<Is...> /*unused*/) {
            ((mParamsDescription[argNames[Is + 1]] = paramsDescription[Is]), ...);
        }(std::make_index_sequence<sizeof...(ArgNames) - 1>{});
    }

    std::map<std::string_view, std::string> mParamsDescription;
};

CCMCP_EN
