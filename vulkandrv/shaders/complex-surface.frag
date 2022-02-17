#version 450

//#define ALPHA_TEST

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec2 v_LightmapTexCoord;
layout(location = 2) in vec4 v_DetailTexCoord; // xy = uv, z=viewpos.z w = is_fog
layout(location = 3) in vec2 v_MacroTexCoord;

////////////////////////////////////////////////////////////////////////////////

layout(set=0, binding=0) uniform sampler2D DiffuseTex;
layout(set=0, binding=1) uniform sampler2D LightmapTex;
layout(set=0, binding=2) uniform sampler2D DetailTex;
layout(set=0, binding=3) uniform sampler2D MacroTex;

////////////////////////////////////////////////////////////////////////////////
// Should be in sync with VS
layout(set=0, binding=4) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
    vec4 DetailTexColor;
} PerFrameData;

layout(set=0, binding=5) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

////////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 o_Color;

#define Z_SCALE 0.002631578947
//#define 0.999f, 0.0f, 1.0f\n"

vec4 BGRA7_to_RGBA8(vec4 c) {
    return 2*c.bgra;
}

vec4 gamma2linear_rgb(vec4 c) {
#if defined(USE_GAMMA)
    vec4 r;
    r.rgb = pow(c.rgb,vec3(2.2));
    r.w = c.w;
    return r;
#else
    return c;
#endif
}

vec3 gamma2linear(vec3 c) {
#if defined(USE_GAMMA)
    return pow(c,vec3(2.2));
#else
    return c;
#endif
}

vec4 linear2gamma_rgb(vec4 c) {
#if defined(USE_GAMMA)
    vec4 r;
    r.rgb = pow(c.rgb,vec3(1.0/2.8));
    r.w = c.w;
    return r;
#else 
    return c;
#endif
}

void main() {
    vec4 albedo = gamma2linear_rgb(texture(DiffuseTex, v_TexCoord));

#if defined(ALPHA_TEST)
    if(albedo.a < 0.5) {
        discard;
    }
#endif


    const float OneXBlending = 0;
    const vec4 c0 = vec4(0,0,0, 2 - OneXBlending);
    const vec4 c1 = vec4(1,1,1,1);
    const float is_fog = v_DetailTexCoord.w;

    if(v_MacroTexCoord.x>=0 && v_MacroTexCoord.y>=0) {
        vec4 macro = gamma2linear_rgb(texture(MacroTex, v_MacroTexCoord.xy));
        albedo.rgb = c0.a * albedo.rgb * macro.rgb;
    }

    // detail
    if(v_DetailTexCoord.x>=0 && v_DetailTexCoord.y>=0 && is_fog<0.5) {
        vec4 detail = gamma2linear_rgb(texture(DetailTex, v_DetailTexCoord.xy));
        // reconstructed after D3D9 assembly
        float vPosZ = v_DetailTexCoord.z;
        float k = clamp(vPosZ *Z_SCALE, 0,1); // vpos.z
        vec3 detail_color = (PerFrameData.DetailTexColor.rgb); // linear or gamma?
        albedo.rgb = c0.a*albedo.rgb*(detail_color*k + (1-k)*detail.rgb);
    }

    // in D3D9 first lightmap is blended and then detail, but I think applying lightmapshould be done after detail
    if(v_LightmapTexCoord.x>=0 && v_LightmapTexCoord.y>=0) {
        vec4 lightmap = gamma2linear_rgb(texture(LightmapTex, v_LightmapTexCoord));
        albedo = c0.a * albedo * BGRA7_to_RGBA8(lightmap);
    }

    // fog
    if(v_DetailTexCoord.x>=0 && v_DetailTexCoord.y>=0 && is_fog>0.5) {
        vec4 detail = gamma2linear_rgb(texture(DetailTex, v_DetailTexCoord.xy));
        vec4 fog = BGRA7_to_RGBA8(detail);
        float k = c1.a - fog.a;
        albedo.rgb = albedo.rgb*k + fog.rgb;
    }

    o_Color = linear2gamma_rgb(albedo);

}
