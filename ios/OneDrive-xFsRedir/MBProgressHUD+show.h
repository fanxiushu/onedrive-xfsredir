//
//  MBProgressHUD+show.h
//  iOS开发Ui篇-实现一个私人通讯录小应用(一)
//
//  Created by 谢俊伟 on 14-8-11.
//  Copyright (c) 2014年 a. All rights reserved.
//

#import "MBProgressHUD.h"

@interface MBProgressHUD (show)
+(void)showMessage:(NSString *)message;
//+(void)showError:(NSString *)error;
+(void)hideHUD;

@end
