#include "ccmcp/model/annotated.hpp"

#include <any>
#include <nekoproto/jsonrpc/jsonrpc.hpp>
#include <tuple>

CCMCP_BN

NEKO_USE_NAMESPACE
namespace traits {

using detail::InputSerializer;

struct ParamMetadata {
    std::string name;
    std::string description;
    std::string type;
    std::optional<std::string> format;
    bool isOptional = false;

    template <typename T>
    void init(T& raw) {
        value  = &raw;
        setter = [this](std::any value) { *reinterpret_cast<T*>(this->value) = *std::any_cast<T>(&value); };
        getter = [this]() { return *reinterpret_cast<T*>(this->value); };
    }

    template <typename T>
    void set(const T& value) {
        setter(value);
    }

    template <typename T>
    T get() const {
        return getter();
    }

private:
    std::function<void(std::any)> setter;
    std::function<std::any()> getter;
    void* value;
};

class ReflectionHelper : InputSerializer<ReflectionHelper> {
public:
    template <typename T>
    bool loadValue(SizeTag<T> const& /*unused*/) {
        return true;
    }
    bool loadValue(int8_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "int8";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(uint8_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "uint8";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(int16_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "int16";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(uint16_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "uint16";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(int32_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "int32";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(uint32_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "uint32";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(int64_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "int64";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(uint64_t& value) NEKO_NOEXCEPT {
        mParams.back().format = "uint64";
        mParams.back().type   = "integer";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(float& value) NEKO_NOEXCEPT {
        mParams.back().format = "float";
        mParams.back().type   = "float";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(double& value) NEKO_NOEXCEPT {
        mParams.back().format = "double";
        mParams.back().type   = "float";
        mParams.back().init(value);
        return true;
    }
    bool loadValue(bool& value) NEKO_NOEXCEPT {
        mParams.back().type = "boolean";
        mParams.back().init(value);
        return true;
    }
    template <typename CharT, typename Traits, typename Alloc>
    bool loadValue(std::basic_string<CharT, Traits, Alloc>& value) NEKO_NOEXCEPT {
        mParams.back().type = "string";
        mParams.back().init(value);
        return true;
    }
    template <typename T>
    bool loadValue(const NameValuePair<T>& value) NEKO_NOEXCEPT {
        mParams.push_back({
            .name        = std::string{value.name, value.nameLen},
            .description = "",
            .type        = "",
            .format      = nullptr,
            .isOptional  = NEKO_NAMESPACE::traits::is_optional<T>::value,
        });
        return loadValue(value.value);
    }

    auto get_params_metadata() const -> const std::vector<ParamMetadata>& { return mParams; }
    auto get_params_metadata() -> std::vector<ParamMetadata>& { return mParams; }

private:
    std::vector<ParamMetadata> mParams;
};

template <typename T>
struct RequestParams {
    std::string name;
    T arguments;

    NEKO_SERIALIZER(name, arguments)
};

template <typename T>
struct ResultContent;
template <>
struct ResultContent<std::string> {
    std::string type = "text";
    std::string text;
    NEKO_SERIALIZER(type, text)
};

template <>
struct ResultContent<int> {

};

template <typename... Args>
struct ResultParams {
    std::tuple<Args...> content;

    NEKO_SERIALIZER(content)
};

template <typename CallableT>
struct ToolFunctionTraits;
template <typename Ret, typename SerializableT>
struct ToolFunctionTraits<Ret(SerializableT)> {
    using ParamsTupleT = SerializableT;
    using ReturnT      = Ret;
    using RequestType  = RequestParams<SerializableT>;
};
} // namespace traits

template <typename T, typename NEKO_NAMESPACE::traits::ConstexprString FuncName>
struct ToolFunction {
    using TypeTraits   = traits::ToolFunctionTraits<T>;
    using RequestType  = TypeTraits::RequestType;
    using ResponseType = TypeTraits::ResponseType;

    ToolFunction(std::string description, std::vector<std::string> paramsDescription) {
        
    }

    std::map<std::string_view, std::string> mParamsDescription;
};

CCMCP_EN
