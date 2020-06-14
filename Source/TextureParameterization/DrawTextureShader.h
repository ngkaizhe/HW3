#pragma once

#include <ShaderObject.h>

class DrawTextureShader: public ShaderObject
{
public:
	DrawTextureShader();
	~DrawTextureShader();

	bool Init();
	void SetMVMat(const glm::mat4& mat);
	void SetPMat(const glm::mat4& mat);
	void Enable();
	void Disable();

	int currentTextureUsedID;

private:
	GLuint um4pLocation;
	GLuint um4mvLocation;

	unsigned int textures[9];
};

