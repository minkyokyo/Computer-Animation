#include "glSetup.h"
#include "mesh.h"

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
using namespace std;

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>

void init(const char* filename);
void setupLight();

//void update(float elapsed);
void render(GLFWwindow* window);
void reshape(GLFWwindow* window, int w, int h);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);
void getTimeperT();
// Play configuration
bool pause = false;

// Camera configuation
Vector3f eye(9, 9, 9);
Vector3f center(0, 0, 0);
Vector3f up(0, 1, 0);

// Light configuration
Vector4f light(5.0, 5.0, 0.0, 1);

// Global coordinate frame
float AXIS_LENGTH = 3;
float AXIS_LINE_WIDTH = 2;

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Default mesh file name
const char* defaultMeshFileName = "m01_dinosaur.off";

//Mesh
MatrixXf vertex1;
MatrixXf normal1;
ArrayXXi face1;

MatrixXf vertex2;
MatrixXf normal2;
ArrayXXi face2;

// Time
float timeStep = 1.0f / 120; // 120fps
float currTime = 0;

// Transformation1
Matrix4f T;			// Current orientation and position
Quaternionf q1, q2; // Two key orientations
Vector3f p1, p2;	// Two key positions

// Transformation2
Matrix4f Q_T;			// Current orientation and position
Quaternionf Q_q1, Q_q2; // Two key orientations
Vector3f Q_p1, Q_p2;	// Two key positions


Matrix4f Ts[11] = {};
Matrix4f Q_Ts[11] = {};

// Interpolation duration
float interval = 3; // Seconds. 3초에 한번 인터폴레이션됨.

// Interpolation method
int method = 0;

const char* methodString[] =
{
	"Linear interpolation of rotation matrices",
	"Spherical linear interpolation using Eigen's slerp",
};

int main(int argc, char* argv[])
{
	// vsync should be 0 for precise time stepping
	vsync = 0;

	// Filename for deformable body configuration
	const char* filename;
	if (argc >= 2) filename = argv[1];
	else           filename = defaultMeshFileName;

	// Field of view of 85mm lens in degree
	fovy = 16.1;

	// Initialize the OpenGL system
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor);
	if (window == NULL) return -1;

	// Callbacks
	glfwSetKeyCallback(window, keyboard);

	// Depth test
	glEnable(GL_DEPTH_TEST);

	// Normal vectors are normalized after transformation.
	glEnable(GL_NORMALIZE);

	// Back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);


	// Viewport and perspective setting.
	reshape(window, windowW, windowH);

	// Usage
	cout << endl;
	for (int i = 0; i < 2; i++)
		cout << "Keyboard Input: " << (i + 1) << " for" << methodString[i] << endl;
	cout << endl;

	// Initialization - Main loop - Finalization
	init(filename);
	getTimeperT();

	// Main loop
	float previous = (float)glfwGetTime();
	float elapsed = 0;
	while (!glfwWindowShouldClose(window))
	{
		// Time passed during a single loop
		float now = (float)glfwGetTime();
		float delta = now - previous;
		previous = now;

		// Time passed after the previous frame
		elapsed += delta;

		// Deal with the current frame
		if (elapsed > timeStep)
		{
			//// Animate 1 frame
			//if (!pause) update(elapsed);
			//else        update(0);	// To compare interpolation methods

			elapsed = 0; // Reset the elapsed time
		}

		render(window);			 // Draw one frame
		glfwSwapBuffers(window); // Swap buffers
		glfwPollEvents();		 // Events
	}

	// Terminate the glfw system
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void init(const char* filename)
{
	// Mesh from the file
	cout << "Reading" << filename << endl;
	readMesh(filename, vertex1, normal1, face1);
	readMesh(filename, vertex2, normal2, face2);

	// Initial transformation of the mesh
	T.setIdentity();
	Q_T.setIdentity();

	// Position
	p1 = Vector3f(-1, 0.5, 2);
	p2 = Vector3f(2, 0.5, -1);

	// Position
	Q_p1 = Vector3f(-1, 0.5, 2);
	Q_p2 = Vector3f(2, 0.5, -1);

	// Orientations: no rotation and 181 degree rotation
	Vector3f	axis = Vector3f(1, 1, 1).normalized();
	float		theta = 181;
	q1 = Quaternionf(AngleAxisf(0, axis));
	Q_q1 = Quaternionf(AngleAxisf(0, axis));
	cout << "q1 = " << q1.w() << ", " << q1.vec().transpose() << endl << endl;

	q2 = Quaternionf(AngleAxisf(theta * float(M_PI) / 180, axis)); // Radian으로 변환
	Q_q2 = Quaternionf(AngleAxisf(theta * float(M_PI) / 180, axis));

	cout << "q2 from theta = " << theta << " and axis = " << axis.transpose() << endl;
	cout << "q2 = " << q2.w() << ", " << q2.vec().transpose() << endl;
	// q2는 181도로 회전...(방향) 반대방향으로 179도 회전한 방향이랑 똑같움.

	// Orientation difference. Angle Axis deals with antipodal equivalence.
	// 181도 겠지뫈,, 179 회전으로 나온다.
	// aa.angle()/ M_PI * 180 값이 179.
	// 회전축은 반대로 나와야함.
	AngleAxisf aa(q1.inverse() * q2); // 쿼터니언으로부터 angleAxis를 얻어옴.
	cout << "q1 -> q2 : Angle = " << aa.angle() / M_PI * 180 << " degree, ";
	cout << "Axis = " << aa.axis().transpose() << endl << endl;

	// q3 ( = -q2 ) represent the same orientation with q2 owing to antipodal equivalece.
	// q3는 -q2
	Quaternionf q3(-q2.w(), -q2.x(), -q2.y(), -q2.z());
	cout << " q3 form -q2" << endl;
	cout << "q3 = " << q3.w() << ", " << q3.vec().transpose() << endl;

	// AngleAxis already deals with antipodal equivalence.
	// i.e. q1.inverse() * (-q2) was already employed int (1).
	// It can be verfied by the fllowing computation.
	aa = AngleAxisf(q1.inverse() * q3);
	cout << "q1 -> q2 : Angle = " << aa.angle() / M_PI * 180 << " degree, ";
	cout << "Axis = " << aa.axis().transpose() << endl << endl;
}

