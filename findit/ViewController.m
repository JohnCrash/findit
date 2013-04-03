//
//  ViewController.m
//  FindIt
//
//  Created by zuzu on 13-3-25.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import "ViewController.h"
#import "Rgn.h"

@interface ViewController ()

@end

@implementation ViewController
@synthesize mImageScrollView;
@synthesize mImageView;
@synthesize mSeeBut;

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
}

- (void)viewDidUnload
{
    [self setMSeeBut:nil];
    [self setMImageView:nil];
    [self setMImageScrollView:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
    } else {
        return YES;
    }
}

/*
    打开摄像头拍摄一张围棋照片进行识别
 */
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

- (void)dealloc
{
    [mImageView release];
    [mImageScrollView release];
    [super dealloc];
}

- (void)rgnIt:(UIImage *) img
{
    if( img )
    {
        UIImage* pimg = RgnImage(img);
        if( pimg )
        {
            CGRect rect;
            mImageScrollView.contentSize = [pimg size];
            rect.origin.x = 0;
            rect.origin.y = 0;
            rect.size = [pimg size];
            mImageView.frame = rect;
            [mImageView setImage:pimg];
        }
        else
            [mImageView setImage:img];
    }
}
@end
