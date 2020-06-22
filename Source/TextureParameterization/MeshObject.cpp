#include "MeshObject.h"
#include <Eigen/Sparse>
#include <map>
#include <algorithm>
#include<Eigen/Sparse>

using namespace Eigen;

#define Quad
//#define Harmonic

struct OpenMesh::VertexHandle const OpenMesh::PolyConnectivity::InvalidVertexHandle;

// helper function to find the degree between 3 vertex
float getRadianBetween2Lines(MyMesh::Point pBase, MyMesh::Point p1, MyMesh::Point p2);
// helper function to match RowNumber to VertexHandleID
unsigned int getMatNumber(std::vector<unsigned int>, int);
unsigned int getVHID(std::vector<unsigned int>, int);

#pragma region MyMesh

MyMesh::MyMesh()
{
	request_vertex_normals();
	request_vertex_status();
	request_face_status();
	request_edge_status();
}

MyMesh::~MyMesh()
{

}

void MyMesh::ClearMesh()
{
	if (!faces_empty())
	{
		for (MyMesh::FaceIter f_it = faces_begin(); f_it != faces_end(); ++f_it)
		{
			delete_face(*f_it, true);
		}

		garbage_collection();
	}
}

#pragma endregion

#pragma region GLMesh

GLMesh::GLMesh()
{

}

GLMesh::~GLMesh()
{

}

bool GLMesh::Init(std::string fileName)
{
	if (LoadModel(fileName))
	{
		LoadToShader();
		return true;
	}
	return false;
}

void GLMesh::Render()
{
	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, mesh.n_faces() * 3, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}


bool GLMesh::LoadModel(std::string fileName)
{
	OpenMesh::IO::Options ropt;
	if (OpenMesh::IO::read_mesh(mesh, fileName, ropt))
	{
		if (!ropt.check(OpenMesh::IO::Options::VertexNormal) && mesh.has_vertex_normals())
		{
			mesh.request_face_normals();
			mesh.update_normals();
			mesh.release_face_normals();
		}

		return true;
	}

	return false;
}

