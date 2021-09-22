#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h> // mbstowcs_s
#include <io.h>
#include <FCNTL.H>

#include <new>

//#include "resource.h"
#include "vulkandrv.h"
//#include "texconverter.h"
//#include "customflags.h"
//#include "misc.h"
//#include "vertexformats.h"
//#include "shader_gouraudpolygon.h"
//#include "shader_tile.h"
//#include "shader_complexsurface.h"
//#include "shader_fogsurface.h"

//UObject glue
IMPLEMENT_PACKAGE(VulkanDrv);
IMPLEMENT_CLASS(UVulkanRenderDevice);

void UVulkanRenderDevice::debugs(char* s)
{
	WCHAR buf[255];
	size_t n;
	mbstowcs_s(&n,buf,255,s,254);
	GLog->Log(buf);
	#ifdef _DEBUG //In debug mode, print output to console
	puts(s);
	#endif

}

/**
Attempts to read a property from the game's config file; on failure, a default is written (so it can be changed by the user) and returned.
\param name A string identifying the config file options.
\param defaultVal The default value to write and return if the option is not found.
\param isBool Whether the parameter's a boolean or integer
\return The value for the property.
\note The default value is written so it can be user modified (either from the config or preferences window) from then on.
*/
int UVulkanRenderDevice::getOption(TCHAR* name,int defaultVal, bool isBool)
{
	TCHAR* Section = L"VulkanDrv.VulkanRenderDevice";
	int out;
	if(isBool)
	{
		if(!GConfig->GetBool( Section, name, (INT&) out))
		{
			GConfig->SetBool(Section,name,defaultVal);
			out = defaultVal;
		}
	}
	else
	{
		if(!GConfig->GetInt( Section, name, (INT&) out))
		{
			GConfig->SetInt(Section,name,defaultVal);
			out = defaultVal;
		}
	}
	return out;
}

UVulkanRenderDevice::UVulkanRenderDevice()
{
	debugs("Initializing vulkan driver...");
}

/**
Constructor called by the game when the renderer is first created.
\note Required to compile for Unreal Tournament. 
\note Binding settings to the preferences window needs to done here instead of in init() or the game crashes when starting a map if the renderer's been restarted at least once.
*/
void UVulkanRenderDevice::StaticConstructor()
{
	//Make the property appear in the preferences window; this will automatically pick up the current value and write back changes.	
	new(GetClass(), L"Precache", RF_Public) UBoolProperty(CPP_PROPERTY(options.precache), TEXT("Options"), CPF_Config);
	new(GetClass(), L"Antialiasing", RF_Public) UIntProperty(CPP_PROPERTY(VulkanOptions.samples), TEXT("Options"), CPF_Config);
	new(GetClass(), L"Anisotropy", RF_Public) UIntProperty(CPP_PROPERTY(VulkanOptions.aniso), TEXT("Options"), CPF_Config);
	new(GetClass(), L"VSync", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.VSync), TEXT("Options"), CPF_Config);
	new(GetClass(), L"ParallaxOcclusionMapping", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.POM), TEXT("Options"), CPF_Config);
	new(GetClass(), L"LODBias", RF_Public) UIntProperty(CPP_PROPERTY(VulkanOptions.LODBias), TEXT("Options"), CPF_Config);
	new(GetClass(), L"AlphaToCoverage", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.alphaToCoverage), TEXT("Options"), CPF_Config);
	new(GetClass(), L"BumpMapping", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.bumpMapping), TEXT("Options"), CPF_Config);
	new(GetClass(), L"ClassicLighting", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.classicLighting), TEXT("Options"), CPF_Config);
	new(GetClass(), L"AutoFOV", RF_Public) UBoolProperty(CPP_PROPERTY(options.autoFOV), TEXT("Options"), CPF_Config);
	new(GetClass(), L"FPSLimit", RF_Public) UIntProperty(CPP_PROPERTY(options.FPSLimit), TEXT("Options"), CPF_Config);
	new(GetClass(), L"SimulateMultiPassTexturing", RF_Public) UBoolProperty(CPP_PROPERTY(VulkanOptions.simulateMultipassTexturing), TEXT("Options"), CPF_Config);
	new(GetClass(), L"UnlimitedViewDistance", RF_Public) UBoolProperty(CPP_PROPERTY(options.unlimitedViewDistance), TEXT("Options"), CPF_Config);

	//Turn on parent class options by default. If done here (instead of in Init()), the ingame preferences still work
	getOption(L"Coronas",1,true);
	getOption(L"HighDetailActors",1,true);
	getOption(L"VolumetricLighting",1,true);
	getOption(L"ShinySurfaces",1,true);
	getOption(L"DetailTextures",1,true);

	//Create a console to print debug stuff to.
	#ifdef _DEBUG
	AllocConsole();
	stdout->_file = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE),_O_TEXT);
	#endif
}

