//
//  ViewController.m
//  FindIt
//
//  Created by zuzu on 13-3-25.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import "ViewController.h"

struct RgnConfig gRgnConfig;

void initRgnConfig()
{
    gRgnConfig.minWidth = 480;
    gRgnConfig.mutiCore = true;
}

@interface ViewController ()

@end

@implementation ViewController
@synthesize mImageScrollView;
@synthesize mImageView;
@synthesize mSeeBut;
@synthesize mProgressBar;
@synthesize mRgnShowType;
@synthesize mRgnBackground;
@synthesize mConfigController;

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    mRgn = [[Rgn alloc] init];
    initRgnConfig();
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
    [mRgn release];
    [mRgnShowType release];
    [mRgnBackground release];
    [mRgnShowType release];
    [mRgnBackground release];
    [mConfigController release];
    [super dealloc];
}

- (void)showProgress:(NSArray*) thresults
{
    [thresults retain];
    NSNumber* v = [thresults objectAtIndex:0];
    mProgressBar.progress = [v floatValue];
    [thresults release];
}

- (void)progress:(float)v
{
    //单线程允许,保证主线更新进度条
   // NSDate* futureDate = [NSDate dateWithTimeInterval:0.1 sinceDate:[NSDate date]];
   // [[NSRunLoop currentRunLoop] runUntilDate:futureDate];
   // mProgressBar.progress = v;
    @autoreleasepool
    {
        NSArray* params = [[[NSArray alloc] initWithObjects:[[[NSNumber alloc] initWithFloat:v] autorelease], nil] autorelease];
        [self performSelectorOnMainThread:@selector(showProgress:) withObject:params waitUntilDone:NO];
    }
}

- (void)showMainAction:(NSArray*) thresults
{
    [self showAction];
    [mProgressBar setHidden:true];
}

- (void)rgnBackground:(NSArray*) theresults
{
    UIImage* img;
    
    @autoreleasepool
    {
        img = [theresults objectAtIndex:0];
        
        int minWidth = gRgnConfig.minWidth<=0?img.size.width:gRgnConfig.minWidth;
        
        [mRgn rgnIt:img minWidth:minWidth showProgress:self];
      
        [self performSelectorOnMainThread:@selector(showMainAction:) withObject:nil waitUntilDone:NO];
        
        [theresults release];
    }
}

- (void)rgnIt:(UIImage *) img
{
    if( img )
    {
        NSArray * param = [[NSArray alloc] initWithObjects:img,nil];
        
        [mProgressBar setHidden:false];
        [self performSelectorInBackground:@selector(rgnBackground:) withObject:param];
    }
}

- (void)showAction
{
    UIImage* pimg;
    int level,type;
    if( [mRgnShowType selectedSegmentIndex] == 0 )
        level = 1;
    else if( [mRgnShowType selectedSegmentIndex] == 1 )
        level = 2;
    else
        level = 4;
    if( [mRgnBackground selectedSegmentIndex] == 0 )
        type = 0;
    else
        type = 1;
        
    pimg = [mRgn rgnImage:type level:level];
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
}

//对显示进行调整
- (IBAction)rgnLevelChange:(id)sender
{
    [self showAction];
}

- (IBAction)rgnBackgroundChange:(id)sender
{
    [self showAction];
}

//打开配置对话
- (IBAction)rgnConfigDialog:(id)sender
{
    [self presentViewController:mConfigController animated:YES completion:nil];
}

@end
