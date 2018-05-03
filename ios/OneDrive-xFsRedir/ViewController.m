//
//  ViewController.m
//  OneDrive-xFsRedir
//
//  Created by fanxiushu on 2018/4/17.
//  Copyright © 2018年 fanxiushu. All rights reserved.
//

#import "ViewController.h"
#import "onedrive-proxy.h"
#import <WebKit/WebKit.h>
#import "MBProgressHUD+show.h"

@interface ViewController () <UITableViewDataSource, UITableViewDelegate,
                    UINavigationControllerDelegate,UIImagePickerControllerDelegate>

@property(nonatomic) UITableView* tableView;

@property(nonatomic) onedrive_proxy* od;

@property(nonatomic) NSString*       CurrPath;
@property(nonatomic) NSMutableArray* CurrFiles;

@property(nonatomic)BOOL bTransAbort;

///
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    self.tableView=[[UITableView alloc]initWithFrame:self.view.frame style:UITableViewStylePlain];
    self.tableView.delegate=self;
    self.tableView.dataSource=self;
    [self.view addSubview:self.tableView];
    
    /////
    self.od = [[onedrive_proxy alloc] init];
    self.od.tableView = self.tableView;
    
    [self.od Auth:self.view auth_complete:^(NSString* errMsg){
        ////
        
        if(errMsg==nil){
            [self QueryDir:@"/"];
            ///
            UIButton* upfile_Btn=[UIButton buttonWithType:UIButtonTypeCustom];
            upfile_Btn.frame=CGRectMake(210, 30, 130, 40);
            [upfile_Btn setTitle:@"UpFile" forState:UIControlStateNormal];
            [upfile_Btn setTitle:@"Up..." forState:UIControlStateHighlighted];
            upfile_Btn.backgroundColor=[UIColor colorWithRed:0.5 green:0.3 blue:0.2 alpha:0.5];
            upfile_Btn.layer.masksToBounds=YES;
            upfile_Btn.layer.cornerRadius=5;
            [upfile_Btn addTarget:self action:@selector(upfile:) forControlEvents:UIControlEventTouchUpInside];
            [self.view addSubview:upfile_Btn];
            ///
        }
        else{
            UIAlertView *box = [[UIAlertView alloc] initWithTitle:@"Error" message:errMsg delegate:nil cancelButtonTitle:@"Close" otherButtonTitles:nil, nil];
                
            [box show];
            //////
        }
        
    }] ;
    
    /////
    
}

-(void)QueryDir:(NSString*)Path
{
    MBProgressHUD* hud = [[MBProgressHUD alloc]init];
    hud.labelText=@"Query Directory...";
    hud.mode = MBProgressHUDModeIndeterminate;
    hud.animationType = MBProgressHUDAnimationFade;
    
    [self.view addSubview:hud];
    [hud show:YES];
    NSLog(@"QueryDir [ %@ ]\n", Path);
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        self.CurrPath=Path;
        self.CurrFiles = [self.od query_directory:Path];
        //
        NSMutableDictionary* dict =[[NSMutableDictionary alloc] init];
        [dict setObject:@".." forKey:@"name"];
        [dict setObject:[NSNumber numberWithBool:TRUE] forKey:@"is_dir"];
        [dict setObject:[NSNumber numberWithInteger:0] forKey:@"size"];
        
        [self.CurrFiles insertObject:dict atIndex:0]; ///
        ///
        dispatch_async(dispatch_get_main_queue(), ^{
            hud.removeFromSuperViewOnHide = YES;
            [hud hide:YES];
            
            /////
            [self.tableView reloadData];
            /////
        });
        ////
    });
}

