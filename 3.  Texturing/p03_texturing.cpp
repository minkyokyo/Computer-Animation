
#include "glSetup.h"
#include "glShader.h"

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
#include <fstream>
using namespace std;

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>

void render(GLFWwindow* window);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);

// Camera configuation
Vector3f eye(0.0f, 0.0f, 0.5f);
Vector3f center(0.0f, 0.0f, -0.1f);
Vector3f up(0.0f, 1.0f, 0.0f);

// Light configuration
Vector4f light(0.7f, 0.0f, 0.7f, 1.0f);

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Display style
bool aaEnabled = true;

// Model, view, projection matrix
Matrix4f	ViewMatrix, ProjectionMatrix;

// Play configuration
bool pause = true;

float timeStep = 1.0f / 120; // 120fpas
float period = 8.0f;

// Rotation on/off
bool rotation = true;
float angle = 0;

// Program and shaders
struct Program
{
	GLuint vs; // Vertex shader
	GLuint fs; // Fragment shader
	GLuint pg; // Program

	Program() { vs = 0; fs = 0; pg = 0; }

	void create(const char* vertexShaderFileName, const char* fragmentShaderFileName)
	{
		createShaders(vertexShaderFileName, fragmentShaderFileName, pg, vs, fs);
	}

	void destroy() { deleteShaders(pg, vs, fs); }
};

Program pgTexturing;	 // Program for simple Texturing
Program pgDoubleVision;  // Program for double vision
Program pgNormalMapping; // Program for normal mapping

// Geometry
struct Geometry
{
	GLuint vao;		 // Vertex array object
	GLuint indexId;  // Buffer for triangle inddices
	GLuint vertexId; // Buffer for vertex positions
	GLuint normalId; // Buffer for normal vectors
	GLuint coordId;  // Buffer for texture coordinates

	int numTris;     // # of triangles

	Geometry()
	{
		vao = 0;
		indexId = 0;
		vertexId = 0;
		normalId = 0;
		coordId = 0;
	}
};

Geometry tri;	// VAO and VBO for a single triangle
Geometry quad;  // VAO and VBO for a single quad

// Textrue
GLuint texId[5];

// (1) Simple texturing
bool	simpleTexturing = false;

// (2) Alpha texturing
bool	alphaTexturing = false;

// (3) Double vision
bool doubleVision = true;

// Texture
static const GLubyte demonHeadImage[3 * (128 * 128)] =
{
  #include "m02_demon_image.h"
};

// Separation
Vector2f leftSeparation(-0.1f, 0.0f);
Vector2f rightSeparation(0.1f, 0.0f);

// (4) Normal mapping
bool normalMapping = false;
float scale = 1.0;

// (5) Exercise
bool exercise = false;
bool loadColorAlphaTexture(const char* filenameC, const char* filenameA, int w, int h);
void renderColorAlphaNormalMappedQuad(const Vector3f& l);


void reshapeModernOpenGL(GLFWwindow* window, int w, int h)
{
	// Window configuration
	aspect = (float)w / h;
	windowW = w;
	windowH = h;

	// Viewport
	glViewport(0, 0, w, h);

	// Projection matirx
	ProjectionMatrix = orthographic<float>(-aspect, aspect, -1, 1, -1, 1);

	// The Screen size is required for mouse interaction.
	glfwGetWindowSize(window, &screenW, &screenH);
	cerr << "reshape(" << w << "," << h << ")";
	cerr << " with screen " << screenW << " x " << screenH << endl;
}

// Demon head texture
void loadDemonHeadTexture()
{
	// GL_CLAMP is not supported anymore!
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE, demonHeadImage);

	isOK("glTexImage2D()", __FILE__, __LINE__);

}

// RGB texture
bool loadRawTexture(const char* filename, int w, int h, int n)
{
	// Open the raw texture file
	ifstream	is(filename, ios::binary);
	if (is.fail())
	{
		cout << "Can't open " << filename << endl;
		return false;
	}

	// Allocate memory
	GLubyte* raw = new GLubyte[w * h * 3];

	// Only 3 and 1
	if (n == 1)
	{
		// Read all the texels of the single channel
		GLubyte* rawG = new GLubyte[w * h];
		is.read((char*)rawG, w * h * n);

		for (int i = 0; i < w * h; i++)
			raw[3 * i + 0] = raw[3 * i + 1] = raw[3 * i + 2] = rawG[i]; // RGB

		delete[] rawG;
	}
	else if (n == 3)
	{
		// Read all the texels of the rgb channels
		is.read((char*)raw, w * h * n);
	}
	else
	{
		cout << "Texture images with Two channels are not supported!" << endl;
		return false;
	}
	if (!is)		cout << "Error: only " << is.gcount() << "bytes could be read!" << endl;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, raw);
	isOK("glTexImage2D()", __FILE__, __LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Deallocate memory
	delete[]	raw;

	return true;
}

