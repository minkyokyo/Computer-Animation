#include "glSetup.h"
#include "mesh.h"

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
using namespace std;

#ifdef _WIN32
#define _USE_MATH_DEFINES // To include the definition of M_PI in math.h
#endif

#include <math.h>

void init(const char* filename);
void setupLight();
void setupColoredMaterial(const Vector3f& color);
void render(GLFWwindow* window);
void reshape(GLFWwindow* window, int w, int h);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);

// Camera configuation
Vector3f	eye(2, 2, 2); //eye 위치 3차원 벡터
Matrix<float, 3, 1> center(0, 0.2f, 0); 
Vector3f	up(0, 1, 0);

// Light configuration
Vector4f light(5.0, 5.0, 0.0, 1); // Light position

//	Play configuration
bool pause = true;
float timeStep = 1.0f / 120;
float period = 8.0;
float theta = 0;

// Eigen/OpenGL rotation
bool eigen = false;

// Global coordinate frame
bool axes = false;
float AXIS_LENGTH = 1.5;
float AXIS_LINE_WIDTH = 2;

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Default mesh file name
const char* defaultMeshFileName = "m01_bunny.off";

// Display style
bool aaEnabled = true;	// Antialiasing
bool bfcEnabled = true;	// Back face culling

// Mesh
MatrixXf						vertex;		   // Position	
Matrix<float, Dynamic, Dynamic> faceNormal;    // Face normal vector n개의 페이스가 있다면 각각에 대한노말 벡터가 있슴
MatrixXf						vertexNormal;  // Vertex normal vector
ArrayXXi						face;		   // Index , Trianlg로 된  face. 
// Integer로 이루어진 array, row column 전부 가변적이다.

//Mesh with shrunken faces
MatrixXf faceVertex; // n 개의 triangle로 이루어졌다면, shrunken된 face는 3*n Vertex를 가짐.
// 각 face마다 3개의  vertex가 있으니깐.

//Control variables
bool faceWithGapMesh = false; 
bool useFaceNormal = true;
float gap = 0.1f; //10퍼센트 줄음. 알파라 불르는 것.

int main(int argc, char* argv[])
{
	const char* filename;
	if (argc >= 2) filename = argv[1];
	else filename = defaultMeshFileName;

	// Initialize the OpenGL system
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor);
	if (window == NULL) return -1;

	// Callbacks
	glfwSetKeyCallback(window, keyboard);

	// Depth test
	glEnable(GL_DEPTH_TEST);

	// Normal vectors are normalized after transformation.
	glEnable(GL_NORMALIZE); // 노말벡터 노말라이즈 안되어있으면 시키라는 뜻.

	// Viewport and perspective setting
	reshape(window, windowW, windowH);

	// Initialization - Main loop - Finalization
	init(filename);

	// Main loop
	float previous = (float)glfwGetTime();
	float elapsed = 0;
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = (float)glfwGetTime();
		float delta = now - previous;
		previous = now;

		elapsed += delta;

		// Deal with the current frame
		if (elapsed > timeStep)
		{
			if (!pause) theta += float(2.0 * M_PI) / period * elapsed; //시간에 따라 회전 시키기위해서

			elapsed = 0;
		}

		render(window); // Draw one frame
		glfwSwapBuffers(window); // swap buffers
	}

	// Terminate the glfw system
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void buildShrunkenFaces(const MatrixXf& vertex, MatrixXf& faceVertex)
{
	// Face mesh with # faces and (3 x # faces) vertices
	
	// face마다 3개의 vertex가 필요.
	// vertex가 공유되지 않고, triangle마다 독립되게 가져야하니깐용.
	faceVertex.resize(3, 3 * face.cols());
	for (int i = 0; i < face.cols(); i++)
	{
		//face.cols -> 컬럼 개수.

		// Center of the i-th face
		Vector3f center(0, 0, 0);
		for (int j = 0; j < 3; j++)
			center += vertex.col(face(j, i));

		center /= 3.0f; // center of mass.

		// Build 3 shrunken vertices per triangle
		for (int j = 0; j < 3; j++)
		{
			const Vector3f& p = vertex.col(face(j, i));
			faceVertex.col(3 * i + j) = p + gap * (center - p); // gap = 알파. 원래 피에서 센터방향으로 이동시켜주면 돼요.
		}
	}
}

void init(const char* filename)
{
	// Read a mesh
	cout << "Reading... " << filename << endl;
	readMesh(filename, vertex, face, faceNormal, vertexNormal);

	// Shrunken face mesh
	buildShrunkenFaces(vertex, faceVertex);

	// Usage
	cout << endl;
	cout << "Keyboard Input : space for play/pause" << endl;
	cout << "Keyboard Input : x for axes on/off" << endl;
	cout << "Keyboard Input : q/esc for quit" << endl;
	cout << endl;
	cout << "Keyboard Input : g for face with gap mesh/mesh" << endl;
	cout << "Keyboard Input : up/down to increase/decrease the gap between faces" << endl;
	cout << "Keyboard Input : n for vertex/face normal" << endl;
	cout << "Keyboard Input : e for rotation with Eigen" << endl;
	cout << "Keyboard Input : a for antialiasing on/off" << endl;
	cout << "Keyboard Input : b for backface culling on/off" << endl;
}

