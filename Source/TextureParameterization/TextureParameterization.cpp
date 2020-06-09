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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../Include/STB/stb_image_write.h"

using namespace glm;
using namespace std;

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

	//if (model.boundaryPoints.size() != 0) {
	//	// get the dot in right part render space
	//	std::vector<glm::vec2> dotPosition;
	//	std::vector<float> dotRangeLeft;
	//	float interval = 1.0f;
	//	float totalLengthOfLeft = 0;

	//	// get the total length of left part first 
	//	for (int i = 1; i <= model.boundaryPoints.size(); i++) {
	//		dotRangeLeft.push_back(glm::distance(model.boundaryPoints[i - 1], 
	//			model.boundaryPoints[i % model.boundaryPoints.size()]));
	//		totalLengthOfLeft += glm::distance(model.boundaryPoints[i - 1], 
	//			model.boundaryPoints[i % model.boundaryPoints.size()]);
	//	}

	//	// init position is zero
	//	dotPosition.push_back(glm::vec2(-interval / 2, -interval / 2));
	//	float currentRangeLeft = 0;
	//	for (int i = 0; i < dotRangeLeft.size() - 1; i++) {
	//		currentRangeLeft += dotRangeLeft[i] / totalLengthOfLeft * (interval * 4);

	//		// PLUSY
	//		if (currentRangeLeft >= 0 && currentRangeLeft < interval) {
	//			dotPosition.push_back(glm::vec2(-interval/2, -interval/2 + currentRangeLeft));
	//		}

	//		// PLUSX
	//		else if (currentRangeLeft >= interval && currentRangeLeft < interval * 2) {
	//			float intervalReduced = currentRangeLeft - interval;
	//			dotPosition.push_back(glm::vec2(-interval / 2 + intervalReduced, interval / 2));
	//		}

	//		// MINUSY
	//		else if (currentRangeLeft >= interval * 2 && currentRangeLeft < interval * 3) {
	//			float intervalReduced = currentRangeLeft - interval * 2;
	//			dotPosition.push_back(glm::vec2(interval / 2, interval / 2 - intervalReduced));
	//		}

	//		// MINUSX
	//		else if (currentRangeLeft >= interval * 3 && currentRangeLeft < interval * 4) {
	//			float intervalReduced = currentRangeLeft - interval * 3;
	//			dotPosition.push_back(glm::vec2(interval / 2 - intervalReduced, -interval / 2));
	//		}
	//		
	//		else {
	//			dotPosition.push_back(glm::vec2(-interval / 2, -interval / 2));
	//		}
	//	}

	//	// clear matrix
	//	// draw the points that is boundary
	//	glMatrixMode(GL_PROJECTION);
	//	glLoadIdentity();
	//	glMatrixMode(GL_MODELVIEW);
	//	glLoadIdentity();

	//	// draw bg rectangular
	//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//	glBegin(GL_QUADS);
	//	glColor4f(0, 0, 0, 1.0);
	//	glVertex2f(-interval / 2, -interval / 2);
	//	glVertex2f(-interval / 2, interval / 2);
	//	glVertex2f(interval / 2, interval / 2);
	//	glVertex2f(interval / 2, -interval / 2);
	//	glEnd();
	//	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


	//	// draw line first
	//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//	glBegin(GL_LINES);
	//	glColor4f(1, 1, 1, 1);
	//	// center
	//	for (int i = 0; i < dotPosition.size(); i++) {
	//		glVertex2f(0, 0);
	//		glVertex2f(dotPosition[i][0], dotPosition[i][1]);
	//	}

	//	glEnd();
	//	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//	// draw the dot then
	//	std::vector<glm::vec4> colors({
	//		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
	//		glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
	//		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
	//		glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
	//	});
	//	glPointSize(8);
	//	glBegin(GL_POINTS);
	//	glColor3f(1, 0, 1);
	//	glVertex2f(0, 0);
	//	for (int i = 0; i < dotPosition.size(); i++) {
	//		// set color
	//		glm::vec4 color = colors[i % 4];
	//		glColor4f(color[0], color[1], color[2], color[3]);
	//		// draw vertex
	//		glVertex2f(dotPosition[i][0], dotPosition[i][1]);
	//	}
	//	glEnd();
	//}

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

