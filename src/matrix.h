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

#ifndef __MATRIX_H
#define __MATRIX_H

class Q_Matrix
{
public:
	Q_Matrix();

	void identity(void);
	void multiply(float* matrix);
	void translate(float x, float y, float z);
	void scale(float x, float y, float z);
	float degreesToRadians(float degrees);
	void rotateX(float angle);
	void rotateY(float angle);
	void rotateZ(float angle);
	void frustum(float left, float right, float bottom, float top, float zNear, float zFar);
	void perspective(float fieldOfView, float aspectRatio, float zNear, float zFar);
	void ortho(float left, float right, float bottom, float top, float zNear, float zFar);
	float *get(void);
	void set(float *matrix);

private:
	float _matrix[16];
};

#endif /* __MATRIX_H */
