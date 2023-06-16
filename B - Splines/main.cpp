#include "glSetup.h"
#include "hsv2rgb.h"

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
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

// Endpoint interpolation
void prepareRepeatedControlPoints(int repetition);
void deleteRepeatedControlPoints();

void drawControlPolygon();

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Controls
bool samplePointDrawingEnabled = false;
int N_SUB_SEGMENTS = 10;

bool ctrlPolygonDrawingEnabled = false;
int iSegment = 0;

int REPETITION = 0;

//(x,y,z) of data points
int N = 0; // curve segement 
vector<vector<GLdouble>> points; // control points

// sample Points
vector < vector<float>> samples;
int selectedEdgeIndex = -1;
Vector3f dataPointInserted;


// Picking
enum Mode {
	NONE = 0, ADD = 1, ERASE = 2, DRAG = 3, INSERT = 4,
};
Mode interactMode = NONE;

// selected point
//list<vector<GLdouble>>::iterator selectIter;
bool IsSelectedPoint = false;
bool IsSelectedEdge = false;
int selectedIndex = -1;

// mouse info
GLdouble preX, preY;

// Including the repeated control points
int		  nControlPoints = 0;
Vector3f* controlPoints = NULL;

void addDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	vector<GLdouble> v = { x_ws, y_ws, 0 };
	points.push_back(v);
	N += 1;
}

void eraseDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	if (selectedDataPoint(x_ws, y_ws)) {
		points.erase(points.begin() + selectedIndex); // ����� �� ����.
		N -= 1;
	}
}

void dragDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	points[selectedIndex][0] = x_ws;
	points[selectedIndex][1] = y_ws;
}

void insertDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	if (selectedEdge(x_ws, y_ws))
	{
		// ���� edge�� ��� ° control point�� edge���� �˾Ƴ���. -> �����ϱ� ����.
		//0��° �� ��, 0~1���̿� �� �־��Ѵ�.		
		int edgeIndex;
		
        // edge Index�� iSegment���� ���� ���õ� edge�� ���° ������ �˾Ƴ���.
	    edgeIndex = selectedEdgeIndex+ iSegment+1;
	
		vector<GLdouble> v;
		v.push_back(dataPointInserted[0]); v.push_back(dataPointInserted[1]); v.push_back(0);

		// Repetition��ŭ ������ ������ �����.
		points.insert(points.begin()+(edgeIndex-REPETITION), v); 
		N += 1;
	}
}
bool selectedDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	// ���콺�� ���� ��ǥ�� ���� ����� dataPoint�� ã�ƾ� �Ѵ�.
	// ���콺 ��ǥ�� ����� dataPoint �Ÿ��� 0.015 �̳��� Ŭ���� ������ �����Ѵ�.
	// (x1-x2)^2 + (y1-y2)^2 ���� 0.0002 �̳��� Ŭ���� ������ ����.

	float minDist = 1;
	int minIndex = -1;

	for (int i=0; i<N; i++)
	{
		float dist = (points[i][0] - x_ws) * (points[i][0] - x_ws) + (points[i][1] - y_ws) * (points[i][1] - y_ws);
		
		if (dist <= 0.0002)
		{
			if (minDist > dist) {
				minDist = dist;
				minIndex = i;
			}
		}
	}
	if (minDist == 1) // 0.002�̳��� ���� ����
	{
		IsSelectedPoint = false;
		return false;
	}

	selectedIndex = minIndex;
	IsSelectedPoint = true;

	return true;
}

