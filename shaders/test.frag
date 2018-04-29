#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec3 _faceNormal;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 faceNormal  = normalize(cross(dFdx(_faceNormal), dFdy(_faceNormal)));
    vec4 tex = texture(texSampler, vec2(fragTexCoord.x, -fragTexCoord.y));
    outColor = tex.rgba;
}