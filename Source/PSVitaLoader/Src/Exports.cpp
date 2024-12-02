#define _GNU_SOURCE
#define AL_ALEXT_PROTOTYPES

#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <netdb.h>
#include <utime.h>
#include <pthread.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <SDL2/SDL.h>
#include <xmp.h>
#include <vitaGL.h>

#include "PSVitaLauncherPrivate.h"

// libc and libstdc++ internals
extern "C"
{
	extern void *_ZTIi;
	extern void *_ZTIPKc;
	extern void *_ZTIPc;
	extern void *__aeabi_uidiv;
	extern void *__aeabi_idiv;
	extern void *__aeabi_idivmod;
	extern void *__aeabi_ul2d;
}

// Generic stub that returns 0
static int ret0(void)
{
	return 0;
}

// Linux RTLD_ constants have different values, so we need to wrap dlopen()
static void* wrap_dlopen( const char* name, int flags )
{
	int outflags = 0;
	if ( flags & 0x0100 )
		outflags |= VRTLD_GLOBAL;
	if ( flags & 0x0001 )
		outflags |= VRTLD_LAZY;
	return vrtld_dlopen( name, outflags );
}

// glibc opendir/readdir have a different dirent struct
struct dirent_linux
{
	unsigned int d_ino;
	unsigned int d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[256];
};
static struct dirent_linux* wrap_readdir( DIR* Dirp )
{
	// statically allocating this is allowed by spec I think
	static struct dirent_linux Dent;

	struct dirent* DentNative = readdir( Dirp );
	if ( DentNative == nullptr )
		return nullptr;

	Dent.d_type = SCE_SO_ISDIR( DentNative->d_stat.st_mode ) ? 4 : 8;
	Dent.d_reclen = sizeof(Dent);
	strncpy( Dent.d_name, DentNative->d_name, sizeof(Dent.d_name) );
	Dent.d_name[sizeof(Dent.d_name) - 1] = 0;

	return &Dent;
}

// glibc ctype table
static const unsigned short** fake_ctype_b_loc(void)
{
	static const unsigned short tab[384] =
	{
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
		0x200, 0x320, 0x220, 0x220, 0x220, 0x220, 0x200, 0x200,
		0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
		0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
		0x160, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
		0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
		0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8,
		0x8d8, 0x8d8, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
		0x4c0, 0x8d5, 0x8d5, 0x8d5, 0x8d5, 0x8d5, 0x8d5, 0x8c5,
		0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5,
		0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5,
		0x8c5, 0x8c5, 0x8c5, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
		0x4c0, 0x8d6, 0x8d6, 0x8d6, 0x8d6, 0x8d6, 0x8d6, 0x8c6,
		0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6,
		0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6,
		0x8c6, 0x8c6, 0x8c6, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x200,
	};
	static const unsigned short *tabptr = tab + 128;
	return &tabptr;
}

// we only need this for sockets
static int fake_ioctl( int fd, unsigned long op, const DWORD* arg )
{
	// the only thing that's ever passed into this is FIONBIO
	return setsockopt( fd, SOL_SOCKET, SO_NONBLOCK, (const void*)arg, sizeof(DWORD) );
}

static int* fake_errno_location( void )
{
	return __errno();
}

// our pthread does not allow us to set thread names
static int fake_pthread_setname_np( pthread_t Thread, const char* Name )
{
	return 0;
}

