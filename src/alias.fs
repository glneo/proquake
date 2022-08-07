#version 100

precision mediump float;

// Inputs
uniform sampler2D Tex;
uniform sampler2D FullbrightTex;
uniform bool UseFullbrightTex;

varying vec4 var_Color;
varying vec2 var_TexCoords;

void main()
{
	vec4 result = texture2D(Tex, var_TexCoords);

	result *= var_Color;

	if (UseFullbrightTex)
		result.rgb += texture2D(FullbrightTex, var_TexCoords).rgb;

	gl_FragColor = result;
}
