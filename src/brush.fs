#version 100

precision mediump float;

// Inputs
uniform sampler2D Tex;
uniform sampler2D LMTex;
uniform bool UseLMTex;
uniform bool UseOverbright;
uniform float Alpha;

varying vec2 var_TexCoords;
varying vec2 var_LMTexCoords;

void main()
{
	vec4 color = texture2D(Tex, var_TexCoords);

	// Add light map if needed
	if (UseLMTex)
	{
		float lightness = texture2D(LMTex, var_LMTexCoords).r;

		// Stored at half bright to allow >100% overbrights
		lightness *= 2.0;

		// Clamp to only 100% when no overbrighting
		if (!UseOverbright)
			lightness = min(lightness, 1.0);

		color *= (1.0 - ((1.0 - lightness) * (1.0 - color.a)));
	}

	gl_FragColor = vec4(color.rgb, color.a * Alpha);
}
