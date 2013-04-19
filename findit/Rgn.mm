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
#include <mach/mach_host.h>
#include <sys/sysctl.h>

struct RgnWapper
{
    RgnGrid* rg;
};

unsigned int countCores()
{
    host_basic_info_data_t hostInfo;
    mach_msg_type_number_t infoCount;
    
    infoCount = HOST_BASIC_INFO_COUNT;
    host_info(mach_host_self(), HOST_BASIC_INFO,
              (host_info_t)&hostInfo, &infoCount);
    
    return (unsigned int)(hostInfo.max_cpus);
}

unsigned int countCores2()
{
    size_t len;
    unsigned int ncpu;
    
    len = sizeof(ncpu);
    sysctlbyname ("hw.ncpu",&ncpu,&len,NULL,0);
    
    return ncpu;
}

void progress_callback( Rgn* self,float v )
{
    [[self Progress] progress:v];
}

@implementation Rgn
@synthesize BlockSize;
@synthesize Progress;

enum
{
    GO,
    STOP,
    EXIT
};

- (id)init
{
    self = [super init];
    if( self )
    {
        mRgn = new RgnWapper();
        mRgn->rg = new RgnGrid();
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
    delete mRgn->rg;
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
    
    Mat src;
    CGSize size = [img size];
    
    src = CreateMatFromCGImage([img CGImage],
                               width,width*size.height/size.width,
                               1,
                               [img imageOrientation]);
    if( src.empty() )
    {
        NSLog(@"CreateMatFromCGImage(%@) return empty Mat",[img description]);
        return;
    }
    Progress = show;
    if( mRgn->rg->IsMutiCores()!=gRgnConfig.mutiCore )
    {
        cout << "switch thread mode\n";
        mRgn->rg->resetThread(gRgnConfig.mutiCore);
    }
    mRgn->rg->rgnM(src,true,boost::bind(progress_callback,self,_1));
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
            Mat s = mRgn->rg->getSourceMat();
            if( s.empty() )
                return NULL;
            img = s.clone();
        }
        else
        {
            Mat s = mRgn->rg->getBinaryMat();
            if( s.empty() )
                return NULL;
            cvtColor(s, img, CV_GRAY2BGRA);
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