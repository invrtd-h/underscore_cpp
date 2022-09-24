#ifndef UNDERSCORE_CPP_FFFFFF_H
#define UNDERSCORE_CPP_FFFFFF_H

#include <type_traits>
#include <functional>
#include <algorithm>
#include <iostream>
#include <concepts>
#include <utility>
#include <ranges>
#include <tuple>

#include "us_tmf.h"

namespace fff::impl::policydef {
    struct NewDataPolicy {
        static constexpr bool is_new_data_policy = true;
    };
    
    struct ExecutionPolicy {
        static constexpr bool is_execution_policy = true;
    };
}

/**
 * Small operations
 */
namespace fff::impl::elem {
    using namespace policydef;
    
    /**
     * The function object that returns the SZ-th value by perfect forwarding.
     * @example IdentityAt<2>()(1, 2, std::string("a")) \n
     * Returns the rvalue of std::string("a")
     * @tparam SZ the position to return
     * @cite https://en.cppreference.com/w/cpp/utility/functional/identity
     */
    template<size_t SZ>
    struct IdentityAt : public NewDataPolicy {
        /**
         * @tparam T Any-type
         * @tparam Args Any-parameter-pack
         * @param t Any-variable
         * @param args Any-variable-pack
         * @return std::forward<T>(t)
         */
        template<class T, typename... Args>
        constexpr auto &&operator()(T &&t, Args &&...args) const noexcept {
            return IdentityAt<SZ - 1>()(std::forward<Args>(args)...);
        }
    };
    
    template<>
    struct IdentityAt<0> : public NewDataPolicy {
        /**
         * Template specification of IdentityAt<> (read upward)
         */
        template<class T, typename ...Args>
        constexpr T &&operator()(T &&t, Args &&...args) const noexcept {
            return std::forward<T>(t);
        }
    };
    
    using Identity = IdentityAt<0>;
    inline auto identity = Identity();
}

namespace fff::impl::elem {
    
    /**
     * The function object that returns the copy of the SZ-th value.
     * @tparam SZ the position to return
     */
    template<size_t SZ>
    struct CopyAt : public NewDataPolicy {
        /**
         * @tparam T Any-type
         * @tparam Args Any-parameter-pack
         * @param t Any-variable
         * @param args Any-variable-pack
         * @return std::forward<T>(t)
         */
        template<class T, typename ...Args>
        constexpr auto operator()(T &&t, Args &&...args) const noexcept {
            return IdentityAt<SZ - 1>()(std::forward<Args>(args)...);
        }
    };
    template<>
    struct CopyAt<0> : public NewDataPolicy {
        /**
         * Template specification of IdentityAt<> (read upward)
         */
        template<class T, typename ...Args>
        constexpr T operator()(T &&t, Args &&...args) const noexcept {
            return t;
        }
    };
    
    using Copy = CopyAt<0>;
    inline auto copy = Copy();
    
    /**
     * No-operation function obj.
     * @return no_return
     */
    struct Noop {
        template<class ...Args>
        constexpr void operator()(Args &&...args) const noexcept {
            // do nothing
        }
    };
    
    /**
     * A callable that always Returns a constant.
     * @tparam T the type of the constant to return
     */
    template<typename T>
    struct AlwaysConstant {
        using type = T;
        /**
         * @tparam t the value of the constant to return
         */
        template<T t>
        struct Returns {
            /**
             * @tparam Args Any
             * @param args any
             * @return t, pre-defined value by definition of the callable (orthogonal to args)
             */
            template<class ...Args>
            constexpr T operator()(Args &&...args) const noexcept {
                return t;
            }
        };
    };
    
    using AlwaysPositive = AlwaysConstant<bool>::Returns<true>;
    using AlwaysNegative = AlwaysConstant<bool>::Returns<false>;
    
    inline auto always_positive = AlwaysPositive();
    inline auto always_negative = AlwaysNegative();
}

namespace fff::impl::util {
    template<class Fn>
    requires (not std::is_void_v<std::invoke_result_t<Fn>>)
    class Once_nv {
        friend class Once;
        
        Fn f;
        mutable bool flag;
        mutable std::invoke_result_t<Fn> memo;
        
