//
//  Denosie.h
//  findit
//
//  Created by zuzu on 13-3-15.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#ifndef __findit__Denosie__
#define __findit__Denosie__

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

void deNosie(cv::Mat& in,cv::Mat& out,int ns);
void deNosie_OMP(cv::Mat& in,cv::Mat& out,int ns);
#endif /* defined(__findit__Denosie__) */
