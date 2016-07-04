#include "sim900a.h"
#include "usart.h"		
#include "delay.h"	
#include "led.h"   	 
#include "key.h"	 	 	 	 	 
#include "lcd.h" 
#include "dma.h" 	  
#include "flash.h" 	 
#include "touch.h" 	 
#include "malloc.h"
#include "string.h"    
#include "text.h"		
#include "usart2.h" 
#include "ff.h"
//短信发送内容(70个字[UCS2的时候,1个字符/数字都算1个字])
int temperature;
u8 flag = 0, Tem_off = 0;		   //flag--温度正负标志 1-负 0-正\\\\\\\Tem_off--温测模块开关 1-关 0-开。。。
u8 *S1;	                           //将msg + 温度 + du储存于S1便于回发信息
u8 *phonebuf = "18200390109"; 	   //默认信息回发号码

//将收到的AT指令应答数据返回给电脑串口
//mode:0,不清零USART2_RX_STA;
//     1,清零USART2_RX_STA;
void sim_at_response(u8 mode)
{
	if(USART2_RX_STA&0X8000)		//接收到一次数据了
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//添加结束符
		printf("%s",USART2_RX_BUF);	//发送到串口
		if(mode)USART2_RX_STA=0;
	} 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//ATK-SIM900A 各项测试(拨号测试、短信测试、GPRS测试)共用代码

//sim900a发送命令后,检测接收到的应答
//str:期待的应答结果
//返回值:0,没有得到期待的应答结果
//    其他,期待应答结果的位置(str的位置)
u8* sim900a_check_cmd(u8 *str)
{
	char *strx=0;
	if(USART2_RX_STA&0X8000)		//接收到一次数据了
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//添加结束符
		strx=strstr((const char*)USART2_RX_BUF,(const char*)str);
	} 
	return (u8*)strx;
}
//向sim900a发送命令
//cmd:发送的命令字符串(不需要添加回车了),当cmd<0XFF的时候,发送数字(比如发送0X1A),大于的时候发送字符串.
//ack:期待的应答结果,如果为空,则表示不需要等待应答
//waittime:等待时间(单位:10ms)
//返回值:0,发送成功(得到了期待的应答结果)
//       1,发送失败
u8 sim900a_send_cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res=0; 
	USART2_RX_STA=0;
	if((u32)cmd<=0XFF)
	{
		while(DMA1_Channel7->CNDTR!=0);	//等待通道7传输完成   
		USART2->DR=(u32)cmd;
	}else u2_printf("%s\r\n",cmd);//发送命令
	if(ack&&waittime)		//需要等待应答
	{
		while(--waittime)	//等待倒计时
		{
			delay_ms(10);
			if(USART2_RX_STA&0X8000)//接收到期待的应答结果
			{
				if(sim900a_check_cmd(ack))break;//得到有效数据 
				USART2_RX_STA=0;
			} 
		}
		if(waittime==0)res=1; 
	}
	return res;
} 
//将1个字符转换为16进制数字
//chr:字符,0~9/A~F/a~F
//返回值:chr对应的16进制数值
u8 sim900a_chr2hex(u8 chr)
{
	if(chr>='0'&&chr<='9')return chr-'0';
	if(chr>='A'&&chr<='F')return (chr-'A'+10);
	if(chr>='a'&&chr<='f')return (chr-'a'+10); 
	return 0;
}
//将1个16进制数字转换为字符
//hex:16进制数字,0~15;
//返回值:字符
u8 sim900a_hex2chr(u8 hex)
{
	if(hex<=9)return hex+'0';
	if(hex>=10&&hex<=15)return (hex-10+'A'); 
	return '0';
}
//unicode gbk 转换函数
//src:输入字符串
//dst:输出(uni2gbk时为gbk内码,gbk2uni时,为unicode字符串)
//mode:0,unicode到gbk转换;
//     1,gbk到unicode转换;
void sim900a_unigbk_exchange(u8 *src,u8 *dst,u8 mode)
{
	u16 temp; 
	u8 buf[2];
	if(mode)//gbk 2 unicode
	{
		while(*src!=0)
		{
			if(*src<0X81)	//非汉字
			{
				temp=(u16)ff_convert((WCHAR)*src,1);
				src++;
			}else 			//汉字,占2个字节
			{
				buf[1]=*src++;
				buf[0]=*src++; 
				temp=(u16)ff_convert((WCHAR)*(u16*)buf,1); 
			}
			*dst++=sim900a_hex2chr((temp>>12)&0X0F);
			*dst++=sim900a_hex2chr((temp>>8)&0X0F);
			*dst++=sim900a_hex2chr((temp>>4)&0X0F);
			*dst++=sim900a_hex2chr(temp&0X0F);
		}
	}else	//unicode 2 gbk
	{ 
		while(*src!=0)
		{
			buf[1]=sim900a_chr2hex(*src++)*16;
			buf[1]+=sim900a_chr2hex(*src++);
			buf[0]=sim900a_chr2hex(*src++)*16;
			buf[0]+=sim900a_chr2hex(*src++);
 			temp=(u16)ff_convert((WCHAR)*(u16*)buf,0);
			if(temp<0X80){*dst=temp;dst++;}
			else {*(u16*)dst=swap16(temp);dst+=2;}
		} 
	}
	*dst=0;//添加结束符
} 
/////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//短信测试部分代码