void GLMesh::LoadToShader()
{
	std::vector<MyMesh::Point> vertices;
	vertices.reserve(mesh.n_vertices());
	for (MyMesh::VertexIter v_it = mesh.vertices_begin(); v_it != mesh.vertices_end(); ++v_it)
	{
		vertices.push_back(mesh.point(*v_it));

		MyMesh::Point p = mesh.point(*v_it);
	}

	std::vector<MyMesh::Normal> normals;
	normals.reserve(mesh.n_vertices());
	for (MyMesh::VertexIter v_it = mesh.vertices_begin(); v_it != mesh.vertices_end(); ++v_it)
	{
		normals.push_back(mesh.normal(*v_it));
	}

	std::vector<unsigned int> indices;
	indices.reserve(mesh.n_faces() * 3);
	for (MyMesh::FaceIter f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it)
	{
		for (MyMesh::FaceVertexIter fv_it = mesh.fv_iter(*f_it); fv_it.is_valid(); ++fv_it)
		{
			indices.push_back(fv_it->idx());
		}
	}

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vboVertices);
	glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(MyMesh::Point) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &vboNormal);
	glBindBuffer(GL_ARRAY_BUFFER, vboNormal);
	glBufferData(GL_ARRAY_BUFFER, sizeof(MyMesh::Normal) * normals.size(), &normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * indices.size(), &indices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

#pragma endregion

MeshObject::MeshObject()
{
	
}

MeshObject::~MeshObject()
{
}

bool MeshObject::Init(std::string fileName)
{
	selectedFace.clear();

	bool retV = model.Init(fileName);

	// add property for model/ old model
	model.mesh.add_property(newVH);
	model.mesh.add_property(weight);

	// add property for boundary model/ new model
	boundaryModel.mesh.add_property(texCoord);
	boundaryModel.mesh.add_property(oldVH);

	// compute all weight of the edge handle now
	for (MyMesh::EIter e_it = model.mesh.edges_begin(); e_it != model.mesh.edges_end(); ++e_it) {
		MyMesh::Point v0, v1, v2, v3;
		// find v0,v1,v2,v3

		// find one of the half edge handle
		MyMesh::HalfedgeHandle heh0 = model.mesh.halfedge_handle(*e_it , 0);
		v0 = model.mesh.point(model.mesh.from_vertex_handle(heh0));
		v1 = model.mesh.point(model.mesh.to_vertex_handle(heh0));

		heh0 = model.mesh.next_halfedge_handle(heh0);
		v2 = model.mesh.point(model.mesh.to_vertex_handle(heh0));

		// find the opposite half edge
		MyMesh::HalfedgeHandle heh1 = model.mesh.halfedge_handle(*e_it, 1);
		heh1 = model.mesh.next_halfedge_handle(heh1);
		v3 = model.mesh.point(model.mesh.to_vertex_handle(heh1));

		// get the radian of v1, v2, v0
		double rad1 = getRadianBetween2Lines(v2, v1, v0);
		// get the radian of v1, v3, v0
		double rad2 = getRadianBetween2Lines(v3, v1, v0);

		// set the weight
		model.mesh.property(this->weight, *e_it) = (1 / glm::tan(rad1)) + (1 / glm::tan(rad2));
	}

	return retV;
}

void MeshObject::Render()
{
	glBindVertexArray(model.vao);
	glDrawElements(GL_TRIANGLES, model.mesh.n_faces() * 3, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void MeshObject::RenderSelectedFace()
{
	if (selectedFace.size() > 0)
	{
		std::vector<unsigned int*> offsets(selectedFace.size());
		for (int i = 0; i < offsets.size(); ++i)
		{
			offsets[i] = (GLuint*)(selectedFace[i] * 3 * sizeof(GLuint));
		}

		std::vector<int> count(selectedFace.size(), 3);

		glBindVertexArray(model.vao);
		glMultiDrawElements(GL_TRIANGLES, &count[0], GL_UNSIGNED_INT, (const GLvoid **)&offsets[0], selectedFace.size());
		glBindVertexArray(0);
	}
}

bool MeshObject::AddSelectedFace(unsigned int faceID)
{
	if (std::find(selectedFace.begin(), selectedFace.end(), faceID) == selectedFace.end() &&
		faceID >= 0 && faceID < model.mesh.n_faces())
	{
		selectedFace.push_back(faceID);
		return true;
	}
	return false;
}

void MeshObject::DeleteSelectedFace(unsigned int faceID)
{
	selectedFace.erase(std::remove(selectedFace.begin(), selectedFace.end(), faceID), selectedFace.end());
}

bool MeshObject::FindClosestPoint(unsigned int faceID, glm::vec3 worldPos, glm::vec3& closestPos)
{
	OpenMesh::FaceHandle fh = model.mesh.face_handle(faceID);
	if (!fh.is_valid())
	{
		return false;
	}

	double minDistance = 0.0;
	MyMesh::Point p(worldPos.x, worldPos.y, worldPos.z);
	MyMesh::FVIter fv_it = model.mesh.fv_iter(fh);
	MyMesh::VertexHandle closestVH = *fv_it;
	MyMesh::Point v1 = model.mesh.point(*fv_it);
	++fv_it;

	minDistance = (p - v1).norm();
	for (; fv_it.is_valid(); ++fv_it)
	{
		MyMesh::Point v = model.mesh.point(*fv_it);
		double distance = (p - v).norm();
		if (minDistance > distance)
		{
			minDistance = distance;
			closestVH = *fv_it;
		}
	}
	MyMesh::Point closestPoint = model.mesh.point(closestVH);
	closestPos.x = closestPoint[0];
	closestPos.y = closestPoint[1];
	closestPos.z = closestPoint[2];

	// find the closest point upwards to face (find 2 rings upwards)


	return true;
}


void MeshObject::CalculateBoundaryPoints()
{
	boundaryModel.mesh.clear();
	boundaryModel.mesh.request_vertex_normals();
	boundaryModel.mesh.request_face_normals();

	// the color we initial for used in vertex point color
	std::vector<glm::vec3> colors = {
		glm::vec3(0, 0, 1),
		glm::vec3(1, 0, 0),
		glm::vec3(0, 1, 0),
	};

	// record which vertex, we have already added to the boundary model
	std::map<int, int> usedVertex;

	// get the mesh of selected face
	for (int i = 0; i < selectedFace.size(); i++) {
		// get face handler from faceID
		OpenMesh::FaceHandle fh = model.mesh.face_handle(selectedFace[i]);

		// face vhandles
		std::vector<MyMesh::VertexHandle> face_vhandles;

		// find each fv iterator
		int j = 0;
		for (MyMesh::FaceVertexIter fv_it = model.mesh.fv_iter(fh); fv_it.is_valid(); ++fv_it, j++) {
			// get each vertex handler
			MyMesh::Point p = model.mesh.point(*fv_it);
			MyMesh::VertexHandle vh;

			std::map<int, int>::iterator usedVertex_it = usedVertex.find(fv_it->idx());

			// the vertex didn't find out yet
			if (usedVertex_it == usedVertex.end()) {
				vh = boundaryModel.mesh.add_vertex(p);
				usedVertex[fv_it->idx()] = vh.idx();
			}
			else {
				vh = boundaryModel.mesh.vertex_handle(usedVertex_it->second);
			}

			// set the "pointer" to have a link between new vertex
			// old vertex linked to new vertex
			boundaryModel.mesh.property(oldVH, vh) = *fv_it;

			// new vertex linked to old vertex
			model.mesh.property(newVH, *fv_it) = vh;

			// add the vertex handle for the new mesh(as a new face)
			face_vhandles.push_back(vh);
		}

		// add face to the boundary model's mesh
		boundaryModel.mesh.add_face(face_vhandles);
	}

	// find boundary halfedge first
	MyMesh::HalfedgeHandle heh;

	// loop through all halfedge to check is boundary
	for (MyMesh::EdgeIter e_it = boundaryModel.mesh.edges_begin(); e_it != boundaryModel.mesh.edges_end(); ++e_it) {
		// we found the half edge boundary
		if (boundaryModel.mesh.is_boundary(*e_it)) {
			// init our half edge handle
			// get first half edge and check whether it is a boundary or not
			heh_init = boundaryModel.mesh.halfedge_handle(*e_it, 0);
			if (boundaryModel.mesh.is_boundary(heh_init)) {
				break;
			}
			// else go to the opposite side
			else {
				heh_init = boundaryModel.mesh.halfedge_handle(*e_it, 1);
				break;
			}
		}
	}

	// if we have any selected face
	if (selectedFace.size() > 0) {
		// we will run the same loop 2 times
		// first time -> add up the total length of the boundary vertices, and set those vertex handle to booundary
		// second time -> calculate the UV of each boundary vertex handler

		float totalLength3DSpace = 0;

		// init our heh
		heh = heh_init;
		do {
			MyMesh::VertexHandle vh1 = boundaryModel.mesh.from_vertex_handle(heh);
			MyMesh::VertexHandle vh2 = boundaryModel.mesh.to_vertex_handle(heh);

			// get the point of 2 vertices
			MyMesh::Point p1 = boundaryModel.mesh.point(vh1);
			MyMesh::Point p2 = boundaryModel.mesh.point(vh2);

			// get the distance between this 2 vertices
			totalLength3DSpace += glm::distance(glm::vec3(p1[0], p1[1], p1[2]), glm::vec3(p2[0], p2[1], p2[2]));

			// goto next half edge
			heh = boundaryModel.mesh.next_halfedge_handle(heh);
		} while (heh != heh_init);

		// now we have the total length
		// yet we set our uv space to be (0, 0), (0, 1), (1, 1), (1, 0)
		// so the total length of uv space should be 4
		float totalLengthUVSpace = 4;

		// init our heh
		heh = heh_init;
		// init our first point to be (0,0)
		MyMesh::VertexHandle vhFirst = boundaryModel.mesh.from_vertex_handle(heh);
		boundaryModel.mesh.property(this->texCoord, vhFirst) = glm::vec2(0, 0);

		// this variable track the distance for each vertex runned through
		float currentDis = 0;

		// this loop help us calculate the boundary vh's texture coordinate
		// we dont run the last loop because we have already initialized it to (0,0)
		while (boundaryModel.mesh.next_halfedge_handle(heh) != heh_init) {
			// previous vertex
			MyMesh::VertexHandle vh1 = boundaryModel.mesh.from_vertex_handle(heh);
			// next vertex
			MyMesh::VertexHandle vh2 = boundaryModel.mesh.to_vertex_handle(heh);

			// get the point of 2 vertices
			MyMesh::Point p1 = boundaryModel.mesh.point(vh1);
			MyMesh::Point p2 = boundaryModel.mesh.point(vh2);

			// get the distance between this 2 vertices
			currentDis += glm::distance(glm::vec3(p1[0], p1[1], p1[2]), glm::vec3(p2[0], p2[1], p2[2])) 
				/ totalLength3DSpace 
				* totalLengthUVSpace;

			// declare our texture coordinate for this vertex
			glm::vec2 currentTexCoor;

			// transfer current distance to the uv space
			// PLUSY
			if (currentDis >= 0 && currentDis <= 1) {
				currentTexCoor = glm::vec2(0, currentDis);
			}
			// PLUSX
			else if (currentDis > 1 && currentDis <= 2) {
				float tcurrentDis = currentDis - 1;
				currentTexCoor = glm::vec2(tcurrentDis, 1);
			}
			// MINUSY
			else if (currentDis > 2 && currentDis <= 3) {
				float tcurrentDis = currentDis - 2;
				currentTexCoor = glm::vec2(1, 1 - tcurrentDis);
			}
			// MINUSX
			else if (currentDis > 3 && currentDis <= 4) {
				float tcurrentDis = currentDis - 3;
				currentTexCoor = glm::vec2(1 - tcurrentDis, 0);
			}

			else {
				throw "The Current Distance shouldn't exceed 4!";
			}

			// set our texture coordinate value to our property
			boundaryModel.mesh.property(this->texCoord, vh2) = currentTexCoor;

			// goto next half edge
			heh = boundaryModel.mesh.next_halfedge_handle(heh);
		}
	}

	// find the middle point
	std::vector<unsigned int> MatNumMapToVHID;
	// solve linear equation
	// we should run through 2 loops (vertex handle iter)
	// first loop is to initialize the mapping between MatIDMapToVHID (vertex handle iter)
	for (MyMesh::VertexIter v_it = boundaryModel.mesh.vertices_begin(); v_it != boundaryModel.mesh.vertices_end(); ++v_it) {
		// we only fill the vertex that isn't a boundary
		if (!boundaryModel.mesh.is_boundary(*v_it)) {
			MatNumMapToVHID.push_back(v_it->idx());
		}
	}

	if (MatNumMapToVHID.size() > 0) {
		// initialize the matrix
		// known
		SparseMatrix<double> A(MatNumMapToVHID.size(), MatNumMapToVHID.size());
		VectorXd BX(MatNumMapToVHID.size());
		VectorXd BY(MatNumMapToVHID.size());

		int m = MatNumMapToVHID.size();
		// reset all matrix to 0
		for (int i = 0; i < m; i++) {
			for (int j = 0; j < m; j++) {
				A.coeffRef(i, j) = 0;
			}
		}
		for (int i = 0; i < m; i++) {
			BX[i] = 0;
			BY[i] = 0;
		}

		// second loop is to fill up the matrix
		for (MyMesh::VertexIter v_it = boundaryModel.mesh.vertices_begin(); v_it != boundaryModel.mesh.vertices_end(); ++v_it) {
			// if vertex is not boundary vertex
			if (!boundaryModel.mesh.is_boundary(*v_it)) {

				int row = getMatNumber(MatNumMapToVHID, v_it->idx());
				double totalWeight = 0;
				// find the old vertex handle of the base vertex(middle one)
				MyMesh::VertexHandle vh_oldBase = boundaryModel.mesh.property(this->oldVH, *v_it);

				// we use one ring to find the vertex close to not boundary vertex
				for (MyMesh::VertexVertexIter vv_it = boundaryModel.mesh.vv_iter(*v_it); vv_it.is_valid(); ++vv_it) {
					// find the old vertex handle of the outer vertex
					MyMesh::VertexHandle vh_oldOut = boundaryModel.mesh.property(this->oldVH, *vv_it);

					// weight is putting at the old model, so
					// get the edge handle first(from the old model)
					MyMesh::HalfedgeHandle tempheh = model.mesh.find_halfedge(vh_oldBase, vh_oldOut);
					MyMesh::EdgeHandle eh = model.mesh.edge_handle(tempheh);
					// get the weight from the edge handle
					double weight = model.mesh.property(this->weight, eh);
					totalWeight += weight;

					// if the vertex found is boundary we put to B
					// boundary textCoor should be known
					if (boundaryModel.mesh.is_boundary(*vv_it)) {
						glm::vec2 texCoor = boundaryModel.mesh.property(this->texCoord, *vv_it);
						BX[row] += (weight * texCoor.x);
						BY[row] += (weight * texCoor.y);

						/*double tBX = BX[0];
						double tBY = BY[0];

						cout << "TBX -> " << tBX << ", TBY -> " << tBY << '\n';*/
					}
					// else we put to A(-1)
					// unboundary textCoor should be unknown
					else {
						int col = getMatNumber(MatNumMapToVHID, vv_it->idx());
						// we put the weight inside
						A.coeffRef(row, col) = -1 * weight;
					}
				}

				// lastly we put the total weight of the current row weight
				A.coeffRef(row, row) = totalWeight;
			}
		}
		double tBX = BX[0];
		double tBY = BY[0];
		double tA = A.coeffRef(0, 0);

		// solve the linear equation with eigen
		// AX = (BX)
		// X = (A^(-1))(BX)
		// AY = (BY)
		// Y = (A^(-1))(BY)
		A.makeCompressed();
		SparseQR<SparseMatrix<double>, COLAMDOrdering<int>> linearSolver;
		linearSolver.compute(A);
		VectorXd X = linearSolver.solve(BX);
		VectorXd Y = linearSolver.solve(BY);

		// place all X to the correct uv
		for (int i = 0; i < MatNumMapToVHID.size(); i++) {
			int vhID = MatNumMapToVHID[i];
			MyMesh::VertexHandle vh = boundaryModel.mesh.vertex_handle(vhID);
			boundaryModel.mesh.property(texCoord, vh).x = X[i];
			boundaryModel.mesh.property(texCoord, vh).y = Y[i];
		}
	}

	boundaryModel.mesh.update_normals();
}

void MeshObject::RenderTextureFace()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// set the location of position and texture coordinate
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> texCoords;

	// we will loop through all faces
	// loop through its face vertex and save its position to positions
	for (MyMesh::FaceIter f_it = boundaryModel.mesh.faces_begin(); f_it != boundaryModel.mesh.faces_end(); f_it++) {
		// find the face vertex iterator
		for (MyMesh::FaceVertexIter fv_it = boundaryModel.mesh.fv_begin(*f_it); fv_it != boundaryModel.mesh.fv_end(*f_it); fv_it++) {
			glm::vec3 position;
			glm::vec2 texCoor = boundaryModel.mesh.property(this->texCoord, *fv_it);
			MyMesh::Point point = boundaryModel.mesh.point(*fv_it);
			position = glm::vec3(point[0], point[1], point[2]);

			// add position and texcoordinates to the vector
			positions.push_back(position);
			texCoords.push_back(texCoor);
		}
	}

	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * positions.size() + sizeof(glm::vec2) * texCoords.size(), NULL, GL_DYNAMIC_DRAW);

	// add position
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * positions.size(), &positions[0]);
	// add texcoord
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * positions.size(), sizeof(glm::vec2) * texCoords.size(), &texCoords[0]);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(0);

	// texture coord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)(sizeof(glm::vec3) * positions.size()));
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLES, 0, boundaryModel.mesh.n_faces() * 3);
}

