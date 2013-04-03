//
//  Rgn.m
//  findit
//
//  Created by zuzu on 13-3-27.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#include "Rgn.h"
#include "RgnGrid.h"
#include "Denosie.h"

/*
    根据m创建一个CGContextRef
 */
CGContextRef CreateBitmapContextFromMat(const Mat& m)
{
    CGContextRef context = NULL;
    CGColorSpaceRef colorSpace;
    
    assert(m.type()==CV_8UC4);
    
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
void DrawImageToCGContext(CGImageRef img,CGContextRef cgtx)
{
    CGRect rect = {{0,0},{0,0}};
    rect.size.width = CGImageGetWidth(img);
    rect.size.height = CGImageGetHeight(img);
    CGContextDrawImage(cgtx, rect, img);
}

Mat CreateMatFromCGImage(CGImageRef img)
{
    Mat m;
    m.create(CGImageGetHeight(img),CGImageGetWidth(img),CV_8UC4);
    CGContextRef ctx = CreateBitmapContextFromMat(m);
    if( ctx )
    {
        DrawImageToCGContext(img, ctx);
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

UIImage* RgnImage(UIImage* img)
{
    Mat m,dst;
    int block_size = 11;
    
    m = CreateMatFromCGImage([img CGImage]);
    CGContextRef ctx = CreateBitmapContextFromMat(m);
    if( ctx )
    {
        Mat bst;
        DrawImageToCGContext([img CGImage],ctx);
        CGContextRelease(ctx);
        cvtColor(m, dst, CV_RGBA2GRAY,1);
        adaptiveThreshold(dst,bst,255,ADAPTIVE_THRESH_MEAN_C,CV_THRESH_BINARY_INV,block_size,5);
        deNosie(bst,dst,9);
    }
    else
        return NULL;
    
    RgnGrid grid(dst.cols,dst.rows,dst.data);
    Mat src;
    cvtColor(m, src, CV_RGBA2RGB);
    CGImage* pout=CreateCGImageFromMat(src);
    
    grid.drawTL(src);
    grid.End();
    
    if(pout)
    {
        UIImage* pret = [[UIImage alloc] initWithCGImage:pout];
        CGImageRelease(pout);
        return [pret autorelease];
    }
    return NULL;
}