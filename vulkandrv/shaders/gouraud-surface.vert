#version 450

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec4 FogColor;

////////////////////////////////////////////////////////////////////////////////
// Should be in sync with FS
layout(set=0, binding=1) uniform PerFrameData_t {
    mat4 proj; // not really per frame though :-)
} PerFrameData;

layout(set=0, binding=2) uniform PerDrawCallData_t {
    mat4 model;
} PerDrawCallData;

////////////////////////////////////////////////////////////////////////////////

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec4 v_FogColor;

void main() {
    gl_Position = vec4(Pos.xyz,1) * /*PerFrameData.world * PerFrameData.view * */PerFrameData.proj;
	v_TexCoord = TexCoord;
	v_Color = Color;
	v_FogColor = FogColor;
}

