#version 100

precision mediump float;

// Inputs
uniform sampler2D Tex;
uniform bool UseLMTex;
uniform bool UseOverbright;
uniform float Alpha;

varying vec2 var_TexCoords;
varying vec2 var_LMTexCoords;

void main()
{
	// Add light map if needed
	if (UseLMTex)
	{
		float lightness = texture2D(Tex, var_LMTexCoords).r;

		// Clamp to only 50% when no overbrighting
		if (!UseOverbright)
			lightness = min(lightness, 0.5);

		gl_FragColor = vec4(vec3(lightness), 1);
	}
	else
	{
		vec4 color = texture2D(Tex, var_TexCoords);
		gl_FragColor = vec4(color.rgb, color.a * Alpha);
	}
}
