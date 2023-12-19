#include "CH552.H"
#include "Uart.h"
#include "Debug.H"

UINT8X Uart_TxBuff0[64] _at_ 0x0300;
BOOL Uart_TxBuff0Used = 0;
UINT8 Uart_TxBuff0Length = 0;
UINT8X Uart_TxBuff1[64] _at_ 0x0340;
BOOL Uart_TxBuff1Used = 0;
UINT8 Uart_TxBuff1Length = 0;
UINT8X Uart_RxBuff0[64] _at_ 0x0380;
BOOL Uart_RxBuff0Used = 0;
UINT8X Uart_RxBuff1[64] _at_ 0x03C0;
BOOL Uart_RxBuff1Used = 0;
UINT8 Uart_TxPointer = 0;
UINT8 Uart_RxPointer = 0;
BOOL Uart_TxDealingWhich = 0;
BOOL Uart_RxDealingWhich = 0;

void UART_Setup(void) {
	P3_MOD_OC |= 0x03;
	P3_DIR_PU |= 0x03;

	//ʹ��Timer1��Ϊ�����ʷ�����	
	RCLK = 0; //UART0����ʱ��
	TCLK = 0; //UART0����ʱ��
	PCON |= SMOD;

	TMOD = TMOD & ~bT1_GATE & ~bT1_CT & ~MASK_T1_MOD | bT1_M1; //0X20��Timer1��Ϊ8λ�Զ����ض�ʱ��
	T2MOD = T2MOD | bTMR_CLK | bT1_CLK;                        //Timer1ʱ��ѡ��
	TH1 = 0 - (UINT32) (FREQ_SYS + DEFAULT_UART_BUAD * 8) / DEFAULT_UART_BUAD / 16;//12MHz����,buad/12Ϊʵ�������ò�����
	TR1 = 1;                                                                    //������ʱ��1
	SCON = 0x50;//����0ʹ��ģʽ1    TI = 1;    REN = 1;       
	IP |= PS;
	ES = 1;
}

void Config_Uart0(UINT8 *cfg_uart) {
	UINT32 uart0_buad = 0;
	*((UINT8 *) &uart0_buad) = cfg_uart[3];
	*((UINT8 *) &uart0_buad + 1) = cfg_uart[2];
	*((UINT8 *) &uart0_buad + 2) = cfg_uart[1];
	*((UINT8 *) &uart0_buad + 3) = cfg_uart[0];
	ES = 0;
	TH1 = 0 - ((FREQ_SYS + 8 * uart0_buad) / 16 / uart0_buad);
	ES = 1;
}


void Uart0_ISR(void) interrupt INT_NO_UART0 using 2{
	if (RI){
		if(Uart_RxDealingWhich){ // Uart_RxBuff1
			Uart_RxBuff1Used = 1;
			Uart_RxBuff1[Uart_RxPointer++] = SBUF;
			if (!Uart_RxBuff0Used){
				UEP1_T_LEN = Uart_RxPointer;
				Uart_RxPointer = 0;
				UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;
				Uart_RxDealingWhich = 0;
			}
		}else{
			Uart_RxBuff0Used = 1;
			Uart_RxBuff0[Uart_RxPointer++] = SBUF;
			if (!Uart_RxBuff1Used){
				UEP1_T_LEN = Uart_RxPointer;
				Uart_RxPointer = 0;
				UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;
				Uart_RxDealingWhich = 1;
			}
		}
		RI = 0;
	}
	if (TI){
		if(Uart_TxDealingWhich){ // Uart_TxBuff1
			if(Uart_TxPointer < Uart_TxBuff1Length){
				SBUF = Uart_TxBuff1[Uart_TxPointer++];
			}else{
				Uart_TxPointer = 0;
				if(Uart_TxBuff0Used){
					Uart_TxDealingWhich = 0;
					SBUF = Uart_TxBuff0[Uart_TxPointer++];
					UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
				}
				Uart_TxBuff1Used = 0;
			}
		}else{ // Uart_TxBuff0
			if(Uart_TxPointer < Uart_TxBuff0Length){
				SBUF = Uart_TxBuff0[Uart_TxPointer++];
			}else{
				Uart_TxPointer = 0;
				if(Uart_TxBuff1Used){
					Uart_TxDealingWhich = 1;
					SBUF = Uart_TxBuff1[Uart_TxPointer++];
					UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
				}
				Uart_TxBuff0Used = 0;
			}
		}
		TI = 0;
	}
}

void USB_CDC_GetData(void){
	ES = 0;
	if(UEP1_CTRL & bUEP_R_TOG){ // Uart_TxBuff0
		Uart_TxBuff0Length = USB_RX_LEN;
		if(!Uart_TxBuff1Used){ // �����ʱ���ڲ��ڷ������Ļ�����Ҫ����һ������
			Uart_TxDealingWhich = 0;
			SBUF = Uart_TxBuff0[Uart_TxPointer++];
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
		}else{
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_NAK;
		}
		Uart_TxBuff0Used = 1;
	}else{ // Uart_TxBuff1
		Uart_TxBuff1Length = USB_RX_LEN;
		if(!Uart_TxBuff0Used){
			Uart_TxDealingWhich = 1;
			SBUF = Uart_TxBuff1[Uart_TxPointer++];
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_ACK;
		}else{
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_R_RES | UEP_R_RES_NAK;
		}
		Uart_TxBuff1Used = 1;
	}
	ES = 1;
}

void USB_CDC_PushData(void){
	ES = 0;
	if(UEP1_CTRL & bUEP_T_TOG){ // Uart_RxBuff0
		if(Uart_RxBuff1Used){
			UEP1_T_LEN = Uart_RxPointer;
			Uart_RxPointer = 0;
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;
			Uart_RxDealingWhich = 0;
		}else{
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;
		}
		Uart_RxBuff0Used = 0;
	}else{
		if(Uart_RxBuff0Used){
			UEP1_T_LEN = Uart_RxPointer;
			Uart_RxPointer = 0;
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;
			Uart_RxDealingWhich = 1;
		}else{
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;
		}
		Uart_RxBuff1Used = 0;
	}
	ES = 1;
}
