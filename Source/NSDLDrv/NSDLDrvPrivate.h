#include "Engine.h"
#include "SDL.h"

/*-----------------------------------------------------------------------------
	UNSDLViewport.
-----------------------------------------------------------------------------*/

void DLL_EXPORT InitWindowing();

extern DLL_EXPORT UBOOL GTickDue;

//
// A SDL2 viewport.
//
class DLL_EXPORT UNSDLViewport : public UViewport
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT( UNSDLViewport, UViewport, CLASS_Transient )
	NO_DEFAULT_CONSTRUCTOR( UNSDLViewport )

	// Static variables.
	static BYTE KeyMap[SDL_NUM_SCANCODES]; // SDL_Scancode -> EInputKey map
	static const BYTE MouseButtonMap[6]; // SDL_BUTTON_ -> EInputKey map

	// Variables.
	class UNSDLClient* Client;
	SDL_Window* hWnd;
	SDL_Renderer* SDLRen; // for accelerated SoftDrv
	SDL_Texture* SDLTex; // for use with the above renderer
	DWORD SDLTexFormat;
	SDL_GLContext GLCtx; // for OpenGLDrv
	UBOOL Destroyed;
	INT DisplayIndex;
	SDL_Rect DisplaySize;

	// Info saved during captures and fullscreen sessions.
	INT SavedX, SavedY;

	// Constructors.
	UNSDLViewport( ULevel* InLevel, UNSDLClient* InClient );
	static void InternalClassInitializer( UClass* Class );

	// UObject interface.
	virtual void Destroy() override;

	// UViewport interface.
	virtual UBOOL Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData=NULL, INT* HitSize=0 ) override;
	virtual void Unlock( UBOOL Blit ) override;
	virtual UBOOL Exec( const char* Cmd, FOutputDevice* Out ) override;
	virtual void Repaint() override;
	virtual void SetModeCursor() override;
	virtual void UpdateWindow() override;
	virtual void OpenWindow( void* ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY ) override;
	virtual void CloseWindow() override;
	virtual void UpdateInput( UBOOL Reset ) override;
	virtual void MakeCurrent() override;
	virtual void MakeFullscreen( INT NewX, INT NewY, UBOOL UpdateProfile ) override;
	virtual void* GetWindow() override;
	virtual void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly ) override;

	// UNSDLViewport interface.
	static void InitKeyMap();
	void SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile );
	void EndFullscreen();
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
	UBOOL TickInput(); // returns true if the viewport has requested death
};

/*-----------------------------------------------------------------------------
	UNSDLClient.
-----------------------------------------------------------------------------*/

//
// SDL2 implementation of the client.
//
class DLL_EXPORT UNSDLClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS_WITHOUT_CONSTRUCT( UNSDLClient, UClient, CLASS_Transient|CLASS_Config )

	// Configuration.
	INT DefaultDisplay;
	UBOOL StartupFullscreen;
	UBOOL UseJoystick;
	UBOOL DeadZoneXYZ;
	UBOOL DeadZoneRUV;
	UBOOL InvertVertical;
	FLOAT ScaleXYZ;
	FLOAT ScaleRUV;

	// Variables.
	SDL_GameController* Controller;
	SDL_DisplayMode DefaultDisplayMode;
	TArray<SDL_Rect> DisplayResolutions;

	// Constructors.
	UNSDLClient();
	static void InternalClassInitializer( UClass* Class );

	// UObject interface.
	virtual void Destroy() override;
	virtual void PostEditChange() override;
	virtual void ShutdownAfterError() override;

	// UClient interface.
	virtual void Init( UEngine* InEngine ) override;
	virtual void ShowViewportWindows( DWORD ShowFlags, int DoShow ) override;
	virtual void EnableViewportWindows( DWORD ShowFlags, int DoEnable ) override;
	virtual void Poll() override;
	virtual UViewport* CurrentViewport() override;
	virtual UBOOL Exec( const char* Cmd, FOutputDevice* Out=GSystem ) override;
	virtual void Tick() override;
	virtual UViewport* NewViewport( class ULevel* InLevel, const FName Name ) override;
	virtual void EndFullscreen() override;

	// UNSDLClient interface.
	void TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen );
	const TArray<SDL_Rect>& GetDisplayResolutions();
};
