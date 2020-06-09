#pragma once
#include <ShaderObject.h>

class DrawTextureShader : public ShaderObject
{
public:
	DrawTextureShader();
	~DrawTextureShader();

	bool Init();
	void SetMVMat(const GLfloat* value);
	void SetPMat(const GLfloat* value);

private:
	GLuint mvLocation;
	GLuint pLocation;
};

