#version 450
//#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec2 v_UV;

layout(set=0, binding=0) uniform sampler2D my_sampler;
layout(set=0, binding=1) uniform sampler2D Texture;
layout(set=0, binding=2) uniform PerFrameData_t {
    vec4 stuff;
} PerFrameData;

layout(location = 0) out vec4 o_Color;

void main() {
    vec3 albedo = texture(Texture, v_UV * PerFrameData.stuff.y + PerFrameData.stuff.w).rgb;
    vec3 light_dir = -normalize(vec3(-1, -1, .0));
    o_Color.rgb = //0.5f*v_Normal + 0.5f;
                   albedo*max(vec3(0.025), dot(light_dir, normalize(v_Normal) ) );
    o_Color.w = 1;

}
