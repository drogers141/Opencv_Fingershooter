/*
 * bullet.cpp
 *
 *  Created on: Jan 2, 2010
 *      Author: drogers
 */

#include "bullet.h"

#include "cv.h"
#include <cstdio>
#include <cmath>


	Bullet::Bullet()
	: pos(cvPoint(0,0)), velocity(cvPoint(0,0)),
	  color(CV_RGB(255, 0, 0)), radius(5)
	  {}
	Bullet::Bullet(CvPoint position, CvPoint _velocity, CvScalar _color, int _radius)
	: pos(position), velocity(_velocity),
	  color(_color), radius(_radius)
	  {}
	void Bullet::update(){
		pos.x += velocity.x;
		pos.y += velocity.y;
	}
	void Bullet::print() {
		printf("bullet:  pos: (%d, %d),\tvel: (%d, %d)\n",
				pos.x, pos.y, velocity.x, velocity.y);
	}
	void Bullet::set_velocity(CvPoint from, CvPoint to) {
		// normalize velocity so that the abs val of the x component and y component add up
		// to something we can deal with
		int abs_sum_xy = 25;
		velocity.x = to.x - from.x;
		velocity.y = to.y - from.y;
//		print();
		float mag = sqrt( pow(velocity.x, 2) + pow(velocity.y, 2) );
//		printf("mag = %g\n", mag);
		float x=0, y=0;
		if(velocity.x != 0) {
			x = velocity.x/mag;
		}
		if(velocity.y != 0) {
			y = velocity.y/mag;
		}
//		printf("x = %g, y = %g\n", x, y);

		velocity.x = int(x * abs_sum_xy);
		velocity.y = int(y * abs_sum_xy);
	}

	void Bullet::test() {
		CvPoint p1 = cvPoint(0, 0);
		CvPoint p2 = cvPoint(10, 20);
		CvPoint p3 = cvPoint(-10, -10);
		CvPoint p4 = cvPoint(10, 0);
		Bullet b1, b2, b3, b4;
		b1.print();
		b2.print();
		b1.set_velocity(p1, p2);
		printf("b1  "); b1.print();
		b2.set_velocity(p2, p1);
		printf("b2  "); b2.print();

		b3.set_velocity(p3, p1);
		printf("b3  "); b3.print();

		b4.set_velocity(p4, p1);
		printf("b4  "); b4.print();


	}
