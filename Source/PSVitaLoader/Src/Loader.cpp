#include <exception>
#include <typeinfo>
#include <stdarg.h>
#include <kubridge.h>
#include <vitaGL.h>

#include "PSVitaLauncherPrivate.h"

typedef int (*unreal_main_fn)(int argc, const char **argv);

// 200MB libc heap, 512K main thread stack, 8MB for loading game DLLs
// the rest goes to vitaGL
extern "C" { SceUInt32 sceUserMainThreadStackSize = 512 * 1024; }
extern "C" { unsigned int _pthread_stack_default_user = 512 * 1024; }
extern "C" { unsigned int _newlib_heap_size_user = 200 * 1024 * 1024; }
#define VGL_MEM_THRESHOLD ( 8 * 1024 * 1024 )

char GRootPath[MAX_PATH] = "app0:/";

void* GCoreElf = nullptr;
void* GEngineElf = nullptr;
void* GMainElf = nullptr;

int GMainArgc = 1;
char GMainArgvData[MAX_ARGV_NUM][MAX_PATH];
const char* GMainArgv[MAX_ARGV_NUM];

static inline void Logf( const char* Fmt, ... )
{
	va_list va;
	va_start( va, Fmt );
	vfprintf( stderr, Fmt, va );
	fprintf( stderr, "\n" );
	va_end( va );
}

static void __attribute__((noreturn)) FatalError( const char* Fmt, ... )
{
	va_list va;
	FILE* f;
	char Error[1024];
	char Path[MAX_PATH];

	va_start( va, Fmt );
	vsnprintf( Error, sizeof(Error), Fmt, va );
	va_end( va );

	Logf( "FATAL ERROR: %s", Error );

	snprintf( Path, sizeof(Path), "%sfatal.log", GRootPath );
	f = fopen( Path, "w" );
	if ( f )
	{
		fprintf( f, "FATAL ERROR: %s\n", Error );
		fclose( f );
	}

	vrtld_quit();

	abort();
}

static bool FindRootPath( char* Out, int OutLen )
{
	static const char *Drives[] = { "uma0", "imc0", "ux0" };

	// check if a maxpayne folder exists on one of the drives
	// default to the last one (ux0)
	for ( unsigned int i = 0; i < sizeof(Drives) / sizeof(*Drives); ++i )
	{
		snprintf( Out, OutLen, "%s:/" DATA_PATH, Drives[i] );
		SceUID Dir = sceIoDopen( Out );
		if ( Dir >= 0 )
		{
			sceIoDclose( Dir );
			return true;
		}
	}

	// not found
	return false;
}

// we can't catch exceptions that originate from the shared libs, presumably because __dso_handle is different,
// so instead we catch and rethrow them in the terminate handler to at least see what was thrown
static void __attribute__((noreturn)) TerminateHandler()
{
	std::exception_ptr ExPtr = std::current_exception();
	const auto* ExType = ExPtr.__cxa_exception_type();

	Logf( "unhandled exception of type %s, rethrowing", ExType ? ExType->name() : "???" );

	try
	{
		std::rethrow_exception( ExPtr );
	}
	catch ( const char* Err )
	{
		FatalError( "unhandled exception: %s", Err );
	}
	catch ( char* Err )
	{
		FatalError( "unhandled exception: %s", Err );
	}
	catch ( INT Err )
	{
		FatalError( "unhandled exception: %d", Err );
	}

	abort();
}

int main( int argc, const char** argv )
{
	sceTouchSetSamplingState( SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_STOP );
	scePowerSetArmClockFrequency( 444 );
	scePowerSetBusClockFrequency( 222 );
	scePowerSetGpuClockFrequency( 222 );
	scePowerSetGpuXbarClockFrequency( 166 );
	sceSysmoduleLoadModule( SCE_SYSMODULE_NET );

	if ( vrtld_init( 0 ) < 0 )
		FatalError( "could not init vrtld: %s", vrtld_dlerror() );

	vrtld_set_main_exports( GAuxExports, GNumAuxExports );

	if ( !FindRootPath( GRootPath, sizeof(GRootPath) ) )
		FatalError( "could not find Unreal directory" );

	if ( chdir( GRootPath ) < 0 )
		FatalError( "could not chdir to  '%s'", GRootPath );

	Logf( "root directory: `%s`", GRootPath );

	// first open both the dep libs without initializing or relocating them

	GCoreElf = vrtld_dlopen( "Core.so", VRTLD_GLOBAL | VRTLD_LAZY );
	if ( !GCoreElf )
		FatalError( "could not load Core.so: %s", vrtld_dlerror() );

	GEngineElf = vrtld_dlopen( "Engine.so", VRTLD_GLOBAL | VRTLD_LAZY );
	if ( !GEngineElf )
		FatalError( "could not load Core.so: %s", vrtld_dlerror() );

	// then force vrtld to reloc and init the libs

	vrtld_dlsym( GCoreElf, "?" );
	vrtld_dlsym( GEngineElf, "?" );
	vrtld_dlerror();

	GMainElf = vrtld_dlopen( "Unreal.bin", VRTLD_GLOBAL | VRTLD_NOW );
	if ( !GMainElf )
		FatalError( "could not load Unreal.bin: %s", vrtld_dlerror() );

	unreal_main_fn pmain = (unreal_main_fn)vrtld_dlsym( GMainElf, "main" );
	if ( !pmain )
		FatalError( "could not find main() in Unreal.bin: vrtld_dlerror()" );

	vglInitWithCustomThreshold( 0, 960, 544, VGL_MEM_THRESHOLD, 0, 0, 0, SCE_GXM_MULTISAMPLE_NONE );

	std::set_terminate( TerminateHandler );

	GMainArgc = 1;
	snprintf( GMainArgvData[0], sizeof(*GMainArgvData), "%sUnreal.bin", GRootPath );
	GMainArgv[0] = GMainArgvData[0];

	Logf( "entering main with %d args", GMainArgc );
	pmain( GMainArgc, GMainArgv );

	vrtld_quit();
	return 0;
}
