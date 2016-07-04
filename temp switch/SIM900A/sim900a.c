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
//���ŷ�������(70����[UCS2��ʱ��,1���ַ�/���ֶ���1����])
int temperature;
u8 flag = 0, Tem_off = 0;		   //flag--�¶�������־ 1-�� 0-��\\\\\\\Tem_off--�²�ģ�鿪�� 1-�� 0-��������
u8 *S1;	                           //��msg + �¶� + du������S1���ڻط���Ϣ
u8 *phonebuf = "18200390109"; 	   //Ĭ����Ϣ�ط�����

//���յ���ATָ��Ӧ�����ݷ��ظ����Դ���
//mode:0,������USART2_RX_STA;
//     1,����USART2_RX_STA;
void sim_at_response(u8 mode)
{
	if(USART2_RX_STA&0X8000)		//���յ�һ��������
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//��ӽ�����
		printf("%s",USART2_RX_BUF);	//���͵�����
		if(mode)USART2_RX_STA=0;
	} 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//ATK-SIM900A �������(���Ų��ԡ����Ų��ԡ�GPRS����)���ô���

//sim900a���������,�����յ���Ӧ��
//str:�ڴ���Ӧ����
//����ֵ:0,û�еõ��ڴ���Ӧ����
//    ����,�ڴ�Ӧ������λ��(str��λ��)
u8* sim900a_check_cmd(u8 *str)
{
	char *strx=0;
	if(USART2_RX_STA&0X8000)		//���յ�һ��������
	{ 
		USART2_RX_BUF[USART2_RX_STA&0X7FFF]=0;//��ӽ�����
		strx=strstr((const char*)USART2_RX_BUF,(const char*)str);
	} 
	return (u8*)strx;
}
//��sim900a��������
//cmd:���͵������ַ���(����Ҫ��ӻس���),��cmd<0XFF��ʱ��,��������(���緢��0X1A),���ڵ�ʱ�����ַ���.
//ack:�ڴ���Ӧ����,���Ϊ��,���ʾ����Ҫ�ȴ�Ӧ��
//waittime:�ȴ�ʱ��(��λ:10ms)
//����ֵ:0,���ͳɹ�(�õ����ڴ���Ӧ����)
//       1,����ʧ��
u8 sim900a_send_cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res=0; 
	USART2_RX_STA=0;
	if((u32)cmd<=0XFF)
	{
		while(DMA1_Channel7->CNDTR!=0);	//�ȴ�ͨ��7�������   
		USART2->DR=(u32)cmd;
	}else u2_printf("%s\r\n",cmd);//��������
	if(ack&&waittime)		//��Ҫ�ȴ�Ӧ��
	{
		while(--waittime)	//�ȴ�����ʱ
		{
			delay_ms(10);
			if(USART2_RX_STA&0X8000)//���յ��ڴ���Ӧ����
			{
				if(sim900a_check_cmd(ack))break;//�õ���Ч���� 
				USART2_RX_STA=0;
			} 
		}
		if(waittime==0)res=1; 
	}
	return res;
} 
//��1���ַ�ת��Ϊ16��������
//chr:�ַ�,0~9/A~F/a~F
//����ֵ:chr��Ӧ��16������ֵ
u8 sim900a_chr2hex(u8 chr)
{
	if(chr>='0'&&chr<='9')return chr-'0';
	if(chr>='A'&&chr<='F')return (chr-'A'+10);
	if(chr>='a'&&chr<='f')return (chr-'a'+10); 
	return 0;
}
//��1��16��������ת��Ϊ�ַ�
//hex:16��������,0~15;
//����ֵ:�ַ�
u8 sim900a_hex2chr(u8 hex)
{
	if(hex<=9)return hex+'0';
	if(hex>=10&&hex<=15)return (hex-10+'A'); 
	return '0';
}
//unicode gbk ת������
//src:�����ַ���
//dst:���(uni2gbkʱΪgbk����,gbk2uniʱ,Ϊunicode�ַ���)
//mode:0,unicode��gbkת��;
//     1,gbk��unicodeת��;
void sim900a_unigbk_exchange(u8 *src,u8 *dst,u8 mode)
{
	u16 temp; 
	u8 buf[2];
	if(mode)//gbk 2 unicode
	{
		while(*src!=0)
		{
			if(*src<0X81)	//�Ǻ���
			{
				temp=(u16)ff_convert((WCHAR)*src,1);
				src++;
			}else 			//����,ռ2���ֽ�
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
	*dst=0;//��ӽ�����
} 
/////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//���Ų��Բ��ִ���

//SIM900A����������
u8 sim900a_sms_read(void)
{ 
	u8 *p, *p1, *p2, p3[3], *p4;
	u8 *msgindex;			 //Ҫ���Ķ��ű��
	int i, Num_new, Num_old, Num_help = 0;
	u8 msgmaxnum=0;		     //�����������
	p=mymalloc(200);//����200���ֽڵ��ڴ�	Show_Str(55,80,200,16,"��ǰ��Ϣ��:",16,0); 	
	while(1)
	{
		LED0 = !LED0;             //200ms��˸ 
		delay_ms(200);
		if(Tem_off == 0)  Show_Tem();	                    //�²�ģ�鿪��
		else 			  Not_Show_Tem();				    //�²�ģ��ر�
		sim900a_gsminfo_show(40,225);						//GSMģ����Ϣ����(�ź�����,��ص���,����ʱ��)
		if(sim900a_send_cmd("AT+CPMS?","+CPMS:",200)==0)	//��ѯ��ѡ��Ϣ�洢��
		{ 
			p1=(u8*)strstr((const char*)(USART2_RX_BUF),","); 
			p2=(u8*)strstr((const char*)(p1+1),",");
			p2[0]='/'; 
			if(p2[3]==',')//С��64K SIM�������洢��ʮ������
			{
				msgmaxnum=(p2[1]-'0')*10+p2[2]-'0';		//��ȡ���洢��������
				p2[3]=0;
			}else //�����64K SIM�������ܴ洢100�����ϵ���Ϣ
			{
				msgmaxnum=(p2[1]-'0')*100+(p2[2]-'0')*10+p2[3]-'0';//��ȡ���洢��������
				p2[4]=0;
			}
			sprintf((char*)p,"%s",p1+1);
			Show_Str(150,80,200,16,p,16,0); 	   //��ǰ��Ϣ��
			for(i = 0; i < 3; i++) 
			{
				if( *(p+i) == 47 ) break;		   //  '/' == 47
				p3[i] = *(p+i);
			}
			p3[i] = '\0';				           //������Ϣ���
			Num_new = atoi((char *)p3);
			if(Num_help == 0)  Num_old = Num_new;  //��һ�λ�ȡ��ǰ��Ϣ����
			else 
			{
				if(Num_new > Num_old) 		     //�յ�����Ϣ
				{
					Num_old = Num_new;                                                                                                   
					Show_Str_Mid(0,100,"�յ���Ϣ��",16,240);
					msgindex = my_itoa(Num_new);
					sprintf((char*)p,"AT+CMGR=%s",msgindex);
					if(sim900a_send_cmd(p,"+CMGR:",200)==0)             //��ȡ���ţ�����ش���Ϣ
					{
						Show_Str(15,120,200,12,"����:",12,0); 
						Show_Str(140,120,200,12,"����:",12,0);
						Show_Str(15,132,200,12,"����ʱ��:",12,0);
						p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
						p2=(u8*)strstr((const char*)(p1+2),"\"");
						p2[0]=0;               //���������
						sim900a_unigbk_exchange(p1+2,p,0);		       	//��unicode�ַ�ת��Ϊgbk��
						Show_Str(45,120,200,12,p,12,0);	    	        //��ʾ�绰����
						//strcpy(p4, p);

						p1=(u8*)strstr((const char*)(p2+1),"/");
						p2=(u8*)strstr((const char*)(p1),"+");
						p2[0]=0;              //���������
						Show_Str(69,132,180,75,p1-2,12,0);	    	//��ʾ����ʱ��  
						p1=(u8*)strstr((const char*)(p2+1),"\r");	//Ѱ�һس���
						sim900a_unigbk_exchange(p1+2,p,0);			//��unicode�ַ�ת��Ϊgbk��
						Show_Str(170,120,200,12,p,12,0);			//��ʾ��������
						if(strcmp(p, "0") == 0)	         //���ŷ�"0", ��
						{	
							Tem_off = 0;   
							goto out;
						}
						else if(strcmp(p, "1") == 0)	 //���ŷ�"1", ��
						{
							Tem_off = 1;  
							goto out;
						}
						else if(strcmp(p, "2") == 0)     //���ŷ�"2", �ظ���ǰ�²�ϵͳ���¶�  
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

//SIM900A������ 
void sim900a_sms_send(u8* phone, u8* message)
{
	u8 *pp,*p1,*p2, flag = 0;
	u8 smssendsta=0;		//���ŷ���״̬,0,�ȴ�����;1,����ʧ��;2,���ͳɹ� 
	pp=mymalloc(100);    	//����100���ֽڵ��ڴ�,���ڴ�ŵ绰�����unicode�ַ���
	p1=mymalloc(300);       //����300���ֽڵ��ڴ�,���ڴ�Ŷ��ŵ�unicode�ַ���
	p2=mymalloc(100);       //����100���ֽڵ��ڴ� ��ţ�AT+CMGS=p1 
	Show_Str(30,150,200,16,"���͸�:",16,0);  	 
	Show_Str(30,170,200,16,"״̬:",16,0);
	Show_Str(30,190,200,12,"����:",12,0);  
	Show_Str(70,170,170,90,"�ȴ�����",16,0);            //��ʾ״̬	
	Show_Str(66,190,170,90,message,12,0);               //��ʾ��������	
	
	Show_Str(84,150,176,16,phonebuf,16,0);
	Show_Str(70,170,170,90,"���ڷ���",16,0);			//��ʾ���ڷ���		
	smssendsta=1;		 
	sim900a_unigbk_exchange(phonebuf,pp,1);				//���绰����ת��Ϊunicode�ַ���
	sim900a_unigbk_exchange(message,p1,1);              //����������ת��Ϊunicode�ַ���.
	sprintf((char*)p2,"AT+CMGS=\"%s\"",pp); 
			
	while(1)					 //ִ�з��Ͷ���
	{
		if(sim900a_send_cmd(p2,">",500)==0)					//���Ͷ�������+�绰����
		{ 		 				 													 
			u2_printf("%s",p1);		 						//���Ͷ������ݵ�GSMģ�� 
			if(sim900a_send_cmd((u8*)0X1A,"+CMGS:",1500)==0) smssendsta=2;//���ͽ�����,�ȴ��������(��ȴ�10����,��Ϊ���ų��˵Ļ�,�ȴ�ʱ��᳤һЩ)
		}  
		if(smssendsta==1)
		{	
			Show_Str(70,170,170,90,"����ʧ��",16,0);	//��ʾ״̬
		}
		else 
		{
			Show_Str(70,170,170,90,"���ͳɹ�",16,0);	//��ʾ״̬
			flag = 1;
		} 
		USART2_RX_STA=0;
		if(USART2_RX_STA&0X8000)    sim_at_response(1); //����GSMģ����յ������� 
		if(flag == 1)									//���ͳɹ��˳�����
		{	
			flag = 0;
			break;	
		}
		else											//����δ�ɹ�����������
		{
			delay_ms(800);								//���������ʧ�ܡ�״̬
			LCD_Fill(70,170,239,186,BLUE);	
			Show_Str(70,170,170,90,"���·���",16,0);	//��ʾ���ڷ���
		}
	}
	myfree(pp);
	myfree(p1);
	myfree(p2); 	
} 


//GSM��Ϣ����UI
void sim900a_data_ui(u16 x,u16 y)
{
	u8 *p,*p1,*p2; 
	p = mymalloc(50);//����50���ֽڵ��ڴ�
	LCD_Clear(BLUE);      //����
	POINT_COLOR = WHITE;
	BACK_COLOR = RED;
	Show_Str_Mid(0,y,"����GSM��ʵʱ�¶Ȳ�ѯϵͳ",16,240);  			    	 
	Show_Str(x,y+65,200,16,"����ΪGSMģ����Ϣ:",16,0); 				    	 	
	USART2_RX_STA = 0;
	if(sim900a_send_cmd("AT+CGMI","OK",200)==0)				     //��ѯ����������
	{ 
		p1 = (u8*)strstr((const char*)(USART2_RX_BUF+2),"\r\n"); //����һ���ַ�������һ���ַ����еĵ�һ�γ��֡��ҵ����������ַ�������ú������ص�һ��ƥ����ַ����ĵ�ַ
		p1[0] = 0;//���������
		sprintf((char*)p,"������:%s",USART2_RX_BUF+2);			 //�ַ�����ʽ������Ѹ�ʽ��������д��ĳ���ַ�����
		Show_Str(x,y+110,200,16,p,16,0);
		USART2_RX_STA=0;		
	} 
	if(sim900a_send_cmd("AT+CGMM","OK",200)==0)                  //��ѯģ������
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF+2),"\r\n"); 	 //\r�ǻس���,\n�ǻ��з�
		p1[0] = 0;//���������
		sprintf((char*)p,"ģ���ͺ�:%s",USART2_RX_BUF+2);
		Show_Str(x,y+130,200,16,p,16,0);
		USART2_RX_STA = 0;		
	} 
	if(sim900a_send_cmd("AT+CGSN","OK",200)==0)//��ѯ��Ʒ���к�
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF+2),"\r\n");   //���һس�
		p1[0]=0;//��������� 
		sprintf((char*)p,"���к�:%s",USART2_RX_BUF+2);
		Show_Str(x,y+150,200,16,p,16,0);
		USART2_RX_STA=0;		
	}
	if(sim900a_send_cmd("AT+CNUM","+CNUM",200)==0)			//��ѯ��������
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
		p2=(u8*)strstr((const char*)(p1+2),"\"");
		p2[0]=0;//���������
		sprintf((char*)p,"��������:%s",p1+2);
		Show_Str(x,y+170,200,16,p,16,0);
		USART2_RX_STA=0;		
	}
	myfree(p); 										//�ͷ��ڴ�(�ⲿ����)
	Show_Str(40,230,200,16,"GSMģ�鹤������!",16,0);
	delay_ms(1000);
	Show_Str(40,250,200,16,"����������״̬.",16,0);
	delay_ms(1000);
	Show_Str(40,250,200,16,"����������״̬..",16,0);
	delay_ms(1000);
	Show_Str(40,250,200,16,"����������״̬...",16,0);
	delay_ms(1000);
}
//GSM��Ϣ��ʾ(�ź�����,��ص���,����ʱ��)
//����ֵ:0,����
//    ����,�������
u8 sim900a_gsminfo_show(u16 x,u16 y)
{
	u8 *p,*p1,*p2;
	u8 res=0;
	p=mymalloc(50);//����50���ֽڵ��ڴ�	
	USART2_RX_STA=0;
	if(sim900a_send_cmd("AT+CPIN?","OK",200))res|=1<<0;	//��ѯSIM���Ƿ���λ 
	USART2_RX_STA=0;  				    	
	if(sim900a_send_cmd("AT+COPS?","OK",200)==0)		//��ѯ��Ӫ������
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\""); 
		if(p1)//����Ч����
		{
			p2=(u8*)strstr((const char*)(p1+1),"\"");
			p2[0]=0;//���������			
			sprintf((char*)p,"��Ӫ��:%s",p1+1);
			Show_Str(x,y,200,16,p,16,0);
		} 
		USART2_RX_STA=0;		
	}else res|=1<<1;
	if(sim900a_send_cmd("AT+CSQ","+CSQ:",200)==0)		//��ѯ�ź�����
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),":");
		p2=(u8*)strstr((const char*)(p1),",");
		p2[0]=0;//���������
		sprintf((char*)p,"�ź�����:%s",p1+2);
		Show_Str(x,y+20,200,16,p,16,0);
		USART2_RX_STA=0;		
	}else res|=1<<2;
	if(sim900a_send_cmd("AT+CBC","+CBC:",200)==0)		   //��ѯ��ص���
	{ 
		p1=(u8*)strstr((const char*)(USART2_RX_BUF),",");
		p2=(u8*)strstr((const char*)(p1+1),",");
		p2[0]=0;p2[5]=0;//���������
		sprintf((char*)p,"��ص���:%s%%  %smV",p1+1,p2+1);
		Show_Str(x,y+40,200,16,p,16,0);
		USART2_RX_STA=0;		
	}else res|=1<<3; 
