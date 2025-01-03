#include "SDL2/SDL.h"
#ifndef PSP
#include "glad.h"
#else
#include <GL/gl.h>
#define glColorTableEXT glColorTable
void glActiveTexture(int a) { };
#endif
#include "NOpenGLDrvPrivate.h"

/*-----------------------------------------------------------------------------
	Global implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(NOpenGLDrv);
IMPLEMENT_CLASS(UNOpenGLRenderDevice);

/*-----------------------------------------------------------------------------
	UNOpenGLRenderDevice implementation.
-----------------------------------------------------------------------------*/

// from XOpenGLDrv:
// PF_Masked requires index 0 to be transparent, but is set on the polygon instead of the texture,
// so we potentially need two copies of any palettized texture in the cache
// unlike in newer unreal versions the low cache bits are actually used, so we have use one of the
// actually unused higher bits for this purpose, thereby breaking 64-bit compatibility for now
#define MASKED_TEXTURE_TAG (1ULL << 60)

// FColor is adjusted for endianness
#define ALPHA_MASK 0xff000000

// lightmaps are 0-127
#define LIGHTMAP_SCALE 2

// and it also would be nice to overbright them
#define LIGHTMAP_OVERBRIGHT 1.4f
#ifndef PSP
#define GL_CHECK_EXT(ext) GLAD_GL_ ## ext
#define GL_CHECK_VER(maj, min) (((maj) * 10 + (min)) <= (GLVersion.major * 10 + GLVersion.minor))
#endif
void UNOpenGLRenderDevice::InternalClassInitializer( UClass* Class )
{
	guardSlow(UNOpenGLRenderDevice::InternalClassInitializer);
	new(Class, "NoFiltering",  RF_Public)UBoolProperty( CPP_PROPERTY(NoFiltering),  "Options", CPF_Config );
	new(Class, "UseHwPalette", RF_Public)UBoolProperty( CPP_PROPERTY(UseHwPalette), "Options", CPF_Config );
	new(Class, "UseBGRA",      RF_Public)UBoolProperty( CPP_PROPERTY(UseBGRA),      "Options", CPF_Config );
	unguardSlow;
}

UNOpenGLRenderDevice::UNOpenGLRenderDevice()
{
	NoFiltering = false;
	UseHwPalette = true;
	UseBGRA = true;
}

UBOOL UNOpenGLRenderDevice::Init( UViewport* InViewport )
{
	guard(UNOpenGLRenderDevice::Init)
#ifndef PSP
	if( !gladLoadGLLoader( &SDL_GL_GetProcAddress ) )
	{
		debugf( NAME_Warning, "Could not load GL: %s", SDL_GetError() );
		return false;
	}
#else/*
	int ar = 1;
	char* argv[1] = {"Unreal"};
	glutInit(&ar, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);  
	glutInitWindowSize(480, 272); 
	glutCreateWindow("Unreal");*/
#endif
	SupportsFogMaps = true;
	SupportsDistanceFog = true;
#ifndef PSP
	if( UseHwPalette && !GL_CHECK_EXT( EXT_paletted_texture ) )
	{
		debugf( NAME_Warning, "EXT_paletted_texture not available, disabling UseHwPalette" );
#endif
		UseHwPalette = false;
#ifndef PSP
	}
#endif
#ifndef PSP
	if( UseBGRA && !GL_CHECK_VER( 1, 2 ) && !GL_CHECK_EXT( EXT_bgra ) )
	{
#endif
		debugf( NAME_Warning, "EXT_bgra not available, disabling UseBGRA" );
		UseBGRA = false;
#ifndef PSP
	}

	debugf( NAME_Log, "Got OpenGL %d.%d", GLVersion.major, GLVersion.minor );
#endif
	ComposeSize = 256 * 256 * 4;
	Compose = (BYTE*)appMalloc( ComposeSize, "GLComposeBuf" );
	verify( Compose );

	// Set modelview matrix to flip stuff into our coordinate system.
	const FLOAT Matrix[16] =
	{
		+1, +0, +0, +0,
		+0, -1, +0, +0,
		+0, +0, -1, +0,
		+0, +0, +0, +1,
	};
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glMultMatrixf( Matrix );

	// Set permanent state.
	glEnable( GL_DEPTH_TEST );
	glShadeModel( GL_SMOOTH );
	glAlphaFunc( GL_GREATER, 0.5 );
	glDisable( GL_ALPHA_TEST );
	glDepthMask( GL_TRUE );
	glBlendFunc( GL_ONE, GL_ZERO );
	glEnable( GL_BLEND );
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	CurrentPolyFlags = PF_Occlude;
	Viewport = InViewport;

	return true;
	unguard;
}

