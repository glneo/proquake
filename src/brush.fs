#version 100

precision mediump float;

// Inputs
uniform sampler2D Tex;
uniform vec4 Color;

varying vec2 var_TexCoords;

void main()
{
	gl_FragColor = texture2D(Tex, var_TexCoords) * Color;
}
