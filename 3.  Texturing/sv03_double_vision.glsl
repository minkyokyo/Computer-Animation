
#version 400

layout (location = 0) in vec3	VertexPosition;
layout (location = 1) in vec3	VertexNormal;
layout (location = 2) in vec2	VertexTexcoord;

out vec3	position;
out vec3	normal;
out vec2	leftTexcoord;
out vec2	rightTexcoord;


// Transformation matrices: GLSL employ column-major matrices.
uniform mat4	ModelViewProjectionMatrix;
uniform mat4	ModelViewMatrix;
uniform mat3	NormalMatrix;	// Transpose of the inverse of modelViewMatrix

// Separation
uniform vec2	leftSeparation = vec2(-0.1f,0.0f);
uniform vec2	rightSeparation =vec2(0.1f,0.0f);

void
main(void)
{
	gl_Position = ModelViewProjectionMatrix * vec4(VertexPosition, 1.0);

	// View coordinate system
	position = vec3(ModelViewMatrix * vec4(VertexPosition, 1.0));
	normal = normalize(NormalMatrix * VertexNormal);
	
	// Texture coordinates
	leftTexcoord = VertexTexcoord  + leftSeparation;
	rightTexcoord = VertexTexcoord + rightSeparation;
}

