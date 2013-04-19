//
//  CocoaMat.mm
//  findit
//
//  Created by zuzu on 13-4-9.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#include "CocoaMat.h"

/*
 根据m创建一个CGContextRef
 */
CGContextRef CreateBitmapContextFromMat(const Mat& m)
{
    CGContextRef context = NULL;
    CGColorSpaceRef colorSpace;
    
   // NSAssert(m.type()==CV_8UC4,@"Mat type must be CV_8UC4");
    
    colorSpace = CGColorSpaceCreateDeviceRGB();
    if (colorSpace == NULL)
    {
        NSLog(@"Error allocating color space");
        return NULL;
    }
    
    context = CGBitmapContextCreate (m.data, m.cols,m.rows, 8, m.cols*m.elemSize(),
                                     colorSpace, kCGImageAlphaPremultipliedLast);
    if (context == NULL)
    {
        NSLog(@"Context not created!");
    }
    CGColorSpaceRelease( colorSpace );
    return context;
}

/*
 将CGImageRef复制到cgtx
 */
void DrawImageToCGContext(CGImageRef img,CGContextRef cgtx,int width,int height)
{
    CGRect rect = {{0,0},{0,0}};
    rect.size.width = width;
    rect.size.height = height;
    CGContextDrawImage(cgtx, rect, img);
}

Mat CreateMatFromCGImage(CGImageRef img,int width,int height,float scale,UIImageOrientation uiio )
{
    Mat m;
    m.create(height,width,CV_8UC4);
    CGContextRef ctx = CreateBitmapContextFromMat(m);
    if( ctx )
    {
        CGAffineTransform transform = CGAffineTransformIdentity;
        int _width,_height;
        if( uiio == UIImageOrientationLeft )
        {
            _width = height;
            _height = width;
            transform = CGAffineTransformTranslate(transform, width, 0);
            transform = CGAffineTransformRotate(transform, CV_PI/2);
            NSLog(@"OrientationLeft\n");
        }
        else if( uiio == UIImageOrientationRight )
        {
            _width = height;
            _height = width;
            transform = CGAffineTransformTranslate(transform, 0, height);
            transform = CGAffineTransformRotate(transform, -CV_PI/2);
            NSLog(@"OrientationRight\n");
        }
        else if( uiio == UIImageOrientationDown )
        {
            _width = width;
            _height = height;
            transform = CGAffineTransformTranslate(transform, width, height);
            transform = CGAffineTransformRotate(transform, CV_PI);
            NSLog(@"OrientationDown\n");
        }
        else
        {
            _width = width;
            _height = height;
            NSLog(@"OrientationUp\n");
        }
        transform = CGAffineTransformScale(transform, scale, scale);
        CGContextConcatCTM(ctx, transform);
        DrawImageToCGContext(img, ctx,_width,_height);
        CGContextRelease(ctx);
    }
    return m;
}

static void releaseMat(void *info, const void *data, size_t size)
{
    Mat* pm = (Mat*)info;
    if( pm )delete pm;
}

CGImage* CreateCGImageFromMat(Mat& m)
{
    CGImage* img;
    CGColorSpaceRef colorSpace;
    colorSpace = CGColorSpaceCreateDeviceRGB();
    if (colorSpace == NULL)
    {
        NSLog(@"Error allocating color space");
        return NULL;
    }
    /*
     增加Mat引用计数
     */
    Mat* bk = new Mat(m);
    CGDataProviderRef providerRef = CGDataProviderCreateWithData(bk,
                                                                 m.data,
                                                                 m.total()*m.elemSize(),
                                                                 releaseMat);
    img = CGImageCreate(m.cols,m.rows,
                        m.elemSize1()*8,m.elemSize()*8,m.step1(),
                        colorSpace,kCGBitmapByteOrderDefault,providerRef,
                        NULL,YES,kCGRenderingIntentDefault);
    CGColorSpaceRelease( colorSpace );
    CGDataProviderRelease(providerRef);
    return img;
}
