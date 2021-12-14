#version 450

layout(location = 0) in vec2 v_TexCoord;

////////////////////////////////////////////////////////////////////////////////

layout(set=0, binding=0) uniform sampler2D DiffuseTex;

layout(set=0, binding=1) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
} PerFrameData;

layout(set=0, binding=2) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

////////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 o_Color;

void main() {
    vec3 albedo = texture(DiffuseTex, v_TexCoord).rgb;
    o_Color.rgb = albedo;
    o_Color.w = 1;

}
