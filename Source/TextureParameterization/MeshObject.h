#pragma once

#include <string>
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <Common.h>

typedef OpenMesh::TriMesh_ArrayKernelT<>  TriMesh;

class MyMesh : public TriMesh
{
public:
	MyMesh();
	~MyMesh();

	int FindVertex(MyMesh::Point pointToFind);
	void ClearMesh();
};

class GLMesh
{
public:
	GLMesh();
	~GLMesh();

	bool Init(std::string fileName);
	void Render();

	MyMesh mesh;
	GLuint vao;
	GLuint ebo;
	GLuint vboVertices, vboNormal;

private:

	bool LoadModel(std::string fileName);
	void LoadToShader();
};

class MeshObject
{
public:
	MeshObject();
	~MeshObject();

	bool Init(std::string fileName);
	void Render();
	void RenderSelectedFace();
	bool AddSelectedFace(unsigned int faceID);
	void DeleteSelectedFace(unsigned int faceID);
	bool FindClosestPoint(unsigned int faceID, glm::vec3 worldPos, glm::vec3& closestPos);
	void CalculateBoundaryPoints();

	// heh init should be public var, so we could used it in the main render
	MyMesh::HalfedgeHandle heh_init;

	// when getting property from main, we need the mesh
	GLMesh boundaryModel;
	GLMesh model;

	// when getting property from main, we need the property name too
	// property part for old vertex
	OpenMesh::VPropHandleT<glm::vec3> vColor;
	OpenMesh::VPropHandleT<OpenMesh::VertexHandle> newVH;

	// property part for new vertex
	OpenMesh::VPropHandleT<OpenMesh::VertexHandle> oldVH;
	OpenMesh::VPropHandleT<bool> isBoundary;
	OpenMesh::VPropHandleT<glm::vec2> texCoord;

	// property part for old edge
	OpenMesh::EPropHandleT<double> weight;

private:
	std::vector<unsigned int> selectedFace;
	std::vector<unsigned int*> fvIDsPtr;
	std::vector<int> elemCount;
};

