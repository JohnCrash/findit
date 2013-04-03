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
    NSLog(@"Hello world");
}
@end