//	if(sim900a_send_cmd("AT+CCLK?","+CCLK:",200)==0)		//��ѯ����ʱ��
//	{ 
//		p1=(u8*)strstr((const char*)(USART2_RX_BUF),"\"");
//		p2=(u8*)strstr((const char*)(p1+1),":");
//		p2[3]=0;//���������
//		sprintf((char*)p,"����ʱ��:%s",p1+1);
//		Show_Str(x,y+60,200,16,p,16,0);
//		USART2_RX_STA=0;		
//	}else res|=1<<4; 
	myfree(p); 
	return res;
} 

void SIM900A_Link(void)        //����GSMģ��
{
	u8 temp = 0;
	LCD_Clear(BLUE);    //����
	POINT_COLOR=WHITE;
	BACK_COLOR=RED;
	Show_Str_Mid(0,50,"����GSM��ʵʱ�¶Ȳ�ѯϵͳ",16,240); 
	while(sim900a_send_cmd("AT","OK",100))   //����Ƿ�Ӧ��ATָ�� 
	{
		Show_Str(40,85,200,16,"δ��⵽GSMģ��!!!",16,0);
		delay_ms(800);
		LCD_Fill(0,85,240,85+16,BLUE);
		Show_Str(40,85,200,16,"��������GSMģ��...",16,0);
		delay_ms(800); 
	} 	
	temp += sim900a_send_cmd("ATE0","OK",200);      //������  
}
void Show_Tem(void)	      //�²�ģ�鿪,��ʾ�¶���Ϣ
{
	LCD_Fill(0,40,239,76,BLUE); 
	Show_Str_Mid(0,40,"�²�ģ�飺��",16,240);
	LCD_ShowString(60,60,200,16,16,"Temp:   .  C");	
	temperature = DS18B20_Get_Temp();	
	if(temperature<0)
	{
		LCD_ShowChar(60+40,60,'-',16,0);			//��ʾ����
		temperature=-temperature;					//תΪ����
		flag = 1;
	}
	else LCD_ShowChar(60+40,60,' ',16,0);			//ȥ������
	LCD_ShowNum(60+40+ 8,60,temperature / 10,2,16);	//��ʾ��������	    
	LCD_ShowNum(60+40+32,60,temperature % 10,1,16);	//��ʾС������ 
}
void Not_Show_Tem(void)	   //�²�ģ���,��ʾ��ʾ
{
	LCD_Fill(0,40,239,76,BLUE); 
	Show_Str_Mid(0,40,"�²�ģ�飺��",16,240);
}
void Get_Msg(int tem)	//���¶�ת��Ϊ�ַ�������ŷ���ȥ
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
	if( (tem/10) > 99)		  //3λ��
	{
		S2[4] = '.';
		S2[5] = 0;
	}
	else					  //2λ��
	{
		S2[3] = '.';
		S2[4] = 0;
	}
	S1 = my_itoa(tem % 10);
	strcat(S2, S1);
	strcat(S2, "�ȡ�");								 //S2Ϊ  XXX.X�ȡ�	
	strcpy(S1, "ϵͳ��ǰ�¶�Ϊ");					 //S1Ϊ  ϵͳ��ǰ�¶�
	strcat(S1, S2);
	//Show_Str(16,180,240,90,S1,16,0);                 //��ʾ��������
}
//��ת�ַ���  
char *reverse(char *s)  
{  
    char temp;  
    char *p = s;    //pָ��s��ͷ��  
    char *q = s;    //qָ��s��β��  
    while(*q)  
        ++q;  
    q--;  
  
    //�����ƶ�ָ�룬ֱ��p��q����  
    while(q > p)  
    {  
        temp = *p;  
        *p++ = *q;  
        *q-- = temp;  
    }  
    return s;  
}  
  
