#include <fstream>

#include <Eigen/Dense>
using namespace Eigen;

#include <iostream>
using namespace std;

// Vertex, vertex normal, vertex indices for faces

int readMesh(const char* filename, MatrixXf& vertex, MatrixXf& normal, ArrayXXi& face)
{
	ifstream is(filename);
	if (is.fail()) return 0;

	char magicNumber[256];
	is >> magicNumber;

	// # vertices, # faces, # edges

	// 벌텍스 개수 읽고, 페이스 개수 읽고
	int nVertices = 0, nFaces = 0, nEdges = 0;
	is >> nVertices >> nFaces >> nEdges;
	cout << "# vertices = " << nVertices << endl;
	cout << "# faces = " << nFaces << endl;

	// Vertices
	// vertex == MatrixXf -> 3 x nVertices 만큼 매트릭스를 만듬.
	// vertex 개수만큼 column을 가지고, row는 3개만 가짐. x, y, z
	vertex.resize(3, nVertices);
	for (int i = 0; i < nVertices; i++)
		is >> vertex(0, i) >> vertex(1, i) >> vertex(2, i); // x, y, z 읽어

	//Normals
	normal.resize(3, nVertices);
	normal.setZero(); //0으로 초기화

	//Faces
	// face는 ArrayXXi 타입. 크기가 가변적.
	// triangles니깐 3개만.
	face.resize(3, nFaces); //Only support triangles

	int n;
	for (int i = 0; i < nFaces; i++)
	{
		is >> n >> face(0, i) >> face(1, i) >> face(2, i); 
		if (n != 3)cout << "# vertices of the " << i << "-th faces = " << n << endl;

		// Normal vector fo the face
		
		// face(1,i) 값을 가지는 인덱스 column을 3xN matrix에서 꺼내옴.
		// i번쨰 face의 0번쨰 index, i번째 face의 1번째 index...
		Vector3f v1 = vertex.col(face(1, i)) - vertex.col(face(0, i));
		Vector3f v2 = vertex.col(face(2, i)) - vertex.col(face(0, i));
		Vector3f v = v1.cross(v2).normalized();
		
		// 노말 벡터를 더해주어야겠죠?

		// Add it to the normal vector of each vertex
		normal.col(face(0, i)) += v;
		normal.col(face(1, i)) += v;
		normal.col(face(2, i)) += v;

	}

	// Normalization of the normal vectors
	for (int i = 0; i < nVertices; i++)
		normal.col(i).normalize();

	// return 값으론 몇 개의 edge를 읽었는지.
	return nEdges;
}

// Vertex, vertex normal, face normal, vertex indices for faces
int readMesh(const char* filename, MatrixXf& vertex, ArrayXXi& face,
	MatrixXf& faceNormal, MatrixXf& normal)
{
	ifstream is(filename);
	if (is.fail()) return 0;

	char magicNumber[256];
	is >> magicNumber;

	// # vertices, # faces, # edges
	int nVertices = 0, nFaces = 0, nEdges = 0;
	is >> nVertices >> nFaces >> nEdges;
	cout << "# vertices = " << nVertices << endl;
	cout << "# faces = " << nFaces << endl;

	// Vertices
	vertex.resize(3, nVertices);
	for (int i = 0; i < nVertices; i++)
		is >> vertex(0, i) >> vertex(1, i) >> vertex(2, i);

	//Normals
	normal.resize(3, nVertices);
	normal.setZero();

	// Faces
	face.resize(3, nFaces);
	faceNormal.resize(3, nFaces);

	int n;
	for (int i = 0; i < nFaces; i++)
	{
		is >> n >> face(0, i) >> face(1, i) >> face(2, i);
		if (n != 3) cout << "# vertices of the " << i << "-th faces = " << n << endl;

		//Normal vector of the face
		Vector3f v1 = vertex.col(face(1, i)) - vertex.col(face(0, i));
		Vector3f v2 = vertex.col(face(2, i)) - vertex.col(face(0, i));
		Vector3f v = v1.cross(v2).normalized();

		// Set the face normal vector
		faceNormal.col(i) = v;

		// Add it to the normal vector of each vertex
		normal.col(face(0, i)) += v;
		normal.col(face(1, i)) += v;
		normal.col(face(2, i)) += v;
	}

	//Normalization of the normal vectors
	for (int i = 0; i < nVertices; i++)
		normal.col(i).normalize();

	return nEdges;
}