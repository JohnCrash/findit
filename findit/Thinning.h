//
//  Thinning.h
//  findit
//
//  Created by zuzu on 13-3-14.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#ifndef __findit__Thinning__
#define __findit__Thinning__

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

void thinning(cv::Mat& im);
void thinningGuoHall(cv::Mat& im);

#endif /* defined(__findit__Thinning__) */
