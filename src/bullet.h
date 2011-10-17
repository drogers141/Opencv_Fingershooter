/*
 * Bullet.h
 *
 * Simple bullet, movement is not time-based, but set_velocity fakes it.
 * Implementation in bullet.cpp
 *
 *  Created on: Dec 31, 2009
 *      Author: drogers
 */

#ifndef BULLET_H_
#define BULLET_H_

#include "cv.h"
#include <cstdio>
#include <cmath>

class Bullet {
public:
	// bullet's position
	CvPoint pos;
	CvScalar color;
	CvPoint velocity;
	int radius;

	Bullet();
	Bullet(CvPoint position, CvPoint _velocity, CvScalar _color, int _radius);
	void update();
	void print();
	void set_velocity(CvPoint from, CvPoint to);

	static void test();
};

#endif /* BULLET_H_ */