bool selectedEdge(GLdouble x_ws, GLdouble y_ws)
{
	float minDist = 1;
	int minIndex;

	//sample point�� ���� ��ȸ... => ������ 4��
	for (int i = 0; i < 3; i++)
	{
		// samples[i]�� samples[i+1]�� ������ �������� ���Ѵ�.
		// ����
		float m = (samples[i + 1][1] - samples[i][1]) / (samples[i + 1][0] - samples[i][0]);
		// y����
		float c = -m * samples[i][0] + samples[i][1];

		// ������ ������ ���� ������ ���� ���Ѵ�.
		// ������ ������ ������ ���� : -1/m
		// y = -1/m + b <- ���콺 ��ǥ�� �־ b�� ����.
		// �� �� ���� ����������?
		float cPrime = (1 / m) * x_ws + y_ws;

		// x�� y�� ������ ���̴�.
		float x = (cPrime - c) / (m + (1 / m));
		float y = m * x + c;

		float tempX1 = x - samples[i][0]; float tempY1 = y - samples[i][1];
		float tempX2 = samples[i + 1][0] - samples[i][0]; float tempY2 = samples[i + 1][1] - samples[i][1];

		Vector2f v1(tempX1, tempY1); // samples[i]��° ������ ������ �߷� ���� ����.
		Vector2f v2(tempX2, tempY2); // samples[i]���� samples[i+1]�� ���� ����

		float dist;
		Vector2f tempInsert;


		if (v1.dot(v2) > 0 && (v1.dot(v1) <= v2.dot(v2)))// ������ ���� ���̰� v1�� �� �۴ٸ� ������ ���� ���� ���ο� �ִ�.
		{
			dist = (x - x_ws) * (x - x_ws) + (y - y_ws) * (y - y_ws);// �Ÿ��� ���� ���� ������ �Ÿ�. or ������ �߰� ���콺 ��ǥ������ �Ÿ� 
			tempInsert[0] = x;
			tempInsert[1] = y;
		}
		else {
			if (v1.dot(v2) < 0)
			{
				dist = (samples[i][0] - x_ws) * (samples[i][0] - x_ws) + (samples[i][1] - y_ws) * (samples[i][1] - y_ws); // �Ÿ��� sample point���� �Ÿ�.
				tempInsert[0] = samples[i][0];
				tempInsert[1] = samples[i][1];
			}
			else
			{
				dist = (samples[i + 1][0] - x_ws) * (samples[i + 1][0] - x_ws) + (samples[i + 1][1] - y_ws) * (samples[i + 1][1] - y_ws);
				tempInsert[0] = samples[i + 1][0];
				tempInsert[1] = samples[i + 1][1];
			}
		}

		if (dist <= 0.0004)
		{
			if (minDist > dist) {
				dataPointInserted[0] = tempInsert[0];
				dataPointInserted[1] = tempInsert[1];
				minDist = dist;
				minIndex = i;
			}
		}
	}

	if (minDist == 1) // 0.002�̳��� ���� ����
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
	glfwSetMouseButtonCallback(window, mouseButton);
	glfwSetCursorPosCallback(window, mouseMove);
	glfwSetKeyCallback(window, keyboard);

	// Depth Test
	glDisable(GL_DEPTH_TEST);

	// Normal vectors are normalized after transformation
	glEnable(GL_NORMALIZE);

	// Back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	// Viewport and perspective setting
	reshape(window, windowW, windowH);

	// Initialization - Main loop - Fianlization
	//init(); // ���� Ǭ��

	while (!glfwWindowShouldClose(window))
	{
		render(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Finalization
	deleteRepeatedControlPoints();

	// Terminate the glfw system
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}




// repetition = 2 for endpoint interpolation
void prepareRepeatedControlPoints(int repetition)
{
	//����
	deleteRepeatedControlPoints();

	nControlPoints = N + 2 * repetition; // ������ �ִ� POINT ���� + �ݺ�Ƚ�� +(������ ����)
	controlPoints = new Vector3f[nControlPoints];

	for (int i = 0; i < N; i++)
		controlPoints[i + repetition] = Vector3f(points[i][0], points[i][1], points[i][2]);

	for (int i = 0; i < repetition; i++)
	{
		controlPoints[i] = controlPoints[repetition];							// From beggining
		controlPoints[N + repetition + i] = controlPoints[N + repetition - 1];  // Before ending
	}
}

void deleteRepeatedControlPoints()
{
	if (controlPoints) delete[] controlPoints;
}

void init()
{
	// Initially, no repetition // �ʱ⿣ �ݺ� ����.

	// Usage
	cout << endl;
	cout << "Keyboard Input: [0,2] for # of repetitions in the endpoint interpolation" << endl;
	cout << "Keyboard Input: s for drawing sample points on/off" << endl;
	cout << "Keyboard Input: up/down to increase/decrease the number of samples" << endl;
	cout << "Keyboard Input: c for drawing control polygon on/off" << endl;
	cout << "Keyboard Input: left/right to move the control polygon" << endl;
	cout << "Keyboard Input: q/esc for quit" << endl;
	
}

// Compute the point on the B-spline
Vector3f pointOnBspline(const Vector3f b[4],float t1)  // t�� 1��
{
	float t2 = t1 * t1; // t�� 2��
	float t3 = t2 * t1; // t�� 3��

	float B0 = 1 - 3 * t1 + 3 * t2 - t3;
	float B1 = 4 - 6 * t2 + 3 * t3;
	float B2 = 1 + 3 * t1 + 3 * t2 - 3 * t3;
	float B3 = t3;
	
	return (b[0] * B0 + b[1] * B1 + b[2] * B2 + b[3] * B3 ) / 6;
}

// Draw the uniform cubic B-spline curve
void drawBSpline()
{
	// Colors
	float hsv[3] = { 0,1,1 };// [0,360] (degree), [0,1], [0,1]
	float rgb[3];

	// Curve
	Vector3f b[4];
	for (int i = 0; i < nControlPoints - 3; i++)
	{
		// To make an alternating complementary color
		hsv[0] = 180.0f * i / (nControlPoints - 3) + ((i % 2) ? 180.0f : 0);
		HSV2RGB(hsv, rgb);
		glColor3f(rgb[0], rgb[1], rgb[2]);
		
		if (ctrlPolygonDrawingEnabled && i == iSegment)
			glLineWidth(3 * dpiScaling); // ctrl polygon�� �׸� ������ �β���
		else  glLineWidth(1.5f * dpiScaling);
	
		for (int j = 0; j < 4; j++)
			b[j] = controlPoints[i + j];

		glBegin(GL_LINE_STRIP);
		for (int j = 0; j <= N_SUB_SEGMENTS; j++)
		{
			float t = (float)j / N_SUB_SEGMENTS;
			Vector3f pt = pointOnBspline(b, t);

			glVertex3fv(pt.data());
		}
		glEnd();
	}
	
	// Sample points at the curve
	if (samplePointDrawingEnabled)
	{

		
		glPointSize(5 * dpiScaling);
		for (int i = 0; i < nControlPoints-3; i++)
		{
			hsv[0] = 180.0f * i / (nControlPoints - 3) + ((i % 2) ? 180.0f : 0);
			HSV2RGB(hsv, rgb);
			glColor3f(rgb[0], rgb[1], rgb[2]);

			for (int j = 0; j < 4; j++)
				b[j] = controlPoints[i + j];

			glBegin(GL_POINTS);
			for (int j = 0; j <= N_SUB_SEGMENTS; j++)
			{
		
				float t = (float)j / N_SUB_SEGMENTS;
				Vector3f pt = pointOnBspline(b, t);

				glVertex3fv(pt.data());
			
			}
			glEnd();
		}
	
	}
	// Control polygon => exercise
	//void drawControlPolygon();
	if (ctrlPolygonDrawingEnabled) drawControlPolygon();
}

void drawControlPolygon()
{
	glPointSize(5 * dpiScaling);
	// Colors
	float hsv[3] = { 0,1,1 };// [0,360] (degree), [0,1], [0,1]
	float rgb[3];
	glEnable(GL_LINE_STIPPLE);
	hsv[0] = 180.0f * iSegment / (nControlPoints - 3) + ((iSegment % 2) ? 180.0f : 0);
	HSV2RGB(hsv, rgb);
	glColor3f(rgb[0], rgb[1], rgb[2]);

	glLineStipple(2, 0x00FF);
	glBegin(GL_LINE_STRIP);
	vector<vector<float>> samples_;

	for (int j = 0; j < 4; j++)
	{
		vector<float> samplePoints(3);
		glVertex3fv(controlPoints[iSegment + j].data());

		samplePoints[0] = controlPoints[iSegment + j].data()[0];
		samplePoints[1] = controlPoints[iSegment + j].data()[1];
		samplePoints[2] = controlPoints[iSegment + j].data()[2];
		samples_.push_back(samplePoints);
	}
	samples = samples_;
	glEnd();

	glDisable(GL_LINE_STIPPLE);
}

void render(GLFWwindow* window)
{
	// Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	
	// draw control Point
	glPointSize(10 * dpiScaling);
	glColor3f(1, 0, 0);
	glBegin(GL_POINTS);
	for (int i = 0; i < N; i++)
		glVertex3f(points[i][0], points[i][1], points[i][2]);
	glEnd();

	// 4�� �̻��̸� ��� �׸���
	if (N > 3) {
		prepareRepeatedControlPoints(REPETITION);
		drawBSpline();
	}
}

void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
			// Sampled points
		case GLFW_KEY_S: samplePointDrawingEnabled = !samplePointDrawingEnabled; break;

			// Number of samples
		case GLFW_KEY_UP: N_SUB_SEGMENTS++; break;
		case GLFW_KEY_DOWN: N_SUB_SEGMENTS = max(N_SUB_SEGMENTS - 1, 1); break;

			// Reptition
		case GLFW_KEY_0: REPETITION = 0; break;
		case GLFW_KEY_1: REPETITION = 1; break;
		case GLFW_KEY_2: REPETITION = 2; break;
			
			// Quit
		case GLFW_KEY_Q:
		case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GL_TRUE); break;
		
			// c�� ������ �׸��ų� ���ų� -> exercise
		case GLFW_KEY_C: ctrlPolygonDrawingEnabled = !ctrlPolygonDrawingEnabled; break;

			// Control polygon
		case GLFW_KEY_LEFT: iSegment = max(iSegment - 1, 0); break;
		case GLFW_KEY_RIGHT:iSegment = min(iSegment + 1, nControlPoints - 4); break;

			// Add 
		case GLFW_KEY_A: interactMode = ADD; cout << "Mode is Add\n";  break;

			// Erase
		case GLFW_KEY_R: interactMode = ERASE; cout << "Mode is Erase\n"; break;

			// Drag
		case GLFW_KEY_D: interactMode = DRAG; cout << "Mode is Drag\n"; break;

			// Insert
		case GLFW_KEY_I: interactMode = INSERT; cout << "Mode is Insert\n"; break;

		}
	}
}

