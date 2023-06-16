#include "glSetup.h"

#include <Eigen/Dense>

using namespace Eigen;

#include <iostream>
#include <vector>
using namespace std;
using std::cout;


void init();
void quit();
void initializeParticleSystem();
void update(float delta_t);
void solveODE(float h);
void collisionHandling();
void keyboard(GLFWwindow* window, int key, int code, int action, int mods);
void render(GLFWwindow* window);
void setupLight();
void setupMaterial();
void drawSphere(float radius, const Vector3f& color, int N);

// mouse
void mouseButton(GLFWwindow* window, int button, int action, int mods);
void mouseMove(GLFWwindow* window, double xs, double ys);
void unProject(double x, double y, GLdouble* wx, GLdouble* wy, GLdouble* wz);

// add drag n, 
bool selectedDataPoint(GLdouble x_ws, GLdouble y_ws);
bool selectedEdge(GLdouble x_ws, GLdouble y_ws);

void createParticles(GLdouble x_ws, GLdouble y_ws,bool constrained=false);
void attachParticles();
void nailParticles();
void dragParticles(GLdouble x_ws, GLdouble y_ws);

// Picking
enum Mode {
	NONE = 0, ADD = 1, LINK= 2, NAIL= 3, DRAG= 4,
};
Mode interactMode = NONE;

struct Particles
{
	Vector3f initPos;
	Vector3f pos;
	Vector3f velocity;
	bool constrained;
};

vector<Particles> particles; // particle
int nParticles = 0;

vector<int> attached(2); // attach를 위한 저장소.
int LinkedNum = 0;

// Connectivity
int nEdges=0;
vector<int> e1;					// One end of the edge
vector<int> e2;					// The other end of the edge
vector<float> l;				// Rest lengths between particles
vector<float> k;				// Spring constants
float k0 = 1.0f;				// Global spring constant

bool IsSelectedPoint = false;
int selectedIndex = -1;
int dragedIndex = -1;
bool IsDraged = false;


void createParticles(GLdouble x_ws, GLdouble y_ws,bool constrained)
{
	Particles p;
	p.initPos = { (float)x_ws, (float)y_ws, 0 }; // 처음 위치 저장.
	p.pos={ (float)x_ws, (float)y_ws, 0 }; // 현재 위치 저장.
	p.velocity.setZero();	// 속도 0으로 초기화
	p.constrained = constrained; // 고정할 것인지? constrained의 default = false
	
	particles.push_back(p); // 벡터에 저장.
	nParticles += 1; 
}

void attachParticles()
{
	// Rest length
	e1.push_back(attached[0]);
	e2.push_back(attached[1]);
	
	float length = (particles[attached[0]].pos - particles[attached[1]].pos).norm();
	l.push_back(length);
	k.push_back(k0 / length); // Inversely proportion to the spring length
	
	nEdges += 1;
}

void nailParticles() 
{
	particles[selectedIndex].constrained = true;
}

int cursorParticleIdx;
int cursorEdgeIndex;
Vector2f prevXY;
void createCursorParticles(GLdouble x_ws, GLdouble y_ws)
{
	createParticles(x_ws, y_ws,true);
	cursorParticleIdx = nParticles-1; // 맨 마지막 인덱스.
	
	e1.push_back(selectedIndex);
	e2.push_back(cursorParticleIdx);

	float length = (particles[cursorParticleIdx].pos - particles[selectedIndex].pos).norm();
	l.push_back(length);
	k.push_back(k0 / length); // Inversely proportion to the spring length

	nEdges += 1;
	cursorEdgeIndex = nEdges - 1;

	particles[selectedIndex].constrained = true;
}


void dragParticles(GLdouble x_ws, GLdouble y_ws)
{
	particles[cursorParticleIdx].pos[0] = x_ws;
	particles[cursorParticleIdx].pos[1] = y_ws;

	particles[selectedIndex].pos[0] += (x_ws - prevXY[0]);
	particles[selectedIndex].pos[1] += (y_ws - prevXY[1]);
}