// Alpha texture
bool loadAlphaTexture(const char* filename, int w, int h)
{
	// Open the raw texture file
	ifstream	is(filename, ios::binary);
	if (is.fail())
	{
		cout << "Can't open " << filename << endl;
		return false;
	}

	// Allocate memory
	GLubyte* raw = new GLubyte[w * h * 4];	//RGBA
	{
		GLubyte* rawG = new GLubyte[w * h];

		// Read all the texels
		is.read((char*)rawG, w * h);
		if (!is)		cout << "Error: only" << is.gcount() << "bytes could be read!" << endl;

		for (int i = 0; i < w * h; i++)
		{
			raw[4 * i + 0] = 0;			// R
			raw[4 * i + 1] = 0;			// G
			raw[4 * i + 2] = 0;			// B
			raw[4 * i + 3] = rawG[i];	// A
		}
		delete[] rawG;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw);

	// Deallocate memory
	delete[] raw;

	return true;
}

bool loadColorAlphaTexture(const char* filenameC, const char* filenameA, int w, int h)
{
	// Open the raw texture file (color)
	ifstream	isC(filenameC, ios::binary);
	if (isC.fail())
	{
		cout << "Can't open " << filenameC << endl;
		return false;
	}

	// Open the raw texture file (alpha)
	ifstream	isA(filenameA, ios::binary);
	if (isA.fail())
	{
		cout << "Can't open " << filenameA << endl;
		return false;
	}

	// Allocate memory for Color & Alpha
	GLubyte* raw = new GLubyte[w * h * 4];	//RGBA
	{
		// Allocate memory for color
		GLubyte* rawC = new GLubyte[w * h * 3];
		// Read all the texels of the rgb channels
		isC.read((char*)rawC, w * h * 3);

		// Alpha값을 읽는다.
		GLubyte* rawA = new GLubyte[w * h];
		// Read all the texels
		isA.read((char*)rawA, w * h);
		if (!isA)		cout << "Error: only" << isA.gcount() << "bytes could be read!" << endl;

		for (int i = 0; i < w * h; i++)
		{
			raw[4 * i + 0] = rawC[3*i+0];		// R
			raw[4 * i + 1] = rawC[3 * i + 1];	// G
			raw[4 * i + 2] = rawC[3 * i + 2];	// B
			raw[4 * i + 3] = rawA[i];			// A
		}
		delete[] rawA;
		delete[] rawC;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw);

	// Deallocate memory
	delete[]	raw;

	return true;
}

