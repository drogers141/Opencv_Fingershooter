/**
 * Dave Rogers
 * Dec 2009 	CS6825 Computer Vision
 *
 * Fingershooter
 *
 * This program looks for open hands from a camera feed and shoots multi-colored
 * "bullets" from the finger tips.  In order to be somewhat robust in changing light conditions, there
 * is a calibration routine that presents a red square in the center of the screen.  The user is prompted
 * to position the palm of the hand (any flesh surface will do), in such a way that the entire selection
 * rectangle is filled with an appropriate flesh color, then to hit a key to continue.  Alternatively, the
 * user can hit 't' to start a timer that counts down 5 seconds, so the user can move away from the
 * keyboard.  At this time a 1D hue histogram is captured from the selection region.  (There is code for a
 * 2D hue/saturation histogram, but I didn't have good results from it.)  An image of the histogram and
 * the frame it was taken from are displayed until the user hits another key.
 *
 * After calibration, the image is presented in a window, as well as the backprojection of the histogram
 * against the image.  The user's hand(s) should be spread apart to cause firing from the fingertips.
 * Beware of background confusion as well as bad lighting--particularly glare--when taking the histogram.
 * If an open hand is not shooting--try moving the window to expose the backprojection and make sure that
 * the hand is showing white, and move it around so it's contour is not confused with a background contour.
 * You can really watch what is happening with the debug mode turned on--another window opens with colored
 * lines and curves showing information on what contours and convexity defects the program is picking up.
 *
 * Keyboard actions are displayed on the console during the program.  The following are available during
 * the non-calibration part of the program.
 *
 * ****** KEYS *********

 *	The following keys are active now:
 *	esc		quit
 *	d       toggle debug mode -- shows a window with pretty debug imagery (ie contours, hulls,
 *								convexity defects, etc)
 *	s       toggle save mode -- writes video to avi file ("fingershooter.avi"), stops when you hit it
 *								a second time, or you can let it run until you quit
 *						-- if you are already in debug mode, a debug video file will be saved as well
 *							("fingershooter_debug.avi")
 *	f       save frames of image, backproject, hue, hsv, and debug if on, as jpegs in ./temp

 *
 *	Video Writing Issues:
 *	Note that if you want to save the video, you may have to tweak the camera parameters, especially the
 *	codec.
 *	Look in main() for "writer = cvCreateVideoWriter(" to see other possible codecs.  The mjpg works on
 *	my desktop machine with the unibrain fire-i and on my laptop--resulting in a compressed file size.
 */

#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <iostream>

#include "bullet.h"
#include "open_hands.h"

//******* unix/linux only for sleeping
#include "time.h"

using namespace std;

// Returns a histogram with hue and saturation values obtained by sampling img in rect selection
// remember to cvReleaseHist(&hist) when done
// param: show [false]  -- if true show red rectangle in image of selection and pic of hist
CvHistogram* createHueSatHist(IplImage* img, CvRect selection, bool show=false);

// Returns a histogram with hue values obtained by sampling img in rect selection
// remember to cvReleaseHist(&hist) when done
// param: show [false]  -- if true show red rectangle in image of selection and pic of hist
CvHistogram* createHueHist(IplImage* img, CvRect selection, bool show=false);

CvScalar hsv2rgb( float hue );

// test hue histogram creation
void test_huehist();

// draw red rect around selection on image
void draw_selection(IplImage *img, CvRect selection);

// returns a hue (or hue/sat if modified) histogram
//based on selection drawn on image taken from capture
CvHistogram* calibrate();

// initializes video writer to write frames to output file filename
// at the given frame rate and size
// tries to use mjpg codec first, then CV_FOURCC_DEFAULT, if neither work,
// writes message and exits--so writer will be NULL
void init_vidwriter(CvVideoWriter **writer, const char *filename,
		int framerate, CvSize frame_size);

// all bullets drawn on screen
vector<Bullet*> g_bullets = vector<Bullet*>();

void fire_bullet(Bullet *b) {
	g_bullets.push_back(b);
}

void update_bullets(IplImage *image) {
	if(g_bullets.empty()) {
		return;
	}
	vector<Bullet*>::iterator it = g_bullets.end();
	while(it != g_bullets.begin()) {
		--it;
		Bullet *b = *it;
		b->update();

//		b->print();

		int x = b->pos.x;
		int y = b->pos.y;
		int w = image->width;
		int h = image->height;
		// cleanup old bullets
		if(x <= 0 || x >= w  ||
				y <= 0 || y >= h ) {
			g_bullets.erase(it);
			delete b;
		}
	}
//	cout<<"g_bullets.size(): " << g_bullets.size() << endl;
}

