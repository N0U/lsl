#ifndef LSL_FUNCTION_PTR_H
#define LSL_FUNCTION_PTR_H

#include <boost/extension/shared_library.hpp>
#include <string>
#include <lslutils/debug.h>

namespace LSL {

void _FreeLibrary(void* handle)
{
	if (handle == NULL)
		return;
#ifdef WIN32
	FreeLibrary((HMODULE)handle);
#else
	dlclose(handle);
#endif
}

void* _LoadLibrary(const std::string& libpath)
{
	void * res =NULL;
#ifdef WIN32
	res = LoadLibrary(libpath.c_str());
	if (res == NULL) {
		const int err = GetLastError();
		LslDebug( "UNITSYNC, loading failed %s, nulling handle: %d\n", libpath.c_str(), err );
		LSL_THROW( unitsync, "Couldn't load the unitsync library" );
	}
#else
	res = dlopen(libpath.c_str(), RTLD_LAZY);
	if (res == NULL) {
		const char* err = dlerror();
		LslDebug( "UNITSYNC, loading failed, nulling handle: %s\n", err );
		LSL_THROW( unitsync, "Couldn't load the unitsync library" );
	}
#endif
	return res;
}

void* GetLibFuncPtr( void* libhandle, const std::string& name)
{
	if (libhandle == NULL) return NULL;

#if defined _WIN32
	void* p = (void*)GetProcAddress((HMODULE)libhandle, name.c_str());
#else // defined _WIN32
        void* p = dlsym(libhandle, name.c_str());
#endif // else defined _WIN32

	if ( p == NULL ) {
		LslError( "Couldn't load %s from unitsync library",name.c_str() );
	}
	return p;
}


} // namespace LSL

#endif // LSL_FUNCTION_PTR_H
