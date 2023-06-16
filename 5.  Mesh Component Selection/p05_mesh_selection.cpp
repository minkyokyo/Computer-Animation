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

#include <vector>
#include <unordered_map>
#include <map>
#include <queue> //Breath - first traversal

#undef NDEBUG
#include <assert.h>

void init(const char* filename);
void setupLight(const Vector4f& position);
void setupColoredMaterial(const Vector3f& color);
void render(GLFWwindow* window, bool selectionMode);
void reshape(GLFWwindow* window, int w, int h);
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButton(GLFWwindow* window, int button, int action, int mods);
void mouseMove(GLFWwindow* window, double x, double y);

// Camera configuration
Vector3f eye(2, 2, 2);
Vector3f center(0, 0.2f, 0);
Vector3f up(0, 1, 0);

// Light configuration
Vector4f light(5.0, 5.0, 0.0, 1);

// Play configuration
bool  pause = true;
float timeStep = 1.0f / 120;
float period = 8.0;
float theta = 0;

// Global coordinate frame
bool axes = false;
float AXIS_LENGTH = 1.5f;
float AXIS_LINE_WIDTH = 2;

// Colors
GLfloat bgColor[4] = { 1,1,1,1 };

// Default mesh file name
const char* defaultMeshFileName = "m01_bunny.off";

// Display style
bool aaEnabled = true;	// Antialiasing
bool bfcEnabled = true; // Back face culling

// Mesh
MatrixXf	vertex;		//Position
MatrixXf	faceNormal;	// Face Normal vector
MatrixXf	vertexNormal; // Vertex normal vector
ArrayXXi	face;		  // Index

// Mesh that consists of faces with gap
MatrixXf faceVertex;
bool	faceWithGapMesh = true;
bool	useFaceNormal = true;
float	gap = 0.3f;

// Picking
enum PickMode {
	NONE = 0, VERTEX = 1, EDGE = 2, FACE = 3,
};
PickMode pickMode = FACE;

int				picked = -1;
unordered_map<int, int> pickedNeighborVertices; // {vertex, n-ring}
unordered_map<int, int> pickedNeighborEdges; // {edge, n-ring}
unordered_map<int, int> pickedNeighborFaces; // {face, n-ring}

bool vertexAdjacent = true;

// This is for click and drag
double preX, preY,newX, newY; // this is screen coordinate in window
bool drag = false;
void drawDragRectangle();

