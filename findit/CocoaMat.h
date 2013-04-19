//
//  CocoaMat.h
//  findit
//
//  Created by zuzu on 13-4-9.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#ifndef findit_CocoaMat_h
#define findit_CocoaMat_h

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;

/*
    创建一个CGContextRef依据Mat的大小
    m必须是一个CV_8UC4格式
 */
CGContextRef CreateBitmapContextFromMat(const Mat& m);

/*
    将img完整的复制到cgtx中
 */
void DrawImageToCGContext(CGImageRef img,CGContextRef cgtx,int width,int height);

/*
    创建一个宽度和高度为width,height的Mat,缩放img图像到Mat中
 */
Mat CreateMatFromCGImage(CGImageRef img,int width,int height,float scale,UIImageOrientation uiio);

/*
    根据m创建一个CGImage,注意m和该CGImage共享数据.
 */
CGImage* CreateCGImageFromMat(Mat& m);

#endif
