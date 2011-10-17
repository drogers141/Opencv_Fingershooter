/*
 * open_hands.h
 *
 * Find open hands in image, as well as create Bullets shooting from
 * fingertips.  See open_hands.cpp for implementation.
 *
 *  Created on: Dec 31, 2009
 *      Author: drogers
 */

#ifndef OPEN_HANDS_H_
#define OPEN_HANDS_H_

#include "cv.h"
#include "highgui.h"

#include "bullet.h"

// colors
const CvScalar RED = CV_RGB(255, 0, 0);
const CvScalar GREEN = CV_RGB(0, 255, 0);
const CvScalar BLUE = CV_RGB(0, 0, 255);
const CvScalar WHITE = CV_RGB(255, 255, 255);
const CvScalar BLACK = CV_RGB(0, 0, 0);

/** client calls this func
 * param: mask - binary mask image for segmentation (eg a backprojected image)
 * param: bullets - output- new bullets to draw on image
 * param: perimScale - [4] contours with len < image-perimeter len / perimScale will
 * 			be ignored
 * param: debug - [false] if true, draw debug imagery to debug_image
 * param: debug_image - if debug is true, this should be a 3 channel image for debug output
 */
void find_hands_and_shoot(
		IplImage* mask,
		std::vector<Bullet*>& bullets,
		float perimScale = 4,
		bool debug = false,
		IplImage* debug_image = NULL);


bool is_open_hand(CvContour *c);

// prints the point using printf
void print_pt(CvPoint p);
// allocates and returns c string rep of point
char* pt_str(CvPoint p);
// distance between points
float pt_dist(CvPoint pt1, CvPoint pt2);

// client does not call this
// called within find_hands_and_shoot
// bullets, num_bullets -- output params
// debug_image -- [NULL] if not null, debug stuff will be drawn to it (3 channel)
void fire(std::vector<CvConvexityDefect*>& defects, CvPoint bbcenter,
		std::vector<Bullet*>& bullets, IplImage *debug_image=NULL);





















#endif /* OPEN_HANDS_H_ */
