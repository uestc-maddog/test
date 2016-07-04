#include "include.h" 	
#include <stdlib.h>
#include <stdio.h>

/*********************���ŷ�"0", ��*********************/
/*********************���ŷ�"1", ��*********************/
/***********���ŷ�"2", �ظ���ǰ�²�ϵͳ���¶�***********/
/***********Ĭ����Ϣ�ط������� sim900a.c �ļ���*********/
														    xn
void Sys_Init(void);								//ϵͳ��ʼ��

int main(void)
{
	Sys_Init();										//ϵͳ��ʼ��
	DS18B20_Link(); 								//����DS18B20ģ��
	SIM900A_Link();									//����GSMģ��
	if(sim900a_send_cmd("AT+CMGF=1","OK",200))         return 1;	//�����ı�ģʽ 
	if(sim900a_send_cmd("AT+CSCS=\"UCS2\"","OK",200))  return 2;	//����TE�ַ���ΪUCS2��2�ֽڵ�UNICODE���롣����UCS4��Ҳ����4�ֽڵ�UNICODE���룩 
	if(sim900a_send_cmd("AT+CSMP=17,0,2,25","OK",200)) return 3;	//���ö���Ϣ�ı�ģʽ����
	sim900a_data_ui(40,30);											//GSM��Ϣ����UI
	LCD_Clear(BLUE);        //����
	while(1)
	{ 
		Show_Str_Mid(0,20,"����GSM��ʵʱ�¶Ȳ�ѯϵͳ",16,240); 				    	 	
		sim900a_sms_read();  
		LED0=!LED0;             //1.5s��˸ 
		delay_ms(1500);
		LCD_Fill(0,100,239,224,BLUE);
	} 	
}

void Sys_Init(void)
{
	u8 key,fontok=0;  
	Stm32_Clock_Init(9);	//ϵͳʱ������
	delay_init(72);			//��ʱ��ʼ��
	uart_init(72,115200); 	//����1��ʼ�� 
	LCD_Init();				//��ʼ��Һ�� 
	LED_Init();         	//LED��ʼ��	 
	KEY_Init();				//������ʼ��	 	
 	USART2_Init(36,115200);	//��ʼ������2 
	mem_init();		        //��ʼ���ڲ��ڴ��	    
 	exfuns_init();			//Ϊfatfs��ر��������ڴ�  
    f_mount(fs[0],"0:",1); 	//����SD��  
	key = KEY_Scan(0);  
	fontok = font_init();		//����ֿ��Ƿ�OK
	if(fontok||key == KEY1_PRES)//��Ҫ�����ֿ� (KEY1���»��ֿⶪʧ��				 
	{
		LCD_Clear(WHITE);		   	//����
 		POINT_COLOR=RED;			//��������Ϊ��ɫ	   	   	  
		LCD_ShowString(70,50,200,16,16,"GSM  TEST");
		while(SD_Initialize())		//���SD��
		{
			LCD_ShowString(60,70,200,16,16,"SD Card Failed!");
			delay_ms(200);
			LCD_Fill(60,70,200+60,70+16,WHITE);
			delay_ms(200);		    
		}								 						    
		LCD_ShowString(60,70,200,16,16,"SD Card OK");
		LCD_ShowString(60,90,200,16,16,"Font Updating...");
		key = update_font(20,110,16);    //�����ֿ�
		while(key)  //����ʧ��		
		{			 		  
			LCD_ShowString(60,110,200,16,16,"Font Update Failed!");
			delay_ms(200);
			LCD_Fill(20,110,200+20,110+16,WHITE);
			delay_ms(200);		       
		} 		  
		LCD_ShowString(60,110,200,16,16,"Font Update Success!");
		delay_ms(1500);	
		LCD_Clear(WHITE);//����	       
	} 
}








