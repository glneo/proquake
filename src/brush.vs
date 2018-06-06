#version 100

// Inputs
uniform mat4 projection;
uniform mat4 modelView;

attribute vec2 TexCoords;
attribute vec4 Vert;

// Outputs
varying vec2 var_TexCoords;

void main()
{
	var_TexCoords = TexCoords;
	gl_Position = projection * modelView * Vert;
}
