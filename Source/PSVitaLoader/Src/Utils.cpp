#include <vitaGL.h>
#include "PSVitaLauncherPrivate.h"

static char GError[4096];

static int MsgDialogInit( const char *msg )
{
	SceMsgDialogUserMessageParam msg_param;
	memset( &msg_param, 0, sizeof(msg_param) );
	msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	msg_param.msg = (SceChar8 *)msg;

	SceMsgDialogParam param;
	sceMsgDialogParamInit( &param );
	_sceCommonDialogSetMagicNumber( &param.commonParam );
	param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	param.userMsgParam = &msg_param;

	return sceMsgDialogInit( &param );
}

static bool MsgDialogFinished()
{
	if ( sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED )
		return false;
	sceMsgDialogTerm();
	return true;
}

static void ShowMsg( const char* Text )
{
	vglInit( 0 );
	MsgDialogInit( Text );
	while ( !MsgDialogFinished() )
		vglSwapBuffers( GL_TRUE );
}

void Logf( const char* Fmt, ... )
{
	va_list va;
	va_start( va, Fmt );
	vfprintf( stderr, Fmt, va );
	fprintf( stderr, "\n" );
	va_end( va );
}

void FatalError( const char* Fmt, ... )
{
	va_list va;
	FILE* f;
	char Path[MAX_PATH];

	va_start( va, Fmt );
	vsnprintf( GError, sizeof(GError), Fmt, va );
	va_end( va );

	Logf( "FATAL ERROR: %s", GError );

	snprintf( Path, sizeof(Path), "%sfatal.log", GRootPath );
	f = fopen( Path, "w" );
	if ( f )
	{
		fprintf( f, "FATAL ERROR: %s\n", GError );
		fclose( f );
	}

	ShowMsg( GError );

	vrtld_quit();

	sceKernelExitProcess( 0 );
	abort();
}
