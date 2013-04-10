//
//  Rgn.m
//  findit
//
//  Created by zuzu on 13-3-27.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#include "Rgn.h"
#include "CocoaMat.h"
#include "RgnGrid.h"
#include "Denosie.h"

struct RgnWapper
{
    RgnGrid* rg;
    Mat src; //原彩色图
    Mat dst; //降噪完的二值图
};

@implementation Rgn
@synthesize Source;
@synthesize BlockSize;

- (id)init
{
    self = [super init];
    if( self )
    {
        mRgn = new RgnWapper();
        BlockSize = 11;
    }
    return self;
}

- (void)dealloc
{
    [self releaseRgn];
    delete mRgn;
    [super dealloc];
}

- (void)releaseRgn
{
    [Source release];
    delete mRgn->rg;
    mRgn->src.release();
    mRgn->dst.release();
}
/*
    识别img中的图像
    对原图像进行缩放,width给出最小的宽度.(在不影响结果的情况下减少数据处理量)
    show进度回调函数
 */
- (void)rgnIt: (UIImage*) img
     minWidth: (int) width
        showProgress: (id<RgnProgress>) show
{
    NSAssert(mRgn!=NULL,@"mRgn is NULL");
    
    [show progress:0];
    
    if( Source == img )
        return;
    else
        [self releaseRgn];
    
    Mat src,dst,bst;
    CGSize size = [img size];
    src = CreateMatFromCGImage([img CGImage],width,width*size.height/size.width);
    if( src.empty() )
    {
        NSLog(@"CreateMatFromCGImage(%@) return empty Mat",[img description]);
        return;
    }
    //转换为灰度图
    cvtColor(src, dst, CV_RGBA2GRAY,1);
    //再将灰度图转换为二值图
    adaptiveThreshold(dst,bst,255,ADAPTIVE_THRESH_MEAN_C,CV_THRESH_BINARY_INV,BlockSize,5);
    //然后在降低噪点
    deNosie(bst,dst,9);
    //开始分析
    mRgn->rg = new RgnGrid(dst.cols,dst.rows,dst.data);
    mRgn->src = src;
    mRgn->dst = dst;
    
    [show progress:1/4];
    //分析TL+
    mRgn->rg->Rgn();
    [show progress:2/4];
    //对边进行识别
    mRgn->rg->RgnEdge();
    [show progress:3/4];
    //猜测
    mRgn->rg->GuessGrid();
    
    Source = img;
    [Source retain];
    
    [show progress:1];
}

//返回网格的大小,m,n

/*
    返回分析中的中间图
    type = 0 背景为原图
         = 1 二值图
    level = 1 绘制初级的识别物,T,L,+角将被绘制
    level = 2 绘制中间识别物,如四个角,边
    level = 4 绘制最终识别的网格
    允许使用组合方式
 */
- (UIImage*)rgnImage: (int) type level: (int) level
{
    Mat img;
    if( mRgn )
    {
        if( type == 0 )
        {
            if( mRgn->src.empty() )
                return NULL;
            img = mRgn->src.clone();
        }
        else
        {
            if( mRgn->dst.empty() )
                return NULL;
            cvtColor(mRgn->dst, img, CV_GRAY2BGRA);
        }
        CGImage* cim = CreateCGImageFromMat(img);
        mRgn->rg->drawTL(img, level);
        UIImage* pret = [[UIImage alloc] initWithCGImage:cim];
        CGImageRelease(cim);
        return [pret autorelease];
    }
    else
    {
        NSLog(@"mRgn==NULL");
    }
    return nil;
}
@end