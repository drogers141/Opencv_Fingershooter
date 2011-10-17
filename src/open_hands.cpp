/*
 * open_hands.cpp
 *
 * Find open hands in image, as well as create Bullets shooting from fingertips.
 * Implementation of searching for contours and deciding which are open hands.  Other than
 * getting into background subtraction, the next things to do to improve the algorithm would involve
 * getting more rigorous about the grouping and number of convexity defects that make up the hand.
 * An image of the rough polygon approximation of the hand could also be used to get image moments
 * and these could be used for matching as well.
 *
 *  Created on: Dec 31, 2009
 *      Author: drogers
 */

#include "open_hands.h"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace std;

// For connected components:
// Approx.threshold - the bigger it is, the simpler is the boundary
//
#define CVCONTOUR_APPROX_LEVEL 4    // orig 2

// How many iterations of erosion and/or dilation there should be
//
#define CVCLOSE_ITR 1

/** client calls this func
 * param: mask - binary mask image for segmentation (eg a backprojected image)
 * param: bullets - output- new bullets to draw on image
 * param: perimScale - [4] contours with len < image-perimeter len / perimScale will
 * 			be ignored
 * param: debug - [false] if true, draw debug imagery to debug_image
 * param: debug_image - if debug is true, this should be a 3 channel image, the same size
 * 		as mask for debug output
 */
void find_hands_and_shoot(
		IplImage* mask,
		vector<Bullet*>& bullets,
		float perimScale,
		bool debug,
		IplImage* debug_image) {
	// for drawing
	CvScalar color;
//	IplImage *debug_image = 0;
//	if(debug) {
//		debug_image = cvCreateImage(cvGetSize(mask), 8, 3);
//	}
	if(debug) {
		cvZero(debug_image);
	}

	static CvMemStorage* mem_storage = NULL;
//	static CvMemStorage* mem_storage2 = NULL;
	static CvSeq* contours = NULL;

	//CLEAN UP RAW MASK
	// note I added the thresholding
	cvThreshold( mask, mask, 15, 255, CV_THRESH_BINARY );
	cvMorphologyEx( mask, mask, 0, 0, CV_MOP_OPEN, CVCLOSE_ITR );
	cvMorphologyEx( mask, mask, 0, 0, CV_MOP_CLOSE, CVCLOSE_ITR );

	//FIND CONTOURS AROUND ONLY BIGGER REGIONS
	//
	if( mem_storage==NULL ) {
		mem_storage = cvCreateMemStorage(0);
	} else {
		cvClearMemStorage(mem_storage);
	}
//	if( mem_storage2==NULL ) {
//		mem_storage2 = cvCreateMemStorage(0);
//	} else {
//		cvClearMemStorage(mem_storage2);
//	}
	CvContourScanner scanner = cvStartFindContours(
			mask,
			mem_storage,
			sizeof(CvContour),
			//*** orig
			CV_RETR_EXTERNAL,
//			CV_RETR_CCOMP,
			CV_CHAIN_APPROX_SIMPLE
	);

	CvSeq* c;
	while( (c = cvFindNextContour( scanner )) != NULL ) {
		double len = cvContourPerimeter( c );
		// calculate perimeter len threshold:
		//
		double q = (mask->height + mask->width)/perimScale;

		// Get rid of contour if its perimeter is too small:
		//
		if( len < q ) {
//			cvSubstituteContour( scanner, NULL );
		} else {
			// contour is big enough
			// poly and hull approximations
			CvSeq *poly, *hull;

			// set a random color
			color = CV_RGB( rand()&255, rand()&255, rand()&255 );
			int linesz = 2;

			// get bounding box for contour
			CvRect bb = cvBoundingRect(c);


			// width to measure defects, etc against
			int min_width_across = min(bb.width, bb.height);

			//*******debug
//			printf("min_width_across = %d\n", min_width_across);

			//******** cutting off contours by width here *********
			//
			int too_small = mask->width / 10;
			if(min_width_across < too_small) {
				continue;
			}

			// create mat for holding hull
			// -- necessary for getting convexity defects
			CvMat *hullmat = cvCreateMat(1, c->total, CV_32SC1);

			hull = cvConvexHull2(
					c,
					hullmat,
					CV_CLOCKWISE,
					1
			);

			//******************* debug drawing ***********
			if(debug) {
				// poly approximation for contour
				poly = cvApproxPoly(
						c,
						sizeof(CvContour),
						mem_storage,
						CV_POLY_APPROX_DP,
						CVCONTOUR_APPROX_LEVEL,
						0
				);


				// this way populates hull
				hull = cvConvexHull2(
						c,
						mem_storage,
						CV_CLOCKWISE,
						1
				);

				//			printf("made it: 3\n");
				// draw color stuff onto color copy
				cvDrawContours(
						debug_image,
						poly,
						color,
						BLACK,
						-1,
						linesz,
						8
				);
				cvDrawContours(
						debug_image,
						hull,
						color,
						BLUE,
						-1,
						linesz,
						8
				);
//				// Draw hull and poly into mask
//				//
//				cvDrawContours(
//						mask,
//						poly,
//						color,
//						color,
//						-1,
//						linesz,
//						8
//				);
//				cvDrawContours(
//						mask,
//						hull,
//						color,
//						color,
//						-1,
//						linesz,
//						8
//				);

				// draw bounding box and
				// it's center
				cvRectangle(debug_image, cvPoint(bb.x, bb.y),
						cvPoint(bb.x + bb.width,
								bb.y + bb.height),
								color, linesz);
				cvCircle(debug_image, cvPoint(bb.x + bb.width,
						bb.y + bb.height), 5,
						color, linesz, CV_FILLED );


			}
			//******************* end debug drawing ***********



			float depth_threshold = .25 * min_width_across;
//			int num_deep_enough = 0;
			vector<CvConvexityDefect*> deep_enough = vector<CvConvexityDefect*>();

			CvSeq *defects = 0;
			defects = cvConvexityDefects(c, hullmat, mem_storage);

			//*******debug
//			cout << "num defects: " << defects->total << endl;
			if(defects->total > 3) {
				for(int i=0; i<defects->total; i++) {
					CvConvexityDefect *d = (CvConvexityDefect *)cvGetSeqElem(defects, i);

					// defects big enough to be fingers
					if(d->depth > depth_threshold) {

						// ******** debug drawing *********
						if(debug) {
							int radius = 5;
							if(i==0) {
								color = GREEN;
								radius = 10;
							} else {
								color = CV_RGB( rand()&255, rand()&255, rand()&255 );
							}

							cvCircle(debug_image, *(d->depth_point), radius, color, CV_FILLED);
							cvLine(debug_image, *(d->start), *(d->depth_point), color, 1);
							cvLine(debug_image, *(d->end), *(d->depth_point), color, 1);
						}
						//********************************

						deep_enough.push_back(d);
					}
					// hopefully we have a hand
					if(deep_enough.size() > 3 && deep_enough.size() < 7) {
						//*******debug
//						cout<<"deep_enough.size()  = "<< deep_enough.size()  << endl;
//						printf("min_width_across = %d\n", min_width_across);

						CvPoint bbcenter = cvPoint(bb.x + bb.width/2,
								bb.y + bb.height/2);
						// fire bullets from fingertips
						fire(deep_enough, bbcenter, bullets);

						if(debug) {
							fire(deep_enough, bbcenter, bullets, debug_image);
						}
						//					printf("done firing\n");
					}


				}
			}


			// release our hull mat
			cvReleaseMat(&hullmat);

		}
	}
	//	cvReleaseMemStorage(&mem_storage);

}
// prints the point using printf
void print_pt(CvPoint p) {
	printf("(%d, %d)", p.x, p.y);
}