void UNOpenGLRenderDevice::Exit()
{
	guard(UNOpenGLRenderDevice::Exit);

	debugf( NAME_Log, "Shutting down OpenGL renderer" );

	Flush();

	if( Compose )
	{
		appFree( Compose );
		Compose = NULL;
	}
	ComposeSize = 0;

	unguard;
}

void UNOpenGLRenderDevice::Flush()
{
	guard(UNOpenGLRenderDevice::Flush);

	if( TexAlloc.Num() )
	{
		debugf( NAME_Log, "Flushing %d/%d textures", TexAlloc.Num(), BindMap.Size() );
		ResetTexture( 0 );
		ResetTexture( 1 );
		ResetTexture( 2 );
		glFinish();
		glDeleteTextures( TexAlloc.Num(), &TexAlloc(0) );
		TexAlloc.Empty();
		BindMap.Empty();
	}

	unguard;
}

UBOOL UNOpenGLRenderDevice::Exec( const char* Cmd, FOutputDevice* Out )
{
	return false;
}

void UNOpenGLRenderDevice::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
{
	guard(UNOpenGLRenderDevice::Lock);

	glClearColor( 1.f, ScreenClear.Y, ScreenClear.Z, ScreenClear.W );
	glClearDepth( 1.0 );
	glDepthFunc( GL_LEQUAL );
	SetBlend( PF_Occlude );

	GLbitfield ClearBits = GL_DEPTH_BUFFER_BIT;
	if( RenderLockFlags & LOCKR_ClearScreen )
		ClearBits |= GL_COLOR_BUFFER_BIT;
	glClear( ClearBits );

	if( FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f) )
		ColorMod = FPlane( FlashFog.X, FlashFog.Y, FlashFog.Z, 1.f - Min( FlashScale.X * 2.f, 1.f ) );
	else
		ColorMod = FPlane( 0.f, 0.f, 0.f, 0.f );

	unguard;
}

void UNOpenGLRenderDevice::Unlock( UBOOL Blit )
{
	guard(UNOpenGLRenderDevice::Unlock);

	glFlush();

	unguard;
}

void UNOpenGLRenderDevice::DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
{
	guard(UNOpenGLRenderDevice::DrawComplexSurface);

	check(Surface.Texture);

	SetSceneNode( Frame );

	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	// Draw texture.
	SetBlend( Surface.PolyFlags );
	SetTexture( 0, *Surface.Texture, ( Surface.PolyFlags & PF_Masked ), 0.f );
	glColor4f( 1.f, 1.f, 1.f, 1.f );
	for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
	{
		glBegin( GL_TRIANGLE_FAN );
		for( INT i = 0; i < Poly->NumPts; i++ )
		{
			FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
			glTexCoord2f( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
			glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
		}
		glEnd();
	}

	// Draw lightmap.
	if( Surface.LightMap )
	{
		SetBlend( PF_Modulated );
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_EQUAL );
		SetTexture( 0, *Surface.LightMap, 0, -0.5 );
		glColor4f( 1.f, 1.f, 1.f, 1.f );
		for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
		{
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < Poly->NumPts; i++ )
			{
				FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
				FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
				glTexCoord2f( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
				glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
			}
			glEnd();
		}
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_LEQUAL );
	}

	// Draw fog.
	if( Surface.FogMap )
	{
		SetBlend( PF_Highlighted );
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_EQUAL );
		SetTexture( 0, *Surface.FogMap, 0, -0.5 );
		for( FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next )
		{
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < Poly->NumPts; i++ )
			{
				FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
				FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
				glTexCoord2f( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
				glVertex3f( Poly->Pts[i]->Point.X, Poly->Pts[i]->Point.Y, Poly->Pts[i]->Point.Z );
			}
			glEnd();
		}
		if( Surface.PolyFlags & PF_Masked )
			glDepthFunc( GL_LEQUAL );
	}

	unguard;
}

