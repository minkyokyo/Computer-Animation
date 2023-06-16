// 길이에 따라 샘플을 달리 하는 것이 exercise

#include "glSetup.h"

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
#include <list> 
#include <vector>
using namespace std;

void init();
void render(GLFWwindow* window);
void reshape(GLFWwindow* window, int w, int h);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButton(GLFWwindow* window, int button, int action, int mods);
void mouseMove(GLFWwindow* window, double xs, double ys);
void unProject(double x, double y, GLdouble* wx, GLdouble* wy, GLdouble* wz);

bool selectedDataPoint(GLdouble x_ws, GLdouble y_ws);
bool selectedEdge(GLdouble x_ws, GLdouble y_ws);
void addDataPoint(GLdouble x_ws, GLdouble y_ws);
void eraseDataPoint(GLdouble x_ws, GLdouble y_ws);
void dragDataPoint(GLdouble x_ws, GLdouble y_ws);
void insertDataPoint(GLdouble x_ws, GLdouble y_ws);


// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Controls
bool sampledPointsEnabled = false;
int N_SUB_SEGMENTS = 5; // 각 커브 세그먼트에서 몇개의 샘플링을 할 것인지?

// Picking
enum Mode {
	NONE = 0, ADD = 1, ERASE = 2, DRAG= 3, INSERT =4,
};
Mode interactMode = NONE;

// dataPoint
list<vector<GLdouble>> p; 
list<vector<GLdouble>>::iterator _iter = p.begin(); // dataPoint list iter
int N = -1;											// curve segement -> 4개
int dataPointIdx = -1;

// Samples
vector<vector<float>> samples;
int selectedEdgeIndex = -1;
Vector3f dataPointInserted;

// selected point
list<vector<GLdouble>>::iterator selectIter;
bool IsSelectedPoint = false;
bool IsSelectedEdge = false;

// mouse info
GLdouble preX, preY;

void mouseMove(GLFWwindow* window, double xs, double ys)
{
	// Drag 모드가 아니거나 드래그 하고있지 않으면 return
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE || interactMode != DRAG) return;
	
	if (!IsSelectedPoint) // 선택된게 없어서 움직일 게 없다.
		return;

	// Mouse cursor position in the framebuffer coordinate
	double x = xs * dpiScaling;
	double y = ys * dpiScaling;

	// 마우스 좌표 unProject.
	GLdouble x_ws, y_ws, z_ws;
	unProject(x, y, &x_ws, &y_ws, &z_ws);
	dragDataPoint(x_ws, y_ws);

}

// 버튼을 누를 때 마다, 마우스 클릭의 월드 좌표를 알아낸다.
void mouseButton(GLFWwindow* window, int button, int action, int mods)
{
	double xs, ys;
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
	{
		// Mouse cursor position in the screen coordinate
		glfwGetCursorPos(window, &xs, &ys);

		// Mouse cursor position in the framebuffer coordinate
		// 이거를 unproject하면 world space에서 x,y,z를 가질 수 있다.
		// z는 상관하지 않는다. 어차피 x,y평면에 있기 때문
		xs = xs * dpiScaling;
		ys = ys * dpiScaling;

		if (interactMode == NONE)
			return;

		GLdouble x_ws, y_ws, z_ws;
		unProject(xs, ys, &x_ws, &y_ws, &z_ws);
		if (preX == x_ws && preY == y_ws) {
			return;
		}

		if (interactMode == ADD) addDataPoint(x_ws, y_ws);
		else if (interactMode == ERASE) eraseDataPoint(x_ws, y_ws);
		else if (interactMode == DRAG) selectedDataPoint(x_ws,y_ws);
		else if (interactMode == INSERT) insertDataPoint(x_ws,y_ws);

		preX = x_ws; preY = y_ws;
	}
	
}


void unProject(double sx, double sy, GLdouble* wx, GLdouble* wy, GLdouble* wz)
{
	GLdouble projection[16];
	GLdouble modelView[16];
	GLint viewPort[4];
	glGetDoublev(GL_PROJECTION_MATRIX, projection);
	glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
	glGetIntegerv(GL_VIEWPORT, viewPort);

	GLfloat zCursor, winX, winY;
	// window coordinate X, Y, Z change to openGL screen coordinates
	winX = (float)sx;
	winY = (float)viewPort[3] - (float)sy;

	if (gluUnProject(winX, winY, 0, modelView, projection, viewPort, wx, wy, wz) == GLU_FALSE) {
		printf("failed to unproject\n");
	}
}

void addDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	vector<GLdouble> v = { x_ws, y_ws, 0 };
	p.push_back(v);
	N += 1;
}

void eraseDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	if (selectedDataPoint(x_ws, y_ws))
	{
		p.erase(selectIter); // 가까운 것 삭제.
		N -= 1;
	}
}

void dragDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	(*selectIter)[0] = x_ws;
	(*selectIter)[1] = y_ws;
}

void insertDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	if (selectedEdge(x_ws, y_ws))
	{
		list<vector<GLdouble>>::iterator iter = p.begin();

		// 현재 egde가 몇 번 째 datapoint의 edge인지 알기 위해서 iter를 증가시킨다.
		for (int i = 0; i < int(selectedEdgeIndex / (N_SUB_SEGMENTS - 1))+1; i++, iter++) {}

		//0번째 일 때, 0~1사이에 껴 넣야한다. 이러면 iter++해서 넣으면 된다.
		
		vector<GLdouble> v;
		v.push_back(dataPointInserted[0]);
		v.push_back(dataPointInserted[1]);
		v.push_back(0);

		// iter가 맨 끝이라면, 그 앞에다 추가해야하기 때문에
		if (iter == p.end()) p.insert(--iter, v);
		else p.insert(iter,v);
		N += 1;

	}
}

bool selectedDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	// 마우스로 찍은 좌표와 가장 가까운 dataPoint를 찾아야 한다.
	// 마우스 좌표와 가까운 dataPoint 거리가 0.015 이내면 클릭한 것으로 간주한다.
	// (x1-x2)^2 + (y1-y2)^2 값이 0.0002 이내면 클릭한 것으로 간주.

	float minDist = 1;
	list<vector<GLdouble>>::iterator minIter;

	for (list<vector<GLdouble>>::iterator iter = p.begin(); iter != p.end(); iter++)
	{
		float dist = ((*iter)[0] - x_ws) * ((*iter)[0] - x_ws) + ((*iter)[1] - y_ws) * ((*iter)[1] - y_ws);
		if (dist <= 0.0002)
		{
			if (minDist > dist) {
				minDist = dist;
				minIter = iter;
			}
		}
	}
	if (minDist == 1) // 0.002이내에 점이 없다
	{
		IsSelectedPoint = false;
		return false;
	}

	selectIter = minIter;
	IsSelectedPoint = true;
	
	return true;
}

bool selectedEdge(GLdouble x_ws, GLdouble y_ws)
{
	float minDist = 1;
	int minIndex;

	//sample point를 전부 순회...
	for (int i = 0; i < samples.size()-1; i++)
	{
		// samples[i]와 samples[i+1]의 직선의 방정식을 구한다.
		// 기울기
		float m = (samples[i+1][1] - samples[i][1]) / (samples[i + 1][0] - samples[i][0]);
		// y절편
		float c = -m * samples[i][0] + samples[i][1];

		// 점에서 직선에 내린 수선의 발을 구한다.
		// 수직을 지나는 직선의 기울기 : -1/m
		// y = -1/m + b <- 마우스 좌표를 넣어서 b를 득템.
		// 그 후 연립 방정식으로?
		float cPrime = (1 / m) * x_ws + y_ws;

		// x랑 y는 수선의 발이다.
		float x = (cPrime - c) / (m + (1 / m));
		float y = m * x + c;

		float tempX1 = x-samples[i][0]; float tempY1 = y-samples[i][1];
		float tempX2 = samples[i + 1][0]-samples[i][0]; float tempY2 = samples[i + 1][1]-samples[i][1];

		Vector2f v1(tempX1,tempY1); // samples[i]번째 점에서 수선의 발로 가는 벡터.
		Vector2f v2(tempX2,tempY2); // samples[i]에서 samples[i+1]로 가는 벡터

		float dist;
		Vector2f tempInsert;


		if (v1.dot(v2) > 0 && (v1.dot(v1) <= v2.dot(v2)))// 방향이 같고 길이가 v1이 더 작다면 수선의 발이 선분 내부에 있다.
		{
			dist = (x - x_ws) * (x - x_ws) + (y - y_ws) * (y - y_ws);// 거리는 점과 직선 사이의 거리. or 수선의 발과 마우스 좌표까지의 거리 
			tempInsert[0] = x;
			tempInsert[1] = y;
		}
		else {
			if (v1.dot(v2) < 0)
			{
				dist = (samples[i][0] - x_ws) * (samples[i][0] - x_ws) + (samples[i][1] - y_ws) * (samples[i][1] - y_ws); // 거리는 sample point와의 거리.
				tempInsert[0] = samples[i][0];
				tempInsert[1] = samples[i][1];
			}
			else
			{
				dist = (samples[i + 1][0] - x_ws) * (samples[i + 1][0] - x_ws) + (samples[i + 1][1] - y_ws) * (samples[i + 1][1] - y_ws);
				tempInsert[0] = samples[i+1][0];
				tempInsert[1] = samples[i+1][1];
			}
		}
		
		if (dist <= 0.0004)
		{
			if (minDist > dist) {
				dataPointInserted[0] = tempInsert[0];
				dataPointInserted[1] = tempInsert[1];
				minDist = dist;
				minIndex= i;
			}
		}
	}

	if (minDist == 1) // 0.002이내에 점이 없다
	{
		IsSelectedEdge = false;
		return false;
	}

	IsSelectedEdge = true;
	selectedEdgeIndex = minIndex;


	return true;
	
}