int main(int argc, char* argv[])
{
	// Initialize the OpenGL system: true for modern OpenGL
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor, true);
	if (window == NULL) return -1;

	// Viewport and perspective setting
	reshapeModernOpenGL(window, windowW, windowH);

	
	// Callbacks
	glfwSetFramebufferSizeCallback(window, reshapeModernOpenGL);
	glfwSetKeyCallback(window, keyboard);

	// Depth test
	glEnable(GL_DEPTH_TEST);

	// Texture
	glGenTextures(5, texId);

	// GL_TEXTURE0 for a color texture
	{
		// Specify the activate texture uint
		glActiveTexture(GL_TEXTURE0 + 0);
		isOK("glActiveTexture()", __FILE__, __LINE__);

		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, texId[0]);
		isOK("glBindTexture()", __FILE__, __LINE__);

		// Raw texture
		loadRawTexture("m02_snow_color_map.raw", 256, 256, 3);
	}

	// GL_TEXTURE1 for the demon head texture
	{
		// Specify the activate texture uint
		glActiveTexture(GL_TEXTURE0 + 1);
		isOK("glActiveTexture()", __FILE__, __LINE__);

		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, texId[1]);
		isOK("glBindTexture()", __FILE__, __LINE__);

		// Demon head texture
		loadDemonHeadTexture();
	}

	// GL_TEXTURE2 for an alpha texture
	{
		// Specify the activate texture unit
		glActiveTexture(GL_TEXTURE0 + 2);
		isOK("glActiveTexture()", __FILE__, __LINE__);

		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, texId[2]);
		isOK("glBindTexture()", __FILE__, __LINE__);

		loadAlphaTexture("m02_logo.raw", 512, 512);
	}

	// GL_TEXTURE3 for a normal mapp
	{
		// Specify the activate texture unit
		glActiveTexture(GL_TEXTURE0 + 3);
		isOK("glActiveTexture()", __FILE__, __LINE__);

		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, texId[3]);
		isOK("glBindTexture()", __FILE__, __LINE__);

		loadRawTexture("m02_snow_normal_map.raw", 256, 256, 3);
	}

	// GL_TEXTURE4 for color-alpha mapp used in Execrise
	{
		// Specify the activate texture unit
		glActiveTexture(GL_TEXTURE0 + 4);
		isOK("glActiveTexture()", __FILE__, __LINE__);

		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, texId[4]);
		isOK("glBindTexture()", __FILE__, __LINE__);

		loadColorAlphaTexture("m02_snow_color_map.raw","m02_snow_alpha_map.raw", 256, 256);
	}

	// Initialization
	{
		// Create shaders, VAO and VBO for simple texturing and double vision
		pgTexturing.create("sv03_texturing.glsl", "sf03_texturing.glsl");
		pgDoubleVision.create("sv03_double_vision.glsl", "sf03_double_vision.glsl");
		pgNormalMapping.create("sv03_texturing.glsl", "sf03_normal.glsl");

		// Prepare a single quad
		ArrayXXi face;
		MatrixXf vertex;
		MatrixXf normal;
		MatrixXf texture;

		face.resize(3, 2); // 삼각형 두개로 만들어지는 quad
		vertex.resize(3, 4); // 4개의 점에서 ..? x,y,z?
		normal.resize(3, 4); // 4개로 x,y,z?
		texture.resize(2, 4); // texture좌표 줌.

		face(0, 0) = 0; face(1, 0) = 1; face(2, 0) = 3;
		face(0, 1) = 1; face(1, 1) = 2; face(2, 1) = 3;

		vertex(0, 0) = -1; vertex(1, 0) = 1; vertex(2, 0) = 0;
		vertex(0, 1) = -1; vertex(1, 1) = -1; vertex(2, 1) = 0;
		vertex(0, 2) = 1; vertex(1, 2) = -1; vertex(2, 2) = 0;
		vertex(0, 3) = 1; vertex(1, 3) = 1; vertex(2, 3) = 0;

		// normal 전부 z로.

		normal(0, 0) = 0; normal(1, 0) = 0; normal(2, 0) = 1;
		normal(0, 1) = 0; normal(1, 1) = 0; normal(2, 1) = 1;
		normal(0, 2) = 0; normal(1, 2) = 0; normal(2, 2) = 1; 
		normal(0, 3) = 0; normal(1, 3) = 0; normal(2, 3) = 1;

		texture(0, 0) = 0; texture(1, 0) = 0;	// Upside down
		texture(0, 1) = 0; texture(1, 1) = 1;
		texture(0, 2) = 1; texture(1, 2) = 1;
		texture(0, 3) = 1; texture(1, 3) = 0;

		// Create VAO and VBO for a single quad // VBO 만듬.
		createVBO(quad.vao, quad.indexId, quad.vertexId, quad.normalId, quad.coordId);

		// UPload the triangle data into the buffers
		quad.numTris = uploadMesh2VBO(face, vertex, normal, texture, quad.vao, quad.indexId, quad.vertexId, quad.normalId, quad.coordId);

		// Prepare a single triangle
		face.resize(3, 1);
		vertex.resize(3, 3);
		normal.resize(3, 3);
		texture.resize(2, 3);

		face(0, 0) = 0; face(1, 0) = 1; face(2, 0) = 2;

		vertex(0, 0) = 0.8f; vertex(1, 0) = 0.8f; vertex(2, 0) = 0;
		vertex(0, 1) = -0.8f; vertex(1, 1) = 0.8f; vertex(2, 1) = 0;
		vertex(0, 2) = 0.0f; vertex(1, 2) = -0.8f; vertex(2, 2) = 0;

		normal(0, 0) = 0; normal(1, 0) = 0; normal(2, 0) = 1;
		normal(0, 1) = 0; normal(1, 1) = 0; normal(2, 1) = 1;
		normal(0, 2) = 0; normal(1, 2) = 0; normal(2, 2) = 1;

		texture(0, 0) = 1; texture(1, 0) = 0;
		texture(0, 1) = 0; texture(1, 1) = 0;
		texture(0, 2) = 0.5f; texture(1, 2) = 1;

		// Create VAO and VBO for a single triangle
		createVBO(tri.vao, tri.indexId, tri.vertexId, tri.normalId, tri.coordId);

		// UPload the triangle data into the buffers
		tri.numTris = uploadMesh2VBO(face, vertex, normal, texture, tri.vao, tri.indexId, tri.vertexId, tri.normalId, tri.coordId);
	}

	// Ussage
	cout << endl;
	cout << "Keyboard Input : space for play/pause" << endl;
	cout << "Keyboard Input : q/esc for quit" << endl;
	cout << endl;
	cout << "Keyboard Input : s for simple texturing" << endl;
	cout << "Keyboard Input : a for alpha texturing" << endl;
	cout << "Keyboard Input : d for double vision" << endl;
	cout << "Keyboard Input : n for normal mapping" << endl;
	cout << endl;
	cout << "Keyboard Input : r for rotation on/off" << endl;
	cout << "Keyboard Input : i for resetting the rotation angle" << endl;

	// Main loop
	float previous = (float)glfwGetTime();
	float elapsed = 0;
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents(); // Events

		// Time passed during a single loop
		float now = (float)glfwGetTime();
		float delta = now - previous;
		previous = now;

		// Time passed after the previous frame
		elapsed += delta;

		// Dleta with the current frame
		if (elapsed > timeStep)
		{
			// Update if not paused
			if (!pause)
			{
				// Rotation on / off
				angle += (rotation) ? float(2.0 * M_PI) / period * elapsed : 0;

				// Double vision
				leftSeparation[0] = -0.1f + 0.1f * sin(angle);
				rightSeparation[0] = 0.1f - 0.1f * sin(angle);

				// Rotating light position
				light[0] = 0.7f * cos(angle);
				light[1] = 0.7f * sin(angle);
			}

			elapsed = 0; // Reset the elapsed time
		}

		render(window);		// Draw one frame
		glfwSwapBuffers(window);	// Swap buffers
	}

	// Finalization
	{
		// Texture
		glDeleteTextures(5, texId); // exercise를 위해 texture를 5개로 늘렸습니다.

		// Delete VBO and shaders
		deleteVBO(tri.vao, tri.indexId, tri.vertexId, tri.normalId, tri.coordId);
		deleteVBO(quad.vao, quad.indexId, quad.vertexId, quad.normalId, quad.coordId);
		pgTexturing.destroy();
		pgDoubleVision.destroy();
		pgNormalMapping.destroy();
	}

	// Terminate the glfw system
	glfwDestroyWindow(window);
	glfwTerminate();
	
	return 0;
}

