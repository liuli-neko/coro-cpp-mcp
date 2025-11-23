#pragma once

#include <type_traits>
#include <functional>
#include <tuple>

namespace minihttp {
namespace detail {

// Helper function to extract arguments from a member function pointer.
template <typename T>
struct FnTraitsBase : public FnTraitsBase<decltype(&T::operator ())> { // For lambdas

};

template <typename R, typename C, typename ...Args>
struct FnTraitsBase<R(C::*)(Args...) const> {
    using ReturnType = R;
    using Arguments = std::tuple<Args...>;
};

template <typename R, typename C, typename ...Args>
struct FnTraitsBase<R(C::*)(Args...)> {
    using ReturnType = R;
    using Arguments = std::tuple<Args...>;
};

template <typename R, typename ...Args>
struct FnTraitsBase<R(*)(Args...)> {
    using ReturnType = R;
    using Arguments = std::tuple<Args...>;
};

template <typename R, typename ...Args>
struct FnTraitsBase<R(Args...)> {
    using ReturnType = R;
    using Arguments = std::tuple<Args...>;
};

template <typename T>
struct FnTraitsBase<std::function<T> > : public FnTraitsBase<T> {};

} // namespace detail

template <typename Fn>
struct FnTraits : public detail::FnTraitsBase<Fn> {

};

/**
 * @brief For the function type, without any overloading
 * 
 * @tparam Fn 
 */
template <typename Fn>
concept FnLike = requires {
    typename FnTraits<Fn>::ReturnType;
    typename FnTraits<Fn>::Arguments;
};

} // namespace minihttp