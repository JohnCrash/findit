//
//  MainWindow.m
//  findit
//
//  Created by zuzu on 13-4-4.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import "MainWindow.h"
#import "Rgn.h"

@implementation MainWindow
@synthesize mImageScrollView;
@synthesize mImageView;
@synthesize mSeeBut;

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code
    }
    return self;
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    // Drawing code
}
*/

- (void)dealloc {
    [mSeeBut release];
    [mImageScrollView release];
    [mImageView release];
    [super dealloc];
}

- (IBAction)OnSeeIt:(id)sender
{
    if(mPopover && [mPopover isPopoverVisible])
    { // 如果已经存在释放
        [mPopover dismissPopoverAnimated:YES];
        [mPopover release];
    }
    mImagePicker = [[UIImagePickerController alloc] init];
    
    UIImagePickerControllerSourceType sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
    
    if([UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
    {
        sourceType = UIImagePickerControllerSourceTypeCamera;
    }
    mImagePicker.sourceType = sourceType;
    mImagePicker.mediaTypes = [UIImagePickerController availableMediaTypesForSourceType:
                               sourceType];
    //   mImagePicker.mediaTypes = @[(NSString *) kUTTypeImage,(NSString *) kUTTypeMovie];
    
    mImagePicker.allowsEditing = NO;
    
    mImagePicker.delegate = (id <UIImagePickerControllerDelegate,
                             UINavigationControllerDelegate>)self;
    
    NSString *deviceType = [UIDevice currentDevice].model;
    if([deviceType isEqualToString:@"iPad"] || [deviceType isEqualToString:@"iPad Simulator"])
    {
        mPopover = [[UIPopoverController alloc] initWithContentViewController:mImagePicker];
        mPopover.delegate = (id <UIPopoverControllerDelegate>)self;
        
        [mPopover presentPopoverFromBarButtonItem:sender permittedArrowDirections:UIPopoverArrowDirectionUp animated:YES];
    }
    else
    {
        [self presentModalViewController: mImagePicker animated: YES];
    }
    [mImagePicker release];    
}

- (void)imagePickerController:(UIImagePickerController *)picker
didFinishPickingMediaWithInfo:(NSDictionary *)info
{
    //选择图片
    [self rgnIt:[info objectForKey:@"UIImagePickerControllerOriginalImage"]];
    if( mPopover )
    {
        [mPopover dismissPopoverAnimated:true];
        [mPopover release];
        mPopover = nil;
    }
    else
        [self dismissModalViewControllerAnimated:NO];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker
{
    if( mPopover )
    {
        [mPopover dismissPopoverAnimated:true];
        [mPopover release];
        mPopover = nil;
    }
    else
        [self dismissModalViewControllerAnimated:NO];
}

- (void)navigationController:(UINavigationController *)navigationController
      willShowViewController:(UIViewController *)viewController
                    animated:(BOOL)animated
{
}

- (void)navigationController:(UINavigationController *)navigationController
       didShowViewController:(UIViewController *)viewController
                    animated:(BOOL)animated
{
}
@end