// all the exports that were not caught by GEN_EXPORTS
static const vrtld_export_t GAuxExports[] =
{
	/* libstdc++ internals */
	VRTLD_EXPORT_SYMBOL( _ZTIi ),
	VRTLD_EXPORT_SYMBOL( _ZTIPKc ),
	VRTLD_EXPORT_SYMBOL( _ZTIPc ),
	/* libgcc internals */
	VRTLD_EXPORT_SYMBOL( __aeabi_uidiv ),
	VRTLD_EXPORT_SYMBOL( __aeabi_idiv ),
	VRTLD_EXPORT_SYMBOL( __aeabi_idivmod ),
	VRTLD_EXPORT_SYMBOL( __aeabi_ul2d ),
	/* libc functions */
	VRTLD_EXPORT_SYMBOL( ceilf ),
	VRTLD_EXPORT_SYMBOL( difftime ),
	VRTLD_EXPORT_SYMBOL( ferror ),
	VRTLD_EXPORT_SYMBOL( floorf ),
	VRTLD_EXPORT_SYMBOL( gethostbyname ),
	VRTLD_EXPORT_SYMBOL( gethostname ),
	VRTLD_EXPORT_SYMBOL( getpid ),
	VRTLD_EXPORT_SYMBOL( getpid ),
	VRTLD_EXPORT_SYMBOL( localtime ),
	VRTLD_EXPORT_SYMBOL( qsort ),
	VRTLD_EXPORT_SYMBOL( rand ),
	VRTLD_EXPORT_SYMBOL( srand ),
	VRTLD_EXPORT_SYMBOL( stpcpy ),
	VRTLD_EXPORT_SYMBOL( strcasecmp ),
	VRTLD_EXPORT_SYMBOL( strcasestr ),
	VRTLD_EXPORT_SYMBOL( strncasecmp ),
	VRTLD_EXPORT_SYMBOL( strncat ),
	VRTLD_EXPORT_SYMBOL( strstr ),
	VRTLD_EXPORT_SYMBOL( time ),
	VRTLD_EXPORT_SYMBOL( toupper ),
	VRTLD_EXPORT_SYMBOL( unlink ),
	VRTLD_EXPORT_SYMBOL( utime ),
	VRTLD_EXPORT_SYMBOL( vsprintf ),
	VRTLD_EXPORT_SYMBOL( recvfrom ),
	VRTLD_EXPORT_SYMBOL( getsockopt ),
	VRTLD_EXPORT_SYMBOL( inet_addr ),
	VRTLD_EXPORT_SYMBOL( sendto ),
	VRTLD_EXPORT_SYMBOL( recv ),
	VRTLD_EXPORT_SYMBOL( listen ),
	VRTLD_EXPORT_SYMBOL( bind ),
	VRTLD_EXPORT_SYMBOL( socket ),
	VRTLD_EXPORT_SYMBOL( setsockopt ),
	VRTLD_EXPORT_SYMBOL( select ),
	VRTLD_EXPORT_SYMBOL( connect ),
	VRTLD_EXPORT_SYMBOL( accept ),
	VRTLD_EXPORT_SYMBOL( send ),
	/* SDL2 functions */
	VRTLD_EXPORT_SYMBOL( SDL_GetCPUCount ),
	VRTLD_EXPORT_SYMBOL( SDL_GetKeyboardFocus ),
	VRTLD_EXPORT_SYMBOL( SDL_GetPerformanceCounter ),
	VRTLD_EXPORT_SYMBOL( SDL_GetPerformanceFrequency ),
	VRTLD_EXPORT_SYMBOL( SDL_GetPlatform ),
	VRTLD_EXPORT_SYMBOL( SDL_GetTicks ),
	VRTLD_EXPORT_SYMBOL( SDL_GetBasePath ),
	VRTLD_EXPORT_SYMBOL( SDL_GetClipboardText ),
	VRTLD_EXPORT_SYMBOL( SDL_HasClipboardText ),
	VRTLD_EXPORT_SYMBOL( SDL_SetClipboardText ),
	VRTLD_EXPORT_SYMBOL( SDL_ShowMessageBox ),
	VRTLD_EXPORT_SYMBOL( SDL_ShowSimpleMessageBox ),
	VRTLD_EXPORT_SYMBOL( SDL_PushEvent ),
	VRTLD_EXPORT_SYMBOL( SDL_free ),
	/* OpenAL functions */
	VRTLD_EXPORT_SYMBOL( alAuxiliaryEffectSloti ),
	VRTLD_EXPORT_SYMBOL( alBufferData ),
	VRTLD_EXPORT_SYMBOL( alDeleteBuffers ),
	VRTLD_EXPORT_SYMBOL( alDeleteSources ),
	VRTLD_EXPORT_SYMBOL( alDistanceModel ),
	VRTLD_EXPORT_SYMBOL( alDopplerFactor ),
	VRTLD_EXPORT_SYMBOL( alEffectf ),
	VRTLD_EXPORT_SYMBOL( alEffectfv ),
	VRTLD_EXPORT_SYMBOL( alEffecti ),
	VRTLD_EXPORT_SYMBOL( alGenAuxiliaryEffectSlots ),
	VRTLD_EXPORT_SYMBOL( alGenBuffers ),
	VRTLD_EXPORT_SYMBOL( alGenEffects ),
	VRTLD_EXPORT_SYMBOL( alGenSources ),
	VRTLD_EXPORT_SYMBOL( alGetProcAddress ),
	VRTLD_EXPORT_SYMBOL( alGetSourcei ),
	VRTLD_EXPORT_SYMBOL( alIsBuffer ),
	VRTLD_EXPORT_SYMBOL( alListenerf ),
	VRTLD_EXPORT_SYMBOL( alListenerfv ),
	VRTLD_EXPORT_SYMBOL( alSource3f ),
	VRTLD_EXPORT_SYMBOL( alSource3i ),
	VRTLD_EXPORT_SYMBOL( alSourcePause ),
	VRTLD_EXPORT_SYMBOL( alSourcePlay ),
	VRTLD_EXPORT_SYMBOL( alSourceQueueBuffers ),
	VRTLD_EXPORT_SYMBOL( alSourceUnqueueBuffers ),
	VRTLD_EXPORT_SYMBOL( alSourceStop ),
	VRTLD_EXPORT_SYMBOL( alSourcef ),
	VRTLD_EXPORT_SYMBOL( alSourcefv ),
	VRTLD_EXPORT_SYMBOL( alSourcei ),
	VRTLD_EXPORT_SYMBOL( alcCloseDevice ),
	VRTLD_EXPORT_SYMBOL( alcCreateContext ),
	VRTLD_EXPORT_SYMBOL( alcDestroyContext ),
	VRTLD_EXPORT_SYMBOL( alcGetError ),
	VRTLD_EXPORT_SYMBOL( alcGetProcAddress ),
	VRTLD_EXPORT_SYMBOL( alcMakeContextCurrent ),
	VRTLD_EXPORT_SYMBOL( alcOpenDevice ),
	/* libxmp functions */
	VRTLD_EXPORT_SYMBOL( xmp_create_context ),
	VRTLD_EXPORT_SYMBOL( xmp_end_player ),
	VRTLD_EXPORT_SYMBOL( xmp_free_context ),
	VRTLD_EXPORT_SYMBOL( xmp_load_module_from_memory ),
	VRTLD_EXPORT_SYMBOL( xmp_play_buffer ),
	VRTLD_EXPORT_SYMBOL( xmp_release_module ),
	VRTLD_EXPORT_SYMBOL( xmp_set_player ),
	VRTLD_EXPORT_SYMBOL( xmp_set_position ),
	VRTLD_EXPORT_SYMBOL( xmp_start_player ),
	/* unimplemented/wrapped functions */
	VRTLD_EXPORT( "__libc_start_main", (void *)ret0 ),
	VRTLD_EXPORT( "__isoc99_sscanf", (void *)sscanf ),
	VRTLD_EXPORT( "__ctype_b_loc", (void *)fake_ctype_b_loc ),
	VRTLD_EXPORT( "__errno_location", (void *)fake_errno_location ),
	VRTLD_EXPORT( "ioctl", (void *)fake_ioctl ),
	VRTLD_EXPORT( "pthread_setname_np", (void *)fake_pthread_setname_np ),
	/* libdl functions */
	VRTLD_EXPORT( "dlopen", (void *)wrap_dlopen ),
	VRTLD_EXPORT( "dlclose", (void *)vrtld_dlclose ),
	VRTLD_EXPORT( "dlsym", (void *)vrtld_dlsym ),
	VRTLD_EXPORT( "dlerror", (void *)vrtld_dlerror ),
	VRTLD_EXPORT( "dladdr", (void *)vrtld_dladdr ),
};

/* override exports; prioritized over module exports */
static const vrtld_export_t GOverrideExports[] =
{
	VRTLD_EXPORT( "readdir", (void *)wrap_readdir ),
};

const vrtld_export_t* __vrtld_exports = GAuxExports;
const size_t __vrtld_num_exports = sizeof(GAuxExports) / sizeof(vrtld_export_t);

const vrtld_export_t* __vrtld_override_exports = GOverrideExports;
const size_t __vrtld_num_override_exports = sizeof(GOverrideExports) / sizeof(vrtld_export_t);