bool selectedDataPoint(GLdouble x_ws, GLdouble y_ws)
{
	// 마우스로 찍은 좌표와 가장 가까운 dataPoint를 찾아야 한다.
	// 마우스 좌표와 가까운 dataPoint 거리가 0.015 이내면 클릭한 것으로 간주한다.
	// (x1-x2)^2 + (y1-y2)^2 값이 0.0002 이내면 클릭한 것으로 간주.

	float minDist = 1;
	int minIndex = -1;

	for (int i = 0; i < nParticles; i++)
	{
		float dist =
			(particles[i].pos[0] - x_ws) * (particles[i].pos[0] - x_ws) +
			(particles[i].pos[1] - y_ws) * (particles[i].pos[1] - y_ws);

		if (dist <= 0.0002)
		{
			if (minDist > dist) {
				minDist = dist;
				minIndex = i;
			}
		}
	}

	if (minDist == 1) // 0.002이내에 점이 없다
	{
		IsSelectedPoint = false;
		return false;
	}

	selectedIndex = minIndex;
	IsSelectedPoint = true;

	return true;
}

// Play configuration
bool pause = true;
float timeStep = 1.0f / 120; // 120fps
int N_SUBSTEPS = 1; // Time stepping : h = timeStep / N_SUBSTEPS

// Light configuration
Vector4f light(0.0f, 0.0f, 5.0f, 1.0f); // Light position

// Global coordinate frame
float AXIS_LENGTH = 3;
float AXIS_LINE_WIDTH = 2;

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Sphere
GLUquadricObj* sphere = NULL;

bool usePointDamping = true; 
bool useSpringDamping = false;
float damping = 0.01f;

// Geometry and mass
float radius = 0.02f;		// 2cm
float m = 0.01f;				// 10g

// External force
float	useGravity = true;
Vector3f	gravity(0, -9.8f, 0); // Gravity - 9.8m/s^2

// Collision
float k_r = 0.9f;
float epsilon = 1.0E-4f;

const int nWalls = 4;
Vector3f wallP[nWalls];		// Points in the walls
Vector3f wallN[nWalls];		// Normal vectors of the walls

// Method
enum IntegrationMethod
{
	EULER = 1,
	MODIFIED_EULER,
};
IntegrationMethod  intMethod=MODIFIED_EULER;

int main(int argc, char* argv[])
{
	// Vertical sync on
	vsync = 1;
	// Orthographics viewing
	perspectiveView = false;

	// Initialize the OpenGL system
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor);
	if (window == NULL) return -1;
	
	// Vertical sync for 60fps
	glfwSwapInterval(1);

	// Callbacks
	glfwSetKeyCallback(window, keyboard);
	glfwSetMouseButtonCallback(window, mouseButton);
	glfwSetCursorPosCallback(window, mouseMove);

	// Detph test
	glEnable(GL_DEPTH_TEST);

	// Normal vectors are normalized after transformation
	glEnable(GL_NORMALIZE);

	// Back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	// Viewport and perspective setting
	reshape(window, windowW, windowH);

	// Intialization - Main loop - Finalization
	init();
	initializeParticleSystem();

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

		// Deal with the current frame
		if (elapsed > timeStep)
		{
			if (!pause) update(elapsed);

			elapsed = 0; // Rest the elapsed time
		}

		render(window);
		glfwSwapBuffers(window);
	}

	//Finalization
	quit();
	
	// Terminate the glfw system
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void rebuildSpringK()
{
	cout << "Spring constant = " << k0 << endl;

	// Spring constants
	// 값이 작을 수록 많이 늘어남.
	for (int i = 0; i < nEdges; i++) {
		k.push_back(k0 / l[i]); // Inversely proportion to the spring length
	}
}
//// particle들의 geometry
void initializeParticleSystem()
{
	// To generate the same random values at each execution
	srand(0);

	if (nParticles > 0)
	{
		// Per-paricle properties
		for (int i = 0; i < nParticles; i++)
		{
			// initial position
			particles[i].pos = particles[i].initPos;
			// Zero initial velocity
			particles[i].velocity.setZero();
			// Not contrained
			particles[i].constrained = false;
		}

		for (int i = 0; i < nEdges; i++)
		{
			l[i] = (particles[e1[i]].initPos - particles[e2[i]].initPos).norm();
		}
	}

	rebuildSpringK();

	// Normal vectors of the 4 surrounding walls
	wallN[0] = Vector3f(1.0f, 0, 0); // Left
	wallN[1] = Vector3f(-1.0f, 0, 0); // right
	wallN[2] = Vector3f(0, 1.0f, 0); // Bottom
	wallN[3] = Vector3f(0, -1.0f, 0); // Top

	for (int i = 0; i < nWalls; i++)
		wallN[i].normalize();

	//// Collision handling
	collisionHandling();
}

