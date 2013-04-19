//
//  Rgn.h
//  findit
//
//  Created by zuzu on 13-3-27.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#ifndef __Rgn__h__
#define __Rgn__h__

struct RgnWapper;

struct RgnConfig
{
    int minWidth;
    bool mutiCore;
};

extern struct RgnConfig gRgnConfig;

//监视处理进度
@protocol RgnProgress <NSObject>
- (void)progress:(float) v;
@end

@interface Rgn: NSObject
{
@private
    struct RgnWapper* mRgn;
}
- (void)rgnIt: (UIImage*) img minWidth: (int) width showProgress: (id<RgnProgress>) show;
- (UIImage*)rgnImage: (int) type level: (int) level;

@property (assign) int BlockSize;
@property (assign) id<RgnProgress> Progress;
@end
#endif
