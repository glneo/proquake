/*
 * Copyright (C) 2018 Andrew F. Davis
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

#include <cmath>
#include <cstddef>

#include "matrix.h"

#include "quakedef.h"

static const float identityMatrix[] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
};

Q_Matrix::Q_Matrix(void)
{
	this->identity();
}

void Q_Matrix::identity(void)
{
	for (int i = 0; i < 16; i++)
		_matrix[i] = identityMatrix[i];
}

void Q_Matrix::multiply(float* matrix)
{
	float result[16];

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			result[4 * i + j] = _matrix[ j    ] * matrix[4 * i    ] +
			                    _matrix[ 4 + j] * matrix[4 * i + 1] +
			                    _matrix[ 8 + j] * matrix[4 * i + 2] +
			                    _matrix[12 + j] * matrix[4 * i + 3];

	for (int i = 0; i < 16; i++)
		_matrix[i] = result[i];
}

void Q_Matrix::translate(float x, float y, float z)
{
	float translationMatrix[] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		   x,    y,    z, 1.0f,
	};

	multiply(translationMatrix);
}

void Q_Matrix::scale(float x, float y, float z)
{
	float scaleMatrix[] = {
		   x, 0.0f, 0.0f, 0.0f,
		0.0f,    y, 0.0f, 0.0f,
		0.0f, 0.0f,    z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	multiply(scaleMatrix);
}

float matrixDegreesToRadians(float degrees)
{
	return M_PI * degrees / 180.0f;
}

void Q_Matrix::rotateX(float angle)
{
	float angle_r = matrixDegreesToRadians(angle);

	float rotateMatrix[] = {
		1.0f,           0.0f,          0.0f, 0.0f,
		0.0f,  cosf(angle_r), sinf(angle_r), 0.0f,
		0.0f, -sinf(angle_r), cosf(angle_r), 0.0f,
		0.0f,           0.0f,          0.0f, 1.0f,
	};

	multiply(rotateMatrix);
}
void Q_Matrix::rotateY(float angle)
{
	float angle_r = matrixDegreesToRadians(angle);

	float rotateMatrix[] = {
		cosf(angle_r), 0.0f, -sinf(angle_r), 0.0f,
		         0.0f, 1.0f,           0.0f, 0.0f,
		sinf(angle_r), 0.0f,  cosf(angle_r), 0.0f,
		         0.0f, 0.0f,           0.0f, 1.0f,
	};

	multiply(rotateMatrix);
}
void Q_Matrix::rotateZ(float angle)
{
	float angle_r = matrixDegreesToRadians(angle);

	float rotateMatrix[] = {
		 cosf(angle_r), sinf(angle_r), 0.0f, 0.0f,
		-sinf(angle_r), cosf(angle_r), 0.0f, 0.0f,
		          0.0f,          0.0f, 1.0f, 0.0f,
		          0.0f,          0.0f, 0.0f, 1.0f,
	};

	multiply(rotateMatrix);
}

void Q_Matrix::frustum(float left, float right, float bottom, float top, float zNear, float zFar)
{
	float temp = 2.0 * zNear;
	float xDistance = right - left;
	float yDistance = top - bottom;
	float zDistance = zFar - zNear;

	float frustumMatrix[] = {
		        (temp) / (xDistance),                         0.0f,                          0.0f,  0.0f,
		                        0.0f,         (temp) / (yDistance),                          0.0f,  0.0f,
		(right + left) / (xDistance), (top + bottom) / (yDistance), -(zFar + zNear) / (zDistance), -1.0f,
		                        0.0f,                         0.0f,  -(temp * zFar) / (zDistance),  0.0f,
	};

	multiply(frustumMatrix);
}

void Q_Matrix::perspective(float fieldOfView, float aspectRatio, float zNear, float zFar)
{
	float ymax = zNear * tanf(fieldOfView * M_PI / 360.0);
	float ymin = -ymax;

	float xmin = ymin * aspectRatio;
	float xmax = ymax * aspectRatio;

	this->frustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void Q_Matrix::ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
	float xDistance = right - left;
	float yDistance = top - bottom;
	float zDistance = zFar - zNear;

	float orthoMatrix[] = {
		           2.0f / (xDistance),                          0.0f,                          0.0f, 0.0f,
		                         0.0f,            2.0f / (yDistance),                          0.0f, 0.0f,
		                         0.0f,                          0.0f,           -2.0f / (zDistance), 0.0f,
		-(right + left) / (xDistance), -(top + bottom) / (yDistance), -(zFar + zNear) / (zDistance), 1.0f,
	};

	multiply(orthoMatrix);
}

float *Q_Matrix::get(void)
{
	return _matrix;
}

void Q_Matrix::set(float *matrix)
{
	for (int i = 0; i < 16; i++)
		_matrix[i] = matrix[i];
}
