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
#include<Eigen/Sparse>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "STB/stb_image_write.h"

using namespace glm;
using namespace std;
using namespace Eigen;

glm::vec3 worldPos;

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

// load all models in init
vector<MeshObject> models;
MeshObject* modelPtr;

// shaders
DrawModelShader drawModelShader;
DrawPickingFaceShader drawPickingFaceShader;
PickingShader pickingShader;
PickingTexture pickingTexture;

// load all texture inside
DrawTextureShader drawTextureShader;

int mainWindow, subWindow1, subWindow2;
enum SelectionMode
{
	ADD_FACE,
	DEL_FACE,
	ADD_TEXTURE,
};
SelectionMode selectionMode = ADD_FACE;

enum ModelSelected {
	UNIONSPHERE,
	ARMADILLO,
	BEAR,
	DANCER,
	DANCING_CHILDREN,
	FELINE,
	FERTILTY,
	GARGOYLE,
	NEPTUNE,
	OCTOPUS,
	SCREWDRIVER,
	XYZRGB_DRAGON_100K,
};
ModelSelected modelSelected = BEAR;
ModelSelected currentModelSelected = modelSelected;

TwBar* bar;
TwEnumVal SelectionModeEV[] = { {ADD_FACE, "Add face"}, {DEL_FACE, "Delete face"}, {ADD_TEXTURE, "Add texture"} };
TwType SelectionModeType;

TwEnumVal ModelSelectedEV[] = { 
	{UNIONSPHERE, "Union Sphere"},
	{ARMADILLO, "Armadillo"},
	{BEAR, "Bear"},
	{DANCER, "Dancer"},
	{DANCING_CHILDREN, "Dancing children"},
	{FELINE, "Feline"},
	{FERTILTY, "Fertilty"},
	{GARGOYLE, "Gargoyle"},
	{NEPTUNE, "Neptune"},
	{OCTOPUS, "Octopus"},
	{SCREWDRIVER, "Screw Driver"},
	{XYZRGB_DRAGON_100K, "XYZ dragon"},
};
TwType ModelSelectedType;


// the offset moved
float TextureXOffset = 0.5;
float TextureYOffset = 0.5;

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

	// model part
	ModelSelectedType = TwDefineEnum("Model Selected Object", ModelSelectedEV, 12);
	// Adding season to bar
	TwAddVarRW(bar, "Model Selected", ModelSelectedType, &modelSelected, NULL);
}

void My_LoadModel()
{
	vector<string> modelPaths = {
		"./Model/UnionSphere.obj",
		"./Model/armadillo.obj",
		"./Model/bear.obj",
		"./Model/dancer.obj",
		"./Model/dancing_children.obj",
		"./Model/feline.obj",
		"./Model/fertilty.obj",
		"./Model/gargoyle.obj",
		"./Model/neptune.obj",
		"./Model/octopus.obj",
		"./Model/screwdriver.obj",
		"./Model/xyzrgb_dragon_100k.obj",
	};

	models.resize(modelPaths.size());
	for (int i = 0; i < modelPaths.size(); i++) {
		models[i].Init(modelPaths[i]);
	}

	modelPtr = &models[currentModelSelected];
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
	ResourcePath::imagePath = "./Imgs/";
	ResourcePath::modelPath = "./Model/UnionSphere.obj";

	//Initialize shaders
	///////////////////////////	
	drawModelShader.Init();
	pickingShader.Init();
	pickingTexture.Init(windowWidth, windowHeight);
	drawPickingFaceShader.Init();
	drawTextureShader.Init();

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
	// recheck whether do we need to change the pointer
	if (currentModelSelected != modelSelected) {
		modelPtr = &models[modelSelected];
		currentModelSelected = modelSelected;
	}

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

	modelPtr->Render();

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

	modelPtr->Render();

	drawModelShader.Disable();
	
	// render selected face
	if (selectionMode == SelectionMode::ADD_FACE || 
		selectionMode == SelectionMode::DEL_FACE ||
		selectionMode == SelectionMode::ADD_TEXTURE)
	{
		drawPickingFaceShader.Enable();
		drawPickingFaceShader.SetMVMat(value_ptr(mvMat));
		drawPickingFaceShader.SetPMat(value_ptr(pMat));
		modelPtr->RenderSelectedFace();
		drawPickingFaceShader.Disable();

		// calculate the total boundary points from the selected face
		if (selectionMode == SelectionMode::ADD_TEXTURE) {
			modelPtr->CalculateBoundaryPoints();

			// draw the texture on the boundary model face
			drawTextureShader.Enable();
			drawTextureShader.SetMVMat(mvMat);
			drawTextureShader.SetPMat(pMat);
			glUniform2f(drawTextureShader.GetUniformLocation("offset"), TextureXOffset, TextureYOffset);
			modelPtr->RenderTextureFace();
			drawTextureShader.Disable();
		}
	}

	glUseProgram(0);

	TwDraw();
	glutSwapBuffers();
}

void RenderTextureWindow() {
	glutSetWindow(subWindow2);

	glClearColor(.7f, .7f, .7f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
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
	for (MyMesh::FIter f_it = modelPtr->boundaryModel.mesh.faces_begin(); f_it != modelPtr->boundaryModel.mesh.faces_end(); ++f_it) {
		// for each face, we draw its vertex
		for (MyMesh::FaceVertexIter fv_it = modelPtr->boundaryModel.mesh.fv_begin(*f_it); fv_it != modelPtr->boundaryModel.mesh.fv_end(*f_it); ++fv_it) {
			// get vh handler
			MyMesh::VertexHandle vhF = *fv_it;
			// get texture coordinate from property
			glm::vec2 textCoor = modelPtr->boundaryModel.mesh.property(modelPtr->texCoord, vhF);

			glVertex3f(textCoor[0], textCoor[1], 0);
		}
	}
	glEnd();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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
			modelPtr->AddSelectedFace(faceID - 1);
		}
	}
	else if (selectionMode == DEL_FACE)
	{
		if (faceID != 0)
		{
			modelPtr->DeleteSelectedFace(faceID - 1);
		}
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

	if (key == 'i') {
		TextureYOffset += 0.1f;
	}
	else if (key == 'k') {
		TextureYOffset -= 0.1f;
	}
	else if (key == 'j') {
		TextureXOffset += 0.1f;
	}
	else if (key == 'l') {
		TextureXOffset -= 0.1f;
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

