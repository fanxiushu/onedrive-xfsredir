//
//  onedrive-proxy.h
//  OneDrive-xFsRedir
//
//  Created by fanxiushu on 2018/4/18.
//  Copyright © 2018年 fanxiushu. All rights reserved.
//

#import <Foundation/Foundation.h>
#include <string>
#include <map>
using namespace std;
#include "onedrive.h"
#import "onedrive-proxy.h"
#import "MBProgressHUD+show.h"
#import <WebKit/WebKit.h>

////////
@interface onedrive_proxy()
{
@private void*   handle;
@private string  refresh_token; //
}
@property(nonatomic)NSString* RefreshToken;
@property(copy, nonatomic) void(^auth_complete)(NSString* errMsg);

-(int)CreateOneDrive:(NSString*)strCode view:(UIView*)view;

@end

@interface myWeb: WKWebView <WKNavigationDelegate>
@property() onedrive_proxy*  od;
@end

@implementation myWeb
-(void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    NSString* str_url = navigationAction.request.URL.absoluteString;
    NSString* local_uri=@"http://localhost/?code=";
    if(strncasecmp(str_url.UTF8String, local_uri.UTF8String, local_uri.length) == 0){ // found
        ////
        NSString* str_code = [str_url substringFromIndex:local_uri.length];
        
        NSLog(@"CODE=%@",str_code);

        [self.od CreateOneDrive:str_code view:self];
        //////
        decisionHandler(WKNavigationActionPolicyCancel);
   //     [webView setHidden:true];
        
        return;
    }
    /////
    decisionHandler(WKNavigationActionPolicyAllow); ////
    
}
@end

static int auth_func(const char* url, void* param1, void* param2)
{
    onedrive_proxy* od = (__bridge onedrive_proxy*)param1;
    UIView* view = (__bridge UIView*)param2;
    /////
    NSURLRequest* nsurl = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:url] ]];
    myWeb *webView =[[myWeb alloc] initWithFrame:view.frame];
    webView.od=od;
    webView.navigationDelegate = webView;     //
    webView.allowsBackForwardNavigationGestures=YES;
    [view addSubview:webView]; //
    
    [webView loadRequest:nsurl];
    
 //   NSLog(@"*** auth_func [%s]", url );
    ////
    return 0;
}

static int refresh_token_func(void* handle, char* access_token, void* param)
{
    onedrive_proxy* od = (__bridge onedrive_proxy*)param;
    
    string acc_tok, ref_tok; int expire;
    ///
    ::onedrive_refresh_token(od.RefreshToken.UTF8String, acc_tok, ref_tok, expire); ///
    
    if(acc_tok.length()>0){
        strcpy(access_token, acc_tok.c_str());
        NSLog(@"Refresh Access Token OK.\n");
    }
    else{
        printf("*** \n");
        return -1;
    }
    /////
    return 0;
}

@interface od_progress:NSObject
////
@property(nonatomic) onedrive_proxy* od;
@property(copy, nonatomic) int(^progress)(NSInteger, NSInteger, BOOL, NSString *);
@property(nonatomic) NSInteger curr;
@property(nonatomic) NSInteger total;
@property(nonatomic) BOOL AbortTransfer;
@property(nonatomic) BOOL Async;

@end
@implementation od_progress
@end

static int upfile_progress(int64_t curr, int64_t total, void* param)
{
    od_progress* p = (__bridge od_progress*)param;
    /////
    if(p.AbortTransfer==YES) return -1; // abort
    p.curr=curr;
    p.total=total;
    if(p.Async){
        dispatch_async(dispatch_get_main_queue(), ^{
           int r = p.progress( curr, total, FALSE, nil);
           if(r<0) p.AbortTransfer=YES;
        });
    }
    else{
        int r = p.progress( curr, total, FALSE, nil);
        if(r<0) p.AbortTransfer=YES;
        return r;
    }
    ////
    return 0;
}
static int offread_progress(int curr, int total, void* param)
{
    od_progress* p = (__bridge od_progress*)param;
    /////
    if(p.AbortTransfer==YES) return -1; // abort
    p.curr=curr;
    p.total=total;
    if(p.Async){
        dispatch_async(dispatch_get_main_queue(), ^{
            int r = p.progress( curr, total, FALSE, nil);
            if(r<0) p.AbortTransfer=YES;
        });
    }
    else{
        int r = p.progress( curr, total, FALSE, nil);
        if(r<0) p.AbortTransfer=YES;
        return r;
    }
    
    ////
    return 0;
}
////////

