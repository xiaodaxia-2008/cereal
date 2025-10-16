#pragma once
#define _PARENS ()
#define _EXPAND(...) _EXPAND3(_EXPAND3(_EXPAND3(_EXPAND3(__VA_ARGS__))))
#define _EXPAND3(...) _EXPAND2(_EXPAND2(_EXPAND2(_EXPAND2(__VA_ARGS__))))
#define _EXPAND2(...) _EXPAND1(_EXPAND1(_EXPAND1(_EXPAND1(__VA_ARGS__))))
#define _EXPAND1(...) __VA_ARGS__

#ifndef FOR_EACH
#define FOR_EACH(macro, ...) \
    __VA_OPT__(_EXPAND(_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define _FOR_EACH_HELPER(macro, a1, ...) \
    macro(a1) __VA_OPT__(, _FOR_EACH_AGAIN _PARENS(macro, __VA_ARGS__))
#define _FOR_EACH_AGAIN() _FOR_EACH_HELPER
#endif

#define FOR_EACH2(macro, arg0, ...) \
    __VA_OPT__(_EXPAND(_FOR_EACH_HELPER2(macro, arg0, __VA_ARGS__)))
#define _FOR_EACH_HELPER2(macro, arg0, a1, ...) \
    macro(arg0, a1) __VA_OPT__(_FOR_EACH_AGAIN2 _PARENS(macro, arg0, __VA_ARGS__))
#define _FOR_EACH_AGAIN2() _FOR_EACH_HELPER2