void draw_bullets(IplImage *image) {
	for(int i=0; i< g_bullets.size(); i++) {
		cvCircle(image, g_bullets[i]->pos,
				g_bullets[i]->radius,
				g_bullets[i]->color, CV_FILLED);
	}
}

int main(int argc, char* argv[])
{
//	Bullet::test();
//	test();
//	return 0;

	IplImage *image = 0, *debug_image = 0,
			*hsv = 0, *hue = 0, *sat = 0, *v = 0,
			*backproject = 0, *backproject_copy = 0;
	const int NUM_TEMPS = 3;
	IplImage *temp_images[NUM_TEMPS];

	CvHistogram *hist = 0;


	// bullets coming from hands found
	vector<Bullet*> new_bullets = vector<Bullet*>();


	// **** if initializing hist to flesh colors based on stored image
//	CvRect sel;
//
////	image = cvLoadImage("./images/dl4.jpg");
////	sel = cvRect(230, 220, 150, 170);
//
//	image = cvLoadImage("./images/lml1.jpg");
//	sel = cvRect(150, 320, 100, 120);
//
////	image = cvLoadImage("./images/lncl4.jpg");
////	sel = cvRect(150, 320, 100, 120);
//
//	hist = createHueHist(image, sel);

	//******************************************************************

	// set to true if working with image rather than vid capture
	bool image_only = false;

	// set to true to show debug image from find_hands_and_shoot
	bool debug_mode = false;

	// write output avi to file
	// set to true if writing video
	bool save_mode = false;
	CvVideoWriter* writer = 0;
	// ouput filename
	char avi_file[256] = "/share/eclipse/workspaces/ws_current/OpencvCs6825FingerShooter/fingershooter.avi";
	// write debug output to debug avi file
	CvVideoWriter* debug_writer = 0;
	char debug_avi_file[256] = "/share/eclipse/workspaces/ws_current/OpencvCs6825FingerShooter/fingershooter_debug.avi";

	// for testing with image only, then need selection rect as well
	// to get histogram with
	// assume argv[1] is image filename, argv[2-5] are x,y,width,height of selection in image
	if(argc == 6) {
		image_only = true;
		printf("image_only\n");
	}
	// set histogram here if not image only
	if(!image_only) {
		// prompt user to calibrate histogram of flesh color
		hist = calibrate();
	}

	// let user know about the latest features ..
	printf("\n****** KEYS *********\n\n");
	printf("The following keys are active now:\n");
	printf("esc    quit\n\n");
	printf("d      toggle debug mode -- shows a window with pretty debug imagery (ie contours, hulls, \n");
	printf("							convexity defects, etc)\n");
	printf("s      toggle save mode -- writes video to avi file (\"fingershooter.avi\"), stops when you hit it\n");
	printf("							a second time, or you can let it run until you quit\n");
	printf("					-- if you are already in debug mode, a debug video file will be saved as well\n");
	printf("						(\"fingershooter_debug.avi\")\n");
	printf("f      save frames of image, backproject, hue, hsv, and debug if on, as jpegs in ./temp\n\n");

	CvCapture* capture = NULL;
	if(image_only) {
		capture = cvCreateFileCapture( argv[1] );
	} else {
		capture = cvCaptureFromCAM(CV_CAP_ANY);
	}
	if(capture == NULL) {
		printf("No capture\n");
		return 1;
	}

	cvNamedWindow("Backproject", CV_WINDOW_AUTOSIZE );
	cvNamedWindow("Image", CV_WINDOW_AUTOSIZE );
//	cvNamedWindow("DebugImage", CV_WINDOW_AUTOSIZE );
//	cvNamedWindow("Hsv", CV_WINDOW_AUTOSIZE );
//	cvNamedWindow("Hue", CV_WINDOW_AUTOSIZE );


	image = cvQueryFrame( capture );
	if( !image ) {
		printf("No image\n");
		return 1;
	}
	// set output writing size stuff
	int framerate = (int)cvGetCaptureProperty(capture, CV_CAP_PROP_FPS);
	int f_width = (int)cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
	int f_height = (int)cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
	framerate = framerate > 0 ? framerate: 15;
	f_width = f_width > 0 ? f_width: 640;
	f_height = f_height > 0 ? f_height: 480;
	printf("cam capture: "
			"framerate=%d, f_width=%d, f_height=%d\n", framerate, f_width, f_height);

	// set histogram if image only
	if(image_only) {
		CvRect selection = cvRect(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
		hist = createHueHist(image, selection, true);
	}

	debug_image = cvCreateImage( cvGetSize(image), 8, 3 );
	hsv = cvCreateImage( cvGetSize(image), 8, 3 );
	hue = cvCreateImage( cvGetSize(image), 8, 1 );
	sat = cvCreateImage( cvGetSize(image), 8, 1 );
	v = cvCreateImage( cvGetSize(image), 8, 1 );

	backproject = cvCreateImage( cvGetSize(image), 8, 1);
	backproject_copy = cvCreateImage( cvGetSize(image), 8, 1);


	for(int i=0; i<NUM_TEMPS; i++) {
			temp_images[i] = cvCreateImage( cvGetSize(image), 8, 1);
	}

	try {
	while(1) {

		if(!image_only) {
			image = cvQueryFrame( capture );
		}
		if( !image ) {
			printf("No image\n");
			break;
		}

		// set up hsv, hue, and sat images
		cvCvtColor( image, hsv, CV_BGR2HSV );
		cvSplit( hsv, hue, sat, v, 0 );

		// if only using 1d hist with hue
		cvCalcBackProject( &hue, backproject, hist );

		// test
//		Bullet b = Bullet(cvPoint(100, 100), cvPoint(25, 25), CV_RGB(255, 0, 0), 5);
//		fire_bullet(b);

		cvCopy(backproject, backproject_copy);
		cvShowImage("Backproject", backproject_copy);

		// find hands and get new bullets from them if found
		if(debug_mode) {
			// show the debug image
			find_hands_and_shoot(backproject, new_bullets, 6, true, debug_image);
		} else {
			// normal
			find_hands_and_shoot(backproject, new_bullets, 6);
		}


//		printf("new bullets: %d\n", new_bullets.size());
		for(int i=0; i<new_bullets.size(); i++) {
			fire_bullet(new_bullets[i]);
		}
		new_bullets.clear();
//		cout << "past fire bullets" << endl;
		update_bullets(image);
		draw_bullets(image);
//		cout << "past drawing bullets" << endl;

		// if using 2d hist with hue and saturation
//		IplImage* planes[] = { hue, sat };
//		cvCalcBackProject( planes, backproject, hist );

		cvShowImage("Image", image);
		if (debug_mode) {
			cvShowImage("DebugImage", debug_image);
		}
		if(save_mode) {
			if(writer) {
				cvWriteFrame(writer, image);
			}
			if(debug_writer) {
				cvWriteFrame(debug_writer, debug_image);
			}
		}
//		cvShowImage("Hsv", hsv);

//		cvShowImage("Hue", hue);
//		cvShowImage("HsvMask", hsv_mask);

		char c;
		if(image_only) {
			c = cvWaitKey(0);
			break;
		}
		c = cvWaitKey(10);
		if(c == 27){
			break;
		} else if(c == 'f') {
			// save frames
			cvSaveImage("./temp/image.jpg", image);
			cvSaveImage("./temp/hsv.jpg", hsv);
			cvSaveImage("./temp/backproject.jpg", backproject_copy);
			cvSaveImage("./temp/hue.jpg", hue);
			if(debug_mode) {
				cvSaveImage("./temp/debug.jpg", debug_image);
			}
		} else if(c == 'd') {
			// toggle debug mode
			// ie show the debug image frames
			debug_mode = !debug_mode;
			if(debug_mode) {
				cvNamedWindow("DebugImage", CV_WINDOW_AUTOSIZE );
			} else {
				cvDestroyWindow("DebugImage");
			}
		} else if(c == 's') {
			// toggle video save mode
			// save video to output avi
			save_mode = !save_mode;
			if(save_mode) {
				printf("Saving video to %s\n", avi_file);
				init_vidwriter(&writer, avi_file, framerate, cvSize(f_width, f_height));

			} else {
				if(writer) {
					cvReleaseVideoWriter(&writer);
					printf("Done writing %s\n", avi_file);
				}
				if(debug_writer) {
					cvReleaseVideoWriter(&debug_writer);
					printf("Done writing %s\n", debug_avi_file);
				}
			}
			// if in debug_mode save debug to debug output avi
			if(save_mode && debug_mode) {
				printf("Saving debug video to %s\n", debug_avi_file);
				init_vidwriter(&debug_writer, debug_avi_file, framerate, cvSize(f_width, f_height));
			}
		}

	}
	} catch (exception e) {
		cvReleaseCapture(&capture);
		cerr << "std::exception caught:" << endl;
		cerr << e.what() << endl;
	} catch (...) {
		cvReleaseCapture(&capture);
		cerr << "unknown exception caught" << endl;
	}

	cvReleaseHist(&hist);

//	cvReleaseImage( &image);
	cvReleaseImage( &debug_image);
	cvReleaseImage( &hsv);
	cvReleaseImage( &hue);
	cvReleaseImage( &sat);
	cvReleaseImage( &v);
	cvReleaseImage( &backproject);
	cvReleaseImage( &backproject_copy);

	for(int i=0; i<NUM_TEMPS; i++) {
		cvReleaseImage(&temp_images[i]);
	}
	cvReleaseCapture(&capture);
	if(writer != NULL) {
		cvReleaseVideoWriter(&writer);
	}
	if(debug_writer != NULL) {
		cvReleaseVideoWriter(&debug_writer);
	}

	cvDestroyAllWindows();
}

// Prompts user to create a flesh color histogram by positioning hand and hitting a key
// Displays selection region in image and image of histogram (see createHueHist)
// returns a hue histogram (or hue/sat if modified)
CvHistogram* calibrate() {
	CvCapture *capture = 0;
	capture = cvCaptureFromCAM(CV_CAP_ANY);
	if( capture == NULL ) {
		printf("calibrate: No capture\n");
		exit(1);
	}
	IplImage *img = 0;
	img = cvQueryFrame( capture );
	if( !img ) {
		printf("calibrate: No image\n");
		exit(1);
	}
	int width = 80, height = 80;
	int cx = img->width / 2;
	int cy = img->height / 2;
	CvRect selection = cvRect(cx-width/2, cy-height/2,
							width, height);

	// *** timer stuff
	// delay in seconds -- use 1 more than you think, as the first sec is < 1 sec
	int delay = 6;
	int remaining_secs = delay;
	bool timer_mode = false;
	time_t start_time, curr_time;
	CvFont* font =  new CvFont();
	cvInitFont( font, CV_FONT_HERSHEY_SIMPLEX, 1, 1, 0, 3, 8);
	char time_str[5];
	CvPoint time_pt = cvPoint(100, 100);


	cvNamedWindow("calibrate", CV_WINDOW_AUTOSIZE);
	printf("****** FINGERSHOOTER *********\n\n");
	printf("Calibrating flesh color: \nposition palm of your hand such that rectangle contains\n");
	printf("only flesh color, then hit any key other than 't'\n");
	printf("Or, to allow moving away from the keyboard, hit 't'\n");
	printf("A timer will count down to zero, at zero a histogram will be taken \nfrom the selection rectangle.\n");
	char c;
	while(1) {
		img = cvQueryFrame( capture );
		if( !img ) {
			printf("calibrate: No image\n");
			break;
		}
		draw_selection(img, selection);

		if(timer_mode) {
			curr_time = time(NULL);
			remaining_secs = delay - (curr_time - start_time);
			printf("%d\n", remaining_secs);
			if(remaining_secs <= 0) {
				break;
			} else {
				CvRect r = cvRect(time_pt.x-25, time_pt.y-50, 70, 70);
				cvSetImageROI(img, r);
				cvZero(img);
				cvResetImageROI(img);
				sprintf(time_str, "%d", remaining_secs);
				cvPutText(img, time_str, time_pt, font, CV_RGB(255, 255, 255));
			}
		}
		cvShowImage("calibrate", img);
		c = cvWaitKey(30);
		if(c == 't' && (!timer_mode)) {
			timer_mode = true;
			start_time = time(NULL);
			curr_time = time(NULL);
//			sprintf(time_str, "&d", remaining_secs);
//			cvPutText(img, time_str, time_pt, font, CV_RGB(127, 127, 0));
		} else if(c != -1) {
			break;
		}
	}
	CvHistogram *hist = createHueHist(img, selection, true);
//	CvHistogram *hist = createHueSatHist(img, selection, true);


	cvDestroyWindow("calibrate");
	cvReleaseCapture(&capture);

	return hist;
}

// initializes video writer to write frames to output file filename
// at the given frame rate and size
// tries to use mjpg codec first, then CV_FOURCC_DEFAULT, if neither work,
// writes message and exits--so writer will be NULL
void init_vidwriter(CvVideoWriter **writer, const char *filename,
		int framerate, CvSize frame_size) {
	// open video writer--first try to get camera's settings and decent codec
	try {
		*writer = cvCreateVideoWriter(filename,
				// try another codec if as needed for your platform
				//    		CV_FOURCC('P', 'I', 'M', '1'),
				//   	    		CV_FOURCC_DEFAULT,
				CV_FOURCC('M', 'J', 'P', 'G'),
				framerate,
				frame_size);
		if(!writer) {
			cerr << "writer not initialized." << endl;
			throw "Null Writer";
		}
		cerr << "fingershooter.main(): " << endl
				<< "Initializing video writer, able to use mjpg codec and cam settings." << endl;
	} catch (...) {
		// if that doesn't work
		// try default codec
		try {
			*writer = cvCreateVideoWriter(filename,
					// try another codec if as needed for your platform
					//    		CV_FOURCC('P', 'I', 'M', '1'),
					CV_FOURCC_DEFAULT,
					//    		CV_FOURCC('M', 'J', 'P', 'G'),
					framerate,
					frame_size);;
			cerr << "fingershooter.main(): " << endl
					<< "Initializing video writer, unable to use compressed codec." << endl
					<< "Forcing default codec." << endl;
		} catch (...) {
			// if that doesn't work, no video
			cerr << "fingershooter.main(): can't initialize video writer, no video output file." << endl;

		}
	}
}


// Returns a histogram with hue values obtained by sampling img in rect selection
// remember to cvReleaseHist(&hist) when done
// param: show [false]  -- if true show red rectangle in image of selection and pic of hist
CvHistogram* createHueHist(IplImage* img, CvRect selection, bool show) {
	int hdims = 16;
	float hranges_arr[] = {0,180};
	float* hranges = hranges_arr;
//	int vmin = 10, vmax = 256, smin = 30;
	CvHistogram *hist = 0;
	IplImage *hsv = 0, *hue = 0; // *mask = 0;
	hsv = cvCreateImage( cvGetSize(img), 8, 3 );
	hue = cvCreateImage( cvGetSize(img), 8, 1 );
//	mask = cvCreateImage( cvGetSize(img), 8, 1 );
	hist = cvCreateHist( 1, &hdims, CV_HIST_ARRAY, &hranges, 1 );

	cvCvtColor( img, hsv, CV_BGR2HSV );
	cvSplit( hsv, hue, 0, 0, 0 );

	float max_val = 0.f;
	cvSetImageROI( hue, selection );
//	cvSetImageROI( mask, selection );
	cvCalcHist( &hue, hist, 0 );  // orig cvCalcHist( &hue, hist, 0, mask );
	cvGetMinMaxHistValue( hist, 0, &max_val, 0, 0 );
	cvConvertScale( hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0 );
//	cvResetImageROI( hue );
//	cvResetImageROI( mask );

	cvReleaseImage( &hsv);
	cvReleaseImage( &hue);
//	cvReleaseImage( &mask);

	// ********* create and save image of histogram, image, and image with rect *********
	//
	IplImage *histimg = 0;
//	copy = cvCreateImage( cvGetSize(img), 8, 3 );
	histimg = cvCreateImage( cvSize(320,200), 8, 3 );
	cvZero( histimg );
//	cvCopy(img, copy, 0);
	// draw red rect around selection
//	int line_sz = 2;
//	cvRectangle(copy, cvPoint(selection.x, selection.y),
//			cvPoint(selection.x + selection.width,
//					selection.y + selection.height),
//					CV_RGB(255, 0, 0), line_sz);

	// create hist img
	int i, bin_w = histimg->width / hdims;
	for( i = 0; i < hdims; i++ )
	{
		int val = cvRound( cvGetReal1D(hist->bins,i)*histimg->height/255 );
		CvScalar color = hsv2rgb(i*180.f/hdims);
		cvRectangle( histimg, cvPoint(i*bin_w,histimg->height),
				cvPoint((i+1)*bin_w,histimg->height - val),
				color, -1, 8, 0 );
	}
	// show user histogram and image with rect
	if(show) {
		printf("Showing calibration image and 1d histogram. \nHit any key to continue.\n");
		cvNamedWindow("img", CV_WINDOW_AUTOSIZE );
		cvNamedWindow("hist", CV_WINDOW_AUTOSIZE );
		cvShowImage("img", img);
		cvShowImage("hist", histimg);

		char c = cvWaitKey(0);
		cvDestroyWindow("img");
		cvDestroyWindow("hist");
	}
	printf("Histogram image and calibration image saved in ./images\n");
	cvSaveImage("./images/calibrate_image.jpg", img);
//	cvSaveImage("./images/calibrate_image_w_selection.jpg", copy);
	cvSaveImage("./images/calibrate_hist.jpg", histimg);
//	cvReleaseImage(&copy);
	cvReleaseImage( &histimg);

	// ***************


	return hist;
}

// Returns a histogram with hue and saturation values obtained by sampling img in rect selection
// remember to cvReleaseHist(&hist) when done
// param: show [false]  -- if true show red rectangle in image of selection and pic of hist
CvHistogram* createHueSatHist(IplImage* img, CvRect selection, bool show) {
	// ********** orig ********
//    IplImage* hsv = cvCreateImage( cvGetSize(img), 8, 3 );
////    IplImage* copy = cvCreateImage( cvGetSize(img), 8, 3 );
//    cvZero(hsv);
//    cvSetImageROI(img, selection);
//    cvSetImageROI(hsv, selection);
//    cvCvtColor( img, hsv, CV_BGR2HSV );
//    cvResetImageROI(img);
//    cvResetImageROI(hsv);
//
//    // debug
//	cvNamedWindow("huesat", CV_WINDOW_AUTOSIZE );
//	cvShowImage("huesat", hsv);
//
//	char c = cvWaitKey(0);
//	cvDestroyWindow("huesat");
////	return 0;
//
//
//    IplImage* h_plane  = cvCreateImage( cvGetSize(img), 8, 1 );
//    IplImage* s_plane  = cvCreateImage( cvGetSize(img), 8, 1 );
//    IplImage* v_plane  = cvCreateImage( cvGetSize(img), 8, 1 );
	// ****************************

	CvSize sel_size = cvSize(selection.width, selection.height);
	IplImage* hsv = cvCreateImage( sel_size, 8, 3 );
    cvSetImageROI(img, selection);
    cvCvtColor( img, hsv, CV_BGR2HSV );
    cvResetImageROI(img);

    // debug
//	cvNamedWindow("huesat", CV_WINDOW_AUTOSIZE );
//	cvShowImage("huesat", hsv);
//	char c = cvWaitKey(0);
//	cvDestroyWindow("huesat");

    IplImage* h_plane  = cvCreateImage( sel_size, 8, 1 );
    IplImage* s_plane  = cvCreateImage( sel_size, 8, 1 );
    IplImage* v_plane  = cvCreateImage( sel_size, 8, 1 );


    IplImage* planes[] = { h_plane, s_plane };
//    cvCvtPixToPlane( hsv, h_plane, s_plane, v_plane, 0 );

    cvSplit(hsv, h_plane, s_plane, v_plane, 0);

    // Build the histogram and compute its contents.
    //
    int h_bins = 30, s_bins = 32;
    CvHistogram* hist;
    {
      int    hist_size[] = { h_bins, s_bins };
      float  h_ranges[]  = { 0, 180 };          // hue is [0,180]
      float  s_ranges[]  = { 0, 255 };
      float* ranges[]    = { h_ranges, s_ranges };
      hist = cvCreateHist(
        2,
        hist_size,
        CV_HIST_ARRAY,
        ranges,
        1
      );
    }
    cvCalcHist( planes, hist, 0, 0 );

    // Create an image to use to visualize our histogram.
    //
    int scale = 10;
    IplImage* hist_img = cvCreateImage(
      cvSize( h_bins * scale, s_bins * scale ),
      8,
      3
    );
    cvZero( hist_img );

    // populate our visualization with little gray squares.
    //
    float max_value = 0;
    cvGetMinMaxHistValue( hist, 0, &max_value, 0, 0 );

    for( int h = 0; h < h_bins; h++ ) {
        for( int s = 0; s < s_bins; s++ ) {
            float bin_val = cvQueryHistValue_2D( hist, h, s );
            int intensity = cvRound( bin_val * 255 / max_value );
            cvRectangle(
              hist_img,
              cvPoint( h*scale, s*scale ),
              cvPoint( (h+1)*scale - 1, (s+1)*scale - 1),
              CV_RGB(intensity,intensity,intensity),
              CV_FILLED
            );
        }
    }

//    // Normalize the hist instead--see if it helps with backprojectio
//    cvNormalizeHist(hist, 1);
//
//    for( int h = 0; h < h_bins; h++ ) {
//        for( int s = 0; s < s_bins; s++ ) {
//            float bin_val = cvQueryHistValue_2D( hist, h, s );
//            int intensity = cvRound( bin_val * 255 );
//            cvRectangle(
//              hist_img,
//              cvPoint( h*scale, s*scale ),
//              cvPoint( (h+1)*scale - 1, (s+1)*scale - 1),
//              CV_RGB(intensity,intensity,intensity),
//              CV_FILLED
//            );
//        }
//    }

//	IplImage *histimg = 0;
//	copy = cvCreateImage( cvGetSize(img), 8, 3 );
//	histimg = cvCreateImage( cvGetSize(hist_img), 8, 3 );
//	cvZero( histimg );
//	cvCopy(img, copy, 0);
//	// draw red rect around selection
//	int line_sz = 2;
//	cvRectangle(copy, cvPoint(selection.x, selection.y),
//			cvPoint(selection.x + selection.width,
//					selection.y + selection.height),
//					CV_RGB(255, 0, 0), line_sz);


    // show user histogram and image with rect
    if(show) {
    	cvNamedWindow("img", CV_WINDOW_AUTOSIZE );
    	cvNamedWindow("hist", CV_WINDOW_AUTOSIZE );
    	cvShowImage("img", img);
    	cvShowImage("hist", hist_img);

    	char c = cvWaitKey(0);
    	cvDestroyWindow("img");
    	cvDestroyWindow("hist");
    }
    cvSaveImage("./images/calibrate_image.jpg", img);
//    cvSaveImage("./images/calibrate_image_w_selection.jpg", copy);
    cvSaveImage("./images/calibrate_hist.jpg", hist_img);

    cvReleaseImage(&hsv);
//    cvReleaseImage(&copy);
    cvReleaseImage(&hist_img);
//    cvReleaseImage(&h_plane);
//    cvReleaseImage(&s_plane);
    cvReleaseImage(&v_plane);

    return hist;
}

// taken from camshiftdemo.c
CvScalar hsv2rgb( float hue )
{
    int rgb[3], p, sector;
    static const int sector_data[][3]=
        {{0,2,1}, {1,2,0}, {1,0,2}, {2,0,1}, {2,1,0}, {0,1,2}};
    hue *= 0.033333333333333333333333333333333f;
    sector = cvFloor(hue);
    p = cvRound(255*(hue - sector));
    p ^= sector & 1 ? 255 : 0;

    rgb[sector_data[sector][0]] = 255;
    rgb[sector_data[sector][1]] = 0;
    rgb[sector_data[sector][2]] = p;

    return cvScalar(rgb[2], rgb[1], rgb[0],0);
}

void draw_selection(IplImage *img, CvRect selection) {
	int line_sz = 2;
	cvRectangle(img, cvPoint(selection.x-line_sz, selection.y-line_sz),
			cvPoint(selection.x + selection.width + line_sz,
					selection.y + selection.height + line_sz),
					CV_RGB(255, 0, 0), line_sz);
}

void test_huehist() {
	IplImage *image = 0;
	CvRect sel;

	image = cvLoadImage("./images/dl4.jpg");
	sel = cvRect(230, 220, 150, 170);
	draw_selection(image, sel);
	createHueHist(image, sel, true);

	image = cvLoadImage("./images/lml1.jpg");
	sel = cvRect(150, 320, 100, 120);
	draw_selection(image, sel);
	createHueHist(image, sel, true);

	image = cvLoadImage("./images/lncl4.jpg");
	sel = cvRect(150, 320, 100, 120);
	draw_selection(image, sel);
	createHueHist(image, sel, true);

	// image of Thibault's example
	// cmd line:
	//  ./images/thib.jpg 330 360 75 60
	image = cvLoadImage("./images/thib.jpg");
	sel = cvRect(330, 360, 75, 60);
	draw_selection(image, sel);
	createHueHist(image, sel, true);

	cvReleaseImage(&image);
}