//SIM900A读短信条数
u8 sim900a_sms_read(void)
{ 
	u8 *p, *p1, *p2, p3[3], *p4;
	u8 *msgindex;			 //要读的短信编号
	int i, Num_new, Num_old, Num_help = 0;
	u8 msgmaxnum=0;		     //短信最大条数
	p=mymalloc(200);//申请200个字节的内存	Show_Str(55,80,200,16,"当前信息量:",16,0); 	
	while(1)
	{
		LED0 = !LED0;             //200ms闪烁 
		delay_ms(200);
		if(Tem_off == 0)  Show_Tem();	                    //温测模块开启
		else 			  Not_Show_Tem();				    //温测模块关闭
		sim900a_gsminfo_show(40,225);						//GSM模块信息更新(信号质量,电池电量,日期时间)
		if(sim900a_send_cmd("AT+CPMS?","+CPMS:",200)==0)	//查询优选消息存储器
		{ 
			p1=(u8*)strstr((const char*)(USART2_RX_BUF),","); 
			p2=(u8*)strstr((const char*)(p1+1),",");
			p2[0]='/'; 
			if(p2[3]==',')//小于64K SIM卡，最多存储几十条短信
			{
				msgmaxnum=(p2[1]-'0')*10+p2[2]-'0';		//获取最大存储短信条数
				p2[3]=0;
			}else //如果是64K SIM卡，则能存储100条以上的信息
			{
				msgmaxnum=(p2[1]-'0')*100+(p2[2]-'0')*10+p2[3]-'0';//获取最大存储短信条数
				p2[4]=0;
			}
			sprintf((char*)p,"%s",p1+1);
			Show_Str(150,80,200,16,p,16,0); 	   //当前信息量
			for(i = 0; i < 3; i++) 
			{
				if( *(p+i) == 47 ) break;		   //  '/' == 47
				p3[i] = *(p+i);
			}
			p3[i] = '\0';				           //最新信息编号
			Num_new = atoi((char *)p3);
			if(Num_help == 0)  Num_old = Num_new;  //第一次获取当前信息总数
			else 
			{
				if(Num_new > Num_old) 		     //收到新信息
				{
					Num_old = Num_new;                                                                                                   
					Show_Str_Mid(0,100,"收到信息！",16,240);
					msgindex = my_itoa(Num_new);
					sprintf((char*)p,"AT+CMGR=%s",msgindex);
					if(sim900a_send_cmd(p,"+CMGR:",200)==0)             //读取短信，方便回传信息
					{
						Show_Str(15,120,200,12,"来自:",12,0); 
						Show_Str(140,120,200,12,"内容:",12,0);
						Show_Str(15,132,200,12,"接收时间:",12,0);
						p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
						p2=(u8*)strstr((const char*)(p1+2),"\"");
						p2[0]=0;               //加入结束符
						sim900a_unigbk_exchange(p1+2,p,0);		       	//将unicode字符转换为gbk码
						Show_Str(45,120,200,12,p,12,0);	    	        //显示电话号码
						//strcpy(p4, p);

						p1=(u8*)strstr((const char*)(p2+1),"/");
						p2=(u8*)strstr((const char*)(p1),"+");
						p2[0]=0;              //加入结束符
						Show_Str(69,132,180,75,p1-2,12,0);	    	//显示接收时间  
						p1=(u8*)strstr((const char*)(p2+1),"\r");	//寻找回车符
						sim900a_unigbk_exchange(p1+2,p,0);			//将unicode字符转换为gbk码
						Show_Str(170,120,200,12,p,12,0);			//显示短信内容
						if(strcmp(p, "0") == 0)	         //短信发"0", 开
						{	
							Tem_off = 0;   
							goto out;
						}
						else if(strcmp(p, "1") == 0)	 //短信发"1", 关
						{
							Tem_off = 1;  
							goto out;
						}
						else if(strcmp(p, "2") == 0)     //短信发"2", 回复当前温测系统的温度  
						{
							if(Tem_off == 0)
							{
								Get_Msg(temperature);
								sim900a_sms_send(p4, S1);
							}
						}      
					}
					
				out:myfree(p);;
					return 1;
				}																							  
			}
			Num_help = 1;                 
		}
	}
	myfree(p); 
	return 0;
}