void quit()
{
	// Delete quadric shapes
	gluDeleteQuadric(sphere);
}

void update(float delta_t)
{
	// Solve ordinary differential equation
	for (int i = 0; i < N_SUBSTEPS; i++)
	{
		float h = delta_t / N_SUBSTEPS;
		solveODE(h);
	}
}

void solveODE(float h)
{
	// Total force

	vector<Vector3f> f(nParticles);

	for (int i = 0; i < nParticles; i++)
	{
		// Initialization
		f[i].setZero();

		// Gravity
		if (useGravity) f[i] += m * gravity;

		//Point damping
		if (usePointDamping)
		{
			f[i] -= damping * particles[i].velocity;
		}
	}

	// Spring force
	for (int i = 0; i < nEdges; i++)
	{
		Vector3f v_i = particles[e1[i]].pos - particles[e2[i]].pos;
		float L_i = v_i.norm();				// Current length
		Vector3f f_i = k[i] * (L_i - l[i]) * (v_i / L_i);

		f[e2[i]] += f_i;
		f[e1[i]] -= f_i;
		
		if (useSpringDamping)
		{
				Vector3f x_ij = particles[e1[i]].pos - particles[e2[i]].pos;
				Vector3f n_ij = x_ij / x_ij.norm();				// Current length
				Vector3f v_ij = particles[e1[i]].velocity - particles[e2[i]].velocity;

				Vector3f f_damped_i = damping * v_ij.dot(n_ij) * n_ij;

				f[e1[i]] -= f_damped_i;
				f[e2[i]] += f_damped_i;
		}	
	}

	for (int i = 0; i < nParticles; i++)
	{
		// Constraint
		if (particles[i].constrained)continue;

		// Time stepping
		switch (intMethod)
		{
		case EULER:
			particles[i].pos+= h * particles[i].velocity;
			particles[i].velocity+= h* f[i] / m;
			break;

		case MODIFIED_EULER:
			particles[i].velocity+= h * f[i] / m;
			particles[i].pos+= h * particles[i].velocity;
			break;
		}
	}
	// Collision handling
	collisionHandling();

}

void collisionHandling()
{
	//Points of the 4 surrounding walls : It can be changed.
	wallP[0] = Vector3f(-1.0f * aspect, 0, 0); // Left
	wallP[1] = Vector3f(1.0f * aspect, 0, 0); //Right
	wallP[2] = Vector3f(0, -1.0f, 0);		// Bottom
	wallP[3] = Vector3f(0, 1.0f, 0);		// Top

	// Colision wrt the walls
	for (int i = 0; i < nParticles; i++)
	{
		for (int j = 0; j < nWalls; j++)
		{
			float d_n = wallN[j].dot(particles[i].pos - wallP[j]); // Distance to the wall
			if (d_n < radius + epsilon) 
			{
				// Position correction
				particles[i].pos+= (radius + epsilon - d_n) * wallN[j];

				// Velocity in the normal direction
				float v_n = wallN[j].dot(particles[i].velocity);

				if (v_n < 0) // Heading into the wall
				{
					// Velocity correction : v[i] - v_n = v_t
					particles[i].velocity -= (1 + k_r) * v_n * wallN[j];
				}
			}
		}
	}
}

