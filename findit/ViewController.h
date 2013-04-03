//
//  ViewController.h
//  FindIt
//
//  Created by zuzu on 13-3-25.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface ViewController : UIViewController <UIImagePickerControllerDelegate,UINavigationControllerDelegate,UIPopoverControllerDelegate>
{
    UIImagePickerController* mImagePicker;
    UIPopoverController* mPopover;    
}

@property (retain, nonatomic) IBOutlet UIScrollView *mImageScrollView;
@property (retain, nonatomic) IBOutlet UIImageView *mImageView;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *mSeeBut;
@end
