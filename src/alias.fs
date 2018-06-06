#version 100

precision mediump float;

// Inputs
uniform sampler2D Tex;
uniform sampler2D FullbrightTex;
uniform bool UseFullbrightTex;
uniform bool UseOverbright;

varying vec4 var_Color;
varying vec2 var_TexCoords;

void main()
{
	vec4 result = texture2D(Tex, var_TexCoords);

	result *= var_Color;

	if (UseOverbright)
		result.rgb *= 2.0;

	if (UseFullbrightTex)
		result += texture2D(FullbrightTex, var_TexCoords);

	result = clamp(result, 0.0, 1.0);

	result.a = var_Color.a;

	gl_FragColor = result;
}