//SIM900A发短信 
void sim900a_sms_send(u8* phone, u8* message)
{
	u8 *pp,*p1,*p2, flag = 0;
	u8 smssendsta=0;		//短信发送状态,0,等待发送;1,发送失败;2,发送成功 
	pp=mymalloc(100);    	//申请100个字节的内存,用于存放电话号码的unicode字符串
	p1=mymalloc(300);       //申请300个字节的内存,用于存放短信的unicode字符串
	p2=mymalloc(100);       //申请100个字节的内存 存放：AT+CMGS=p1 
	Show_Str(30,150,200,16,"发送给:",16,0);  	 
	Show_Str(30,170,200,16,"状态:",16,0);
	Show_Str(30,190,200,12,"内容:",12,0);  
	Show_Str(70,170,170,90,"等待发送",16,0);            //显示状态	
	Show_Str(66,190,170,90,message,12,0);               //显示短信内容	
	
	Show_Str(84,150,176,16,phonebuf,16,0);
	Show_Str(70,170,170,90,"正在发送",16,0);			//显示正在发送		
	smssendsta=1;		 
	sim900a_unigbk_exchange(phonebuf,pp,1);				//将电话号码转换为unicode字符串
	sim900a_unigbk_exchange(message,p1,1);              //将短信内容转换为unicode字符串.
	sprintf((char*)p2,"AT+CMGS=\"%s\"",pp); 
			
	while(1)					 //执行发送短信
	{
		if(sim900a_send_cmd(p2,">",500)==0)					//发送短信命令+电话号码
		{ 		 				 													 
			u2_printf("%s",p1);		 						//发送短信内容到GSM模块 
			if(sim900a_send_cmd((u8*)0X1A,"+CMGS:",1500)==0) smssendsta=2;//发送结束符,等待发送完成(最长等待10秒钟,因为短信长了的话,等待时间会长一些)
		}  
		if(smssendsta==1)
		{	
			Show_Str(70,170,170,90,"发送失败",16,0);	//显示状态
		}
		else 
		{
			Show_Str(70,170,170,90,"发送成功",16,0);	//显示状态
			flag = 1;
		} 
		USART2_RX_STA=0;
		if(USART2_RX_STA&0X8000)    sim_at_response(1); //检查从GSM模块接收到的数据 
		if(flag == 1)									//发送成功退出发送
		{	
			flag = 0;
			break;	
		}
		else											//发送未成功，继续发送
		{
			delay_ms(800);								//清除“发送失败”状态
			LCD_Fill(70,170,239,186,BLUE);	
			Show_Str(70,170,170,90,"重新发送",16,0);	//显示正在发送
		}
	}
	myfree(pp);
	myfree(p1);
	myfree(p2); 	
} 


