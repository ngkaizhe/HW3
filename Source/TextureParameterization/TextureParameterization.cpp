#include <Common.h>
#include <ViewManager.h>
#include <AntTweakBar/AntTweakBar.h>
#include <ResourcePath.h>

#include "OpenMesh.h"
#include "MeshObject.h"
#include "DrawModelShader.h"
#include "PickingShader.h"
#include "PickingTexture.h"
#include "DrawPickingFaceShader.h"
#include "DrawTextureShader.h"
#include "DrawPointShader.h"
#include<Eigen/Sparse>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../Include/STB/stb_image_write.h"

using namespace glm;
using namespace std;
using namespace Eigen;

glm::vec3 worldPos;
bool updateFlag = false;
bool isRightButtonPress = false;
GLuint currentFaceID = 0;
int currentMouseX = 0;
int currentMouseY = 0;
int windowWidth = 1200;
int windowHeight = 600;
const std::string ProjectName = "TextureParameterization";

GLuint			program;			// shader program
mat4			proj_matrix;		// projection matrix
float			aspect;
ViewManager		meshWindowCam;

MeshObject model;

// shaders
DrawModelShader drawModelShader;
DrawPickingFaceShader drawPickingFaceShader;
PickingShader pickingShader;
PickingTexture pickingTexture;
DrawPointShader drawPointShader;

// vbo for drawing point
GLuint vboPoint;

int mainWindow, subWindow1, subWindow2;
enum SelectionMode
{
	ADD_FACE,
	DEL_FACE,
	SELECT_POINT,
	ADD_TEXTURE,
};
SelectionMode selectionMode = ADD_FACE;

TwBar* bar;
TwEnumVal SelectionModeEV[] = { {ADD_FACE, "Add face"}, {DEL_FACE, "Delete face"}, {SELECT_POINT, "Point"}, {ADD_TEXTURE, "Add texture"} };
TwType SelectionModeType;

// helper function to match RowNumber to VertexHandleID
unsigned int getMatNumber(std::vector<unsigned int> , int);
unsigned int getVHID(std::vector<unsigned int> , int);


void SetupGUI()
{
#ifdef _MSC_VER
	TwInit(TW_OPENGL, NULL);
#else
	TwInit(TW_OPENGL_CORE, NULL);
#endif
	TwGLUTModifiersFunc(glutGetModifiers);
	bar = TwNewBar("Texture Parameter Setting");
	TwDefine(" 'Texture Parameter Setting' size='220 90' ");
	TwDefine(" 'Texture Parameter Setting' fontsize='3' color='96 216 224'");

	// Defining season enum type
	SelectionModeType = TwDefineEnum("SelectionModeType", SelectionModeEV, 3);
	// Adding season to bar
	TwAddVarRW(bar, "SelectionMode", SelectionModeType, &selectionMode, NULL);
}

void My_LoadModel()
{
	if (model.Init(ResourcePath::modelPath))
	{
		/*int id = 0;
		while (model.AddSelectedFace(id))
		{
			++id;
		}
		model.Parameterization();
		drawTexture = true;*/

		puts("Load Model");
	}
	else
	{
		puts("Load Model Failed");
	}
}

void InitOpenGL()
{
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_POINT_SMOOTH);
}

void InitData()
{
	ResourcePath::shaderPath = "./Shader/" + ProjectName + "/";
	ResourcePath::imagePath = "./Imgs/" + ProjectName + "/";
	ResourcePath::modelPath = "./Model/UnionSphere.obj";

	//Initialize shaders
	///////////////////////////	
	drawModelShader.Init();
	pickingShader.Init();
	pickingTexture.Init(windowWidth, windowHeight);
	drawPickingFaceShader.Init();
	drawPointShader.Init();

	glGenBuffers(1, &vboPoint);

	//Load model to shader program
	My_LoadModel();
}

void Reshape(int width, int height)
{
	windowWidth = width;
	windowHeight = height;

	TwWindowSize(windowWidth / 2, height);
	// left part
	glutSetWindow(subWindow1);
	glutPositionWindow(0, 0);
	glutReshapeWindow(windowWidth / 2, windowHeight);
	glViewport(0, 0, windowWidth / 2, windowHeight);

	// right part
	glutSetWindow(subWindow2);
	glutPositionWindow(windowWidth / 2, 0);
	glutReshapeWindow(windowWidth / 2, windowHeight);
	glViewport(windowWidth / 2, 0, windowWidth / 2, windowHeight);

	aspect = windowWidth / 2 * 1.0f / windowHeight;
	meshWindowCam.SetWindowSize(windowWidth / 2, windowHeight);
	pickingTexture.Init(windowWidth/2, windowHeight);
}

