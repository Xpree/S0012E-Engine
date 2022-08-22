#version 430
layout(location=0) in vec3 in_Position;
layout(location=3) in vec2 in_TexCoord_0;

layout(location=3) out vec2 out_TexCoords;

uniform mat4 View;
uniform mat4 Projection;
uniform mat4 Model;

void main()
{
	out_TexCoords = in_TexCoord_0;
	gl_Position = Projection * View * Model * vec4(in_Position, 1.0f);
}