void setUniformMVP(GLuint program, Matrix4f& M, Matrix4f& V, Matrix4f& P)
{
	// ModelView matrix
	Matrix4f ModelViewMatrix = V * M;
	setUniform(program, "ModelViewMatrix", ModelViewMatrix);

	// Normal matrix: Inverse of the transpose of the model view matrix
	Matrix3f	NormalMatrix = ModelViewMatrix.block<3, 3>(0, 0).inverse().transpose();
	setUniform(program, "NormalMatrix", NormalMatrix);

	// ModelViewProjection matrix in the vertex shader
	Matrix4f ModelViewProjectionMatrix = P * ModelViewMatrix;
	setUniform(program, "ModelViewProjectionMatrix", ModelViewProjectionMatrix);
}

void render(GLFWwindow* window)
{
	// Antialiasing
	if (aaEnabled) glEnable(GL_MULTISAMPLE);
	else           glDisable(GL_MULTISAMPLE);

	// Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Camera configuration
	ViewMatrix = lookAt<float>(eye, center, up);

	// Light position in the eye coordinate system for the fragment shader
	Vector3f l = (ViewMatrix * light).head<3>(); //Vector4f to Vector3f

	// Draw the quad
	if (simpleTexturing && !normalMapping)
	{
		// Model matrix
		Affine3f T; T = Translation3f(0.0f, 0.0f, 0.0f) * Scaling(0.7f, 0.7f, 0.7f);
		Matrix4f ModelMatrix = T.matrix();

		// Model, view, projection matrices
		setUniformMVP(pgTexturing.pg, ModelMatrix, ViewMatrix, ProjectionMatrix);

		// Texture
		setUniformi(pgTexturing.pg, "tex", 0); // 0 for GL_TEXTURE0

		// Light Position
		setUniform(pgTexturing.pg, "LightPosition", l);

		// Material is dependent of the object
		setUniform(pgTexturing.pg, "Ka", Vector3f(0.10f, 0.10f, 0.10f));
		setUniform(pgTexturing.pg, "Kd", Vector3f(0.95f, 0.95f, 0.95f));
		setUniform(pgTexturing.pg, "Ks", Vector3f(0.50f, 0.50f, 0.50f));
		setUniform(pgTexturing.pg, "Shininess", 128.0f);

		// Draw the mesh using the program and the vertex buffer object
		glUseProgram(pgTexturing.pg);
		drawVBO(quad.vao, quad.numTris);
	}

	// Draw the triangle
	if (doubleVision)
	{
		// Model matrix
		Affine3f T; T = Translation3f(0.0f, 0.0f, 0.1f);
		Matrix4f ModelMatrix = T.matrix();

		// Model, view, projection matrices
		setUniformMVP(pgDoubleVision.pg, ModelMatrix, ViewMatrix, ProjectionMatrix);

		// Texture
		setUniformi(pgDoubleVision.pg, "tex", 1); // 1 for GL_TEXTURE1

		// Separation
		setUniform(pgDoubleVision.pg, "leftSeparation", leftSeparation);
		setUniform(pgDoubleVision.pg, "rightSeparation", rightSeparation);

		// Light position
		setUniform(pgDoubleVision.pg, "LightPosition", l);

		// Material is dependent of  the object
		setUniform(pgDoubleVision.pg, "Ka", Vector3f(0.10f, 0.10f, 0.10f));
		setUniform(pgDoubleVision.pg, "Kd", Vector3f(0.95f, 0.95f, 0.95f));
		setUniform(pgDoubleVision.pg, "Ks", Vector3f(0.10f, 0.10f, 0.10f));
		setUniform(pgDoubleVision.pg, "Shininess",128.0f);

		// Draw the mesh using the program and the vertex buffer object
		glUseProgram(pgDoubleVision.pg);
		drawVBO(tri.vao, tri.numTris);
	}

	if (normalMapping)
	{
		// Model matrix
		Affine3f	T; T = Translation3f(0.0f, 0.0f, 0.0f) * Scaling(0.7f, 0.7f, 0.7f);
		Matrix4f	ModelMatrix = T.matrix();

		// Model, view, projection matrices
		setUniformMVP(pgNormalMapping.pg, ModelMatrix, ViewMatrix, ProjectionMatrix);

		// Textures
		setUniformi(pgNormalMapping.pg, "texDiffuse", 0);//컬러텍스처 // 0 for GL_TEXTURE0
		setUniformi(pgNormalMapping.pg, "texNormal", 3);//노말텍스처 // 3 for GL_TEXTURE3

		// Scale
		setUniform(pgNormalMapping.pg, "scale", scale); // Height scale for normal mapping

		// Light position
		setUniform(pgNormalMapping.pg, "LightPosition", l);

		setUniform(pgNormalMapping.pg, "Ka", Vector3f(0.10f, 0.10f, 0.10f));
		setUniform(pgNormalMapping.pg, "Kd", Vector3f(0.95f, 0.95f, 0.95f));
		setUniform(pgNormalMapping.pg, "Ks", Vector3f(0.50f, 0.50f, 0.50f));
		setUniform(pgNormalMapping.pg, "Shininess", 128.0f);

		// Draw the mesh using the program and the vertex buffer object
		glUseProgram(pgNormalMapping.pg);
		drawVBO(quad.vao, quad.numTris);
	}

	// Draw the textured quad
	if (alphaTexturing)
	{
		// Model matrix
		Affine3f	T;	T = Translation3f(0.0f, 0.0f, 0.2f)
			* Matrix3f(AngleAxisf(angle, Vector3f::UnitZ()))
			* Scaling(0.7f, 0.7f, 0.7f);

		Matrix4f	ModelMatrix = T.matrix();
		
		// Model, view, projection matrices
		setUniformMVP(pgTexturing.pg, ModelMatrix, ViewMatrix, ProjectionMatrix);

		// Alpha texturing on
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		isOK("glBlendFunc()", __FILE__, __LINE__);

		// Texture
		setUniformi(pgTexturing.pg, "tex", 2); // 2 for GL_TEXTURE2

		// Light Position
		setUniform(pgTexturing.pg, "LightPosition", l);

		// Material is dependent of the object
		setUniform(pgTexturing.pg, "Ka", Vector3f(0.10f, 0.10f, 0.10f));
		setUniform(pgTexturing.pg, "Kd", Vector3f(0.95f, 0.95f, 0.95f));
		setUniform(pgTexturing.pg, "Ks", Vector3f(0.10f, 0.10f, 0.10f));
		setUniform(pgTexturing.pg, "Shininess", 128.0f);

		// Draw the mesh using the program and the vertex buffer object
		glUseProgram(pgTexturing.pg);
		drawVBO(quad.vao, quad.numTris);

		// Alpha texturing off
		glDisable(GL_BLEND);
	}

	// Exercise
	if(exercise) renderColorAlphaNormalMappedQuad(l);

	// Check the status
	isOK("render()", __FILE__, __LINE__);
}

