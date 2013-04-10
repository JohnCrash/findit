//
//  ViewController.h
//  FindIt
//
//  Created by zuzu on 13-3-25.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ConfigViewController.h"
#import "Rgn.h"

struct RgnConfig
{
    int minWidth;
};

extern struct RgnConfig gRgnConfig;

@interface ViewController : UIViewController
        <UIImagePickerControllerDelegate,
        UINavigationControllerDelegate,
        UIPopoverControllerDelegate,
        RgnProgress>
{
    UIImagePickerController* mImagePicker;
    UIPopoverController* mPopover;
    Rgn* mRgn;
}

- (void)progress:(float)v;

@property (retain, nonatomic) IBOutlet UIScrollView *mImageScrollView;
@property (retain, nonatomic) IBOutlet UIImageView *mImageView;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *mSeeBut;
//进度条
@property (retain, nonatomic) IBOutlet UIProgressView *mProgressBar;
//显示分析阶段
@property (retain, nonatomic) IBOutlet UISegmentedControl *mRgnShowType;

//显示背景
@property (retain, nonatomic) IBOutlet UISegmentedControl *mRgnBackground;
@property (retain, nonatomic) IBOutlet ConfigViewController *mConfigController;

@end
