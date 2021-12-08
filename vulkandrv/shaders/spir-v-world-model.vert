#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(set=0, binding=2) uniform PerFrameData_t {
    vec4 stuff; // this should be clear, yo 
    mat4 fake_camera;
    mat4 world;
    mat4 view;
    mat4 proj; // not really per frame though :-)
    mat4 normal_tr;
    mat4 vwp;
} PerFrameData;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec2 v_UV;

void main() {
    gl_Position = vec4(pos.xyz,1) * PerFrameData.vwp;
    // or equivalent
    //gl_Position = vec4(pos.xyz,1) * PerFrameData.world * PerFrameData.view * PerFrameData.proj;

    v_Normal.xyz = (vec4(normal,0) * PerFrameData.normal_tr ).xyz;
    v_UV = uv;
}

