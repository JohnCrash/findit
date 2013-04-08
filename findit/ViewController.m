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
@synthesize mProgressBar;

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
    在图库中打开一个图像文件
 */
- (IBAction)OnSeeFromFile:(id)sender
{
    [self pickItAndRgn:false sender:sender];
}

/*
    打开摄像头拍摄一张围棋照片进行识别
 */
- (IBAction)OnSeeIt:(id)sender
{
    [self pickItAndRgn:true sender:sender];
}

//如果b true马上拍摄一张照片进行识别,如果false将从已有的图片中选择一张进行识别
- (void) pickItAndRgn : (bool) b sender:(id) sender
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
        //弹出Popover并且将Popover的箭头指向sender
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
    if( mPopover )
    {
        [mPopover dismissPopoverAnimated:true];
        [mPopover release];
        mPopover = nil;
    }
    else
        [self dismissModalViewControllerAnimated:NO];
    //选择图片
    [self rgnIt:[info objectForKey:@"UIImagePickerControllerOriginalImage"]];
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
    [mProgressBar release];
    [super dealloc];
}

- (void)rgnIt:(UIImage *) img
{
    if( img )
    {
        UIImage* pimg;
        [mProgressBar setHidden:false];
        [mProgressBar setNeedsDisplay];
        
        for( int i = 0;i<10;i++ )
        {
        //   [NSThread sleepForTimeInterval:1];
            NSDate* futureDate = [NSDate dateWithTimeInterval:0.1 sinceDate:[NSDate date]];
            [[NSRunLoop currentRunLoop] runUntilDate:futureDate];
            mProgressBar.progress = i/10.0;
 //           [mProgressBar setProgress:i/10.0];
        }
        pimg = RgnImage(img);
        
        [mProgressBar setHidden:true];
        
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
