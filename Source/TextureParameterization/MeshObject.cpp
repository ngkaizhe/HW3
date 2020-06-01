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

	return model.Init(fileName);
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

	std::sort(selectedFace.begin(), selectedFace.end());

	std::map<int, int> usedVertex;

	// get the mesh of selected face
	for (int i = 0; i < selectedFace.size(); i++) {
		// get face handler from faceID
		OpenMesh::FaceHandle fh = model.mesh.face_handle(selectedFace[i]);

		// face vhandles
		std::vector<MyMesh::VertexHandle> face_vhandles;
		face_vhandles.reserve(3);

		// find each fv iterator
		for (MyMesh::FaceVertexIter fv_it = model.mesh.fv_iter(fh); fv_it.is_valid(); ++fv_it) {
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

			face_vhandles.push_back(vh);
		}

		// add face to the boundary model's mesh
		boundaryModel.mesh.add_face(face_vhandles);
	}

	std::cout << "Boundary Mesh total faces are -> " << boundaryModel.mesh.n_faces() << '\n';
	
	// get boundaries' vertex
	std::vector<MyMesh::Point> boundaryVertices;

	// find boundary halfedge first
	MyMesh::HalfedgeHandle heh, heh_init;

	// loop through all halfedge to check is boundary
	for (MyMesh::EdgeIter e_it = boundaryModel.mesh.edges_begin(); e_it != boundaryModel.mesh.edges_end(); ++e_it) {
		// we found the half edge boundary
		if (boundaryModel.mesh.is_boundary(*e_it)) {
			if (!heh_init.is_valid()) {
				heh_init = boundaryModel.mesh.halfedge_handle(*e_it, 1);
			}

			//// get from vertex handle by the half edge handle
			//MyMesh::VertexHandle vh = boundaryModel.mesh.from_vertex_handle(heh);
			//boundaryVertices.push_back(boundaryModel.mesh.point(vh));

			//heh = boundaryModel.mesh.next_halfedge_handle(heh);
			//// loop through other half edge boundary from the first one
			//// use adjacent halfedge to find all boundary halfedge
			//while (heh != heh_init) {
			//	MyMesh::VertexHandle vh = boundaryModel.mesh.from_vertex_handle(heh);
			//	boundaryVertices.push_back(boundaryModel.mesh.point(vh));

			//	heh = boundaryModel.mesh.next_halfedge_handle(heh);
			//}
			//break;
		}
	}

	if (selectedFace.size() > 0) {
		heh = heh_init;
		MyMesh::VertexHandle vh = boundaryModel.mesh.from_vertex_handle(heh);
		boundaryVertices.push_back(boundaryModel.mesh.point(vh));

		heh = boundaryModel.mesh.next_halfedge_handle(heh);
		while (heh != heh_init) {
			MyMesh::VertexHandle vh = boundaryModel.mesh.from_vertex_handle(heh);
			boundaryVertices.push_back(boundaryModel.mesh.point(vh));

			heh = boundaryModel.mesh.next_halfedge_handle(heh);
		}
	}

	std::vector<glm::vec4> colors({
		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
	});

	// draw the boundary vertices
	glPointSize(8);
	glBegin(GL_POINTS);
	for (int i = 0; i < boundaryVertices.size(); i++) {
		// set color
		glm::vec4 color = colors[i % 4];
		glColor4f(color[0], color[1], color[2], color[3]);
		// draw vertex
		glVertex3f(boundaryVertices[i][0], boundaryVertices[i][1], boundaryVertices[i][2]);
	}
	glEnd();

	boundaryModel.mesh.update_normals();
}
