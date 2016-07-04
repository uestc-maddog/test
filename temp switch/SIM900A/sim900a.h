#ifndef __SIM900A_H__
#define __SIM900A_H__	 
#include "sys.h"

#define swap16(x) (x&0XFF)<<8|(x&0XFF00)>>8		//�ߵ��ֽڽ����궨��

 
void sim_send_sms(u8*phonenumber,u8*msg);
void sim_at_response(u8 mode);	
u8*  sim900a_check_cmd(u8 *str);
u8   sim900a_send_cmd(u8 *cmd,u8 *ack,u16 waittime);
u8   sim900a_chr2hex(u8 chr);
u8   sim900a_hex2chr(u8 hex);
void sim900a_unigbk_exchange(u8 *src,u8 *dst,u8 mode);
void sim900a_load_keyboard(u16 x,u16 y,u8 **kbtbl);
void sim900a_key_staset(u16 x,u16 y,u8 keyx,u8 sta);
u8   sim900a_sms_read(void);	                       //SIM900A����������
void sim900a_sms_send(u8* phone, u8* message);	                   //������ 
void sim900a_data_ui(u16 x,u16 y);	               //GSM��Ϣ����UI
u8   sim900a_gsminfo_show(u16 x,u16 y);              //��ʾGSMģ����Ϣ
void SIM900A_Link(void);                           //����GSMģ��
void Show_Tem(void);			                   //��ʾ�¶���Ϣ
void Not_Show_Tem(void);                           //�²�ģ���,��ʾ��ʾ
char *reverse(char *s);                            //��ת�ַ���
char *my_itoa(int n);
void sim900a_sms_send_test(void);                  //�����Ų��� 
void Get_Msg(int tem);				               //���¶�ת��Ϊ�ַ�������ŷ���ȥ

#endif





