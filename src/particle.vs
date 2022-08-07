#version 100

// Inputs
uniform mat4 Projection;
uniform mat4 ModelView;

attribute vec4 Vert;
attribute vec3 Color;

// Outputs
varying vec3 var_Color;

void main()
{
	var_Color = Color;
	gl_Position = Projection * ModelView * Vert;
	gl_PointSize = 1024.0 / gl_Position.z;
	gl_PointSize = clamp(gl_PointSize, 2.0, 64.0);
}
