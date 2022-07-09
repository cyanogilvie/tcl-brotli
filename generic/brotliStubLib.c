#undef USE_TCL_STUBS
#undef USE_BROTLI_STUBS
#define USE_TCL_STUBS 1
#define USE_BROTLI_STUBS 1

#include "brotli.h"

MODULE_SCOPE const BrotliStubs*	brotliStubsPtr;
const BrotliStubs*					brotliStubsPtr = NULL;

#undef BrotliInitializeStubs
MODULE_SCOPE const char* BrotliInitializeStubs(Tcl_Interp* interp)
{
	const char*	got = NULL;
	//fprintf(stderr, "In BrotliInitializeStubs, verion: (%s)\n", PACKAGE_VERSION);
	got = Tcl_PkgRequireEx(interp, PACKAGE_NAME, PACKAGE_VERSION, 0, &brotliStubsPtr);
	//fprintf(stderr, "Got ver: (%s), brotliStubsPtr: %p\n", got, brotliStubsPtr);
	return got;
}