void render(GLFWwindow* window)
{
	//Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// ModelView matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Lighting
	setupLight();

	// Material
	setupMaterial();

	// Particles
	for (int i = 0; i < nParticles; i++)
	{
		glPushMatrix();
		glTranslatef(particles[i].pos[0], particles[i].pos[1], 0);
		if (particles[i].constrained) drawSphere(radius, Vector3f(1, 1, 0), 20);
		else                drawSphere(radius, Vector3f(0, 1, 0), 20);
		glPopMatrix();
	}

	// Edges
	if (nEdges > 0) {
		glLineWidth(7 * dpiScaling);
		glColor3f(0, 0, 1);
		glBegin(GL_LINES);
		for (int i = 0; i < nEdges; i++)
		{
			glVertex3fv(particles[e1[i]].pos.data());
			glVertex3fv(particles[e2[i]].pos.data());
		}
		glEnd();
	}
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

			// Controls
		case GLFW_KEY_SPACE:	pause = !pause;	break;				// Play on/off
		case GLFW_KEY_G:		useGravity = !useGravity;	break;	// Gravity on/off
		case GLFW_KEY_I:		initializeParticleSystem();	break;	// Initialization
		case GLFW_KEY_E:		intMethod = EULER;			break;
		case GLFW_KEY_M:		intMethod = MODIFIED_EULER;	break;
		// Spinrg constants
		case GLFW_KEY_UP: k0 = min(k0 + 0.1f, 10.0f); rebuildSpringK(); break;
		case GLFW_KEY_DOWN: k0 = max(k0 - 0.1f, 0.1f); rebuildSpringK(); break;

			// Damping constant
		case GLFW_KEY_S: useSpringDamping = !useSpringDamping;
			if (useSpringDamping)cout << "spring damping On\n";
			else cout << "spring damping Off\n";
			break;
		case GLFW_KEY_D: usePointDamping = !usePointDamping;
			if (usePointDamping)cout << "point damping On\n";
			else cout << "porint damping Off\n";
			break;
		case GLFW_KEY_LEFT:
			damping = max(damping - 0.01f, 0.0f);
			cout << "Damping coeff = " << damping << endl;
			break;
		case GLFW_KEY_RIGHT:
			damping += 0.01f;
			cout << "Damping coeff = " << damping << endl;
			break;


			// Add 
		case GLFW_KEY_A: interactMode = ADD; cout << "Mode is Add\n";  break;
		case GLFW_KEY_L: 
			interactMode = LINK;
			LinkedNum = 0;
			cout << "Mode is ATTACH(LINK).- Attaching is initialized.\n";
			break;
		case GLFW_KEY_N: interactMode = NAIL; cout << "Mode is Nail\n";  break;
		case GLFW_KEY_R: interactMode = DRAG; cout << "Mode is Drag\n";  break;

			// Constraints
		case GLFW_KEY_1:  N_SUBSTEPS = 1; break;
		case GLFW_KEY_2:  N_SUBSTEPS = 2; break;
		case GLFW_KEY_3:  N_SUBSTEPS = 3; break;
		case GLFW_KEY_4:  N_SUBSTEPS = 4; break;
		case GLFW_KEY_5:  N_SUBSTEPS = 5; break;
		case GLFW_KEY_6:  N_SUBSTEPS = 6; break;
		case GLFW_KEY_7:  N_SUBSTEPS = 7; break;
		case GLFW_KEY_8:  N_SUBSTEPS = 8; break;
		case GLFW_KEY_9:  N_SUBSTEPS = 9; break;
		}
			
	}
}

void
init()
{
	// Prepare quadric shapes
	sphere = gluNewQuadric();
	gluQuadricDrawStyle(sphere, GLU_FILL);
	gluQuadricNormals(sphere, GLU_SMOOTH);
	gluQuadricOrientation(sphere, GLU_OUTSIDE);
	gluQuadricTexture(sphere, GL_FALSE);

	// Keyboard and mouse
	cout << "Keyboard input: space for play/pause" << endl;
	cout << "Keyboard Input: g for gravity on/off" << endl;
	cout << "Keyboard Input: i for the system initialization" << endl;
	cout << "Keyboard Input: c for constraint specification" << endl;
	cout << "Keyboard Input: e for the Euler integration" << endl;
	cout << "Keyboard Input: m for the modified Euler integration" << endl;
	cout << "Keyboard Input: d for damping on/off" << endl;
	cout << "Keyboard Input: [1:9] for the # of sub-time steps" << endl;
	cout << "Keyboard Input: [1:4] for constraint specification" << endl;
	cout << "Keyboard Input: up/down to increase/decrease the spring constant" << endl;
	cout << "Keyboard Input: left/right to increase/decrease the damping constant" << endl;
}