/* 
 * ���ܣ�����ת��Ϊ�ַ��� 
 * char s[] �������Ǵ洢������ÿһλ 
 */  
char *my_itoa(int n)  
{  
    int i = 0,isNegative = 0;  
    static char s[100];      //����Ϊstatic������������ȫ�ֱ���  
    if((isNegative = n) < 0) //����Ǹ�������תΪ����  
    {  
        n = -n;  
    }  
    do      //�Ӹ�λ��ʼ��Ϊ�ַ���ֱ�����λ�����Ӧ�÷�ת  
    {  
        s[i++] = n%10 + '0';  
        n = n/10;  
    }while(n > 0);  
  
    if(isNegative < 0)   //����Ǹ��������ϸ���  
    {  
        s[i++] = '-';  
    }  
    s[i] = '\0';    //�������ַ���������  
    return reverse(s);    
}  

//SIM900A�����Ų��� 
void sim900a_sms_send_test(void)
{
	u8 *p,*p1,*p2;
	u8 smssendsta = 0;		//���ŷ���״̬,0,�ȴ�����;1,����ʧ��;2,���ͳɹ� 
	p  = mymalloc(100);	//����100���ֽڵ��ڴ�,���ڴ�ŵ绰�����unicode�ַ���
	p1 = mymalloc(300);//����300���ֽڵ��ڴ�,���ڴ�Ŷ��ŵ�unicode�ַ���
	p2 = mymalloc(100);//����100���ֽڵ��ڴ� ��ţ�AT+CMGS=p1 
	LCD_Clear(BLUE);  
	POINT_COLOR=WHITE;
	BACK_COLOR=RED;
	Show_Str_Mid(0,30,"ATK-SIM900A �����Ų���",16,240);				    	 
	Show_Str(30,50,200,16,"���͸�:",16,0); 
	Show_Str(84,50,176,16,phonebuf,16,0);	 
	Show_Str(30,70,200,16,"״̬:",16,0);
	Show_Str(30,90,200,16,"����:",16,0);  
	Show_Str(30+40,70,170,90,"�ȴ�����",16,0);//��ʾ״̬	
	Show_Str(30+40,90,170,90,S1,16,0);//��ʾ��������		
						 //ִ�з��Ͷ���
	Show_Str(30+40,70,170,90,"���ڷ���",16,0);			//��ʾ���ڷ���		
	smssendsta=1;		 
	sim900a_unigbk_exchange(phonebuf,p,1);				//���绰����ת��Ϊunicode�ַ���
	sim900a_unigbk_exchange(S1,p1,1);//����������ת��Ϊunicode�ַ���.
	sprintf((char*)p2,"AT+CMGS=\"%s\"",p); 
	if(sim900a_send_cmd(p2,">",200)==0)					//���Ͷ�������+�绰����
	{ 		 				 													 
		u2_printf("%s",p1);		 						//���Ͷ������ݵ�GSMģ�� 
		if(sim900a_send_cmd((u8*)0X1A,"+CMGS:",2000)==0)smssendsta=2;//���ͽ�����,�ȴ��������(��ȴ�10����,��Ϊ���ų��˵Ļ�,�ȴ�ʱ��᳤һЩ)
	}  
	if(smssendsta==1)Show_Str(30+40,70,170,90,"����ʧ��",16,0);	//��ʾ״̬
	else Show_Str(30+40,70,170,90,"���ͳɹ�",16,0);				//��ʾ״̬	
	USART2_RX_STA=0;
	if(USART2_RX_STA&0X8000)  sim_at_response(1);//����GSMģ����յ������� 
	myfree(p);
	myfree(p1);
	myfree(p2); 
} 