-(void)upfile:(UIButton*)button
{
    NSLog(@"*** UpFile....");
    UIImagePickerController* pick = [[UIImagePickerController alloc]init];
    pick.sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
    pick.allowsEditing=YES;
    pick.delegate=self;
    [self presentViewController:pick animated:YES completion:^(){
        
    }];
}
-(void)viewDidAppear:(BOOL)animated
{
    
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
//
-(void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary<NSString *,id> *)info
{
    NSLog(@"***=%@", info);
    ///
    [self dismissViewControllerAnimated:YES completion:nil];
    
    NSString* lpath = [[info objectForKey:@"UIImagePickerControllerImageURL"] absoluteString];
    const char* ff="file://";
    if(strncasecmp(lpath.UTF8String, ff, strlen(ff))==0){
        lpath = [lpath substringFromIndex:strlen(ff)];
    }
    NSString* name=[lpath lastPathComponent];
    NSString* rpath;
    if([self.CurrPath compare:@"/"]==NSOrderedSame)
        rpath=[NSString stringWithFormat:@"%@%@", self.CurrPath, name];
    else rpath=[NSString stringWithFormat:@"%@/%@", self.CurrPath,name];
    NSLog(@"lpath=%s, \nrpath=%@", lpath.UTF8String,rpath);
    
    /////
    MBProgressHUD* hud = [[MBProgressHUD alloc]init];
    hud.mode = MBProgressHUDModeDeterminateHorizontalBar;
    hud.animationType = MBProgressHUDAnimationFade;
    hud.removeFromSuperViewOnHide=YES;
    hud.labelText=@"Upload...";
    [self.view addSubview:hud];
    ////
    UIButton* btn=[UIButton buttonWithType:UIButtonTypeCustom];
    btn.frame=CGRectMake((self.view.frame.size.width-150)/2, self.view.frame.size.height/2-80, 150, 40);
    [btn setTitle:@"Up Cancel" forState:UIControlStateNormal];
    btn.backgroundColor=[UIColor colorWithRed:1.0 green:0.3 blue:0.3 alpha:0.6];
    [btn addTarget:self action:@selector(CancelTrans:) forControlEvents:UIControlEventTouchUpInside];
    
    [hud addSubview:btn];
    
    [hud show:YES];
    self.bTransAbort = NO;
    
 
    /////
    [self.od upfile:TRUE rpath:rpath lpath:lpath timeout:30 progress:^int(NSInteger curr, NSInteger total, BOOL is_complete, NSString *errMsg) {
        ///
        hud.progress = 1.0*curr/total;
        /////
        if(is_complete){
            ///
            [hud hide:YES];
            if(errMsg){// error
                ////
                UIAlertView *box = [[UIAlertView alloc] initWithTitle:@"Error" message:errMsg delegate:nil cancelButtonTitle:@"Close" otherButtonTitles:nil, nil];
                
                [box show];
                ////
            }
            else{
                [self QueryDir:self.CurrPath];
                ////
            }
        }
        if(self.bTransAbort)return -1; /// abort
        return 0;
        ///
    }];
    ///////
}

////tableview delegate

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [self.CurrFiles count];
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell* cell=nil;
    cell=[tableView dequeueReusableCellWithIdentifier:@"cell"];
    if(cell==nil){
       cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"cell"];
    }
    ////
    int row = indexPath.row;
    if(row>=self.CurrFiles.count){
        return nil;
    }
    NSDictionary* dict = [self.CurrFiles objectAtIndex:row];
    
    BOOL is_dir = [[dict objectForKey:@"is_dir"] boolValue];
    NSInteger size = [[dict objectForKey:@"size"] integerValue];
    NSString* name = [dict objectForKey:@"name"];
    cell.textLabel.text = [NSString stringWithFormat:@"%@%s" ,name ,(is_dir)?"/":"" ]; ///
    NSDateFormatter* fmt=[[NSDateFormatter alloc]init];
    [fmt setDateFormat:@"yyyy-mm:hh:ss"];
    NSString* str_date = [fmt stringFromDate:
                          [NSDate dateWithTimeIntervalSince1970:[[dict objectForKey:@"mtime"] integerValue]]];
    NSString* sz=nil;
    if(size<1024)sz = [NSString stringWithFormat:@"%ld Bytes", size ];
    else if(size>=1024&& size<1024*1024) sz = [NSString stringWithFormat:@"%.2f KB", size*1.0/1024];
    else if(size >=1024*1024&&size<1024*1024*1024) sz=[NSString stringWithFormat:@"%.2f MB", size*1.0/1024/1024];
    else sz=[NSString stringWithFormat:@"%.2f GB", size*1.0/1024/1024/1024];
    cell.detailTextLabel.text=[NSString stringWithFormat:@"%@ %@", str_date, sz];
    
    cell.imageView.image=[UIImage imageNamed:is_dir?@"folder.png":@"file.png"];
    CGSize itemSize = CGSizeMake(40, 40);
    UIGraphicsBeginImageContextWithOptions(itemSize, NO, UIScreen.mainScreen.scale);
    CGRect imageRect = CGRectMake(0.0, 0.0, itemSize.width, itemSize.height);
    [cell.imageView.image drawInRect:imageRect];
    cell.imageView.image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    
