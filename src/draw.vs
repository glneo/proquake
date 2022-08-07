#version 100

// Inputs
uniform mat4 Projection;

attribute vec4 Vertex;
attribute vec2 TexCoords;

// Outputs
varying vec2 var_TexCoords;

void main()
{
	var_TexCoords = TexCoords;
	gl_Position = Projection * Vertex;
}