void UNOpenGLRenderDevice::DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )
{
		guard(UNOpenGLRenderDevice::DrawGouraudPolygon);

		SetSceneNode( Frame );
		SetBlend( PolyFlags );
		SetTexture( 0, Texture, ( PolyFlags & PF_Masked ), 0 );

		const UBOOL IsModulated = ( PolyFlags & PF_Modulated );

		if( IsModulated )
			glColor4f( 1.f, 1.f, 1.f, 1.f );

		glBegin( GL_TRIANGLE_FAN  );
		for( INT i=0; i<NumPts; i++ )
		{
			FTransTexture* P = Pts[i];
			if( !IsModulated )
				glColor4f( P->Light.X, P->Light.Y, P->Light.Z, 1.f );
			glTexCoord2f( P->U*TexInfo[0].UMult, P->V*TexInfo[0].VMult );
			glVertex3f( P->Point.X, P->Point.Y, P->Point.Z );
		}
		glEnd();

		if( (PolyFlags & (PF_RenderFog|PF_Translucent|PF_Modulated)) == PF_RenderFog )
		{
			ResetTexture( 0 );
			SetBlend( PF_Highlighted );
			glBegin( GL_TRIANGLE_FAN );
			for( INT i = 0; i < NumPts; i++ )
			{
				FTransTexture* P = Pts[i];
				glColor4f( P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W );
				glVertex3f( P->Point.X, P->Point.Y, P->Point.Z );
			}
			glEnd();
		}

		unguard;
}

void UNOpenGLRenderDevice::DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags )
{
	guard(UNOpenGLRenderDevice::DrawTile);

	SetSceneNode( Frame );
	SetBlend( PolyFlags );
	SetTexture( 0, Texture, ( PolyFlags & PF_Masked ), 0.f );

	if( PolyFlags & PF_Modulated )
		glColor4f( 1.f, 1.f, 1.f, 1.f );
	else
		glColor4f( Light.X, Light.Y, Light.Z, 1.f );

	glBegin( GL_TRIANGLE_FAN );
		glTexCoord2f( (U   )*TexInfo[0].UMult, (V   )*TexInfo[0].VMult );
		glVertex3f( RFX2*Z*(X   -Frame->FX2), RFY2*Z*(Y   -Frame->FY2), Z );
		glTexCoord2f( (U+UL)*TexInfo[0].UMult, (V   )*TexInfo[0].VMult );
		glVertex3f( RFX2*Z*(X+XL-Frame->FX2), RFY2*Z*(Y   -Frame->FY2), Z );
		glTexCoord2f( (U+UL)*TexInfo[0].UMult, (V+VL)*TexInfo[0].VMult );
		glVertex3f( RFX2*Z*(X+XL-Frame->FX2), RFY2*Z*(Y+YL-Frame->FY2), Z );
		glTexCoord2f( (U   )*TexInfo[0].UMult, (V+VL)*TexInfo[0].VMult );
		glVertex3f( RFX2*Z*(X   -Frame->FX2), RFY2*Z*(Y+YL-Frame->FY2), Z );
	glEnd();

	unguard;
}

void UNOpenGLRenderDevice::Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
{

}

void UNOpenGLRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2 )
{

}

void UNOpenGLRenderDevice::EndFlash( )
{
	guard(UNOpenGLESRenderDevice::EndFlash);

	if( ColorMod == FPlane( 0.f, 0.f, 0.f, 0.f ) )
		return;

	ResetTexture( 0 );
	ResetTexture( 1 );
	ResetTexture( 2 );
	SetBlend( PF_Highlighted );

	const FLOAT Z = 1.f;
	const FLOAT RFX2 = RProjZ;
	const FLOAT RFY2 = RProjZ * Aspect;

	glDisable( GL_DEPTH_TEST );

	glColor4fv( &ColorMod.R );
	glBegin( GL_TRIANGLE_FAN );
		glVertex3f( RFX2 * -Z, RFY2 * -Z, Z );
		glVertex3f( RFX2 * +Z, RFY2 * -Z, Z );
		glVertex3f( RFX2 * +Z, RFY2 * +Z, Z );
		glVertex3f( RFX2 * -Z, RFY2 * +Z, Z );
	glEnd();

	glEnable( GL_DEPTH_TEST );

	unguard;
}

