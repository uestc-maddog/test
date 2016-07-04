#ifndef __SIM900A_H__
#define __SIM900A_H__	 
#include "sys.h"

#define swap16(x) (x&0XFF)<<8|(x&0XFF00)>>8		//高低字节交换宏定义

 
void sim_send_sms(u8*phonenumber,u8*msg);
void sim_at_response(u8 mode);	
u8*  sim900a_check_cmd(u8 *str);
u8   sim900a_send_cmd(u8 *cmd,u8 *ack,u16 waittime);
u8   sim900a_chr2hex(u8 chr);
u8   sim900a_hex2chr(u8 hex);
void sim900a_unigbk_exchange(u8 *src,u8 *dst,u8 mode);
void sim900a_load_keyboard(u16 x,u16 y,u8 **kbtbl);
void sim900a_key_staset(u16 x,u16 y,u8 keyx,u8 sta);
u8   sim900a_sms_read(void);	                       //SIM900A读短信条数
void sim900a_sms_send(u8* phone, u8* message);	                   //发短信 
void sim900a_data_ui(u16 x,u16 y);	               //GSM信息界面UI
u8   sim900a_gsminfo_show(u16 x,u16 y);              //显示GSM模块信息
void SIM900A_Link(void);                           //连接GSM模块
void Show_Tem(void);			                   //显示温度信息
void Not_Show_Tem(void);                           //温测模块关,显示提示
char *reverse(char *s);                            //反转字符串
char *my_itoa(int n);
void sim900a_sms_send_test(void);                  //发短信测试 
void Get_Msg(int tem);				               //把温度转化为字符方便短信发出去

#endif





