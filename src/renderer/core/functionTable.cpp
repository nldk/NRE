// nreGetFunctionTable(): the entry point renderer_dynamic.h's loader looks up with dlsym / GetProcAddress.
// Assigning &NRE::name to each pointer also checks renderer_functions.inl against the headers:
// a signature that doesn't match is a compile error here.

#include "renderer_dynamic.h"

namespace {
    const NRE::FunctionTable table = {
        sizeof(NRE::FunctionTable),
        NRE_VERSION_MAJOR,
        NRE_VERSION_MINOR,
        NRE_VERSION_PATCH,
#ifdef NRE_UNSAFE_API
        1,
#else
        0,
#endif
#define NRE_FUNCTION(ret, name, params, args) &NRE::name,
#define NRE_UNSAFE_FUNCTION(ret, name, params, args) &NRE::Unsafe::name,
#include "renderer_functions.inl"
#undef NRE_FUNCTION
#undef NRE_UNSAFE_FUNCTION
    };
}

extern "C" RENDERER_API const NRE::FunctionTable* nreGetFunctionTable() {
    return &table;
}
