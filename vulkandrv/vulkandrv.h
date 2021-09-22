#pragma once
#include "Engine.h"
#include "UnRender.h"
//#include "vulkan_device.h"

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#define CLAMP(p,min,max)	{ if(p < min) p = min; else if (p>max) p = max; }

/** Options, some user configurable */
struct VulkanOptions
{
	int samples; /**< Number of MSAA samples */
	int VSync; /**< VSync on/off */
	int aniso; /**< Anisotropic filtering levels */
	int LODBias; /**< Mipmap LOD bias */
	int POM; /**< Parallax occlusion mapping */
	int bumpMapping; /**<Bumpmapping */
	int alphaToCoverage; /**< Alpha to coverage support */
	int classicLighting; /**< Lighting that matches old renderers */
	int simulateMultipassTexturing; /**< Simulate look of multi-pass world texturing */
};

class UVulkanRenderDevice: public URenderDevice
{

//UObject glue
#if (UNREALTOURNAMENT || RUNE || NERF)
DECLARE_CLASS(UVulkanRenderDevice,URenderDevice,CLASS_Config,VulkanDrv)
#else
DECLARE_CLASS(UVulkanRenderDevice,URenderDevice,CLASS_Config)
#endif

private:
	VulkanOptions VulkanOptions;
	/** User configurable options */
	struct
	{
		int precache; /**< Turn on precaching */
		int autoFOV; /**< Turn on auto field of view setting */
		int FPSLimit; /**< 60FPS frame limiter */
		int unlimitedViewDistance; /**< Set frustum to max map size */
	} options;

public:
	UVulkanRenderDevice();

	/**@name Helpers */
	//@{	
	static void debugs(char *s);
	int getOption(TCHAR* name,int defaultVal, bool isBool);
	//@}
	
	/**@name Abstract in parent class */
	//@{	
	virtual UBOOL Init(UViewport *InViewport,INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	virtual UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	virtual void Exit();
	#if UNREALGOLD
	virtual void Flush();	
	#else
	virtual void Flush(UBOOL AllowPrecache);
	#endif
	virtual void Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize );
	virtual void Unlock(UBOOL Blit );
	virtual void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet );
	virtual void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span );
	virtual void DrawTile( FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags );
	virtual void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 );
	virtual void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z );
	virtual void ClearZ( FSceneNode* Frame );
	virtual void PushHit( const BYTE* Data, INT Count );
	virtual void PopHit( INT Count, UBOOL bForce );
	virtual void GetStats( TCHAR* Result );
	virtual void ReadPixels( FColor* Pixels );
	//@}

	/**@name Optional but implemented*/
	//@{
	virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	virtual void SetSceneNode( FSceneNode* Frame );
	virtual void PrecacheTexture( FTextureInfo& Info, DWORD PolyFlags );
	virtual void EndFlash();
	void StaticConstructor();
	//@}

	#if (RUNE)
	/**@name Rune fog*/
	//@{
	virtual void DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf);
	virtual void PreDrawGouraud(FSceneNode *Frame, FLOAT FogDistance, FPlane FogColor);
	virtual void PostDrawGouraud(FLOAT FogDistance);
	//@}
	#endif
};