int main(int argc, char* argv[])
{
	// Filename for deformable body configuration
	const char* filename;
	if (argc >= 2) filename = argv[1];
	else           filename = defaultMeshFileName;

	// Initialize the OpenGL system
	GLFWwindow* window = initializeOpenGL(argc, argv, bgColor);
	if (window == NULL) return -1;

	// Callbacks
	glfwSetKeyCallback(window, keyboard);
	glfwSetMouseButtonCallback(window, mouseButton);
	glfwSetCursorPosCallback(window, mouseMove);

	// Depth test
	glEnable(GL_DEPTH_TEST);

	// Normal vectors are normalized after transformation.
	glEnable(GL_NORMALIZE);

	// Viewport and perspective setting.
	reshape(window, windowW, windowH);

	// Initialization - Main loop - Finalization
	init(filename);

	// Main loop
	float previous = (float)glfwGetTime();
	float elapsed = 0;
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();	// Evenets

		// Time passed during a single loop
		float now = (float)glfwGetTime();
		float delta = now - previous;
		previous = now;

		// Time passed after the previous frame
		elapsed += delta;

		// Dela with the current frame
		if (elapsed > timeStep)
		{
			if (!pause) theta += float(2.0 * M_PI) / period * elapsed;

			elapsed = 0; // Rest the elapsed time;
		}

		render(window, false);
		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

// Data structures for mesh traversal
int nEdges = 0;
//int nVertices = 0;
//int nFaces = 0;

// Eijf[i] = {j, f} i번쨰 벌텍스에서 j번쨰로 가는 엣지인데, 오른쪽 페이스.
// The vertex 1 has an edge (i,j) and the face f is adjacent to (i, j) from the right.
vector<unordered_map<int, int>> Eijf;
//j가 key, f가 value.

// E[e] = {start, end}
// The edge e has the start vertex and the end vertex/
vector<pair<int, int>> E;	// E[e] = { start, end}
// E는 edge만큼 있음.


// Edges adjacent to a vertex : edges sharing the vertex . 버텍스에 인접한 엣지들 기록하기 위함.
// Ev[v][j] = e
// e is the j - the edge sharing the vertex v
vector<vector<int>> Ev; // Ev[v][j] =e
// 버텍스 개수만큼 있음.
// j는... 가변적.


// Faces adjacent to a vertex : faces sharing the vertex
// Fv[v][j] = f
// f is the j-th face sharing the vertex v.

vector<vector<int>>Fv; // Fv[v][j]=f

// Distance from the picked face, edge, and vertex
vector<int> Dv, De, Df;
// DV는 버텍스마다, dE는 엣지마다, dF는 DF마다.
// distance다.
// 제로링이면 빨간색, 1링이면 노란색, 3링이면 연두색... 


// N -ring neighborhood
int nRing = 5;

// Data structures for mesh traversal
void prepareMeshTraversal()
{
	// Directed edge Eijf[i] = {j, f}.
	// The vertex i has an edge (i , j) and the face f is adjacent to (i, j) from the right.
	Eijf.resize(vertex.cols());
	Fv.resize(vertex.cols());
	for (int f = 0; f < face.cols(); f++)
	{
		// The vertex indices are in CCW. Thus, f become the right face in CW.
		for (int i = 0; i < 3; i++)
			Eijf[face((i + 1) % 3, f)].insert({ face(i,f),f });

		// Faces adjacent to a vertex: faces sharing the vertex
		for (int i = 0; i < 3; i++)
			Fv[face(i, f)].push_back(f);
	}

	// Undirected edge E[e] = {start, end} s.t. start <end
	// The edge e has the start vertex and the end vetex.
	Ev.resize(vertex.cols());
	for (int i = 0; i < vertex.cols(); i++)
	{
		// For all the directed edges(i, j)
		for (auto& e : Eijf[i])
		{
			int j = e.first; // End vertex
			// fisrt면 Key.

			if (i < j)
			{
				E.push_back({ i,j });
			}
			else {
				// Check to see if there exists the edge(j , i)
				unordered_map<int, int>::const_iterator e = Eijf[j].find(i);

				// If exits, skip this directed edge. It will be added later.
				if (e != Eijf[j].end()) continue; //  찾아지면 스킵.

				// 없다면 boundary edge. 두개의 페이스에서 공유되는게 아니고 하나의 페이스에서...

				// Otherwise, add this edge at this time. this is a boundary edge.
				E.push_back({ j,i }); // j < i
			}

			// E[e] = {i,j} : The edge e has hte start vertex i and the end vertex j.
			int e_id = int(E.size()) - 1;

			// Ev는 벌텍스마다 있는거.
			Ev[i].push_back(e_id);
			Ev[j].push_back(e_id);
		}
	}
	nEdges = int(E.size());
	cout << "# undirected edges = " << nEdges << endl;

	// Initialize distances form the selected entities
	Df.resize(face.cols());
	for (int i = 0; i < face.cols(); i++)
		Df[i] = -1;

	De.resize(nEdges);

	for (int i = 0; i < nEdges; i++)
		De[i] = -1;

	Dv.resize(vertex.cols());
	for (int i = 0; i < vertex.cols(); i++)
		Dv[i] = -1;
}

// Find all the vertices adjacent to the seed vertex
void findNeighborVertices(int seedVertex, int nRing, unordered_map<int, int>& neighborVertices)
{
	// seedVertex는 picking된 애.
	Dv[seedVertex] = 0;

	queue<int> Q; //Queue for breadth - first search
	Q.push(seedVertex);
	for (; !Q.empty(); Q.pop())
	{
		// Dealing with the next vertex with the distance Dv[v] from the seed vertex
		int v = Q.front();

		// 버텍스의 인접한 모든 엣지들을 확인하기.
		// 버텍스의 이웃이란. 버텍스의 인접한 엣지들의 끝 버텍스이다.
		// Obtain the edge adjacent to the vertex v
		for (int e : Ev[v]) // 현재 버텍스의 모든 엣지들을 가져와서
		{
			// 엣지의 first = i 버텍스, 엣지의 second = j 버텍스
			int a[2] = { E[e].first, E[e].second };
			for (int i = 0; i < 2; i++)
			{
				// Check to see if the adjacent vertex a[i] is already visited
				if (Dv[a[i]] != -1) continue;

				Dv[a[i]] = Dv[v] + 1;
				neighborVertices.insert({ a[i],Dv[a[i]] });

				// Check to see the desired level is reached
				if (Dv[a[i]] >= nRing)continue;

				// Insert the adjacent edge a to the queue
				Q.push(a[i]);
			}
		}
	}
}

// Find all the edges adjacent to the seed edge
void findNeighborEdges(int seedEdge, int nRing, unordered_map<int, int>& neighborEdges)
{
	De[seedEdge] = 0;

	queue<int> Q;
	Q.push(seedEdge);
	for (; !Q.empty(); Q.pop())
	{
		// Dealing with the next edge with the distance De[e] from the seed edge
		int e = Q.front();

		// 엣지의 양 끝점과 인접한 엣지들을 보자.
		int v[2] = { E[e].first, E[e].second };
		for (int i = 0; i < 2; i++)
		{

			// Obtain the edge adjacent to the vertex v[i]
			for (int a : Ev[v[i]])
			{

				// Check to see if the adjacent edge a is already visited
				if (De[a] != -1)continue;

				De[a] = De[e] + 1;	// Distance
				neighborEdges.insert({ a,De[a] });

				// Check to see the desired level is reached
				if (De[a] >= nRing) continue;

				// Insert the adjacent edge a to the queue
				Q.push(a);
			}
		}

	}
}

// Find the face adjacent to the edge (i, j) from the right
int findEdgeAdjacentFace(int i, int j)
{
	// Find the edge(i,j) which is already inserted
	unordered_map<int, int>::const_iterator e = Eijf[i].find(j);
	if (e == Eijf[i].end()) return -1;

	return e->second;	// e ->first = j and e->second is the face id
}

// Find all the faces adjacent to the seed face at an EDGE
void findEdgeAdjacentFaces(int seedFace, int nRing, unordered_map<int, int>& neighborFaces)
{
	queue<int> Q;	// Queue for breadth-first search
	Q.push(seedFace); // The seed face to be visited
	for (; !Q.empty(); Q.pop())
	{
		// Dealing with the next face with the distance Df[f] from the seed face
		int f = Q.front();

		for (int i = 0; i < 3; i++)
		{
			// Obtain the face adjacent to the edge(i, i+1)
			int a = findEdgeAdjacentFace(face(i, f), face((i + 1) % 3, f));
			if (a == -1) continue;

			// Check to see if the adjacent face a is already visited
			if (Df[a] != -1)  continue;

			Df[a] = Df[f] + 1; // Distance
			neighborFaces.insert({ a,Df[a] });

			// Check to see the desired level is reached
			if (Df[a] >= nRing) continue;

			// Insert the adjacent face a to the queue
			Q.push(a);
		}
	}
}

// Find all the faces adjacent to the seed face at a VERTEX
void findVertexAdjacentFaces(int seedFace, int nRing, unordered_map<int, int>& neighborFaces)
{
	queue<int> Q;
	Q.push(seedFace);
	for (; !Q.empty(); Q.pop())
	{
		// Dealing with the next face with the distance Df[f] from the seed face
		int f = Q.front();

		for (int i = 0; i < 3; i++)
		{
			// Obtain the face adjacent to the vertex i
			for (int a : Fv[face(i, f)])
			{
				// Check to see if the adjacent face a is already visited
				if (Df[a] != -1)continue;

				Df[a] = Df[f] + 1;	//Distance
				neighborFaces.insert({ a,Df[a] });

				// Check to see the desired level is reached
				if (Df[a] >= nRing) continue;

				// Insert the adjacent face a to the queue
				Q.push(a);
			}
		}
	}
}

// Find all the faces adjacent to the seed face
void findNeighborFaces(int seedFace, int nRing, unordered_map<int, int>& neighborFaces)
{
	if (vertexAdjacent) findVertexAdjacentFaces(picked, nRing, neighborFaces);
	else                findEdgeAdjacentFaces(picked, nRing, neighborFaces);
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
	cout << "Reading " << filename << endl;
	nEdges = readMesh(filename, vertex, face, faceNormal, vertexNormal);
	cout << "# undirected edges = " << nEdges << endl;

	// Shrunken face mesh
	buildShrunkenFaces(vertex, faceVertex);

	// Prepare data structures for the mesh traversal
	prepareMeshTraversal();

	// Usage
	cout << endl;
	cout << "Keyboard Input : space for play/pause" << endl;
	cout << "Keyboard Input : x for axes on/off" << endl;
	cout << "Keyboard Input : q/esc for quit" << endl;
	cout << endl;
	cout << "Keyboard Input : v for vertex selection" << endl;
	cout << "Keyboard Input : e for edge selection" << endl;
	cout << "Keyboard Input : f for face selection" << endl;
	cout << "Keyboard Input : g for face with gap mesh/mesh selection" << endl;
	cout << "Keyboard Input : up/down to increase/decrease the gap inner faces" << endl;
	cout << "Keyboard Input : [0:5] for n-ring" << endl;
	cout << "Keyboard Input : a for vertex/Edge adjacency" << endl;
	cout << "Keyboard Input : b for backface culling on/off" << endl;
}

Vector3f selectionColor[6] =
{
	{1,0,0}, // Red
	{1,1,0}, // Red
	{0,1,0}, // Red
	{0,1,1}, // Red	
	{0,0,1}, // Red
	{1,0,1}, // Red
};

// Draw a sphere after setting up its material
void drawFaces()
{
	// 계층구조. 처음엔 일단 FACE냐 아니냐,
	// Picking
	glPushName(FACE); // Push the category name in the stack
	glPushName(-1);   // Face id in the category

	// Material
	setupColoredMaterial(Vector3f(0.95f, 0.95f, 0.95f));

	// Triangles
	// mesh에서 페이스 개수만큼 트라이앵글을 그린다.
	for (int i = 0; i < face.cols(); i++)
	{
		// Picking
		// I번째 FACE라고 하는거임.
		glLoadName(i); // Replace the name for the i-the face

		// Material for selected objects
		if (!faceWithGapMesh && Df[i] != -1) setupColoredMaterial(selectionColor[Df[i]]);

		glBegin(GL_TRIANGLES);
		for (int j = 0; j < 3; j++)
		{
			glNormal3fv(vertexNormal.col(face(j, i)).data());
			glVertex3fv(vertex.col(face(j, i)).data());
		}
		glEnd();

		// Material for non-selected objects
		if (!faceWithGapMesh && Df[i] != -1)
			setupColoredMaterial(Vector3f(0.95f, 0.95f, 0.95f));
	}

	// Picking
	glPopName();
	glPopName();
}


void drawFaceWithGapMesh()
{
	// Material
	setupColoredMaterial(Vector3f(0, 1, 1));

	// Triangles
	for (int i = 0; i < face.cols(); i++)
	{
		// Material for selected objects
		if (Df[i] != -1)
		{
			glDisable(GL_LIGHTING);
			glColor3fv(selectionColor[Df[i]].data());
		}
		else continue;

		glBegin(GL_TRIANGLES);
		glNormal3fv(faceNormal.col(i).data());
		for (int j = 0; j < 3; j++)
			glVertex3fv(faceVertex.col(3 * i + j).data());
		glEnd();

		// Material for non-selected objects
		if (Df[i] != -1)
		{
			glEnable(GL_LIGHTING);
			setupColoredMaterial(Vector3f(0, 1, 1));
		}
	}
}

void drawVertices()
{
	// Picking
	glPushName(VERTEX); // Push the category name in the stack
	glPushName(-1);		// Vertex id in the category

	// Material
	glDisable(GL_LIGHTING);
	glColor3f(0.2f, 0.2f, 0.2f);

	// Point size
	glPointSize(7 * dpiScaling);

	// Edges (i, j) : i < j
	for (int i = 0; i < vertex.cols(); i++)
	{
		// Picking
		glLoadName(i); // Replace the name for the i-th vertex

		// Material for selected objects
		if (Dv[i] != -1) glColor3fv(selectionColor[Dv[i]].data());

		glBegin(GL_POINTS);
		glVertex3fv(vertex.col(i).data());
		glEnd();

		// Material for non-selected objects
		if (Dv[i] != -1) glColor3f(0.2f, 0.2f, 0.2f);
	}

	// Material
	glEnable(GL_LIGHTING);

	// Picking
	glPopName();
	glPopName();
}

void drawEdges()
{
	// Picking
	glPushName(EDGE); // Push the category name in the stack
	glPushName(-1);		// Vertex id in the category

	// Material
	glDisable(GL_LIGHTING);
	glColor3f(0, 0, 0);

	// Line width
	glLineWidth(1.5f * dpiScaling);

	// Edges
	for (int e = 0; e < nEdges; e++)
	{
		int i = E[e].first;
		int j = E[e].second;

		// Picking
		glLoadName(e); // Replace the name for the i-th edge

		// Material for selected objects
		if (De[e] != -1)
		{
			glLineWidth(2.0f * 1.5f * dpiScaling);
			assert(De[e] < 6);
			glColor3fv(selectionColor[De[e]].data());
		}

		glBegin(GL_LINES);
		glNormal3fv(vertexNormal.col(i).data());
		glVertex3fv(vertex.col(i).data()); // Start vertex

		glNormal3fv(vertexNormal.col(j).data());
		glVertex3fv(vertex.col(j).data()); // Start vertex
		glEnd();

		// Material for non-selected objects
		if (De[e] != -1)
		{
			glLineWidth(1.5f * dpiScaling);
			glColor3f(0, 0, 0);
		}
	}

	// Material
	glEnable(GL_LIGHTING);

	// Picking
	glPopName();
	glPopName();
}

void render(GLFWwindow* window, bool selectionMode)
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

	// Picking
	if (selectionMode) // selection 모드일 때, 이름을 쓰겠다?
	{
		glInitNames(); // Initialize the name stack
	}

	// Background color
	glClearColor(bgColor[0], bgColor[1], bgColor[2], bgColor[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(eye[0], eye[1], eye[2], center[0], center[1], center[2], up[0], up[1], up[2]);

	//Axes 
	if (!selectionMode && axes)
	{
		glDisable(GL_LIGHTING);
		drawAxes(AXIS_LENGTH, AXIS_LINE_WIDTH * dpiScaling);
	}

	// Smooth shading
	glShadeModel(GL_SMOOTH);

	// Lighting
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	setupLight(light);

	// Roatation about the z-axis
	glRotatef(theta / float(M_PI) * 180.0f, 0, 1, 0);

	// Polygon offset
	glEnable(GL_POLYGON_OFFSET_FILL);

	// Draw the mesh after setting up the material
	if (!selectionMode && faceWithGapMesh)
	{
		glPolygonOffset(1.0f, 1.0f);
		drawFaceWithGapMesh();
	}

	// Draw all the faces
	glPolygonOffset(2.0f, 1.0f);
	drawFaces();

	// Draw all the edges once
	drawEdges();

	// Draw all the vertices
	drawVertices();

	// Draw rectangle if dragging
	if (drag)
	{
		drawDragRectangle();
	}

}

void unProject(double x, double y, GLdouble *wx, GLdouble *wy, GLdouble *wz)
{
	GLdouble projection[16];
	GLdouble modelView[16];
	GLint viewPort[4];
	glGetDoublev(GL_PROJECTION_MATRIX, projection);
	glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
	glGetIntegerv(GL_VIEWPORT, viewPort);

	GLfloat zCursor, winX, winY;
	// window coordinate X, Y, Z change to openGL screen coordinates
	winX = (float)x;
	winY = (float)viewPort[3] - (float)y;

	if (gluUnProject(winX, winY, 0, modelView, projection, viewPort, wx, wy, wz) == GLU_FALSE) {
		printf("failed to unproject\n");
	}
}

void drawDragRectangle()
{
	GLdouble X1_ws, Y1_ws,Z1_ws , X2_ws, Y2_ws, Z2_ws, X3_ws, Y3_ws, Z3_ws, X4_ws, Y4_ws, Z4_ws;

	// screen coordinate to world coordinate.
	// x1, y1,z1 , x2, y2,z3, x3, y3,z3, x4, y4,z4 are corner of rectangle in world coordinate.
	unProject(preX,preY,&X1_ws,&Y1_ws,&Z1_ws); 
	unProject(newX, preY,&X2_ws, &Y2_ws, &Z2_ws);
	unProject(preX, newY, &X3_ws, &Y3_ws, &Z3_ws);
	unProject(newX, newY, &X4_ws, &Y4_ws, &Z4_ws);

	glColor3f(0.0, 0.0, 1.0);
	glLineWidth(11);
	glBegin(GL_LINE_LOOP);
	glVertex3f(X1_ws, Y1_ws,Z1_ws);
	glVertex3f(X2_ws, Y2_ws,Z2_ws);
	glVertex3f(X4_ws, Y4_ws,Z4_ws);
	glVertex3f(X3_ws, Y3_ws,Z3_ws);
	glEnd();
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
void setupLight(const Vector4f& p)
{
	GLfloat ambient[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	GLfloat diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat specular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_POSITION, p.data());

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

			// Face with gap mesh/mesh
		case GLFW_KEY_G: faceWithGapMesh = !faceWithGapMesh; break;

			// Gap increase/decrease
		case GLFW_KEY_UP:
			gap = min(gap + 0.05f, 0.5f);
			buildShrunkenFaces(vertex, faceVertex);
			break;
		case GLFW_KEY_DOWN:
			gap = max(gap - 0.05f, 0.5f);
			buildShrunkenFaces(vertex, faceVertex);
			break;

			// n-ring
		case GLFW_KEY_0: nRing = 0; break;
		case GLFW_KEY_1: nRing = 1; break;
		case GLFW_KEY_2: nRing = 2; break;
		case GLFW_KEY_3: nRing = 3; break;
		case GLFW_KEY_4: nRing = 4; break;
		case GLFW_KEY_5: nRing = 5; break;

			// Component selection
		case GLFW_KEY_V: pickMode = VERTEX; break;
		case GLFW_KEY_E: pickMode = EDGE; break;
		case GLFW_KEY_F: pickMode = FACE; break;

			// Vertex/edge adjacent
		case GLFW_KEY_A: vertexAdjacent = !vertexAdjacent; break;

			//Normal vector
		case GLFW_KEY_N: useFaceNormal = !useFaceNormal; break;

			// Back face culling
		case GLFW_KEY_B: bfcEnabled = !bfcEnabled; break;
			// Axes on/off
		case GLFW_KEY_X: axes = !axes; break;
		}
	}
}

vector<int> findNearestHits(int hits, GLuint selectBuffer[1000])
{
	bool diagnosis = false;

	if (diagnosis) cout << "hits = " << hits << endl;

	vector<int> names;

	int name = -1;
	float nearest = 2.0;
	int index = 0;

	for (int i = 0; i < hits; i++)
	{
		int n = selectBuffer[index + 0];
		float z1 = (float)selectBuffer[index + 1] / 0xffffffff;
		float z2 = (float)selectBuffer[index + 2] / 0xffffffff;

		if (n >= 2) // Category and component. 
		{
			int categoryName = selectBuffer[index + 3]; // Face,vertex, edge
			if (categoryName == pickMode)
			{
				int		componentName = selectBuffer[index + 4]; // 각 Category에서 몇 번째인지

				names.push_back(componentName);

				//if (z1 < nearest) { nearest = z1; name = componentName; }

				if (diagnosis)
				{
					cout << "\t#of names = " << n << endl;
					cout << "\tz1 = " << z1 << endl;
					cout << "\tz2 = " << z2 << endl;
					cout << "\tnames: ";
					for (int j = 0; j < n; j++)
						cout << selectBuffer[index + 3 + j] << " ";
					cout << endl;
				}
			}
		}
		// To the next available one
		index += (3 + n);
	}
	//if (diagnosis) cout << "picked = " << name << endl;
	return names;
}

vector<int> selectObjects(GLFWwindow* window, double x, double y)
{
	// Width and height of picking region in window coordinates
	double delX = x - preX;
	if (delX < 0)
		delX *= -1;
	double delY = y - preY;
	if (delY < 0)
		delY *= -1;

	// Maximum 64 selections: 5 int for one selection / 한개 고를 때 계층이라서 5개를 고른다. 

	GLuint selectBuffer[1000 * 5];
	glSelectBuffer(1000 * 5, selectBuffer);

	// Current viewport
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Backup the projection matrix
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	glLoadIdentity();
	
	// Creates a projection matrix that can be used to restirct drawing to
	// a small region of the viewport
	gluPickMatrix((preX + x) / 2, (viewport[3] - (preY + y) / 2), delX, delY, viewport); // y screen to viewport

	// Expolit the projection matrix for normal rendering
	setupProjectionMatrix();

	// Enter selection mdoe
	glRenderMode(GL_SELECT);

	// Render the objects for selection
	render(window, true);

	// Return to normal rendering mode and getting the picked object
	GLint hits = glRenderMode(GL_RENDER); // 지금까지 SELECTION 된 것을 알려준다.
	vector<int> names = findNearestHits(hits, selectBuffer); // z값이 가장 작은애?

	// Restore the projection matrix
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	return names;
}

vector<int> Multiplepicked;
void select(GLFWwindow* window, double x, double y)
{
	// Rest the previous selections
	if (Multiplepicked.size() != 0)
	{
		for (int i = 0; i < Multiplepicked.size(); i++)
		{
			if(nFaces> Multiplepicked[i])
				Df[Multiplepicked[i]]= -1;
			if (nVertices> Multiplepicked[i])
				Dv[Multiplepicked[i]] = -1;
			if (nEdges > Multiplepicked[i])
				De[Multiplepicked[i]] = -1;
		}
		Multiplepicked.clear();
	}
	// Retrieve the selected object
	Multiplepicked = selectObjects(window, x, y);

	if (Multiplepicked.size() != 0)
	{
		switch (pickMode)
		{
		case VERTEX:
			for (int i = 0; i < Multiplepicked.size(); i++)
			{
				Dv[Multiplepicked[i]] = 0;
			}
			break;
		case EDGE:
			for (int i = 0; i < Multiplepicked.size(); i++)
			{
				De[Multiplepicked[i]] = 0;
			}
			break;
		case FACE:
			for (int i = 0; i < Multiplepicked.size(); i++)
			{
				Df[Multiplepicked[i]] = 0;
			}
			break;
		case NONE: break;
		}
	}
}


void mouseButton(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
	{
		drag = false;
		// Mouse cursor position in the screen coordinate
		double xs, ys;
		glfwGetCursorPos(window, &xs, &ys);

		// Mouse cursor position in the framebuffer coordinate
		preX = xs * dpiScaling;
		preY = ys * dpiScaling;
	}
	else if(action == GLFW_RELEASE)
	{
		select(window, newX, newY);
	}
}

void mouseMove(GLFWwindow* window, double xs, double ys)
{
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) return;
	drag = true;

	// Mouse cursor position in the framebuffer coordinate
	double x = xs * dpiScaling;
	double y = ys * dpiScaling;

	newX = x;
	newY = y;
}