        constexpr explicit Once_nv(const Fn &f) noexcept: f(f), flag(false) {}
        constexpr explicit Once_nv(Fn &&f) noexcept: f(std::move(f)), flag(false) {}
        
    public:
        constexpr std::invoke_result_t<Fn> operator()() const
        noexcept(noexcept(f())) {
            if (flag) {
                return memo;
            }
            flag = true;
            return memo = f();
        }
    };
    
    template<class Fn>
    requires std::is_void_v<std::invoke_result_t<Fn>>
    class Once_v {
        friend class Once;
        
        Fn f;
        mutable bool flag;
        
        constexpr explicit Once_v(const Fn &f) noexcept: f(f), flag(false) {}
        constexpr explicit Once_v(Fn &&f) noexcept: f(std::move(f)), flag(false) {}
        
    public:
        constexpr void operator()() const noexcept(noexcept(f())) {
            if (flag) {
                return;
            }
            flag = true;
            f();
        }
    };
    
    struct Once {
        template<class F>
        requires std::invocable<F>
        constexpr auto operator()(F &&f) noexcept {
            if constexpr (not std::is_void_v<std::invoke_result_t<F>>) {
                return Once_nv<F>(std::forward<F>(f));
            } else {
                return Once_v<F>(std::forward<F>(f));
            }
        }
    };
}

namespace fff::impl::elem {
    
    template<class Fn_1, class Fn_2>
    class Fconcat {
        Fn_1 f_1;
        Fn_2 f_2;
    public:
        constexpr explicit Fconcat(Fn_1 &&f_1, Fn_2 &&f_2) noexcept
                : f_1(std::move(f_1)), f_2(std::move(f_2)) {}
        constexpr explicit Fconcat(const Fn_1 &f_1, Fn_2 &&f_2) noexcept
                : f_1(f_1), f_2(std::move(f_2)) {}
        constexpr explicit Fconcat(Fn_1 &&f_1, const Fn_2 &f_2) noexcept
                : f_1(std::move(f_1)), f_2(f_2) {}
        constexpr explicit Fconcat(const Fn_1 &f_1, const Fn_2 &f_2) noexcept
                : f_1(f_1), f_2(f_2) {}
    
        template<class... Args>
        requires std::invocable<Fn_1, Args...>
        constexpr auto operator()(Args &&...args) const
        noexcept(noexcept(f_1(std::forward<Args>(args)...)))
        {
            return f_1(std::forward<Args>(args)...);
        }
    
        template<class... Args>
        requires (not std::invocable<Fn_1, Args...> and std::invocable<Fn_2, Args...>)
        constexpr auto operator()(Args &&...args) const
        noexcept(noexcept(f_2(std::forward<Args>(args)...)))
        {
            return f_2(std::forward<Args>(args)...);
        }
    };
    
    struct MakeConcat {
        template<class F1, class F2>
        constexpr auto operator()(F1 &&f1, F2 &&f2) noexcept {
            return Fconcat(std::forward<F1>(f1), std::forward<F2>(f2));
        }
    
        template<class F1, class F2, class... Fp>
        constexpr auto operator()(F1 &&f1, F2 &&f2, Fp &&...fp) noexcept {
            return Fconcat(std::forward<F1>(f1), operator()(std::forward<F2>(f2), std::forward<Fp>(fp)...));
        }
    };
    
}

namespace fff::impl {
    using namespace elem;
    
    /**
     * Making Result-Container function obj.
     * @param cont any std::(container) with type T
     * @param func any function obj with 1 param, say, T -> U
     * @return any std::(container) with type U
     */
    
    struct PreallocCont : public NewDataPolicy {
        template<typename T, class FuncObj, size_t N>
        requires std::invocable<FuncObj, T>
                 and tmf::DefaultConstructible<std::invoke_result_t<FuncObj, T>>
        constexpr auto operator()(const std::array<T, N> &cont, const FuncObj &func) const noexcept {
            return std::array<std::invoke_result_t<FuncObj, T>, N>();
        }
        
        template<template<class> class C, typename T, class FuncObj>
        requires std::ranges::range<C<T>>
                 and std::invocable<FuncObj, T>
                 and tmf::DefaultConstructible<std::invoke_result_t<FuncObj, T>>
        constexpr auto operator()(const C<T> &cont, const FuncObj &func) const noexcept {
            return C<std::invoke_result_t<FuncObj, T>>(cont.size());
        }
    };
    
