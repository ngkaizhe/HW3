#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 um4mv;
uniform mat4 um4p;

// offset
uniform vec2 offset;

void main()
{
	gl_Position = um4p * um4mv * vec4(aPos, 1.0);
	TexCoord = aTexCoord + offset;
}