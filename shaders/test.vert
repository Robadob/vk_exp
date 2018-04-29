#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
layout(push_constant) uniform PushConstantsV {
    layout(offset = 0) mat4 model;//(Redundant layout used for descriptive purposes)
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 vertexNormal;
layout(location = 2) out vec3 _faceNormal;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    //gl_Position = ubo.proj * ubo.view * mat4(1) * vec4(inPosition, 1.0);
    //gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    mat4 modelView = ubo.view * pc.model;
    _faceNormal = (modelView * vec4(inPosition, 1.0)).rgb;//requires dydx in frag shader
    gl_Position = ubo.proj * vec4(_faceNormal, 1.0);
    fragTexCoord = inTexCoord;
    //transpose(inverse()) generates normal matrix
    vertexNormal = normalize(transpose(inverse(mat3(modelView)))*inNormal);
}