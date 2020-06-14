#include "DrawTextureShader.h"
#include "ResourcePath.h"
#include<string>
#include<vector>
using namespace std;


DrawTextureShader::DrawTextureShader()
{
}


DrawTextureShader::~DrawTextureShader()
{
}

bool DrawTextureShader::Init()
{
	if (!ShaderObject::Init())
	{
		return false;
	}

	if (!AddShader(GL_VERTEX_SHADER, ResourcePath::shaderPath + "drawTexture.vs.glsl"))
	{
		return false;
	}

	if (!AddShader(GL_FRAGMENT_SHADER, ResourcePath::shaderPath + "drawTexture.fs.glsl"))
	{
		return false;
	}

	if (!Finalize())
	{
		return false;
	}

	um4mvLocation = GetUniformLocation("um4mv");
	if (um4mvLocation == -1)
	{
		puts("Get uniform loaction error: um4mv");
		return false;
	}

	um4pLocation = GetUniformLocation("um4p");
	if (um4pLocation == -1)
	{
		puts("Get uniform loaction error: um4p");
		return false;
	}

	// generate textures
	vector<string> texturePaths = {
		"./Imgs/brickwork.jpg",
		"./Imgs/checkerboard4.jpg",
		"./Imgs/negx.jpg",
		"./Imgs/negz.jpg",
		"./Imgs/ntust.jpg",
		"./Imgs/posx.jpg",
		"./Imgs/posy.jpg",
		"./Imgs/posz.jpg",
		"./Imgs/wood.jpg",
	};

	glGenTextures(9, textures);
	for (int i = 0; i < texturePaths.size(); i++) {
		TextureData textureData = Common::Load_png(texturePaths[i].c_str());
		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureData.width, textureData.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData.data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	

	return true;
}

void DrawTextureShader::SetMVMat(const glm::mat4& mat)
{
	glUniformMatrix4fv(um4mvLocation, 1, GL_FALSE, glm::value_ptr(mat));
}

void DrawTextureShader::SetPMat(const glm::mat4& mat)
{
	glUniformMatrix4fv(um4pLocation, 1, GL_FALSE, glm::value_ptr(mat));
}

void DrawTextureShader::Enable() {
	ShaderObject::Enable();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[currentTextureUsedID]);
}

void DrawTextureShader::Disable() {
	ShaderObject::Disable();
	glBindTexture(GL_TEXTURE_2D, 0);
}