#version 450

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 uv;

layout(set=0, binding=2) uniform PerFrameData_t {
    vec4 stuff;
    mat4 fake_camera;
} PerFrameData;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_UV;

void main() {
    gl_Position = PerFrameData.fake_camera * pos;
    v_Color = color;
    v_UV = uv;
}

