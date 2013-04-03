//
//  Rgn.h
//  findit
//
//  Created by zuzu on 13-3-27.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import <Foundation/Foundation.h>

#ifndef __Rgn__h__
#define __Rgn__h__

#ifdef __cplusplus
extern "C"
{
	//分析图像img,并且把中间过程存位图返回
    UIImage* RgnImage(UIImage* img);
}
#else
    UIImage* RgnImage(UIImage* img);
#endif

#endif