//GSM信息界面UI
void sim900a_data_ui(u16 x,u16 y)
{
	u8 *p,*p1,*p2; 
	p = mymalloc(50);//申请50个字节的内存
	LCD_Clear(BLUE);      //清屏
	POINT_COLOR = WHITE;
	BACK_COLOR = RED;
	Show_Str_Mid(0,y,"基于GSM的实时温度查询系统",16,240);  			    	 
	Show_Str(x,y+65,200,16,"以下为GSM模块信息:",16,0); 				    	 	
	USART2_RX_STA = 0;
	if(sim900a_send_cmd("AT+CGMI","OK",200)==0)				     //查询制造商名称
	{ 
		p1 = (u8*)strstr((const char*)(USART2_RX_BUF+2),"\r\n"); //搜索一个字符串在另一个字符串中的第一次出现。找到所搜索的字符串，则该函数返回第一次匹配的字符串的地址
		p1[0] = 0;//加入结束符
		sprintf((char*)p,"制造商:%s",USART2_RX_BUF+2);			 //字符串格式化命令，把格式化的数据写入某个字符串中
		Show_Str(x,y+110,200,16,p,16,0);
		USART2_RX_STA=0;		
	} 
	if(sim900a_send_cmd("AT+CGMM","OK",200)==0)                  //查询模块名字
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF+2),"\r\n"); 	 //\r是回车符,\n是换行符
		p1[0] = 0;//加入结束符
		sprintf((char*)p,"模块型号:%s",USART2_RX_BUF+2);
		Show_Str(x,y+130,200,16,p,16,0);
		USART2_RX_STA = 0;		
	} 
	if(sim900a_send_cmd("AT+CGSN","OK",200)==0)//查询产品序列号
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF+2),"\r\n");   //查找回车
		p1[0]=0;//加入结束符 
		sprintf((char*)p,"序列号:%s",USART2_RX_BUF+2);
		Show_Str(x,y+150,200,16,p,16,0);
		USART2_RX_STA=0;		
	}
	if(sim900a_send_cmd("AT+CNUM","+CNUM",200)==0)			//查询本机号码
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
		p2=(u8*)strstr((const char*)(p1+2),"\"");
		p2[0]=0;//加入结束符
		sprintf((char*)p,"本机号码:%s",p1+2);
		Show_Str(x,y+170,200,16,p,16,0);
		USART2_RX_STA=0;		
	}
	myfree(p); 										//释放内存(外部调用)
	Show_Str(40,230,200,16,"GSM模块工作正常!",16,0);
	delay_ms(1000);
	Show_Str(40,250,200,16,"立即进入监测状态.",16,0);
	delay_ms(1000);
	Show_Str(40,250,200,16,"立即进入监测状态..",16,0);
	delay_ms(1000);
	Show_Str(40,250,200,16,"立即进入监测状态...",16,0);
	delay_ms(1000);
}
//GSM信息显示(信号质量,电池电量,日期时间)
//返回值:0,正常
//    其他,错误代码
u8 sim900a_gsminfo_show(u16 x,u16 y)
{
	u8 *p,*p1,*p2;
	u8 res=0;
	p=mymalloc(50);//申请50个字节的内存	
	USART2_RX_STA=0;
	if(sim900a_send_cmd("AT+CPIN?","OK",200))res|=1<<0;	//查询SIM卡是否在位 
	USART2_RX_STA=0;  				    	
	if(sim900a_send_cmd("AT+COPS?","OK",200)==0)		//查询运营商名字
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\""); 
		if(p1)//有有效数据
		{
			p2=(u8*)strstr((const char*)(p1+1),"\"");
			p2[0]=0;//加入结束符			
			sprintf((char*)p,"运营商:%s",p1+1);
			Show_Str(x,y,200,16,p,16,0);
		} 
		USART2_RX_STA=0;		
	}else res|=1<<1;
	if(sim900a_send_cmd("AT+CSQ","+CSQ:",200)==0)		//查询信号质量
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		p2=(u8*)strstr((const char*)(p1),",");
		p2[0]=0;//加入结束符
		sprintf((char*)p,"信号质量:%s",p1+2);
		Show_Str(x,y+20,200,16,p,16,0);
		USART2_RX_STA=0;		
	}else res|=1<<2;
	if(sim900a_send_cmd("AT+CBC","+CBC:",200)==0)		   //查询电池电量
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
		p2=(u8*)strstr((const char*)(p1+1),",");
		p2[0]=0;p2[5]=0;//加入结束符
		sprintf((char*)p,"电池电量:%s%%  %smV",p1+1,p2+1);
		Show_Str(x,y+40,200,16,p,16,0);
		USART2_RX_STA=0;		
	}else res|=1<<3; 