    struct NewCont : public NewDataPolicy {
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                and tmf::DefaultConstructible<Cont>
        constexpr auto operator()(const Cont &cont, const FuncObj &funcObj) const noexcept {
            return Cont();
        }
    };
    
    struct MapExecution : public ExecutionPolicy {
        /**
         * @todo consider if the return value of func is void
         */
        template<class T_cont, class U_cont, class FuncObj>
        requires std::ranges::range<T_cont>
                and std::ranges::range<U_cont>
                and std::is_same_v<typename std::invoke_result<FuncObj, typename T_cont::value_type>::type,
                                 typename U_cont::value_type>
        constexpr auto &operator()(U_cont &u_cont, T_cont &t_cont, const FuncObj &func) const
        noexcept(noexcept(func(t_cont[0])))
        {
            auto it_t = t_cont.begin();
            auto it_u = u_cont.begin();
            
            while (it_t != t_cont.end()) {
                *it_u = func(*it_t);
                ++it_t; ++it_u;
            }
            
            return u_cont;
        }
    };
    
    struct PushExecution : public ExecutionPolicy {
        template<class T_cont, class FuncObj>
        requires std::ranges::range<T_cont>
                 and std::convertible_to<std::invoke_result_t<FuncObj, typename T_cont::value_type>, bool>
        constexpr auto &operator()(T_cont &res_cont, T_cont &var_cont, const FuncObj &func) const
        noexcept(noexcept(func(var_cont[0])))
        {
            for (const auto &t : var_cont) {
                if (func(t)) {
                    PushPolicy()(res_cont, t);
                }
            }
            
            return res_cont;
        }
        
        struct PushPolicy {
            /**
             * If the container has push_back() method, apply it
             */
            template<template<class> class C, typename T>
            requires tmf::BackPushable<C>
            constexpr void operator()(C<T> &res_cont, const T &val) const noexcept {
                res_cont.push_back(val);
            }
            
            /**
             * If the container has no push_back() method and has insert() method,
             * apply it
             */
            template<template<class> class C, typename T>
            requires (not tmf::BackPushable<C> and tmf::Insertable<C>)
            constexpr void operator()(C<T> &res_cont, const T &val) const noexcept {
                res_cont.insert(val);
            }
        };
    };
    
    struct Each {
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                 and std::invocable<FuncObj, typename Cont::value_type &>
        constexpr void operator()(Cont &cont, const FuncObj &func) const
        noexcept(noexcept(func(cont[0])))
        {
            std::ranges::for_each(cont, func);
        }
    };
    
    struct Map {
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                 and std::invocable<FuncObj, typename Cont::value_type &>
        constexpr auto operator()(const Cont &cont, const FuncObj &func) const
        noexcept(noexcept(func(cont[0])))
        {
            auto ret = PreallocCont()(cont, func);
    
            {
                auto it_t = cont.begin();
                auto it_u = ret.begin();
        
                while (it_t != cont.end()) {
                    *it_u = func(*it_t);
                    ++it_t; ++it_u;
                }
            }
    
            return ret;
        }
    };
    
    struct Filter {
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                 and std::convertible_to<std::invoke_result_t<FuncObj, typename Cont::value_type &>, bool>
        constexpr auto operator()(const Cont &cont, const FuncObj &func) const
        noexcept(noexcept(func(cont[0])))
        {
            auto ret = NewCont()(cont, elem::copy);
            
            for (const auto &v : cont) {
                if (func(v)) {
                    PushPolicy()(ret, v);
                }
            }
            
            return ret;
        }
        
        struct PushPolicy {
            /**
             * If the container has push_back() method, apply it
             */
            template<template<class> class C, typename T>
            requires tmf::BackPushable<C>
            constexpr void operator()(C<T> &res_cont, const T &val) const noexcept {
                res_cont.push_back(val);
            }
        
            /**
             * If the container has no push_back() method and has insert() method,
             * apply it
             */
            template<template<class> class C, typename T>
            requires (not tmf::BackPushable<C> and tmf::Insertable<C>)
            constexpr void operator()(C<T> &res_cont, const T &val) const noexcept {
                res_cont.insert(val);
            }
        };
    };
    