void mouseButton(GLFWwindow* window, int button, int action, int mods)
{
	if (interactMode == NONE)
		return;

	double xs, ys;
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
	{
		// Mouse cursor position in the screen coordinate
		glfwGetCursorPos(window, &xs, &ys);

		// Mouse cursor position in the framebuffer coordinate
		// �̰Ÿ� unproject�ϸ� world space���� x,y,z�� ���� �� �ִ�.
		// z�� ������� �ʴ´�. ������ x,y��鿡 �ֱ� ����
		xs = xs * dpiScaling;
		ys = ys * dpiScaling;

		GLdouble x_ws, y_ws, z_ws;
		unProject(xs, ys, &x_ws, &y_ws, &z_ws);
		if (preX == x_ws && preY == y_ws) {
			return;
		}

		if (interactMode == ADD) addDataPoint(x_ws, y_ws); 
		else if (interactMode == ERASE) eraseDataPoint(x_ws, y_ws);
		else if (interactMode == DRAG) selectedDataPoint(x_ws, y_ws);
		else if (interactMode == INSERT) insertDataPoint(x_ws, y_ws);

		preX = x_ws; preY = y_ws;
	}
}
void mouseMove(GLFWwindow* window, double xs, double ys)
{
	// Drag ��尡 �ƴϰų� �巡�� �ϰ����� ������ return
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE || interactMode != DRAG) return;

	if (!IsSelectedPoint) // ���õȰ� ��� ������ �� ����.
		return;

	// Mouse cursor position in the framebuffer coordinate
	double x = xs * dpiScaling;
	double y = ys * dpiScaling;

	// ���콺 ��ǥ unProject.
	GLdouble x_ws, y_ws, z_ws;
	unProject(x, y, &x_ws, &y_ws, &z_ws);
	dragDataPoint(x_ws, y_ws);

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