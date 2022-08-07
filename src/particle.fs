#version 100

precision mediump float;

// Inputs
uniform float Alpha;

varying vec3 var_Color;

void main()
{
	float radius = 0.5;
	vec2 center = vec2(0.5, 0.5);
	float d = distance(gl_PointCoord, center);
	float t = 1.0 - smoothstep(radius - 0.1, radius, d);
	if (t < 0.666)
		discard;

    gl_FragColor = vec4(var_Color, Alpha * t);
}
