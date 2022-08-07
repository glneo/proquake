#version 100

// Inputs
uniform vec3 ShadeVector;
uniform vec4 LightColor;

uniform mat4 Projection;
uniform mat4 ModelView;

attribute vec4 Vertex;
attribute vec2 TexCoords;
attribute vec3 Normal;

// Outputs
varying vec4 var_Color;
varying vec2 var_TexCoords;

// this reproduces anorm_dots within as reasonable a degree of tolerance
float r_avertexnormal_dot(vec3 vertexnormal)
{
	float dot = dot(vertexnormal, ShadeVector);

	if (dot < 0.0)
		return 1.0 + dot * (13.0 / 44.0);
	else
		return 1.0 + dot;
}

void main()
{
	float dot1 = r_avertexnormal_dot(Normal);
	var_Color = LightColor * vec4(vec3(dot1), 1.0);
	var_TexCoords = TexCoords;
	gl_Position = Projection * ModelView * Vertex;
}
