#pragma once

#include <stddef.h>

#define M_IS_DEFINED(macro) _M_IS_DEFINED(macro)
#define _M_IS_DEFINED(macro) (#macro[0] == '1' && #macro[1] == 0)

template <typename T, size_t N>
char(&COUNTOF_REQUIRES_ARRAY_ARGUMENT(T(&)[N]))[N];
#define countof(x) sizeof(COUNTOF_REQUIRES_ARRAY_ARGUMENT(x))

