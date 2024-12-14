#include <exception>
#include <typeinfo>
#include <stdarg.h>
#include <kubridge.h>
#include <vitaGL.h>

#include "PSVitaLauncherPrivate.h"

typedef int (*unreal_main_fn)(int argc, const char **argv);

// 200MB libc heap, 512K main thread stack, 16MB for loading game DLLs
// the rest goes to vitaGL
extern "C" { SceUInt32 sceUserMainThreadStackSize = 512 * 1024; }
extern "C" { unsigned int _pthread_stack_default_user = 512 * 1024; }
extern "C" { unsigned int _newlib_heap_size_user = 200 * 1024 * 1024; }
#define VGL_MEM_THRESHOLD ( 16 * 1024 * 1024 )

char GRootPath[MAX_PATH] = "app0:/";

void* GCoreElf = nullptr;
void* GEngineElf = nullptr;
void* GMainElf = nullptr;

int GMainArgc = 1;
char GMainArgvData[MAX_ARGV_NUM][MAX_PATH];
const char* GMainArgv[MAX_ARGV_NUM];

static bool FindRootPath( char* Out, int OutLen )
{
	static const char *Drives[] = { "uma0", "imc0", "ux0" };

	// check if an unreal folder exists on one of the drives
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

int main( int argc, const char** argv )
{
	sceTouchSetSamplingState( SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_STOP );
	scePowerSetArmClockFrequency( 444 );
	scePowerSetBusClockFrequency( 222 );
	scePowerSetGpuClockFrequency( 222 );
	scePowerSetGpuXbarClockFrequency( 166 );
	sceSysmoduleLoadModule( SCE_SYSMODULE_NET );

	if ( vrtld_init( VRTLD_TARGET2_IS_GOT ) < 0 )
		FatalError( "Could not init vrtld:\n%s", vrtld_dlerror() );

	if ( !FindRootPath( GRootPath, sizeof(GRootPath) ) )
		FatalError( "Could not find Unreal directory" );

	if ( chdir( GRootPath ) < 0 )
		FatalError( "Could not chdir to\n%s", GRootPath );

	Logf( "root directory: `%s`", GRootPath );

	// first open both the dep libs without initializing or relocating them

	GCoreElf = vrtld_dlopen( "Core.so", VRTLD_GLOBAL | VRTLD_LAZY );
	if ( !GCoreElf )
		FatalError( "Could not load Core.so:\n%s", vrtld_dlerror() );

	GEngineElf = vrtld_dlopen( "Engine.so", VRTLD_GLOBAL | VRTLD_LAZY );
	if ( !GEngineElf )
		FatalError( "Could not load Core.so:\n%s", vrtld_dlerror() );

	// then force vrtld to reloc and init the libs
	vrtld_dlsym( GCoreElf, "?" );
	vrtld_dlsym( GEngineElf, "?" );
	vrtld_dlerror();

	GMainElf = vrtld_dlopen( "Unreal.bin", VRTLD_GLOBAL | VRTLD_NOW );
	if ( !GMainElf )
		FatalError( "Could not load Unreal.bin:\n%s", vrtld_dlerror() );

	unreal_main_fn pmain = (unreal_main_fn)vrtld_dlsym( GMainElf, "main" );
	if ( !pmain )
		FatalError( "Could not find main() in Unreal.bin:\n%s", vrtld_dlerror() );

	vglInitWithCustomThreshold( 0, 960, 544, VGL_MEM_THRESHOLD, 0, 0, 0, SCE_GXM_MULTISAMPLE_2X );

	GMainArgc = 1;
	snprintf( GMainArgvData[0], sizeof(*GMainArgvData), "%sUnreal.bin", GRootPath );
	GMainArgv[0] = GMainArgvData[0];

	Logf( "entering main with %d args", GMainArgc );

	try
	{
		pmain( GMainArgc, GMainArgv );
	}
	catch ( const char* Err )
	{
		FatalError( "Unhandled exception:\n%s", Err );
	}
	catch ( char* Err )
	{
		FatalError( "Unhandled exception:\n%s", Err );
	}
	catch ( INT Err )
	{
		FatalError( "Unhandled exception:\n%d", Err );
	}

	Logf( "exited main, shutting down" );

	vrtld_quit();
	sceKernelExitProcess( 0 );

	return 0;
}
