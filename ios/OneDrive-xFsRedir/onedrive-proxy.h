//
//  onedrive-proxy.h
//  OneDrive-xFsRedir
//
//  Created by fanxiushu on 2018/4/18.
//  Copyright © 2018年 fanxiushu. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreGraphics/CoreGraphics.h>


/////
@interface onedrive_proxy: NSObject

@property(nonatomic) UITableView* tableView;

/////
-(void)Auth:(UIView*)view auth_complete:(void(^)(NSString* errMsg))func;

-(NSMutableArray*) query_directory:(NSString*)Path;

-(NSMutableDictionary*) stat:(NSString*)Path;
-(BOOL)mkdir:(NSString*)Path;
-(BOOL)delfile:(NSString*)Path;

//// upfile
-(void)upfile:(BOOL)async rpath:(NSString*)rpath lpath:(NSString*)lpath timeout:(int)timeout
    progress:(int(^)(NSInteger curr, NSInteger total, BOOL is_complete, NSString* errMsg))progress;

/// read data from offset
-(void)offset_read:(BOOL)async rpath:(NSString*)rpath offset:(NSInteger)offset data:(NSMutableData*)data
    progress:(int(^)(NSInteger curr, NSInteger total, BOOL is_complete, NSString* errMsg))progress;

@end

