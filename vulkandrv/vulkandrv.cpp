#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define USE_VULKAN_IMPLEMENTATION
#include "vulkan_common.h"

#include <stdio.h>
#include <io.h>
#include <FCNTL.H>
#include <stdlib.h>     // for _itow 
#include <new>

//#include "resource.h"
#include "vulkandrv.h"
#include "utils/logging.h"
//#include "texconverter.h"
//#include "customflags.h"
//#include "misc.h"
//#include "vertexformats.h"
//#include "shader_gouraudpolygon.h"
//#include "shader_tile.h"
//#include "shader_complexsurface.h"
//#include "shader_fogsurface.h"

#include "vulkan_rhi.h"
bool vulkan_initialize(HWND rw_handle);

//UObject glue
IMPLEMENT_PACKAGE(VulkanDrv);
IMPLEMENT_CLASS(UVulkanRenderDevice);

static bool drawingWeapon = false; /** Whether the depth buffer was cleared and projection parameters set to draw the weapon model */
static bool drawingHUD = false;
static int customFOV = 0; /**Field of view calculated from aspect ratio */
static bool firstFrameOfLevel = false;
/** See SetSceneNode() */
static float zNear = 0.5f; //Default for the games is 1, but results in cut-off UT weapons with widescreen FOVs
static float zFar = 1.0f;
static LARGE_INTEGER perfCounterFreq;
//static TextureCache *textureCache;
//static TexConverter *texConverter;
//static Shader_GouraudPolygon *shader_GouraudPolygon;
//static Shader_Tile *shader_Tile;
//static Shader_ComplexSurface *shader_ComplexSurface;
//static Shader_FogSurface *shader_FogSurface;

static IRHIDevice* g_vulkan_device = 0;

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
	log_info("constructing vulkan render device...");
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
	//stdout->_file = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE),_O_TEXT);
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
	log_info("Initializing Vulkan render device.");

	//Set parent class params
	URenderDevice::SpanBased = 0;
	URenderDevice::FullscreenOnly = 0;
	URenderDevice::SupportsFogMaps = 1;
	URenderDevice::SupportsTC = 1;
	URenderDevice::SupportsDistanceFog = 0;
	URenderDevice::SupportsLazyTextures = 0;

	//Get/set config options.
	options.precache = getOption(L"Precache",0,true);
	VulkanOptions.samples = getOption(L"Antialiasing",4,false);
	VulkanOptions.aniso = getOption(L"Anisotropy",8,false);
	VulkanOptions.VSync = getOption(L"VSync",1,true);	
	VulkanOptions.POM = getOption(L"ParallaxOcclusionMapping",0,true);	
	VulkanOptions.LODBias = getOption(L"LODBias",0,false);
	VulkanOptions.bumpMapping = getOption(L"BumpMapping",0,true);	
	VulkanOptions.classicLighting = getOption(L"ClassicLighting",1,true);	
	VulkanOptions.alphaToCoverage = getOption(L"AlphaToCoverage",0,true);
	options.autoFOV = getOption(L"AutoFOV",1,true);
	options.FPSLimit = getOption(L"FPSLimit",100,false);
	VulkanOptions.simulateMultipassTexturing = getOption(L"simulateMultipassTexturing",1,true);
	options.unlimitedViewDistance = getOption(L"unlimitedViewDistance",0,true);

	if(options.unlimitedViewDistance)
		zFar = 65536.0f;
	else
		zFar = 32760.0f;
	 
	//Set parent options
	URenderDevice::Viewport = InViewport;

	//Do some nice compatibility fixing: set processor affinity to single-cpu
	SetProcessAffinityMask(GetCurrentProcess(),0x1);
#if USE_GLAD_LOADER
	int glad_vk_version = gladLoaderLoadVulkan(NULL, NULL, NULL);
    if (!glad_vk_version) {
        log_error("gladLoad Failure: Unable to load Vulkan symbols!");
    }
	else
	{
		log_info("Init: glad vulkan load succeded, version: %d", glad_vk_version);
	}
#endif

	// TODO: pass VulkanOptions
	if(!vulkan_initialize((HWND)InViewport->GetWindow()))
	{
		GError->Log(L"Init: Initializing vulkan failed.");
		return 0;
	}

	g_vulkan_device = create_device();
	assert(g_vulkan_device);

	return 1;
	
