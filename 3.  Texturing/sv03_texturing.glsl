
#version 400

layout (location = 0) in vec3   VertexPosition;
layout (location = 1) in vec3   VertexNormal;
layout (location = 2) in vec2   VertexTexcoord;

out vec3    position;
out vec3    normal;
out vec2    texcoord;

uniform mat4    ModelViewProjectionMatrix;
uniform mat4    ModelViewMatrix;
uniform mat3    NormalMatrix;

void main() {
    gl_Position = ModelViewProjectionMatrix * vec4(VertexPosition, 1.0);

    position = vec3(ModelViewMatrix * vec4(VertexPosition, 1.0));
    normal = normalize(NormalMatrix * VertexNormal);

    texcoord = VertexTexcoord;
}