//	if(sim900a_send_cmd("AT+CCLK?","+CCLK:",200)==0)		//查询日期时间
//	{ 
//		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\"");
//		p2=(u8*)strstr((const char*)(p1+1),":");
//		p2[3]=0;//加入结束符
//		sprintf((char*)p,"日期时间:%s",p1+1);
//		Show_Str(x,y+60,200,16,p,16,0);
//		USART2_RX_STA=0;		
//	}else res|=1<<4; 
	myfree(p); 
	return res;
} 

void SIM900A_Link(void)        //连接GSM模块
{
	u8 temp = 0;
	LCD_Clear(BLUE);    //清屏
	POINT_COLOR=WHITE;
	BACK_COLOR=RED;
	Show_Str_Mid(0,50,"基于GSM的实时温度查询系统",16,240); 
	while(sim900a_send_cmd("AT","OK",100))   //检测是否应答AT指令 
	{
		Show_Str(40,85,200,16,"未检测到GSM模块!!!",16,0);
		delay_ms(800);
		LCD_Fill(0,85,240,85+16,BLUE);
		Show_Str(40,85,200,16,"尝试连接GSM模块...",16,0);
		delay_ms(800); 
	} 	
	temp += sim900a_send_cmd("ATE0","OK",200);      //不回显  
}
void Show_Tem(void)	      //温测模块开,显示温度信息
{
	LCD_Fill(0,40,239,76,BLUE); 
	Show_Str_Mid(0,40,"温测模块：开",16,240);
	LCD_ShowString(60,60,200,16,16,"Temp:   .  C");	
	temperature = DS18B20_Get_Temp();	
	if(temperature<0)
	{
		LCD_ShowChar(60+40,60,'-',16,0);			//显示负号
		temperature=-temperature;					//转为正数
		flag = 1;
	}
	else LCD_ShowChar(60+40,60,' ',16,0);			//去掉负号
	LCD_ShowNum(60+40+ 8,60,temperature / 10,2,16);	//显示整数部分	    
	LCD_ShowNum(60+40+32,60,temperature % 10,1,16);	//显示小数部分 
}
void Not_Show_Tem(void)	   //温测模块关,显示提示
{
	LCD_Fill(0,40,239,76,BLUE); 
	Show_Str_Mid(0,40,"温测模块：关",16,240);
}
void Get_Msg(int tem)	//把温度转化为字符方便短信发出去
{	
	char S2[10];
	if(flag == 1) 
	{
		S2[0] = '-';
		S2[1] = 0;
		flag = 0; 
	}
	else  
	{
		S2[0] = '+';
		S2[1] = 0;
	}
	S1 = my_itoa(tem / 10);
	strcat(S2, S1);
	if( (tem/10) > 99)		  //3位数
	{
		S2[4] = '.';
		S2[5] = 0;
	}
	else					  //2位数
	{
		S2[3] = '.';
		S2[4] = 0;
	}
	S1 = my_itoa(tem % 10);
	strcat(S2, S1);
	strcat(S2, "度。");								 //S2为  XXX.X度。	
	strcpy(S1, "系统当前温度为");					 //S1为  系统当前温度
	strcat(S1, S2);
	//Show_Str(16,180,240,90,S1,16,0);                 //显示短信内容
}
//反转字符串  
char *reverse(char *s)  
{  
    char temp;  
    char *p = s;    //p指向s的头部  
    char *q = s;    //q指向s的尾部  
    while(*q)  
        ++q;  
    q--;  
  
    //交换移动指针，直到p和q交叉  
    while(q > p)  
    {  
        temp = *p;  
        *p++ = *q;  
        *q-- = temp;  
    }  
    return s;  
}  
  