void UNOpenGLRenderDevice::PushHit( const BYTE* Data, INT Count )
{

}

void UNOpenGLRenderDevice::PopHit( INT Count, UBOOL bForce )
{

}

void UNOpenGLRenderDevice::GetStats( char* Result )
{
	guard(UNOpenGLRenderDevice::GetStats)

	if( Result ) *Result = '\0';

	unguard;
}

void UNOpenGLRenderDevice::ReadPixels( FColor* Pixels )
{
	guard(UNOpenGLRenderDevice::ReadPixels);

	glPixelStorei( GL_UNPACK_ALIGNMENT, 0 );
	glReadPixels( 0, 0, Viewport->SizeX, Viewport->SizeY, GL_BGRA, GL_UNSIGNED_BYTE, (void*)Pixels );

	unguard;
}

void UNOpenGLRenderDevice::ClearZ( FSceneNode* Frame )
{
	guard(UNOpenGLRenderDevice::ClearZ);

	SetBlend( PF_Occlude );
	glClear( GL_DEPTH_BUFFER_BIT );

	unguard;
}

void UNOpenGLRenderDevice::SetSceneNode( FSceneNode* Frame )
{
	guard(UNOpenGLRenderDevice::SetSceneNode);

	check(Viewport);

	if( !Frame )
	{
		// invalidate current saved data
		CurrentSceneNode.X = -1;
		CurrentSceneNode.FX = -1.f;
		CurrentSceneNode.SizeX = -1;
		return;
	}

	if( Frame->X != CurrentSceneNode.X || Frame->Y != CurrentSceneNode.Y ||
			Frame->XB != CurrentSceneNode.XB || Frame->YB != CurrentSceneNode.YB ||
			Viewport->SizeX != CurrentSceneNode.SizeX || Viewport->SizeY != CurrentSceneNode.SizeY )
	{
		glViewport( Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y );
		CurrentSceneNode.X = Frame->X;
		CurrentSceneNode.Y = Frame->Y;
		CurrentSceneNode.XB = Frame->XB;
		CurrentSceneNode.YB = Frame->YB;
		CurrentSceneNode.SizeX = Viewport->SizeX;
		CurrentSceneNode.SizeY = Viewport->SizeY;
	}

	if( Frame->FX != CurrentSceneNode.FX || Frame->FY != CurrentSceneNode.FY ||
			Viewport->Actor->FovAngle != CurrentSceneNode.FovAngle )
	{
		RProjZ = appTan( Viewport->Actor->FovAngle * PI / 360.0 );
		Aspect = Frame->FY / Frame->FX;
		RFX2 = 2.0f * RProjZ / Frame->FX;
		RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glFrustum( -RProjZ, +RProjZ, -Aspect * RProjZ, +Aspect * RProjZ, 1.0, 32768.0 );
		CurrentSceneNode.FX = Frame->FX;
		CurrentSceneNode.FY = Frame->FY;
		CurrentSceneNode.FovAngle = Viewport->Actor->FovAngle;
	}

	unguard;
}

void UNOpenGLRenderDevice::SetBlend( DWORD PolyFlags, UBOOL InverseOrder )
{
	guard(UNOpenGLRenderDevice::SetBlend);

	// Adjust PolyFlags according to Unreal's precedence rules.
	if( !(PolyFlags & (PF_Translucent|PF_Modulated)) )
		PolyFlags |= PF_Occlude;
	else if( PolyFlags & PF_Translucent )
		PolyFlags &= ~PF_Masked;

	// Detect changes in the blending modes.
	DWORD Xor = CurrentPolyFlags ^ PolyFlags;
	if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted) )
	{
		if( Xor&(PF_Translucent|PF_Modulated|PF_Highlighted) )
		{
			glEnable( GL_BLEND );
			if( PolyFlags & PF_Translucent )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
			}
			else if( PolyFlags & PF_Modulated )
			{
				glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			}
			else if( PolyFlags & PF_Highlighted )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				glDisable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ZERO );
			}
		}
		if( Xor & PF_Invisible )
		{
			UBOOL Show = !( PolyFlags & PF_Invisible );
			glColorMask( Show, Show, Show, Show );
		}
		if( Xor & PF_Occlude )
		{
			glDepthMask( (PolyFlags & PF_Occlude) != 0 );
		}
		if( Xor & PF_Masked )
		{
			if( PolyFlags & PF_Masked )
				glEnable( GL_ALPHA_TEST );
			else
				glDisable( GL_ALPHA_TEST );
		}
	}

	CurrentPolyFlags = PolyFlags;

	unguard;
}

