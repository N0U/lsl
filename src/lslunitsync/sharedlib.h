#ifndef LSL_FUNCTION_PTR_H
#define LSL_FUNCTION_PTR_H

#include <boost/extension/shared_library.hpp>
#include <string>
#include <lslutils/debug.h>

namespace LSL {


//! common loading point for functions from a given library handle
void _FreeLibrary(void* handle);
void* _LoadLibrary(const std::string& libpath);

void* GetLibFuncPtr( void* libhandle, const std::string& name);

} // namespace LSL

#endif // LSL_FUNCTION_PTR_H
