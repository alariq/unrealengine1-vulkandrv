#version 450

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec2 LightmapTexCoord;

////////////////////////////////////////////////////////////////////////////////
// Should be in sync with FS
layout(set=0, binding=2) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
} PerFrameData;

layout(set=0, binding=3) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

// this is dynamic UB, should be setup once and offset provided during bind ds
layout(set=1, binding=0) uniform PerDrawCallVSData_t {
    vec4 AxisX_UDot;
    vec4 AxisY_VDot;
    vec4 Diffuse_PanXY_UVMult;
    vec4 Lightmap_PanXY_UVMult;
    vec4 HasLightmap_UVScale;
} PerDrawVSData;


////////////////////////////////////////////////////////////////////////////////

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec2 v_LightmapTexCoord;

void main() {
    gl_Position = vec4(Pos.xyz,1) * /*PerFrameData.world * PerFrameData.view * */PerFrameData.proj;
    //v_TexCoord = TexCoord;
    //v_LightmapTexCoord = LightmapTexCoord;

    vec3 AxisX = PerDrawVSData.AxisX_UDot.xyz;
    float UDot = PerDrawVSData.AxisX_UDot.w;
    vec3 AxisY = PerDrawVSData.AxisY_VDot.xyz;
    float VDot = PerDrawVSData.AxisY_VDot.w;

    vec2 Diffuse_Pan = PerDrawVSData.Diffuse_PanXY_UVMult.xy;
    vec2 Diffuse_UVMult = PerDrawVSData.Diffuse_PanXY_UVMult.zw;

	float U = dot(AxisX, Pos.xyz);
	float V = dot(AxisY, Pos.xyz);
	vec2 Coord = vec2(U-UDot, V - VDot);

	//Diffuse texture coordinates
	v_TexCoord = (Coord - Diffuse_Pan)*Diffuse_UVMult;

    if(PerDrawVSData.HasLightmap_UVScale.x>0) {
        vec2 Lightmap_Pan = PerDrawVSData.Lightmap_PanXY_UVMult.xy;
        vec2 Lightmap_UVMult = PerDrawVSData.Lightmap_PanXY_UVMult.zw;
        vec2 Lightmap_UVScale = PerDrawVSData.HasLightmap_UVScale.yz;

	    v_LightmapTexCoord = (Coord - (Lightmap_Pan - 0.5*Lightmap_UVScale))*Lightmap_UVMult;

    }
    else {
	    v_LightmapTexCoord = vec2(-1, -1);
    }


}

