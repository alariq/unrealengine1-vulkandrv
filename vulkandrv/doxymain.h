/** \mainpage Deus Ex Direct3D 10 driver
	Marijn Kentie 2009 \n

	Welcome to the D3D10 renderer documentation. Simplicity and documentation were a major focus; I hope I have been succesful in those regards.
 
	\section layout Project layout (also see 'project layout.pdf')
	- d3d10drv.cpp is the renderer interface, implements the URenderDevice subclass UD3D10RenderDevice. The documentation for this class explains the game<->renderer interface.
	- d3d.cpp deals with graphics API. D3D class. Initialization, setting up shaders, creating texture objects, etc, etc. Implements the texture cache.
	Exposes various API-neutral structures with which the renderer interface can pass data.
	- texconversion.cpp is the glue that prepares Unreal textures to be saved in the D3D texture cache. Assigns or converts textures to formats D3D can work with.
	TexConversion class.

	An effort was made to keep the renderer interface reasonably API neutral. Ports to future Direct3D versions should only influence the D3D and to a lesser extent TexConversion classes.

	\section buildset Build settings.
	- Struct member alignment must be set to 4 bytes.
	- Project must be set to unicode, but with wchar_t not as built-in type.
	- See the solution documentation for information on how the Visual Studio solution is set up.

	\section renderer Renderer.
	An Unreal renderer takes care of the following:
	- Initialize and care for the graphics API.
	- Handle resizing of buffers, fullscreen mode.
	- Set scene: projection, viewport.
	- Implement a texture cache, convert textures to API supported format
	- Draw geometry, meanwhile applying textures and setting state such as blending.
	- Misc. tasks, such as returning a dump of the back buffer.

	Geometry is sent worldview transformed and viewport clipped. Maps are lit using lightmaps; 3d models are per-vertex gouraud shaded.

	\section glue Unreal Engine glue
	To integrate with the engine the renderer must:
	- Provide an .int file describing its name etc. This is required to be able to pick the renderer in the game options and to set its advanced preferences.
	- Use the DECLARE_CLASS() macro inside its class declaration.
	- Use the IMPLEMENT_PACKAGE() and IMPLEMENT_CLASS() macros.
	- Have an extra letter in front of its name. If the renderer is set to "D3D10RenderDevice", the game will look for the "UD3D10RenderDevice" class.
	- Link against the game's core and engine libraries.
	- Be built with correct \ref buildset.

	\section lifecycle Lifecycle
	Renderer lifecycle. Whatever renderer is running takes the place of URenderDevice:
	- First the constructor URenderDevice::StaticConstructor() is called. Here 'advanced options' window preferences must be bound.
	- URenderDevice::Init() is called. API initialization, setting up parameters etc. The renderer is reinitialized when switching to fullscreen.
	- If URenderDevice::PrecacheOnFlip is set, URenderDevice::PrecacheTexture() calls are made to cache textures.
	- URenderDevice::Lock() is called. The back and depth buffer are cleared and the renderer prepares for new data.
	- If needed,  URenderDevice::SetSceneNode() calls are made to set up projection and viewport.
	- Geometry is sent using URenderDevice::DrawComplexSurface(), URenderDevice::DrawGouraudPolygon() and URenderDevice::DrawTile().
	- Between geometry sends, URenderDevice::ClearZ() is used to clear the depth buffer. This way, the skybox is drawn in the back and weapon models at the front.
	- URenderDevice::Unlock() is called. The scene should be drawn.
*/