int main(int argc, char* argv[])
{
	// Orthographics viewing
	perspectiveView = false;

	// Initialize the OpenGL system
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor);
	if (window == NULL) return -1;

	// Callbacks
	glfwSetKeyCallback(window, keyboard);
	glfwSetMouseButtonCallback(window, mouseButton);
	glfwSetCursorPosCallback(window, mouseMove);

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
	int i = 0;
	for (list<vector<GLdouble>>::iterator iter = p.begin(); i<N; iter++,i++,row+=2)
	{
		// p_i(0) = c_0^i
		A(row, 4 * i + 0) = 1;

		b(row, 0) = (*iter)[0];
		b(row, 1) = (*iter)[1];
		b(row, 2) = (*iter)[2];

		// p_i(1) = c_0^i + c_1^i + c_2^i + c_3^i
		A(row + 1, 4 * i + 0) = 1;
		A(row + 1, 4 * i + 1) = 1;
		A(row + 1, 4 * i + 2) = 1;
		A(row + 1, 4 * i + 3) = 1;

		iter++;
		b(row + 1, 0) = (*iter)[0];
		b(row + 1, 1) = (*iter)[1];
		b(row + 1, 2) = (*iter)[2];
		iter--;
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
	// Curve
	glLineWidth(1.5f * dpiScaling);
	glColor3f(0, 0, 0);
	for (int i = 0; i < N; i++)
	{
		// N_SUB_SEGMENTS for each curve segment
		glBegin(GL_LINE_STRIP);
		for (int j = 0; j <= N_SUB_SEGMENTS; j++)
		{
			float t = (float)j / N_SUB_SEGMENTS; //[0,1]
			Vector3f p = pointOnNaturalCubicSplineCurve(i, t);

			glVertex3fv(p.data());
		}
		glEnd();
	}

	// Sample points at the curve
	if (sampledPointsEnabled)
	{
		glPointSize(5 * dpiScaling);
		glColor3f(0, 0, 0);
		glBegin(GL_POINTS);

		vector<vector<float>> samples_;
		for (int i = 0; i < N; i++)
		{	
			// N_SUB_SEGMENTS for each curve segment
			for (int j=1; j < N_SUB_SEGMENTS; j++)
			{
				vector<float> points(3);

				float		t = (float)j / N_SUB_SEGMENTS;
				Vector3f	p = pointOnNaturalCubicSplineCurve(i, t);
				glVertex3fv(p.data());
				points[0]=p[0];
				points[1]=p[1];
				points[2]=p[2];
				samples_.push_back(points);
			}
		}
		samples = samples_;
		glEnd();
	}
}

void render(GLFWwindow* window)
{
	// Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


	// DATA POINTS
	glPointSize(10 * dpiScaling);
	glColor3f(1, 0, 0);
	glBegin(GL_POINTS);
	
	// 현재까지 클릭한 dataPointIdx까지 그려주어야한다.
	for (list<vector<GLdouble>>::iterator iter = p.begin(); iter != p.end(); iter++)
	{		
		glVertex3f((*iter)[0], (*iter)[1], (*iter)[2]);
	}
	glEnd();
	
	
	// 찍은 데이터 포인트가 2개 이상이면 곡선을 그린다.
	if (N >=1) {
		buildLinearSystem();
		solveLinearSystem();
		drawNaturalCubicSpline(); // Draw the natural cubic spline curve
	}
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch(key)
		{
			// Sampled points
		case GLFW_KEY_S: sampledPointsEnabled = !sampledPointsEnabled; break;
			
			// Number of samples
		case GLFW_KEY_UP: N_SUB_SEGMENTS++; break;
		case GLFW_KEY_DOWN: N_SUB_SEGMENTS = max(N_SUB_SEGMENTS - 1, 1); break;

			// Add 
		case GLFW_KEY_A: interactMode = ADD; break;

			// Erase
		case GLFW_KEY_R: interactMode = ERASE; break;
			
			// Drag
		case GLFW_KEY_D: interactMode = DRAG; break;
			
			// Insert
		case GLFW_KEY_I: interactMode = INSERT; break;
			// Quit
		case GLFW_KEY_Q:
		case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GL_TRUE); break;
		}
	}
}