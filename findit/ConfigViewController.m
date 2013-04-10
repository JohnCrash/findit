//
//  ConfigViewController.m
//  findit
//
//  Created by zuzu on 13-4-11.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import "ConfigViewController.h"
#import "ViewController.h"

@interface ConfigViewController ()

@end

@implementation ConfigViewController
@synthesize mWidthSwitch;
@synthesize mWidth;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self)
    {
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    mWidth.text = [[NSString alloc] initWithFormat:@"%d",gRgnConfig.minWidth];
    //NSLog(@"viewDidLoad");
	// Do any additional setup after loading the view.
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    //NSLog(@"viewDidUnload");
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
    } else {
        return YES;
    }
}

- (IBAction)enableMinWidth:(id)sender
{
    if( mWidthSwitch.on )
    {
        gRgnConfig.minWidth = [mWidth.text intValue];
        mWidth.enabled = YES;
    }
    else
    {
        gRgnConfig.minWidth = 0;
        mWidth.enabled = NO;
    }
}

- (IBAction)widthReturn:(id)sender
{
 //   NSLog(@"Return");
}
- (IBAction)didOnExit:(id)sender
{
  //  NSLog(@"didOnExit");
}
- (IBAction)editingDidEnd:(id)sender
{
  //  NSLog(@"editingDidEnd");
    gRgnConfig.minWidth = [mWidth.text intValue];
}
- (IBAction)changed:(id)sender
{
  //  NSLog(@"changed");
}
- (IBAction)didBegin:(id)sender
{
  //  NSLog(@"didBegin");
}

- (IBAction)configReturn:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)dealloc {
    [mWidth release];
    [mWidthSwitch release];
    [super dealloc];
}
@end