void renderColorAlphaNormalMappedQuad(const Vector3f& l)
{
	// Model matrix
	Affine3f	T;	T = Translation3f(0.0f, 0.0f, 0.0f)* Scaling(1.0f, 1.0f, 1.0f);
	Matrix4f	ModelMatrix = T.matrix();

	// Model, view, projection matrices
	setUniformMVP(pgNormalMapping.pg, ModelMatrix, ViewMatrix, ProjectionMatrix);

	// Alpha texturing on
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	isOK("glBlendFunc()", __FILE__, __LINE__);

	// Textures
	setUniformi(pgNormalMapping.pg, "texDiffuse", 4);//컬러텍스처 // 4 for GL_TEXTURE4
	setUniformi(pgNormalMapping.pg, "texNormal", 3);//노말텍스처 // 3 for GL_TEXTURE3

	// Scale
	setUniform(pgNormalMapping.pg, "scale", scale); // Height scale for normal mapping

	// Light Position
	setUniform(pgNormalMapping.pg, "LightPosition", l);

	// Material is dependent of the object
	setUniform(pgNormalMapping.pg, "Ka", Vector3f(0.10f, 0.10f, 0.10f));
	setUniform(pgNormalMapping.pg, "Kd", Vector3f(0.95f, 0.95f, 0.95f));
	setUniform(pgNormalMapping.pg, "Ks", Vector3f(0.50f, 0.50f, 0.50f));
	setUniform(pgNormalMapping.pg, "Shininess", 128.0f);

	// Draw the mesh using the program and the vertex buffer object
	glUseProgram(pgNormalMapping.pg);
	drawVBO(quad.vao, quad.numTris);

	// Alpha texturing off
	glDisable(GL_BLEND);
}

void
keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
			// Quit
		case GLFW_KEY_Q:
		case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GL_TRUE); break;

			// Play on/off
		case GLFW_KEY_SPACE:	pause = !pause; break;

			// Simple texturing
		case GLFW_KEY_S: simpleTexturing = !simpleTexturing; break;

			// Double vision
		case GLFW_KEY_D:	doubleVision = !doubleVision; break;	// Diffuse

		// Overlay an alpha-textured quad
		case GLFW_KEY_A:	alphaTexturing = !alphaTexturing; break;	// Specular

		// Overlay an normal-mapped quad
		case GLFW_KEY_N:	normalMapping = !normalMapping; break;

			// Exercise
		case GLFW_KEY_E:	exercise = !exercise; break;	// Ambient

		// Rotation on/off
		case GLFW_KEY_R:	rotation = !rotation; break;

			// Rotation initialization
		case GLFW_KEY_I:	angle = 0; break;

			// Height scaling
		case GLFW_KEY_UP:	scale += 0.1f; cout << "scale = " << scale << endl;	break;
		case GLFW_KEY_DOWN: scale -= 0.1f; cout << "scale = " << scale << endl; break;
		}
	}
}
