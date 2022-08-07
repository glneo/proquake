#version 100

// Inputs
uniform mat4 Projection;
uniform mat4 ModelView;

uniform float Time;
uniform float SpeedScale;
uniform vec3 Vieworg;

attribute vec4 Vertex;
attribute vec2 TexCoords;
attribute vec2 LMTexCoords;

// Outputs
varying vec2 var_TexCoords;
varying vec2 var_LMTexCoords;

void main()
{
	float warpness = 8.0;
	float subdivide = 1.0;
	var_TexCoords = TexCoords;

	if (SpeedScale != 0.0)
	{
		vec3 dir = Vertex.xyz - Vieworg;
		dir.z *= 3.0;	// flatten the sphere

		float length = 6.0 * 63.0 / length(dir);

		var_TexCoords.s = dir.x;
		var_TexCoords.t = dir.y;

		var_TexCoords *= length;

		var_TexCoords += SpeedScale;

		var_TexCoords /= 2.0;

		// Sky warps less than water
		warpness = 2.0;
		
		// Sky is subdivided
		subdivide = 64.0;
	}

	if (Time != 0.0)
	{
		float os = var_TexCoords.s;
		float ot = var_TexCoords.t;

		var_TexCoords.s += (sin(ot * 0.125 + Time) * warpness);
		var_TexCoords.t += (sin(os * 0.125 + Time) * warpness);
		
		// Warps are subdivided
		subdivide = 64.0;
	}

	var_TexCoords /= subdivide;

	var_LMTexCoords = LMTexCoords;
	gl_Position = Projection * ModelView * Vertex;
}
