#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_LightmapTexCoord;
layout(location = 2) in vec4 v_DetailTexCoord;

////////////////////////////////////////////////////////////////////////////////

layout(set=0, binding=0) uniform sampler2D DiffuseTex;
layout(set=0, binding=1) uniform sampler2D LightmapTex;
layout(set=0, binding=2) uniform sampler2D DetailTex;

////////////////////////////////////////////////////////////////////////////////
// Should be in sync with VS
layout(set=0, binding=3) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
    vec4 DetailTexColor;
} PerFrameData;

layout(set=0, binding=4) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

////////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 o_Color;

#define Z_SCALE 0.002631578947
//#define 0.999f, 0.0f, 1.0f\n"

void main() {
    vec4 albedo = texture(DiffuseTex, v_TexCoord);
    vec4 lightmap = texture(LightmapTex, v_LightmapTexCoord);
    vec4 detail = texture(DetailTex, v_DetailTexCoord.xy);

    // reconstructed after D3D9 assembly
    if(v_DetailTexCoord.x>=0 && v_DetailTexCoord.y>=0) {
        float vPosZ = v_DetailTexCoord.z;
        float k = clamp(vPosZ *Z_SCALE, 0,1); // vpos.z
        albedo.rgb = 2*albedo.rgb*(PerFrameData.DetailTexColor.rgb*k + (1-k)*detail.rgb);
    }

    // in D3D9 first lightmap is blended and then detail, but I think applying lightmapshould be done after detail
    if(v_LightmapTexCoord.x>=0 && v_LightmapTexCoord.y>=0) {
        // Lightmap blend scale factor (2 or 1 in case of OneXBlending)
        albedo = 2 * albedo * lightmap.zyxw;
    }

    o_Color = albedo;

}