// allocates and returns c string rep of point
char* pt_str(CvPoint p) {
	char s[20];
	sprintf(s, "(%d, %d)", p.x, p.y);
	char *pstr = (char *)malloc(strlen(s) + 1);
	strcpy(pstr, s);
	return pstr;
}

// distance between points
float pt_dist(CvPoint pt1, CvPoint pt2) {
	return sqrt( pow((pt2.x - pt1.x), 2.0) + pow((pt2.y - pt1.y), 2.0) );

}

// client does not call this
// called within find_hands_and_shoot
// bullets -- output
void fire(std::vector<CvConvexityDefect*>& defects, CvPoint bbcenter,
		std::vector<Bullet*>& bullets,
		IplImage *debug_image) {
	CvScalar color = CV_RGB( rand()&255, rand()&255, rand()&255 );
	int linesz = 2;
	CvPoint exterior_points[10];
	int num_ext_pts = 0;

	//*******debug
//	printf("fire: num_defects: %d\n", defects.size());

	// consider that the start and end point of the convexity defect are hopefully the finger tips
	// which we want to count only one time

	// closer than this and it's the same finger tip
	float proximity_threshold = defects[0]->depth * .3;

	for(int i=0; i< defects.size(); i++) {
		int x = (defects[i]->start)->x;
		int y = (defects[i]->start)->y;
		CvPoint ftip = cvPoint(x, y);
		if(debug_image) {
			cvLine(debug_image, bbcenter, ftip, color, linesz );
		}
		exterior_points[i] = ftip;
		num_ext_pts++;
	}
	//*******debug
//	printf("num_ext_pts=%d\n", num_ext_pts);

	for(int i=0; i< defects.size(); i++) {
		int x = (defects[i]->end)->x;
		int y = (defects[i]->end)->y;
		CvPoint ftip = cvPoint(x, y);
		//		printf("ftip (end): %s\n", pt_str(ftip));

		bool found_close_pt = false;
		for(int j=0; j<num_ext_pts; j++) {
			if( pt_dist(ftip, exterior_points[j]) < proximity_threshold ) {
				found_close_pt = true;
			}
		}
		if(!found_close_pt) {
			// assume this is the last fingertip
			exterior_points[num_ext_pts++] = ftip;

//			printf("** found ftip (end): %s\n", pt_str(ftip));
			if(debug_image) {
				cvCircle(debug_image, ftip, 5, WHITE, CV_FILLED);
				cvLine(debug_image, bbcenter, ftip, color, linesz );
			}
		}
	}
	for(int i=0; i< num_ext_pts; i++) {
		Bullet *b = new Bullet(exterior_points[i], cvPoint(0,0), color, 5);
		b->set_velocity(bbcenter, b->pos);
		bullets.push_back(b);
	}
}

bool is_open_hand(CvContour *c, CvMemStorage *memstorage) {
	return true;
}