//    NSLog(@"Cell=%@",cell.textLabel.text);
    //////
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    if(indexPath.row >= self.CurrFiles.count || indexPath.row<0){
        return;
    }
    ////
    NSDictionary* dict = [self.CurrFiles objectAtIndex:indexPath.row];
    ////
    NSString* name=[dict objectForKey:@"name"];
    BOOL is_dir = [[dict objectForKey:@"is_dir"] boolValue];
    NSInteger size = [[dict objectForKey:@"size"] integerValue];
    
    ////
    NSString* dir=nil;
    if([self.CurrPath compare:@"/"]==NSOrderedSame)
        dir=[NSString stringWithFormat:@"%@%@",self.CurrPath,name];
    else dir = [NSString stringWithFormat:@"%@/%@", self.CurrPath, name];
    ////
    if(is_dir){
        ///
        if([name compare:@".."]==NSOrderedSame){
            // delete last Path Name
            dir = [self.CurrPath stringByDeletingLastPathComponent];
            /////
            NSLog(@"GoTo Parent [%@] -> [%@]", self.CurrPath, dir);
        }
        /////
        [self QueryDir:dir];
    }
    ///
    else{ // file
        [self downfile:dir size:size];
    }
    /////
    
}
-(void)downfile:(NSString*)path size:(NSInteger)size
{
    ///这里采用分块下载，但是实际上OneDrive云存储对随机读取的支持速度较差，因此如果是纯粹下载到文件，还是直接读取全部数据为好.

    int BLOCK_SIZE = 2048*1024;
    NSMutableData* data = [[NSMutableData alloc]initWithLength:BLOCK_SIZE];
    
    MBProgressHUD* hud = [[MBProgressHUD alloc]init];
    hud.mode = MBProgressHUDModeDeterminateHorizontalBar;
    hud.animationType = MBProgressHUDAnimationFade;
    hud.removeFromSuperViewOnHide=YES;
    hud.labelText=@"DownLoad Test...";
    [self.view addSubview:hud];
    ////
    UIButton* btn=[UIButton buttonWithType:UIButtonTypeCustom];
    btn.frame=CGRectMake((self.view.frame.size.width-160)/2, self.view.frame.size.height/2-80, 160, 40);
    [btn setTitle:@"Down Cancel" forState:UIControlStateNormal];
    btn.backgroundColor=[UIColor colorWithRed:1.0 green:0.3 blue:0.3 alpha:0.6];
    [btn addTarget:self action:@selector(CancelTrans:) forControlEvents:UIControlEventTouchUpInside];
    
    [hud addSubview:btn];
    
    [hud show:YES];
    self.bTransAbort = NO;
    
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        NSInteger __block offset = 0;
        NSString* __block err_msg=nil;
        ////
        while( offset < size && err_msg==nil && self.bTransAbort==NO ){
            [self.od offset_read:FALSE rpath:path offset:offset data:data
                 progress:^int(NSInteger curr, NSInteger total, BOOL is_complete, NSString *errMsg) {
                     ////
                     NSInteger cc = offset + curr ;
                     NSLog(@"Offset Read: off=%ld, size=%ld", (long)cc, (long)size);
                     if(is_complete){
                         offset += curr;
                         err_msg=errMsg;
                         //// save data
                         if(errMsg!=nil){ // success, save data
                             ///
                         }
                         //////
                     }
                     //// report progress
                     dispatch_async(dispatch_get_main_queue(), ^{
                         hud.progress=1.0*cc/size;
                         if(is_complete){
                             if(offset>=size || errMsg || self.bTransAbort==YES){
                                 [hud hide:YES];
                             }
                             if(errMsg){// error
                                 ////
                             }
                         }
                         /////
                     });
                     ////
                     if(self.bTransAbort==YES)return -1; // abort trans
                     return 0;
                     ////
            }];
            /////
        }
    });
    /////
}
-(void)CancelTrans:(UIButton*)button
{
    NSLog(@"Cancel Trans");
    self.bTransAbort=YES;
}

-(void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath *)indexPath
{
    NSLog(@"*** delete...");
    /////
    if(indexPath.row >= self.CurrFiles.count || indexPath.row<0){
        return;
    }
    ////
    NSDictionary* dict = [self.CurrFiles objectAtIndex:indexPath.row];
    NSString* name=[dict objectForKey:@"name"];
    
    NSString* dir=nil;
    if([self.CurrPath compare:@"/"]==NSOrderedSame)
        dir=[NSString stringWithFormat:@"%@%@",self.CurrPath,name];
    else dir = [NSString stringWithFormat:@"%@/%@", self.CurrPath, name];
    /////
    BOOL ret =[self.od delfile:dir];
    if(ret){
        [self QueryDir:self.CurrPath];
    }
}

@end