void UNOpenGLRenderDevice::ResetTexture( INT TMU )
{
	guard(UNOpenGLRenderDevice::ResetTexture);

	if( TexInfo[TMU].CurrentCacheID != 0 )
	{
		glActiveTexture( GL_TEXTURE0 + TMU );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glDisable( GL_TEXTURE_2D );
		TexInfo[TMU].CurrentCacheID = 0;
	}

	unguard;
}

void UNOpenGLRenderDevice::SetTexture( INT TMU, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias )
{
	guard(UNOpenGLRenderDevice::SetTexture);

	// Set panning.
	FTexInfo& Tex = TexInfo[TMU];
	Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
	Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

	// Account for all the impact on scale normalization.
	Tex.UMult = 1.f / (Info.UScale * static_cast<FLOAT>(Info.USize));
	Tex.VMult = 1.f / (Info.VScale * static_cast<FLOAT>(Info.VSize));

	// Find in cache.
	QWORD NewCacheID = Info.CacheID;
	if( ( PolyFlags & PF_Masked ) && Info.Palette )
		NewCacheID |= MASKED_TEXTURE_TAG;
	UBOOL RealtimeChanged = ( Info.TextureFlags & TF_RealtimeChanged );
	if( NewCacheID == Tex.CurrentCacheID && !RealtimeChanged )
		return;

	// Make current.
	Tex.CurrentCacheID = NewCacheID;
	FCachedTexture* Bind = BindMap.Find( NewCacheID );
	FCachedTexture* OldBind = Bind;
	if( !Bind )
	{
		// New texture.
		Bind = BindMap.Add( NewCacheID, FCachedTexture() );
		glGenTextures( 1, &Bind->Id );
		TexAlloc.AddItem( Bind->Id );
	}

	glActiveTexture( GL_TEXTURE0 + TMU );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, Bind->Id );

	if( !OldBind || RealtimeChanged )
	{
		// New texture or it has changed, upload it.
		Info.TextureFlags &= ~TF_RealtimeChanged;
		UploadTexture( Info, ( PolyFlags & PF_Masked ), !OldBind );
		// Set mip filtering if there are mips.
		if( ( PolyFlags & PF_NoSmooth ) || ( NoFiltering && Info.Palette ) ) // TODO: This is set per poly, not per texture.
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( Info.NumMips > 1 ) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		}
		else
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ( Info.NumMips > 1 ) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		}
	}

	unguard;
}

void UNOpenGLRenderDevice::EnsureComposeSize( const DWORD NewSize )
{
	if( NewSize > ComposeSize )
	{
		Compose = (BYTE*)appRealloc( Compose, NewSize, "GLComposeBuf" );
	}
	verify( Compose );
}

