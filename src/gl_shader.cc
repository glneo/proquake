/*
 * GL shader loading
 *
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <vector>
#include <algorithm>

#include <GLES2/gl2.h>

#include "quakedef.h"
#include "glquake.h"

GLint GL_GetAttribLocation(GLuint program, const char *name)
{
	GLint location = glGetAttribLocation(program, name);
	if (location == -1)
		Sys_Error("glGetAttribLocation %s failed", name);

	return location;
}

GLint GL_GetUniformLocation(GLuint program, const char *name)
{
	GLint location = glGetUniformLocation(program, name);
	if (location == -1)
		Sys_Error("glGetUniformLocation %s failed", name);

	return location;
}

static bool GL_ShaderCheck(GLint object, GLenum status)
{
	GLint result;
	glGetShaderiv(object, status, &result);

	if (result != GL_TRUE)
	{
		int logLength;
		glGetShaderiv(object, GL_INFO_LOG_LENGTH, &logLength);
		std::vector<char> vertShaderError((logLength > 1) ? logLength : 1);
		glGetShaderInfoLog(object, logLength, nullptr, &vertShaderError[0]);
		Con_Printf("Error compiling shader: %s\n", &vertShaderError[0]);
	}

	return result;
}

static bool GL_ProgramCheck(GLint object, GLenum status)
{
	GLint result;
	glGetProgramiv(object, status, &result);

	if (result != GL_TRUE)
	{
		int logLength;
		glGetProgramiv(object, GL_INFO_LOG_LENGTH, &logLength);
		std::vector<char> vertShaderError((logLength > 1) ? logLength : 1);
		glGetProgramInfoLog(object, logLength, nullptr, &vertShaderError[0]);
		Con_Printf("Error linking shader: %s\n", &vertShaderError[0]);
	}

	return result;
}

GLuint LoadShader(const char *vertex_shader, int vertex_shader_len,
                  const char *fragment_shader, int fragment_shader_len)
{
	// Compile vertex shader
	Con_DPrintf("Compiling vertex shader\n");
	GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertShader, 1, &vertex_shader, &vertex_shader_len);
	glCompileShader(vertShader);
	GL_ShaderCheck(vertShader, GL_COMPILE_STATUS);

	// Compile fragment shader
	Con_DPrintf("Compiling fragment shader\n");
	GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragShader, 1, &fragment_shader, &fragment_shader_len);
	glCompileShader(fragShader);
	GL_ShaderCheck(fragShader, GL_COMPILE_STATUS);

	// Link program
	Con_DPrintf("Linking program\n");
	GLuint program = glCreateProgram();
	glAttachShader(program, vertShader);
	glDeleteShader(vertShader);
	glAttachShader(program, fragShader);
	glDeleteShader(fragShader);
	glLinkProgram(program);
	GL_ProgramCheck(program, GL_LINK_STATUS);

	return program;
}
