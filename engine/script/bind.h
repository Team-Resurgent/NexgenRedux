#pragma once

#include <angelscript.h>

#include <new>
#include <utility>

// Bindings are written once and work with either calling convention. Native calls are faster but
// need support for the platform in AngelScript; generic calls (NX_SCRIPT_GENERIC) work anywhere, so
// they are what a new platform starts on.
namespace nx::bind
{
    template <typename T>
    struct Proxy
    {
        T value;
    };

    template <typename T>
    Proxy<T>& arg(asIScriptGeneric* gen, asUINT index)
    {
        return *static_cast<Proxy<T>*>(gen->GetAddressOfArg(index));
    }

    template <typename R, typename Call>
    void finish(asIScriptGeneric* gen, Call&& call)
    {
        if constexpr (std::is_void_v<R>)
        {
            call();
        }
        else
        {
            new (gen->GetAddressOfReturnLocation()) Proxy<R>{ call() };
        }
    }

    // The bound object, as whatever the function's last parameter wants.
    template <typename T>
    struct Object;

    template <typename T>
    struct Object<T*>
    {
        static T* get(asIScriptGeneric* gen) { return static_cast<T*>(gen->GetObject()); }
    };

    template <typename T>
    struct Object<T&>
    {
        static T& get(asIScriptGeneric* gen) { return *static_cast<T*>(gen->GetObject()); }
    };

    template <auto Fn, typename Signature>
    struct Function;

    template <auto Fn, typename R, typename... A>
    struct Function<Fn, R (*)(A...)>
    {
        template <size_t... I>
        static void expand(asIScriptGeneric* gen, std::index_sequence<I...>)
        {
            (void)gen;
            finish<R>(gen, [&]() -> R { return Fn(static_cast<A>(arg<A>(gen, I).value)...); });
        }

        static void call(asIScriptGeneric* gen) { expand(gen, std::index_sequence_for<A...>()); }

        // All but the last parameter come from the script; the last is the object.
        template <size_t Index, typename P>
        static P pick(asIScriptGeneric* gen)
        {
            if constexpr (Index + 1 == sizeof...(A))
            {
                return Object<P>::get(gen);
            }
            else
            {
                return static_cast<P>(arg<P>(gen, Index).value);
            }
        }

        template <size_t... I>
        static void expandObjectLast(asIScriptGeneric* gen, std::index_sequence<I...>)
        {
            finish<R>(gen, [&]() -> R { return Fn(pick<I, A>(gen)...); });
        }

        static void callObjectLast(asIScriptGeneric* gen) { expandObjectLast(gen, std::index_sequence_for<A...>()); }
    };

    // T is the type registered with the script engine; the method may belong to a base of it.
    template <typename T, auto Fn, typename Signature>
    struct Method;

    template <typename T, auto Fn, typename R, typename... A>
    struct MethodCall
    {
        template <size_t... I>
        static void expand(asIScriptGeneric* gen, std::index_sequence<I...>)
        {
            finish<R>(gen, [&]() -> R { return (static_cast<T*>(gen->GetObject())->*Fn)(static_cast<A>(arg<A>(gen, I).value)...); });
        }

        static void call(asIScriptGeneric* gen) { expand(gen, std::index_sequence_for<A...>()); }
    };

    template <typename T, auto Fn, typename B, typename R, typename... A>
    struct Method<T, Fn, R (B::*)(A...)> : MethodCall<T, Fn, R, A...>
    {
    };

    template <typename T, auto Fn, typename B, typename R, typename... A>
    struct Method<T, Fn, R (B::*)(A...) const> : MethodCall<T, Fn, R, A...>
    {
    };

    template <typename Signature>
    struct Constructor;

    template <typename T, typename... A>
    struct Constructor<T(A...)>
    {
        template <size_t... I>
        static void expand(asIScriptGeneric* gen, std::index_sequence<I...>)
        {
            new (gen->GetObject()) T(static_cast<A>(arg<A>(gen, I).value)...);
        }

        static void call(asIScriptGeneric* gen) { expand(gen, std::index_sequence_for<A...>()); }

        static void native(A... args, void* memory) { new (memory) T(args...); }
    };

    // Converts through the registered type first, so a method of a second base class carries the
    // offset its object pointer needs.
    template <typename T, typename B, typename R, typename... A>
    asSFuncPtr methodPointer(R (B::*fn)(A...))
    {
        R (T::*derived)(A...) = fn;
        return asSMethodPtr<sizeof(void (T::*)())>::Convert(derived);
    }

    template <typename T, typename B, typename R, typename... A>
    asSFuncPtr methodPointer(R (B::*fn)(A...) const)
    {
        R (T::*derived)(A...) const = fn;
        return asSMethodPtr<sizeof(void (T::*)())>::Convert(derived);
    }
}

// Each expands to the function and calling convention arguments of an AngelScript Register call.
#if defined(NX_SCRIPT_GENERIC)

#define NX_FN(fn) asFUNCTION((::nx::bind::Function<&fn, decltype(&fn)>::call)), asCALL_GENERIC
#define NX_OBJ_LAST(fn) asFUNCTION((::nx::bind::Function<&fn, decltype(&fn)>::callObjectLast)), asCALL_GENERIC
#define NX_METHOD(type, fn) asFUNCTION((::nx::bind::Method<type, &type::fn, decltype(&type::fn)>::call)), asCALL_GENERIC
#define NX_METHOD_PR(type, fn, params, ret) \
    asFUNCTION((::nx::bind::Method<type, static_cast<ret(type::*) params>(&type::fn), ret(type::*) params>::call)), asCALL_GENERIC
#define NX_CONSTRUCTOR(type, params) asFUNCTION((::nx::bind::Constructor<type params>::call)), asCALL_GENERIC

#else

#define NX_FN(fn) asFUNCTION(fn), asCALL_CDECL
#define NX_OBJ_LAST(fn) asFUNCTION(fn), asCALL_CDECL_OBJLAST
#define NX_METHOD(type, fn) ::nx::bind::methodPointer<type>(&type::fn), asCALL_THISCALL
#define NX_METHOD_PR(type, fn, params, ret) ::nx::bind::methodPointer<type>(static_cast<ret(type::*) params>(&type::fn)), asCALL_THISCALL
#define NX_CONSTRUCTOR(type, params) asFUNCTION((::nx::bind::Constructor<type params>::native)), asCALL_CDECL_OBJLAST

#endif
