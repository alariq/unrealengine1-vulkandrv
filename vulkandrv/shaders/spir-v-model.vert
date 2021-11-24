#version 450

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 uv;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_UV;

void main() {
    gl_Position = pos;
    v_Color = color;
    v_UV = uv;
}

