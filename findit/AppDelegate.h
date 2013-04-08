//
//  AppDelegate.h
//  FindIt
//
//  Created by zuzu on 13-3-25.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ViewController.h"

@interface AppDelegate : NSObject <UIApplicationDelegate>

//@property (strong, nonatomic) UIWindow *window;
@property (retain, nonatomic) IBOutlet UIWindow *window;
@property (retain, nonatomic) IBOutlet ViewController *controller;

@end
