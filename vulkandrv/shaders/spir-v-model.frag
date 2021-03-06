#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_UV;

layout(set=0, binding=0) uniform sampler2D my_sampler;
layout(set=0, binding=1) uniform sampler2D Texture;
layout(set=0, binding=2) uniform PerFrameData_t {
    vec4 stuff;
    mat4 fake_camera;
} PerFrameData;

layout(location = 0) out vec4 o_Color;

void main() {
    o_Color = v_Color * texture(Texture, v_UV * PerFrameData.stuff.y + PerFrameData.stuff.w);
}
