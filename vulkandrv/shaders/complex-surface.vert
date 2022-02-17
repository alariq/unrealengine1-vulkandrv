#version 450

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 TexCoord;

////////////////////////////////////////////////////////////////////////////////
// Should be in sync with FS
layout(set=0, binding=4) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
} PerFrameData;

layout(set=0, binding=5) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

// this is dynamic UB, should be setup once and offset provided during bind ds
layout(set=1, binding=0) uniform PerDrawCallVSData_t {
    vec4 AxisX_UDot;
    vec4 AxisY_VDot;
    vec4 Diffuse_PanXY_UVMult;
	vec4 Macro_PanXY_UVMult;
	vec4 HasMacro_UVScale;
    vec4 Lightmap_PanXY_UVMult;
    vec4 HasLightmap_UVScale;
	vec4 Detail_PanXY_UVMult;
	vec4 HasDetail_UVScale;//x-has detail, yz UVScale, w -is_fog
    mat4 proj;
} PerDrawVSData;


////////////////////////////////////////////////////////////////////////////////

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec2 v_LightmapTexCoord;
layout(location = 2) out vec4 v_DetailTexCoord;
layout(location = 3) out vec2 v_MacroTexCoord;

void main() {
    gl_Position = vec4(Pos.xyz,1) * /*PerFrameData.world * PerFrameData.view * */PerFrameData.proj;

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

    if(PerDrawVSData.HasMacro_UVScale.x>0) {
        vec2 Macro_Pan = PerDrawVSData.Macro_PanXY_UVMult.xy;
        vec2 Macro_UVMult = PerDrawVSData.Macro_PanXY_UVMult.zw;
        vec2 Macro_UVScale = PerDrawVSData.HasMacro_UVScale.yz;

	    v_MacroTexCoord = (Coord - (Macro_Pan - 0.5*Macro_UVScale))*Macro_UVMult;
    }
    else {
	    v_MacroTexCoord = vec2(-1, -1);
    }

    if(PerDrawVSData.HasLightmap_UVScale.x>0) {
        vec2 Lightmap_Pan = PerDrawVSData.Lightmap_PanXY_UVMult.xy;
        vec2 Lightmap_UVMult = PerDrawVSData.Lightmap_PanXY_UVMult.zw;
        vec2 Lightmap_UVScale = PerDrawVSData.HasLightmap_UVScale.yz;

	    v_LightmapTexCoord = (Coord - (Lightmap_Pan - 0.5*Lightmap_UVScale))*Lightmap_UVMult;
    }
    else {
	    v_LightmapTexCoord = vec2(-1, -1);
    }

    if(PerDrawVSData.HasDetail_UVScale.x>0) {
        float is_fog = PerDrawVSData.HasDetail_UVScale.w;
        vec2 Detail_Pan = PerDrawVSData.Detail_PanXY_UVMult.xy;
        vec2 Detail_UVMult = PerDrawVSData.Detail_PanXY_UVMult.zw;
        vec2 UVScale = is_fog * PerDrawVSData.HasDetail_UVScale.yz;

	    v_DetailTexCoord.xy = (Coord - (Detail_Pan - 0.5*UVScale))*Detail_UVMult;
        // detail scale from D3D9 renderer
        v_DetailTexCoord.xy *= (1-is_fog) * 3.223 + 1;
        v_DetailTexCoord.zw = vec2(Pos.z, is_fog);
    }
    else {
	    v_DetailTexCoord = vec4(-1, -1, 0,0);
    }


}

