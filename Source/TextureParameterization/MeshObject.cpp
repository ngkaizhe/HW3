#include "MeshObject.h"
#include <Eigen/Sparse>
#include <map>
#include <algorithm>

#define Quad
//#define Harmonic

struct OpenMesh::VertexHandle const OpenMesh::PolyConnectivity::InvalidVertexHandle;

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

int MyMesh::FindVertex(MyMesh::Point pointToFind)
{
	int idx = -1;
	for (MyMesh::VertexIter v_it = vertices_begin(); v_it != vertices_end(); ++v_it)
	{
		MyMesh::Point p = point(*v_it);
		if (pointToFind == p)
		{
			idx = v_it->idx();
			break;
		}
	}

	return idx;
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
	boundaryModel.mesh.add_property(isBoundary);
	boundaryModel.mesh.add_property(vColor);

	// compute all weight of the edge handle now
	for (MyMesh::EIter e_it = model.mesh.edges_begin(); e_it != model.mesh.edges_end(); ++e_it) {
		model.mesh.property(this->weight, *e_it) = 1;
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
			boundaryModel.mesh.property(vColor, vh) = colors[j % colors.size()];

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

			// set isBoundary value of property for the boundary model mesh
			boundaryModel.mesh.property(isBoundary, vh1) = true;

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


		// now we have calculated all boundary vertices' texture coordinates
		// start to draw the boundary vertices
		glPointSize(8);
		glBegin(GL_POINTS);

		// run through the boundary vertices and draw it on our left part model
		heh = heh_init;

		do {
			MyMesh::VertexHandle vhFrom = boundaryModel.mesh.from_vertex_handle(heh);
			MyMesh::Point vPoint = boundaryModel.mesh.point(vhFrom);

			// set color
			glm::vec3 color = boundaryModel.mesh.property(this->vColor, vhFrom);
			glColor4f(color[0], color[1], color[2], 1);
			// draw vertex
			glVertex3f(vPoint[0], vPoint[1], vPoint[2]);

			// goto next half edge
			heh = boundaryModel.mesh.next_halfedge_handle(heh);
		} while (heh != heh_init);
		glEnd();
	}

	boundaryModel.mesh.update_normals();
}