    template<class FuncObj>
    struct FilterWith {
        template<class Cont>
        constexpr auto operator()(Cont &cont) const
        noexcept(noexcept(FuncObj()(cont[0])))
        {
            return Filter()(cont, FuncObj());
        }
    };
    
    struct Reject {
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                 and std::convertible_to<std::invoke_result_t<FuncObj, typename Cont::value_type>, bool>
        constexpr auto operator()(const Cont &cont, const FuncObj &func) const
        noexcept(noexcept(func(cont[0])))
        {
            return Filter()(cont, std::not_fn(func));
        }
    };
    
    template<bool func_ret, bool ret>
    struct LogicMake {
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                 and std::convertible_to<std::invoke_result_t
                         <FuncObj, std::remove_cv_t<typename Cont::value_type &>>, bool>
        constexpr bool operator()(const Cont &cont, const FuncObj &func) const
        noexcept(noexcept(func(cont[0])))
        {
            for (auto &v : cont) {
                if (static_cast<bool>(func(v)) == func_ret) {
                    return ret;
                }
            }
            return not ret;
        }
    };
    
    using Some = LogicMake<true, true>;
    using Every = LogicMake<false, false>;
    using None = LogicMake<true, false>;
    
    template<class ANewDataPolicy, class AnExecutionPolicy>
    requires ANewDataPolicy::is_new_data_policy
             and AnExecutionPolicy::is_execution_policy
    struct Bloop {
        using new_data_policy = ANewDataPolicy;
        using execution_policy = AnExecutionPolicy;
        
        template<class Cont, class FuncObj>
        requires std::ranges::range<Cont>
                 and std::invocable<FuncObj, typename Cont::value_type &>
        constexpr auto operator()(Cont &cont, const FuncObj &func) const {
            
            decltype(auto) ret = ANewDataPolicy()(cont, func);
            
            AnExecutionPolicy()(ret, cont, func);
            
            return ret;
        }
    };
    
    using BloopEach = Bloop<IdentityAt<0>, MapExecution>;
    using BloopMap = Bloop<PreallocCont, MapExecution>;
    using BloopFilter = Bloop<NewCont, PushExecution>;
}

/**
 * The laboratory : New features are tested in this space.
 */
namespace impl::lab {
    /**
     * get class Cont == C<T>, typename T
     * return C<U>
     */
    
    struct Foo {
        template<template<class> class C, typename T>
        constexpr auto operator()(const C<T> &cont) {
            std::cout << typeid(C<T>).name() << ' ' << typeid(T).name() << '\n';
        }
    };
    
    template<class Validator, class FuncObj_T, class FuncObj_F>
    struct Conditional {
        template<typename ...Args>
        requires std::is_same_v<std::invoke_result_t<FuncObj_T, Args...>, std::invoke_result_t<FuncObj_F, Args...>>
        constexpr auto operator()(Args &&...args) const noexcept {
            if (Validator()(std::forward<Args>(args)...)) {
                return FuncObj_T()(std::forward<Args>(args)...);
            } else {
                return FuncObj_F()(std::forward<Args>(args)...);
            }
        }
    };
}

namespace fff {
    inline impl::Each                   each;
    inline impl::Map                    map;
    inline impl::Filter                 filter;
    inline impl::Reject                 reject;
    
    inline impl::Some                   some;
    inline impl::Every                  every;
    inline impl::None                   none;
    
    inline impl::AlwaysPositive         always_positive;
    
    template<std::size_t SZ>
    inline impl::IdentityAt<SZ>         identity_at;
    
    template<std::size_t SZ>
    inline impl::CopyAt<SZ>             copy_at;
    
    inline impl::util::Once             once;
    inline impl::elem::MakeConcat       make_concat;
}

#endif //UNDERSCORE_CPP_FFFFFF_H

/*

template<typename T, template<class,class...> class C, class... Args>
std::ostream& operator<<(std::ostream& os, const C<T,Args...>& objs)
{
    os << __PRETTY_FUNCTION__ << '\n';
    for (auto const& obj : objs)
        os << obj << ' ';
    return os;
}
 
 */