#if 0	
	if(!UD3D10RenderDevice::SetRes(NewX,NewY,NewColorBytes,Fullscreen))
	{
		GError->Log(L"Init: SetRes failed.");
		return 0;
	}

	textureCache= new (std::nothrow) TextureCache(D3D::getDevice());
	if(!textureCache)
	{
		GError->Log(L"Error allocating texture cache.");
		return 0;
	}

	texConverter = new (std::nothrow) TexConverter(textureCache);
	if(!texConverter)
	{
		GError->Log(L"Error allocating texture converter.");
		return 0;
	}

	shader_GouraudPolygon = static_cast<Shader_GouraudPolygon*>(D3D::getShader(D3D::SHADER_GOURAUDPOLYGON));
	shader_Tile = static_cast<Shader_Tile*>(D3D::getShader(D3D::SHADER_TILE));
	shader_ComplexSurface = static_cast<Shader_ComplexSurface*>(D3D::getShader(D3D::SHADER_COMPLEXSURFACE));
	shader_FogSurface = static_cast<Shader_FogSurface*>(D3D::getShader(D3D::SHADER_FOGSURFACE));

	//Brightness
	float brightness;
	GConfig->GetFloat(L"WinDrv.WindowsClient",L"Brightness",brightness);
	D3D::setBrightness(brightness);

	URenderDevice::PrecacheOnFlip = 1; //Turned on to immediately recache on init (prevents lack of textures after fullscreen switch)

	QueryPerformanceFrequency(&perfCounterFreq); //Init performance counter frequency.
	
	return 1;
#endif
}

UBOOL UVulkanRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	return 0;
}

void UVulkanRenderDevice::Exit()
{
	delete g_vulkan_device;
	vulkan_finalize();
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

/**
Clear screen and depth buffer, prepare buffers to receive data.
\param FlashScale To do with flash effects, see notes.
\param FlashFog To do with flash effects, see notes.
\param ScreenClear The color with which to clear the screen. Used for Rune fog.
\param RenderLockFlags Signify whether the screen should be cleared. Depth buffer should always be cleared.
\param InHitData Something to do with clipping planes; safe to ignore.
\param InHitSize Something to do with clipping planes; safe to ignore.

\note 'Flash' effects are fullscreen colorization, for example when the player is underwater (blue) or being hit (red).
Depending on the values of the related parameters (see source code) this should be drawn; the games don't always send a blank flash when none should be drawn.
EndFlash() ends this, but other renderers actually save the parameters and start drawing it there so it gets blended with the final scene.
\note RenderLockFlags aren't always properly set, this results in for example glitching in the Unreal castle flyover, in the wall of the tower with the Nali on it.
*/
void UVulkanRenderDevice::Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
	float deltaTime;
	static LARGE_INTEGER oldTime;
	LARGE_INTEGER time;
	if(oldTime.QuadPart==0)
		QueryPerformanceCounter(&oldTime); //Initial time

	
	QueryPerformanceCounter(&time);
	deltaTime  =  (time.QuadPart-oldTime.QuadPart) / (float)perfCounterFreq.QuadPart;
	
	//FPS limiter
	if(options.FPSLimit > 0)
	{		
		while(deltaTime<(float)1/options.FPSLimit) //Busy wait for max accuracy
		{
			QueryPerformanceCounter(&time);
			deltaTime  =  (time.QuadPart-oldTime.QuadPart) / (float)perfCounterFreq.QuadPart;
		}		
	}
	oldTime.QuadPart = time.QuadPart;	
	

	//If needed, set new field of view; the game resets this on level switches etc. Can't be done in config as Unreal doesn't support this.
	if(options.autoFOV && Viewport->Actor->DefaultFOV!=customFOV)
	{		
		TCHAR buf[8]=L"fov ";
		_itow_s(customFOV,&buf[4],4,10);
		Viewport->Actor->DefaultFOV=customFOV; //Do this so the value is set even if FOV settings don't take effect (multiplayer mode) 
		URenderDevice::Viewport->Exec(buf,*GLog); //And this so the FOV change actually happens				
	}

	if (!g_vulkan_device->BeginFrame())
	{
		log_error("BeginFrame failed");
	}

}
void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	if (!g_vulkan_device->Present())
	{
		log_error("Present failed");
	}

	if (!g_vulkan_device->EndFrame())
	{
		log_error("EndFrame failed");
	}
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
