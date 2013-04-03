//
//  MainWindow.h
//  findit
//
//  Created by zuzu on 13-4-4.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface MainWindow : UIWindow <UIImagePickerControllerDelegate,UINavigationControllerDelegate,UIPopoverControllerDelegate>
{
    UIImagePickerController* mImagePicker;
    UIPopoverController* mPopover;
}
@property (retain, nonatomic) IBOutlet UIScrollView *mImageScrollView;
@property (retain, nonatomic) IBOutlet UIImageView *mImageView;
@property (retain, nonatomic) IBOutlet UIBarButtonItem *mSeeBut;
@end