// Light
void setupLight()
{
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	GLfloat ambient[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	GLfloat diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat specular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light.data());

}

// Material
void setupMaterial()
{
	//Material
	GLfloat mat_ambient[4] = { 0.1f,0.1f,0.1f,1.0f };
	GLfloat mat_specular[4] = { 0.5f,0.5f,0.5f,1.0f };
	GLfloat mat_shininess = 128.0f;

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

void setDiffuseColor(const Vector3f& color)
{
	GLfloat mat_diffuse[4] = { color[0],color[1],color[2],1 };
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
}

// Draw a shpere after setting up its material
void drawSphere(float radius, const Vector3f& color, int N)
{
	// Material
	setDiffuseColor(color);

	// Sphere using GLU quadrics
	gluSphere(sphere, radius, N, N);
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
		// In the screen coordinate	
		
		Vector2f goal = Vector2f(float(xs), float(ys));

		// In the workspace. See reshape() in glSetup.cpp
		float aspect = (float)screenW / screenH;
		goal[0] = 2.0f * (goal[0] / screenW - 0.5f) * aspect;
		goal[1] = -2.0f * (goal[1] / screenH - 0.5f);
		
		if (interactMode == ADD) createParticles(goal[0], goal[1]);
		else if (interactMode == LINK)
		{
			if (selectedDataPoint(goal[0], goal[1]))
			{
				if (LinkedNum == 0)
				{
					attached[0] = selectedIndex;
					LinkedNum += 1;
				}
				else if (LinkedNum == 1 && selectedIndex != attached[0]) // 처음에 클릭한 것 중복 방지
				{
					attached[1] = selectedIndex;
					LinkedNum += 1;
					//2번째꺼 오면 그때 연결.
					attachParticles();
					LinkedNum = 0;
				}
			}
		}
		else if (interactMode == NAIL)
		{
			if (selectedDataPoint(goal[0], goal[1])) nailParticles();
		}
		else if (interactMode == DRAG)
		{
			if (selectedDataPoint(goal[0], goal[1]))
			{
				prevXY[0] = goal[0]; prevXY[1] = goal[1];
				createCursorParticles(goal[0], goal[1]);
			}
		}
	}
}

void deleteCursorPoint()
{
	// particle 삭제
	particles.pop_back();
	nParticles--;
	// edge 삭제
	e1.pop_back();
	e2.pop_back();
	nEdges--;

	particles[selectedIndex].constrained = false;
}

void mouseMove(GLFWwindow* window, double xs, double ys)
{
	
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE && interactMode == DRAG && IsDraged==true)
	{
		IsDraged = false;
		deleteCursorPoint();
		return;
	}

	// Drag 모드가 아니거나 드래그 하고있지 않으면 return
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE || interactMode != DRAG)
	{
		return;
	}
	if (!IsSelectedPoint) // 선택된게 없어서 움직일 게 없다.
		return;

	IsDraged = true;
	
	// Mouse cursor position in the screen coordinate
	glfwGetCursorPos(window, &xs, &ys);
	// In the screen coordinate	

	Vector2f goal = Vector2f(float(xs), float(ys));

	// In the workspace. See reshape() in glSetup.cpp
	float aspect = (float)screenW / screenH;
	goal[0] = 2.0f * (goal[0] / screenW - 0.5f) * aspect;
	goal[1] = -2.0f * (goal[1] / screenH - 0.5f);

	dragParticles(goal[0], goal[1]);
	prevXY[0] = goal[0]; prevXY[1] = goal[1];
}