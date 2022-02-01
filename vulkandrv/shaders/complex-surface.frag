#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_LightmapTexCoord;

////////////////////////////////////////////////////////////////////////////////

layout(set=0, binding=0) uniform sampler2D DiffuseTex;
layout(set=0, binding=1) uniform sampler2D LightmapTex;

////////////////////////////////////////////////////////////////////////////////
// Should be in sync with VS
layout(set=0, binding=2) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
} PerFrameData;

layout(set=0, binding=3) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

////////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 o_Color;

void main() {
    vec4 albedo = texture(DiffuseTex, v_TexCoord);
    vec4 lightmap = texture(LightmapTex, v_LightmapTexCoord);
    if(v_LightmapTexCoord.x>=0 && v_LightmapTexCoord.y>=0)
        albedo *= lightmap.zyxw;
    o_Color = albedo;

}
