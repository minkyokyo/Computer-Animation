// 길이에 따라 샘플을 달리 하는 것이 exercise

#include "glSetup.h"

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
using namespace std;

void init();
void render(GLFWwindow* window);
void reshape(GLFWwindow* window, int w, int h);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Controls
bool sampledPointsEnabled = true;
int N_SUB_SEGMENTS = 10; // 각 커브 세그먼트에서 몇개의 샘플링을 할 것인지?

int main(int argc, char* argv[])
{
	// Orthographics viewing
	perspectiveView = false;

	// Initialize the OpenGL system
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor);
	if (window == NULL) return -1;

	// Callbacks
	glfwSetKeyCallback(window, keyboard);

	// Depth Test
	glDisable(GL_DEPTH_TEST);

	// Viewport and perspective setting
	reshape(window, windowW, windowH);

	// Initialization - Main loop - Fianlization
	init(); // 식을 푼다

	while (!glfwWindowShouldClose(window))
	{
		render(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	// Terminate the glfw system
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

//(x,y,z) of data points
const int N = 4; // curve segement -> 4개
const float p[N + 1][3] = { // 5개의 데이터 포인트
	{-1.2f,-0.6f,0},
	{-0.6f,0.3f,0},
	{0,0,0},
	{0.6f,-0.2f,0},
	{1.2f,0.6f,0}
};

// Linear system : Ac=b
//	from n equations of the form:
//	p_i(t) = c_0^i + (c_1^i * t^1) + (c_2^i * t^2) + (c_3^i * t^3)
//
MatrixXf A; // 4n x 4n matrix
MatrixXf b; // 4n x 3 to solve the 3 linear systems at once
MatrixXf c; // 4n x 3 to solve the 3 linear systems at once

void buildLinearSystem()
{
	// Build A and b for N segments. A is independent of the locations of the points.
	A.resize(4 * N, 4 * N);
	A.setZero();

	b.resize(4 * N, 3);

	// Equation number
	int row = 0;

	// 2n equations for the end point interpolation
	for (int i = 0; i < N; i++, row += 2)
	{
		// p_i(0) = c_0^i
		A(row, 4 * i + 0) = 1;

		b(row, 0) = p[i][0];
		b(row, 1) = p[i][1];
		b(row, 2) = p[i][2];

		// p_i(1) = c_0^i + c_1^i + c_2^i + c_3^i
		A(row + 1, 4 * i + 0) = 1;
		A(row + 1, 4 * i + 1) = 1;
		A(row + 1, 4 * i + 2) = 1;
		A(row + 1, 4 * i + 3) = 1;

		b(row + 1, 0) = p[i + 1][0];
		b(row + 1, 1) = p[i + 1][1];
		b(row + 1, 2) = p[i + 1][2];
	}

	// (n-1) equations for the tangential con
	for (int i = 0; i < N - 1; i++, row++)
	{
		A(row, 4 * i + 1) = 1;
		A(row, 4 * i + 2) = 2;
		A(row, 4 * i + 3) = 3;
		A(row, 4 * i + 5) = -1;

		b(row, 0) = 0;
		b(row, 1) = 0;
		b(row, 2) = 0;
	}

	// (n-1) equations for the second - derivative continuity
	for (int i = 0; i < N - 1; i++, row++)
	{
		A(row, 4 * i + 2) = 2;
		A(row, 4 * i + 3) = 6;
		A(row, 4 * i + 6) = -2;

		b(row, 0) = 0;
		b(row, 1) = 0;
		b(row, 2) = 0;
	}

	// 2 equations for the natural boundary condition
	{
		A(row, 2) = 0;

		b(row, 0) = 0;
		b(row, 1) = 0;
		b(row, 2) = 0;

		row++;

		// p''_(n-1)(1) = 2*c_2^(n-1) + 6*c_3^(n-1) = 0
		A(row, 4 * (N - 1) + 2) = 2;
		A(row, 4 * (N - 1) + 3) = 6;

		b(row, 0) = 0;
		b(row, 1) = 0;
		b(row, 2) = 0;

		row++;
	}

}


void solveLinearSystem()
{
	c = A.colPivHouseholderQr().solve(b);
}

void init()
{
	// Build and solve the linear system for given problem
	buildLinearSystem();
	solveLinearSystem();

	// Usage
	cout << endl;
	cout << "Keyboard Input: s for sampled points on/off" << endl;
	cout << "Keyboard Input: up/down to increase/decrease the number of samples" << endl;
	cout << "Keyboard Input: q/esc for quit" << endl;

}

Vector3f pointOnNaturalCubicSplineCurve(int i, float t)
{
	float x = c(4 * i + 0, 0) + (c(4 * i + 1, 0) + (c(4 * i + 2, 0) + c(4 * i + 3, 0) * t) * t) * t;
	float y = c(4 * i + 0, 1) + (c(4 * i + 1, 1) + (c(4 * i + 2, 1) + c(4 * i + 3, 1) * t) * t) * t;
	float z = c(4 * i + 0, 2) + (c(4 * i + 1, 2) + (c(4 * i + 2, 2) + c(4 * i + 3, 2) * t) * t) * t;

	return Vector3f(x, y, z);
}

// Draw the natural cubic spline
void drawNaturalCubicSpline()
{

	for (int i = 0; i < N; i++)
	{
		// N_SUB_SEGMENTS for each curve segment
		
		// 미분 :
		// midpoint rule : ( 0+1 )/2 = 1/2, 1-0 = 1
		// t = 1/2일 때가 적분 값. c = 4n x 3 행렬이다.
	
		float x = c(4 * i + 1, 0) + (2*c(4 * i + 2, 0) + 3*c(4 * i + 3, 0) * 0.5)  * 0.5;
		float y = c(4 * i + 1, 1) + (2*c(4 * i + 2, 1) + 3*c(4 * i + 3, 1) * 0.5)  * 0.5;
		float z = c(4 * i + 1, 2) +  (2*c(4 * i + 2, 2) + 3*c(4 * i + 3, 2) * 0.5)  * 0.5;
		
		Vector3f vec(x,y,z);

		float L =sqrt(vec.dot(vec)); // L은 커브의 길이.

		N_SUB_SEGMENTS =int(L *5 );
		
		// Curve
		glLineWidth(1.5f * dpiScaling);
		glColor3f(0, 0, 0);
		glBegin(GL_LINE_STRIP);

		for (int j = 0; j <= N_SUB_SEGMENTS; j++)
		{
			float t = (float)j / N_SUB_SEGMENTS; //[0,1]
			Vector3f p = pointOnNaturalCubicSplineCurve(i, t);

			glVertex3fv(p.data());
		}
		glEnd();

		glPointSize(5 * dpiScaling);
		glColor3f(0, 0, 0);
		glBegin(GL_POINTS);
		
		// N_SUB_SEGMENTS for each curve segment
		for (int j = 1; j < N_SUB_SEGMENTS; j++)
		{
			float		t = (float)j / N_SUB_SEGMENTS;
			Vector3f	p = pointOnNaturalCubicSplineCurve(i, t);

			glVertex3fv(p.data());
		}
		glEnd();
	}

	// dATA POINTS
	glPointSize(10 * dpiScaling);
	glColor3f(1, 0, 0);
	glBegin(GL_POINTS);
	for (int i = 0; i < N + 1; i++)
		glVertex3f(p[i][0], p[i][1], p[i][2]);
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

	// Draw the natural cubic spline curve
	drawNaturalCubicSpline();
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
			// Sampled points
		case GLFW_KEY_S: sampledPointsEnabled = !sampledPointsEnabled; break;

			// Number of samples
		case GLFW_KEY_UP: N_SUB_SEGMENTS++; break;
		case GLFW_KEY_DOWN: N_SUB_SEGMENTS = max(N_SUB_SEGMENTS - 1, 1); break;

			// Quit
		case GLFW_KEY_Q:
		case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GL_TRUE); break;
		}
	}
}