// helper function to find the degree between 3 vertex
float getRadianBetween2Lines(MyMesh::Point pBase, MyMesh::Point p1, MyMesh::Point p2) {
	glm::vec3 vBase = glm::vec3(pBase[0], pBase[1], pBase[2]);
	glm::vec3 v1 = glm::vec3(p1[0], p1[1], p1[2]);
	glm::vec3 v2 = glm::vec3(p2[0], p2[1], p2[2]);

	glm::vec3 a = vBase - v1;
	glm::vec3 b = vBase - v2;

	float radian = glm::acos(glm::dot(a, b) / (glm::length(a) * glm::length(b)));
	return radian;
}

// helper function to match RowNumber to VertexHandleID
unsigned int getMatNumber(std::vector<unsigned int> MatNumMapToVHID, int vhID) {
	for (int i = 0; i < MatNumMapToVHID.size(); i++) {
		if (MatNumMapToVHID[i] == vhID) {
			return i;
		}
	}
	// we shouldn't meet here
	throw "Row Number couldn't find the index you provided, please check!";
}

unsigned int getVHID(std::vector<unsigned int> MatNumMapToVHID, int rowNumber) {
	if (rowNumber >= MatNumMapToVHID.size()) {
		// we shouldn't meet here
		throw "Row Number couldn't find the index you provided, please check!";
	}
	return MatNumMapToVHID[rowNumber];
}