@implementation  onedrive_proxy

-(instancetype)init
{
    self=[super init];
    if(self){
        handle=0;
    }
    return self;
}

-(void)dealloc{
    ////
    if(handle){
        onedrive_destroy(handle);
        handle=0;
    }
}

///////////
-(void)Auth:(UIView *)view auth_complete:(void (^)(NSString *))func
{
    self.auth_complete = func;
    ///
    NSString* token = nil;//[[NSUserDefaults standardUserDefaults]objectForKey:@"token"];
    ///
    if(token && token.length > 0){
        self.RefreshToken=token;
        func(nil);
        return;
    }
    /////
    ::onedrive_authorize_simple(auth_func, (__bridge void*)self, (__bridge void*)view );
}

-(int)CreateOneDrive:(NSString *)strCode view:(UIView*)view
{
    MBProgressHUD* hud = [[MBProgressHUD alloc]init];
    hud.labelText=@"Auth...";
    hud.mode = MBProgressHUDModeIndeterminate;
    hud.animationType = MBProgressHUDAnimationFade;
    
    [view addSubview:hud];
    [hud show:YES];
    
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        ///
        string acc_token, ref_token;int expire=0;
        NSString* errMsg=nil;
        int r = ::onedrive_fromecode_token(strCode.UTF8String, acc_token, ref_token, expire);
        if(r==0){
            // save refresh token
            self.RefreshToken = [NSString stringWithUTF8String:ref_token.c_str()]; ////
            [[NSUserDefaults standardUserDefaults]setObject:self.RefreshToken forKey:@"token"];
            
            self->handle = ::onedrive_create(acc_token.c_str(), 15, refresh_token_func, (__bridge void*)self);
            if(!self->handle) errMsg=@"onedrive_create error.";
        }
        else{
            errMsg=@"Can not Get Refresh Token.";
        }
        /////
        dispatch_async(dispatch_get_main_queue(), ^{
            hud.removeFromSuperViewOnHide = YES;
            [hud hide:YES];
            
            [view setHidden:YES];
            
            ////////
            self.auth_complete(errMsg);
            
        });
        
        //////
    });
    
    return 0;
}

-(NSMutableArray*)query_directory:(NSString *)Path
{
    NSMutableArray* arr = [[NSMutableArray alloc]init];
    if(!self->handle){
        self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
    }
    int r = ::onedrive_find_open(self->handle, Path.UTF8String, "*");
    if(r==0){
        onedrive_name_t name;
        while(::onedrive_find_next(self->handle, &name) > 0){
            if(!arr) arr = [[NSMutableArray alloc]init];
            ///
            NSMutableDictionary* dict=[[NSMutableDictionary alloc]init];
            [dict setObject:[NSString stringWithUTF8String:name.name] forKey:@"name"];
            [dict setObject:[NSNumber numberWithBool:name.is_dir] forKey:@"is_dir"];
            [dict setObject:[NSNumber numberWithInteger:name.size] forKey:@"size"];
            [dict setObject:[NSNumber numberWithInteger:name.mtime] forKey:@"mtime"];
            [dict setObject:[NSNumber numberWithInteger:name.ctime] forKey:@"ctime"];
        //    NSLog(@"find name=%s", name.name);
            ////
            [arr addObject:dict];
        }
        /////
    }
    else{
        NSLog(@"onedrive_find_open [%@] err=%d", Path, r);
        return arr;
    }
    ////
    return arr;
}