/**
Initialization of renderer.
- Set parent class options. Some of these are settings for the renderer to heed, others control what the game does.
	- URenderDevice::SpanBased; Probably for software renderers.
	- URenderDevice::Fullscreen; Only for Voodoo cards.
	- URenderDevice::SupportsTC; Game sends compressed textures if present.
	- URenderDevice::SupportsDistanceFog; Distance fog. Don't know how this is supposed to be implemented.
	- URenderDevice::SupportsLazyTextures; Renderer loads and unloads texture info when needed (???).
	- URenderDevice::PrefersDeferredLoad; Renderer prefers not to cache textures in advance (???).
	- URenderDevice::ShinySurfaces; Renderer supports detail textures. The game sends them always, so it's meant as a detail setting for the renderer.
	- URenderDevice::Coronas; If enabled, the game draws light coronas.
	- URenderDevice::HighDetailActors; If enabled, game sends more detailed models (???).
	- URenderDevice::VolumetricLighting; If enabled, the game sets fog textures for surfaces if needed.
	- URenderDevice::PrecacheOnFlip; The game will call the PrecacheTexture() function to load textures in advance. Also see Flush().
	- URenderDevice::Viewport; Always set to InViewport.
- Initialize graphics api.
- Resize buffers (convenient to use SetRes() for this).

\param InViewport viewport parameters, can get the window handle.
\param NewX Viewport width.
\param NewY Viewport height.
\param NewColorBytes Color depth.
\param Fullscreen Whether fullscreen mode should be used.
\return 1 if init succesful. On 0, game errors out.

\note D3D10 renderer ignores color depth.
*/
UBOOL UVulkanRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	UVulkanRenderDevice::debugs("Initializing Vulkan renderer.");
	return 0;
}

UBOOL UVulkanRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	return 0;
}

void UVulkanRenderDevice::Exit()
{
}


#if UNREALGOLD
void UVulkanRenderDevice::Flush()
{
}
#else
void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
}
#endif

void UVulkanRenderDevice::Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
}
void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
}
void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
}
void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
}
void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
}
void UVulkanRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
}
void UVulkanRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
}
void UVulkanRenderDevice::ClearZ(FSceneNode* Frame)
{
}
void UVulkanRenderDevice::PushHit(const BYTE* Data, INT Count)
{
}
void UVulkanRenderDevice::PopHit(INT Count, UBOOL bForce)
{
}
void UVulkanRenderDevice::GetStats(TCHAR* Result)
{
}
void UVulkanRenderDevice::ReadPixels(FColor* Pixels)
{
}

/* Optional but implemented */

UBOOL UVulkanRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	return 0;
}

void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
}

void UVulkanRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
}
void UVulkanRenderDevice::EndFlash()
{
}

#if (RUNE)
void UVulkanRenderDevice::DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf)
{
}
void UVulkanRenderDevice::PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor)
{
}
void UVulkanRenderDevice::PostDrawGouraud(FLOAT FogDistance)
{
}
#endif
