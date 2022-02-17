#version 450

#define ALPHA_TEST

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec4 v_FogColor;

////////////////////////////////////////////////////////////////////////////////

layout(set=0, binding=0) uniform sampler2D DiffuseTex;

////////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 o_Color;

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

    o_Color = linear2gamma_rgb(albedo*v_Color + vec4(v_FogColor.rgb, 0));

}