-(void)upfile:(BOOL)async rpath:(NSString *)rpath lpath:(NSString *)lpath timeout:(int)timeout
    progress:(int (^)(NSInteger, NSInteger, BOOL, NSString *))progress
{
    od_progress* p=[[od_progress alloc]init];
    p.progress = progress;
    p.od= self;
    p.AbortTransfer=NO;
    p.total=0;
    p.Async=async;
    ///
    if(async){
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            ///
            if(!self->handle){
                self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
            }
            int r = ::onedrive_upfile(self->handle, rpath.UTF8String,
                lpath.UTF8String,
                timeout, (progress!=nil)?upfile_progress:nil, (__bridge void*)p);
            NSString* errMsg=nil;
            if(r<0) errMsg=@"Error UpFile";
            ///
            dispatch_async(dispatch_get_main_queue(), ^{
                if(progress!=nil)progress(p.total,p.total, TRUE, errMsg); ///
            });
            ////
        });
    }
    else{
        ////
        if(!self->handle){
            self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
        }
        int r = ::onedrive_upfile(self->handle, rpath.UTF8String,
                                  lpath.UTF8String, timeout, (progress!=nil)?upfile_progress:nil, (__bridge void*)p);
        NSString* errMsg=nil;
        if(r<0) errMsg=@"Error UpFile";
        if(progress!=nil)progress(p.total,p.total, TRUE, errMsg);
        ///
    }
    ////
}

-(void)offset_read:(BOOL)async rpath:(NSString*)rpath offset:(NSInteger)offset data:(NSMutableData *)data
          progress:(int (^)(NSInteger, NSInteger, BOOL, NSString *))progress
{
    od_progress* p=[[od_progress alloc]init];
    p.progress = progress;
    p.od= self;
    p.AbortTransfer=NO;
    p.total=0;
    p.Async=async;
    ////
    if(async){
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            ///
            if(!self->handle){
                self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
            }
            int r = ::onedrive_offset_read(self->handle, rpath.UTF8String, (char*)data.bytes, offset, (int)data.length,
                                       (progress==nil)?nil:offread_progress, (__bridge void*)p);
            NSString* errMsg=nil;
            if(r<0) errMsg=@"Error Offset Read";
            ///
            dispatch_async(dispatch_get_main_queue(), ^{
                if(progress!=nil)progress(p.curr,p.total, TRUE, errMsg); ///
            });
            ////
        });
        //////////
    }
    else{
        ////
        if(!self->handle){
            self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
        }
        int r = ::onedrive_offset_read(self->handle, rpath.UTF8String, (char*)data.bytes, offset, (int)data.length,
                                       (progress==nil)?nil:offread_progress, (__bridge void*)p);
        NSString* errMsg=nil;
        if(r<0) errMsg=@"Error Offset Read";
        if(progress!=nil)progress(p.curr,p.total, TRUE, errMsg);
        ///
    }
}

////
-(NSMutableDictionary*)stat:(NSString *)Path
{
    NSMutableDictionary* dict=[[NSMutableDictionary alloc]init];
    if(!self->handle){
        self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
    }
    onedrive_stat_t st;
    int r = ::onedrive_stat(self->handle, Path.UTF8String, &st);
    if(r<0){
        return nil;
    }
    [dict setObject:[NSNumber numberWithBool:st.is_dir] forKey:@"is_dir"];
    [dict setObject:[NSNumber numberWithInteger:st.size] forKey:@"size"];
    [dict setObject:[NSNumber numberWithInteger:st.mtime] forKey:@"mtime"];
    [dict setObject:[NSNumber numberWithInteger:st.ctime] forKey:@"ctime"];
    ///
    return dict;
}
-(BOOL)mkdir:(NSString *)Path
{
    if(!self->handle){
        self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
    }
    int r = ::onedrive_mkdir(self->handle, Path.UTF8String);
    if(r<0) return FALSE;
    
    return TRUE;
}
-(BOOL)delfile:(NSString *)Path
{
    if(!self->handle){
        self->handle = ::onedrive_create("", 15, refresh_token_func, (__bridge void*)self);
    }
    int r = ::onedrive_delfile(self->handle, Path.UTF8String);
    if(r<0) return FALSE;
    
    return TRUE;
    
}

@end