Matrix3f rlerp(float t, Quaternionf& q1, Quaternionf& q2)
{
	// Linear interpolation of the rotation parts in the 4x4 homogenous matrix
	Matrix3f R = (1 - t) * Matrix3f(q1) + t * Matrix3f(q2);
	// q1, q2 방향.
	return R;
}

// quaternion linear interpolation
Matrix3f qlerp(float t, Quaternionf& q1, Quaternionf& q2)
{
	Quaternionf q;

	// Linear interpolation of quaternions
	q.w() = (1 - t) * q1.w() + t * q2.w();
	q.x() = (1 - t) * q1.x() + t * q2.x();
	q.y() = (1 - t) * q1.y() + t * q2.y();
	q.z() = (1 - t) * q1.z() + t * q2.z();

	// Normalization
	q.normalize();
	return Matrix3f(q);
}

void getTimeperT()
{	
	// Transformation
	Matrix4f TTrans;
	Matrix4f QTrans;			// Current orientation and position
	
	// Initial transformation of the mesh
	TTrans.setIdentity();
	QTrans.setIdentity();

	for (int i = 0; i <11; i++) {
		
		float t = 0.1 * i;
		Vector3f p = (1 - t) * p1 + t * p2;
		TTrans.block<3, 1>(0, 3) = p;

		// Linear interpolation of the given two positions in all cases
		// Set the translational part in the 4x4 homogenous matrix
		Vector3f p2 = (1 - t) * Q_p1 + t * Q_p2;
		QTrans.block<3, 1>(0, 3) = p2;


		// Linear interpolation of the given two rotation matrices
		// Set the rotational part in the 4x4 homogenous matrix
		TTrans.block<3, 3>(0, 0) = rlerp(t, q1, q2);

		Quaternionf q;
		// Slerp provided by Eigen
		q = Q_q1.slerp(t, Q_q2);
		// Set the 3x3 rotational part of the 4x4 homogeous matrix
		QTrans.block<3, 3>(0, 0) = Matrix3f(q);

		Ts[i] = TTrans;
		Q_Ts[i] = QTrans;
	}
}


// Draw a sphere after setting up its material
void drawMesh(MatrixXf vertex,MatrixXf normal,ArrayXXi face, bool alpha )
{
	int a = 1;
	GLfloat mat_diffuse[4]= { 0.15f, 0.85f, 0.15f, 1 };
	GLfloat mat_ambient[4] = { 0.10f, 0.10f, 0.10f, 0.2};
	if (alpha)
	{
		a = 0.1;
		mat_diffuse[0] =0.85f ;
		mat_diffuse[1] = 0.35f;
		mat_diffuse[2] = 0.7f;
		mat_diffuse[3] = a;
	}

	GLfloat mat_specular[4] = { 0.10f, 0.10f, 0.10f, 0.4 };
	GLfloat mat_shininess = 128;

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse );
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

	// Mesh
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < face.cols(); i++)
	{
		glNormal3fv(normal.col(face(0, i)).data());
		glVertex3fv(vertex.col(face(0, i)).data());

		glNormal3fv(normal.col(face(1, i)).data());
		glVertex3fv(vertex.col(face(1, i)).data());

		glNormal3fv(normal.col(face(2, i)).data());
		glVertex3fv(vertex.col(face(2, i)).data());
	}
	glEnd();
}

void render(GLFWwindow* window)
{

	// Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(eye[0], eye[1], eye[2], center[0], center[1], center[2], up[0], up[1], up[2]);

	//Axes 
	glDisable(GL_LIGHTING);
	drawAxes(AXIS_LENGTH, AXIS_LINE_WIDTH);

	// Lighting
	setupLight();
	
	// 계쏙 그린다.
	for (int i = 0; i < 11; i++)
	{
		glPushMatrix();
		glMultMatrixf(Ts[i].data());
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Polygon을 Line으로 그린다.
		drawMesh(vertex2, normal2, face2,true);
		glPopMatrix();

		glPushMatrix();
		glMultMatrixf(Q_Ts[i].data());
	    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Polygon을 면을 채우게끔 그린다.
		drawMesh(vertex1, normal1, face1,false);
		glPopMatrix();

	}
}

// Light
void setupLight()
{
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	GLfloat ambient[4] = { 0.1f, 0.1f, 0.1f, 1 };
	GLfloat diffuse[4] = { 1.0f, 1.0f, 1.0f, 1 };
	GLfloat specular[4] = { 1.0f, 1.0f, 1.0f, 1 };

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

		case GLFW_KEY_1:	method = 1; cout << methodString[method - 1] << endl; break;
		case GLFW_KEY_2:	method = 2; cout << methodString[method - 1] << endl; break;

			// Play/pause toggle
		case GLFW_KEY_SPACE: pause = !pause; break;
		}
	}
}