/* 
 * 功能：整数转换为字符串 
 * char s[] 的作用是存储整数的每一位 
 */  
char *my_itoa(int n)  
{  
    int i = 0,isNegative = 0;  
    static char s[100];      //必须为static变量，或者是全局变量  
    if((isNegative = n) < 0) //如果是负数，先转为正数  
    {  
        n = -n;  
    }  
    do      //从各位开始变为字符，直到最高位，最后应该反转  
    {  
        s[i++] = n%10 + '0';  
        n = n/10;  
    }while(n > 0);  
  
    if(isNegative < 0)   //如果是负数，补上负号  
    {  
        s[i++] = '-';  
    }  
    s[i] = '\0';    //最后加上字符串结束符  
    return reverse(s);    
}  

//SIM900A发短信测试 
void sim900a_sms_send_test(void)
{
	u8 *p,*p1,*p2;
	u8 smssendsta = 0;		//短信发送状态,0,等待发送;1,发送失败;2,发送成功 
	p  = mymalloc(100);	//申请100个字节的内存,用于存放电话号码的unicode字符串
	p1 = mymalloc(300);//申请300个字节的内存,用于存放短信的unicode字符串
	p2 = mymalloc(100);//申请100个字节的内存 存放：AT+CMGS=p1 
	LCD_Clear(BLUE);  
	POINT_COLOR=WHITE;
	BACK_COLOR=RED;
	Show_Str_Mid(0,30,"ATK-SIM900A 发短信测试",16,240);				    	 
	Show_Str(30,50,200,16,"发送给:",16,0); 
	Show_Str(84,50,176,16,phonebuf,16,0);	 
	Show_Str(30,70,200,16,"状态:",16,0);
	Show_Str(30,90,200,16,"内容:",16,0);  
	Show_Str(30+40,70,170,90,"等待发送",16,0);//显示状态	
	Show_Str(30+40,90,170,90,S1,16,0);//显示短信内容		
						 //执行发送短信
	Show_Str(30+40,70,170,90,"正在发送",16,0);			//显示正在发送		
	smssendsta=1;		 
	sim900a_unigbk_exchange(phonebuf,p,1);				//将电话号码转换为unicode字符串
	sim900a_unigbk_exchange(S1,p1,1);//将短信内容转换为unicode字符串.
	sprintf((char*)p2,"AT+CMGS=\"%s\"",p); 
	if(sim900a_send_cmd(p2,">",200)==0)					//发送短信命令+电话号码
	{ 		 				 													 
		u2_printf("%s",p1);		 						//发送短信内容到GSM模块 
		if(sim900a_send_cmd((u8*)0X1A,"+CMGS:",2000)==0)smssendsta=2;//发送结束符,等待发送完成(最长等待10秒钟,因为短信长了的话,等待时间会长一些)
	}  
	if(smssendsta==1)Show_Str(30+40,70,170,90,"发送失败",16,0);	//显示状态
	else Show_Str(30+40,70,170,90,"发送成功",16,0);				//显示状态	
	USART2_RX_STA=0;
	if(USART2_RX_STA&0X8000)  sim_at_response(1);//检查从GSM模块接收到的数据 
	myfree(p);
	myfree(p1);
	myfree(p2); 
} 
