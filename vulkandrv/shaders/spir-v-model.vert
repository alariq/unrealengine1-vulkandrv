#version 450

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec4 v_Color;

void main() {
    gl_Position = pos;
    v_Color = color;
}