// GLUT callback. Called to draw the scene.
void RenderMeshWindow()
{
	glutSetWindow(subWindow1);

	//Update shaders' input variable
	///////////////////////////	

	glm::mat4 mvMat = meshWindowCam.GetViewMatrix() * meshWindowCam.GetModelMatrix();
	glm::mat4 pMat = meshWindowCam.GetProjectionMatrix(aspect);

	// write faceID+1 to framebuffer
	pickingTexture.Enable();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	pickingShader.Enable();
	pickingShader.SetMVMat(value_ptr(mvMat));
	pickingShader.SetPMat(value_ptr(pMat));

	model.Render();

	pickingShader.Disable();
	pickingTexture.Disable();

	
	// draw model
	glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawModelShader.Enable();
	glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(mvMat)));

	drawModelShader.SetWireColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	drawModelShader.SetFaceColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	drawModelShader.UseLighting(true);
	drawModelShader.DrawWireframe(true);
	drawModelShader.SetNormalMat(normalMat);
	drawModelShader.SetMVMat(mvMat);
	drawModelShader.SetPMat(pMat);

	model.Render();

	drawModelShader.Disable();
	
	// render selected face
	if (selectionMode == SelectionMode::ADD_FACE || 
		selectionMode == SelectionMode::DEL_FACE ||
		selectionMode == SelectionMode::ADD_TEXTURE)
	{
		drawPickingFaceShader.Enable();
		drawPickingFaceShader.SetMVMat(value_ptr(mvMat));
		drawPickingFaceShader.SetPMat(value_ptr(pMat));
		model.RenderSelectedFace();
		drawPickingFaceShader.Disable();

		// draw the points that is boundary
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMultMatrixf(value_ptr(pMat));
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMultMatrixf(value_ptr(mvMat));

		model.CalculateBoundaryPoints();
	}

	glUseProgram(0);

	// render closest point
	if (selectionMode == SelectionMode::SELECT_POINT)
	{
		if (updateFlag)
		{
			float depthValue = 0;
			int windowX = currentMouseX;
			int windowY = windowHeight - currentMouseY;
			glReadPixels(windowX, windowY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthValue);

			GLint _viewport[4];
			glGetIntegerv(GL_VIEWPORT, _viewport);
			glm::vec4 viewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
			glm::vec3 windowPos(windowX, windowY, depthValue);
			glm::vec3 wp = glm::unProject(windowPos, mvMat, pMat, viewport);
			model.FindClosestPoint(currentFaceID - 1, wp, worldPos);

			updateFlag = false;
		}
		/*
			Using OpenGL 1.1 to draw point
		*/
		glPushMatrix();
		glPushAttrib(GL_POINT_BIT);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glMultMatrixf(glm::value_ptr(pMat));

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glMultMatrixf(glm::value_ptr(mvMat));

			glPointSize(15.0f);
			glColor3f(1.0, 0.0, 1.0);
			glBegin(GL_POINTS);
			glVertex3fv(glm::value_ptr(worldPos));
			glEnd();
		glPopAttrib();
		glPopMatrix();

		
		glBindBuffer(GL_ARRAY_BUFFER, vboPoint);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3), glm::value_ptr(worldPos), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glm::vec4 pointColor(1.0, 0.0, 1.0, 1.0);
		drawPointShader.Enable();
		drawPointShader.SetMVMat(mvMat);
		drawPointShader.SetPMat(pMat);
		drawPointShader.SetPointColor(pointColor);
		drawPointShader.SetPointSize(15.0);

		glDrawArrays(GL_POINTS, 0, 1);

		drawPointShader.Disable();

		glBindBuffer(GL_ARRAY_BUFFER, 0);

	}

	TwDraw();
	glutSwapBuffers();
}

