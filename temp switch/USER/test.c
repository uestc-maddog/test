#include "include.h" 	
#include <stdlib.h>
#include <stdio.h>

/*********************短信发"0", 开*********************/
/*********************短信发"1", 关*********************/
/***********短信发"2", 回复当前温测系统的温度***********/
/***********默认信息回发号码在 sim900a.c 文件中*********/
														    xn
void Sys_Init(void);								//系统初始化

int main(void)
{
	Sys_Init();										//系统初始化
	DS18B20_Link(); 								//连接DS18B20模块
	SIM900A_Link();									//连接GSM模块
	if(sim900a_send_cmd("AT+CMGF=1","OK",200))         return 1;	//设置文本模式 
	if(sim900a_send_cmd("AT+CSCS=\"UCS2\"","OK",200))  return 2;	//设置TE字符集为UCS2（2字节的UNICODE编码。还有UCS4，也就是4字节的UNICODE编码） 
	if(sim900a_send_cmd("AT+CSMP=17,0,2,25","OK",200)) return 3;	//设置短消息文本模式参数
	sim900a_data_ui(40,30);											//GSM信息界面UI
	LCD_Clear(BLUE);        //清屏
	while(1)
	{ 
		Show_Str_Mid(0,20,"基于GSM的实时温度查询系统",16,240); 				    	 	
		sim900a_sms_read();  
		LED0=!LED0;             //1.5s闪烁 
		delay_ms(1500);
		LCD_Fill(0,100,239,224,BLUE);
	} 	
}

void Sys_Init(void)
{
	u8 key,fontok=0;  
	Stm32_Clock_Init(9);	//系统时钟设置
	delay_init(72);			//延时初始化
	uart_init(72,115200); 	//串口1初始化 
	LCD_Init();				//初始化液晶 
	LED_Init();         	//LED初始化	 
	KEY_Init();				//按键初始化	 	
 	USART2_Init(36,115200);	//初始化串口2 
	mem_init();		        //初始化内部内存池	    
 	exfuns_init();			//为fatfs相关变量申请内存  
    f_mount(fs[0],"0:",1); 	//挂载SD卡  
	key = KEY_Scan(0);  
	fontok = font_init();		//检查字库是否OK
	if(fontok||key == KEY1_PRES)//需要更新字库 (KEY1按下或字库丢失）				 
	{
		LCD_Clear(WHITE);		   	//清屏
 		POINT_COLOR=RED;			//设置字体为红色	   	   	  
		LCD_ShowString(70,50,200,16,16,"GSM  TEST");
		while(SD_Initialize())		//检测SD卡
		{
			LCD_ShowString(60,70,200,16,16,"SD Card Failed!");
			delay_ms(200);
			LCD_Fill(60,70,200+60,70+16,WHITE);
			delay_ms(200);		    
		}								 						    
		LCD_ShowString(60,70,200,16,16,"SD Card OK");
		LCD_ShowString(60,90,200,16,16,"Font Updating...");
		key = update_font(20,110,16);    //更新字库
		while(key)  //更新失败		
		{			 		  
			LCD_ShowString(60,110,200,16,16,"Font Update Failed!");
			delay_ms(200);
			LCD_Fill(20,110,200+20,110+16,WHITE);
			delay_ms(200);		       
		} 		  
		LCD_ShowString(60,110,200,16,16,"Font Update Success!");
		delay_ms(1500);	
		LCD_Clear(WHITE);//清屏	       
	} 
}