// Draw a mesh after setting up its material
void drawMesh()
{
	// Material
	setupColoredMaterial(Vector3f(0.95f, 0.95f, 0.95f));

	// Mesh
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < face.cols(); i++)
	{
		glColor3f(0, 0, 0); // 질문.
		glNormal3fv(vertexNormal.col(face(0, i)).data());
		glVertex3fv(vertex.col(face(0, i)).data());

		glNormal3fv(vertexNormal.col(face(1, i)).data());
		glVertex3fv(vertex.col(face(1, i)).data());

		glNormal3fv(vertexNormal.col(face(2, i)).data());
		glVertex3fv(vertex.col(face(2, i)).data());
	}
	glEnd();
}

// Draw shrunken faces
void drawShurnkenFaces()
{
	//Material
	setupColoredMaterial(Vector3f(0.25f, 0.87f, 0.81f));
	
	// Mesh
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < face.cols(); i++)
	{
		//glColor3f(0.0f, 1.0f, 0.0f);
		if (useFaceNormal) glNormal3fv(faceNormal.col(i).data());
		for (int j = 0; j < 3; j++)
		{
			if (!useFaceNormal) glNormal3fv(vertexNormal.col(face(j, i)).data());
			glVertex3fv(faceVertex.col(3 * i + j).data());
		}
	}
	glEnd();
}

void render(GLFWwindow* window)
{
	//Antialiasing
	if (aaEnabled) glEnable(GL_MULTISAMPLE);
	else		   glDisable(GL_MULTISAMPLE);

	// Back face culling
	if (bfcEnabled)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
	}
	else   glDisable(GL_CULL_FACE);

	// Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(eye[0], eye[1], eye[2], center[0], center[1], center[2], up[0], up[1], up[2]);

	//Axes 
	if (axes)
	{
		glDisable(GL_LIGHTING);
		drawAxes(AXIS_LENGTH, AXIS_LINE_WIDTH);
	}

	// Lighting
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	setupLight();

	// Roatation about the y-axis
	if (eigen)
	{
		// 4x4 homogeneous identity matrix
		Matrix4f T; T.setIdentity();

		// Rotation matrix: theta in Radian
		T.block<3, 3>(0, 0) = Matrix3f(AngleAxisf(theta, Vector3f::UnitY()));

		// Translation vector
		T.block<3, 1>(0, 3) = Vector3f(0.0f, 0.0f, 0.0f);

		glMultMatrixf(T.data());
	}
	else
	{
		glRotatef(theta / float(M_PI) * 100.0f, 0, 1, 0);
	}

	// Draw the mesh after setting up the material
	if (faceWithGapMesh)
	{
		//원본 토끼를 그린다.
		drawMesh();

		//wireframe을 그린다.
		glDisable(GL_LIGHTING); // Lighting을 끈다.
		glEnable(GL_POLYGON_OFFSET_LINE); //Offset
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Polygon을 Line으로 그린다.
		glPolygonOffset(0.0f, 0.0f); // offset을 0.0, 0.0으로 준다.
		drawMesh(); //이 상태로 그리면 wireFrame되어있다.
		
		glEnable(GL_LIGHTING); // 다시 Lighting을 킨다.
		glDisable(GL_POLYGON_OFFSET_LINE);

		// shurnkenFace를 그린다.
		glEnable(GL_POLYGON_OFFSET_FILL); 
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Polygon을 면을 채우게끔 그린다.
		glPolygonOffset(-1.0f, -1.0f); // offset을 준다.
		drawShurnkenFaces(); // 이 상태로 그리면 wireframe+원본 토끼 위에 그려진다.
		glDisable(GL_POLYGON_OFFSET_FILL);

	}
	else    drawMesh();
	
}

// Material
void setupColoredMaterial(const Vector3f& color)
{
	//Material
	GLfloat mat_ambient[4] = { 0.1f,0.1f,0.1f,1.0f };
	GLfloat mat_diffuse[4] = { color[0],color[1],color[2],1.0f };
	GLfloat mat_specular[4] = { 0.5f,0.5f,0.5f,1.0f };
	GLfloat mat_shininess = 100.0f;

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

// Light
void setupLight()
{
	GLfloat ambient[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	GLfloat diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat specular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light.data());

}


void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
			//Quit
		case GLFW_KEY_Q:
		case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GL_TRUE); break;

			//Play on/off
		case GLFW_KEY_SPACE: pause = !pause; break;

			//Eigen/OpenGL rotation
		case GLFW_KEY_E: eigen = !eigen; break;
		
			// Face with gap mesh/mesh
		case GLFW_KEY_G: faceWithGapMesh = !faceWithGapMesh; break;

			// Gap increase/decrease
		case GLFW_KEY_UP:
			gap = min(gap + 0.05f, 0.5f);
			buildShrunkenFaces(vertex, faceVertex);
			break;
		case GLFW_KEY_DOWN:
			gap = max(gap - 0.05f, 0.05f);
			buildShrunkenFaces(vertex, faceVertex);
			break;

			//Normal vector
		case GLFW_KEY_N: useFaceNormal = !useFaceNormal; break;

			//Antialiasing on/off
		case GLFW_KEY_A: aaEnabled = !aaEnabled; break;

			// Back face culling
		case GLFW_KEY_B: bfcEnabled = !bfcEnabled; break;

			// Axes on/off
		case GLFW_KEY_X: axes = !axes; break;
		}
	}
}