void RenderTextureWindow() {
	glutSetWindow(subWindow2);

	glClearColor(.7f, .7f, .7f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	std::vector<unsigned int> MatNumMapToVHID;
	// solve linear equation
	// we should run through 2 loops (vertex handle iter)
	// first loop is to initialize the mapping between MatIDMapToVHID (vertex handle iter)
	for (MyMesh::VertexIter v_it = model.boundaryModel.mesh.vertices_begin(); v_it != model.boundaryModel.mesh.vertices_end(); ++v_it) {
		// we only fill the vertex that isn't a boundary
		if (!model.boundaryModel.mesh.is_boundary(*v_it)) {
			MatNumMapToVHID.push_back(v_it->idx());
		}
	}

	// Debug message
	// cout << "Total Unboundary Vertices Amount is -> " << MatIDMapToVHID.size() << '\n';

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
		for (MyMesh::VertexIter v_it = model.boundaryModel.mesh.vertices_begin(); v_it != model.boundaryModel.mesh.vertices_end(); ++v_it) {
			// if vertex is not boundary vertex
			if (!model.boundaryModel.mesh.is_boundary(*v_it)) {

				int row = getMatNumber(MatNumMapToVHID, v_it->idx());
				double totalWeight = 0;
				// find the old vertex handle of the base vertex(middle one)
				MyMesh::VertexHandle vh_oldBase = model.boundaryModel.mesh.property(model.oldVH, *v_it);

				// we use one ring to find the vertex close to not boundary vertex
				for (MyMesh::VertexVertexIter vv_it = model.boundaryModel.mesh.vv_iter(*v_it); vv_it.is_valid(); ++vv_it) {
					// find the old vertex handle of the outer vertex
					MyMesh::VertexHandle vh_oldOut = model.boundaryModel.mesh.property(model.oldVH, *vv_it);

					// weight is putting at the old model, so
					// get the edge handle first(from the old model)
					MyMesh::HalfedgeHandle tempheh = model.model.mesh.find_halfedge(vh_oldBase, vh_oldOut);
					MyMesh::EdgeHandle eh = model.model.mesh.edge_handle(tempheh);
					// get the weight from the edge handle
					double weight = model.model.mesh.property(model.weight, eh);
					totalWeight += weight;

					// if the vertex found is boundary we put to B
					// boundary textCoor should be known
					if (model.boundaryModel.mesh.is_boundary(*vv_it)) {
						glm::vec2 texCoor = model.boundaryModel.mesh.property(model.texCoord, *vv_it);
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
			MyMesh::VertexHandle vh = model.boundaryModel.mesh.vertex_handle(vhID);
			model.boundaryModel.mesh.property(model.texCoord, vh).x = X[i];
			model.boundaryModel.mesh.property(model.texCoord, vh).y = Y[i];
		}
	}
	
	// draw bg rectangular
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(-0.5, -0.5, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBegin(GL_QUADS);
	glColor4f(0, 0, 0, 1.0);
	glVertex2f(0, 0);
	glVertex2f(0, 1);
	glVertex2f(1, 1);
	glVertex2f(1, 0);
	glEnd();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// draw line first
	// we will draw all line using face attribute from open mesh
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBegin(GL_TRIANGLES);
	glColor4f(1, 1, 1, 1.0);
	for (MyMesh::FIter f_it = model.boundaryModel.mesh.faces_begin(); f_it != model.boundaryModel.mesh.faces_end(); ++f_it) {
		// for each face, we draw its vertex
		for (MyMesh::FaceVertexIter fv_it = model.boundaryModel.mesh.fv_begin(*f_it); fv_it != model.boundaryModel.mesh.fv_end(*f_it); ++fv_it) {
			// get vh handler
			MyMesh::VertexHandle vhF = *fv_it;
			// get texture coordinate from property
			glm::vec2 textCoor = model.boundaryModel.mesh.property(model.texCoord, vhF);

			glVertex3f(textCoor[0], textCoor[1], 0);
		}
	}
	glEnd();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// we will draw all faces' points according to it 3 vertices and 3 color
	glPointSize(8);
	glBegin(GL_POINTS);
	for (MyMesh::FIter f_it = model.boundaryModel.mesh.faces_begin(); f_it != model.boundaryModel.mesh.faces_end(); ++f_it) {
		// for each face, we draw its vertex
		for (MyMesh::FaceVertexIter fv_it = model.boundaryModel.mesh.fv_begin(*f_it); fv_it != model.boundaryModel.mesh.fv_end(*f_it); ++fv_it) {
			// get vh handler
			MyMesh::VertexHandle vhF = *fv_it;
			// get color from property
			glm::vec3 color = model.boundaryModel.mesh.property(model.vColor, vhF);
			// get texture coordinate from property
			glm::vec2 textCoor = model.boundaryModel.mesh.property(model.texCoord, vhF);

			glColor3f(color[0], color[1], color[2]);
			glVertex3f(textCoor[0], textCoor[1], 0);
		}
	}
	glEnd();

	// draw the dot then
	// we get the starting heh first
	MyMesh::HalfedgeHandle heh_init = model.heh_init;
	MyMesh::HalfedgeHandle heh = heh_init;

	// we will only draw the dot if heh is valid
	if (heh.is_valid()) {
		glPointSize(8);
		glBegin(GL_POINTS);

		do {
			// get vh handler
			MyMesh::VertexHandle vhF = model.boundaryModel.mesh.from_vertex_handle(heh);
			// get color from property
			glm::vec3 color = model.boundaryModel.mesh.property(model.vColor, vhF);
			// get texture coordinate from property
			glm::vec2 textCoor = model.boundaryModel.mesh.property(model.texCoord, vhF);

			glColor3f(color[0], color[1], color[2]);
			glVertex3f(textCoor[0], textCoor[1], 0);

			heh = model.boundaryModel.mesh.next_halfedge_handle(heh);
		} while (heh != heh_init);

		glEnd();
	}

	glutSwapBuffers();
}

void RenderMain()
{
	glutSetWindow(mainWindow);
	glClear(GL_COLOR_BUFFER_BIT);
	glutSwapBuffers();
}

void RenderAll() {
	RenderMeshWindow();
	RenderTextureWindow();
}


//Timer event
void My_Timer(int val)
{
	glutPostRedisplay();
	glutTimerFunc(16, My_Timer, val);
}

void SelectionHandler(unsigned int x, unsigned int y)
{
	GLuint faceID = pickingTexture.ReadTexture(x, windowHeight - y - 1);
	if (faceID != 0)
	{
		currentFaceID = faceID;
	}

	if (selectionMode == ADD_FACE)
	{
		if (faceID != 0)
		{
			model.AddSelectedFace(faceID - 1);
		}
	}
	else if (selectionMode == DEL_FACE)
	{
		if (faceID != 0)
		{
			model.DeleteSelectedFace(faceID - 1);
		}
	}
	else if (selectionMode == SELECT_POINT)
	{
		currentMouseX = x;
		currentMouseY = y;
		updateFlag = true;
	}

	else if (selectionMode == ADD_TEXTURE) {
		// start to draw the texture to the screen
	}
}

//Mouse event
void MyMouse(int button, int state, int x, int y)
{
	if (!TwEventMouseButtonGLUT(button, state, x, y))
	{
		meshWindowCam.mouseEvents(button, state, x, y);

		if (button == GLUT_RIGHT_BUTTON)
		{
			if (state == GLUT_DOWN)
			{
				isRightButtonPress = true;
				SelectionHandler(x, y);
			}
			else if (state == GLUT_UP)
			{
				isRightButtonPress = false;
			}
		}
	}
}

//Keyboard event
void MyKeyboard(unsigned char key, int x, int y)
{
	if (!TwEventKeyboardGLUT(key, x, y))
	{
		meshWindowCam.keyEvents(key);
	}
}


void MyMouseMoving(int x, int y) {
	if (!TwEventMouseMotionGLUT(x, y))
	{
		meshWindowCam.mouseMoveEvent(x, y);

		if (isRightButtonPress)
		{
			SelectionHandler(x, y);
		}
	}
}

int main(int argc, char* argv[])
{
#ifdef __APPLE__
	//Change working directory to source code path
	chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_ALPHA);
#else
	glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif

	glutInitWindowPosition(100, 100);
	glutInitWindowSize(windowWidth, windowHeight);
	mainWindow = glutCreateWindow(ProjectName.c_str()); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif

	// callback for main window
	glutReshapeFunc(Reshape);
	glutSetOption(GLUT_RENDERING_CONTEXT, GLUT_USE_CURRENT_CONTEXT);
	glutDisplayFunc(RenderMain);
	glutIdleFunc(RenderAll);
	glutTimerFunc(16, My_Timer, 0);
	SetupGUI();

	glutMouseFunc(MyMouse);
	glutKeyboardFunc(MyKeyboard);
	glutMotionFunc(MyMouseMoving);
	InitOpenGL();
	InitData();

	// callback for sub window1
	subWindow1 = glutCreateSubWindow(mainWindow, 0, 0, windowWidth / 2, windowHeight);
	glutDisplayFunc(RenderMeshWindow);
	glutMouseFunc(MyMouse);
	glutKeyboardFunc(MyKeyboard);
	glutMotionFunc(MyMouseMoving);
	

	// callback for sub window2
	subWindow2 = glutCreateSubWindow(mainWindow, 0, 0, windowWidth / 2, windowHeight);
	glutDisplayFunc(RenderTextureWindow);

	//Print debug information 
	Common::DumpInfo();

	// Enter main event loop.
	glutMainLoop();

	return 0;
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