void UNOpenGLRenderDevice::ConvertTextureMipI8( const FMipmap* Mip, const FColor* Palette, const UBOOL Masked, BYTE*& UploadBuf, GLenum& UploadFormat, GLenum& InternalFormat )
{
	// 8-bit indexed. We have to fix the alpha component since it's mostly garbage.
	DWORD i;
	if( UseHwPalette )
	{
		// GL has support for palettized textures, use it. Still have to fix the alpha.
		DWORD* DstPal = (DWORD*)Compose;
		const DWORD* SrcPal = (const DWORD*)Palette;
		EnsureComposeSize( 256 * 4 );
		UploadBuf = Mip->DataPtr;
		InternalFormat = GL_COLOR_INDEX8_EXT;
		UploadFormat = GL_COLOR_INDEX8_EXT;
		i = 0;
		// index 0 is transparent in masked textures
		if( Masked )
		{
			*DstPal++ = 0;
			++i;
		}
		// 255 alpha on the rest of the palette
		for( ; i < 256; ++i )
			*DstPal++ = *SrcPal++ | ALPHA_MASK;
		// set palette pointer
		glColorTableEXT( GL_TEXTURE_2D, GL_RGBA8, 256, GL_RGBA, GL_UNSIGNED_BYTE, (const void*)Compose );
	}
	else
	{
		// No support for palettized textures. Expand to RGBA8888 and fix alpha.
		DWORD* Dst = (DWORD*)Compose;
		const BYTE* Src = (const BYTE*)Mip->DataPtr;
		const DWORD* Pal = (const DWORD*)Palette;
		const DWORD Count = Mip->USize * Mip->VSize;
		EnsureComposeSize( Count * 4 );
		UploadBuf = Compose;
		UploadFormat = GL_RGBA;
		InternalFormat = GL_RGBA8;
		if( Masked )
		{
			// index 0 is transparent
			for( i = 0; i < Count; ++i, ++Src )
				*Dst++ = *Src ? ( Pal[*Src] | ALPHA_MASK ) : 0;
		}
		else
		{
			// index 0 is whatever
			for( i = 0; i < Count; ++i )
				*Dst++ = ( Pal[*Src++] | ALPHA_MASK );
		}
	}
}

void UNOpenGLRenderDevice::ConvertTextureMipBGRA7777( const FMipmap* Mip, BYTE*& UploadBuf, GLenum& UploadFormat, GLenum& InternalFormat )
{
	// BGRA8888. This is actually a BGRA7777 lightmap, so we need to scale it.
	BYTE* Dst = (BYTE*)Compose;
	const BYTE* Src = (const BYTE*)Mip->DataPtr;
	const DWORD Count = Mip->USize * Mip->VSize;
	EnsureComposeSize( Count * 4 );
	UploadBuf = Compose;
	InternalFormat = GL_RGBA8;
	if( UseBGRA )
	{
		UploadFormat = GL_BGRA;
		for( DWORD i = 0; i < Count; ++i )
		{
			*Dst++ = (*Src++) << 1;
			*Dst++ = (*Src++) << 1;
			*Dst++ = (*Src++) << 1;
			*Dst++ = (*Src++) << 1;
		}
	}
	else
	{
		// Swap BGRA -> RGBA
		UploadFormat = GL_RGBA;
		for( DWORD i = 0; i < Count; ++i, Src += 4 )
		{
			*Dst++ = Src[2] << 1;
			*Dst++ = Src[1] << 1;
			*Dst++ = Src[0] << 1;
			*Dst++ = Src[3] << 1;
		}
	}
}
void UNOpenGLRenderDevice::UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture )
{
	guard(UNOpenGLRenderDevice::UploadTexture);

	if( !Info.Mips[0] )
	{
		debugf( NAME_Warning, "Encountered texture with invalid mips!" );
		return;
	}

	// Upload all mips.
	for( INT MipIndex = 0; MipIndex < Info.NumMips; ++MipIndex )
	{
		const FMipmap* Mip = Info.Mips[MipIndex];
		BYTE* UploadBuf;
		GLenum UploadFormat;
		GLenum InternalFormat;
		if( !Mip || !Mip->DataPtr )
			break;
		// Convert texture if needed.
		if( Info.Palette )
			ConvertTextureMipI8( Mip, Info.Palette, Masked, UploadBuf, UploadFormat, InternalFormat );
		else
			ConvertTextureMipBGRA7777( Mip, UploadBuf, UploadFormat, InternalFormat );
		// Upload to GL.
		// if( NewTexture )
			glTexImage2D( GL_TEXTURE_2D, MipIndex, UploadFormat, Mip->USize, Mip->VSize, 0, UploadFormat, GL_UNSIGNED_BYTE, (void*)UploadBuf );
		// else
			// glTexSubImage2D( GL_TEXTURE_2D, MipIndex, 0, 0, Mip->USize, Mip->VSize, UploadFormat, GL_UNSIGNED_BYTE, (void*)UploadBuf );
	}

	unguard;
}
