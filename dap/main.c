
/********************************** (C) COPYRIGHT *******************************
* File Name          :CompatibilityHID.C
* Author             : WCH
* Version            : V1.2
* Date               : 2018/02/28
* Description        : CH554模拟HID兼容设备，支持中断上下传，支持控制端点上下传，支持设置全速，低速
*******************************************************************************/

#include "CH552.H"
#include "Debug.h"
#include "DAP.h"
#include "Uart.h"

#define USE_DAPLINK
#define Fullspeed 1
#define THIS_ENDP0_SIZE 64

UINT8X Ep0Buffer[THIS_ENDP0_SIZE] _at_ 0x0000;      //端点0 OUT&IN缓冲区，必须是偶地址

// UINT8X Ep1BufferO[THIS_ENDP0_SIZE] _at_ 0x0040;     //端点1 OUT双缓冲区,必须是偶地址 Not Change!!!!!!
// UINT8X Ep1BufferI[THIS_ENDP0_SIZE] _at_ 0x0080;     //端点1 IN双缓冲区,必须是偶地址 Not Change!!!!!!

//100,140,180,1C0
UINT8X Ep2BufferO[4 * THIS_ENDP0_SIZE] _at_ 0x0100; //端点2 OUT双缓冲区,必须是偶地址
//200,240,280,2C0
UINT8X Ep3BufferI[4 * THIS_ENDP0_SIZE] _at_ 0x0200; //端点3 IN双缓冲区,必须是偶地址

extern BOOL UART_TX_BUSY;
extern BOOL EP1_TX_BUSY;
BOOL DAP_LED_BUSY;

UINT8I Ep2Oi, Ep2Oo;                                //OUT 索引
UINT8I Ep3Ii, Ep3Io;                                //IN 索引
UINT8I Ep3Is[DAP_PACKET_COUNT];                     //发送包
PUINT8 pDescr;                                      //USB配置标志
UINT8I Endp3Busy = 0;
UINT8I SetupReq, SetupLen, Count, UsbConfig;
#define UsbSetupBuf ((PUSB_SETUP_REQ)Ep0Buffer)

/*HID类报表描述符*/
UINT8C HIDRepDesc[] =
{
    0x06, 0x00, 0XFF,   //Usage Page (Vendor-Defined 1)	
    0x09, 0x01,         //Usage (Vendor-Defined 1)		
    0xA1, 0x01,         //Collection (Application)		
    0x15, 0x00,         //Logical Minimum (0)			
    0x26, 0xFF, 0X00,   //Logical Maximum (255)		
    0x75, 0x08,         //Report Size (8)				
    0x95, 0x40,         //Report Count (64)			
    0x09, 0x01,         //Usage (Vendor-Defined 1)					
    0x81, 0x02,         //Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)	
    0x95, 0x40,         //Report Count (64)									
    0x09, 0x01,         //Usage (Vendor-Defined 1)							
    0x91, 0x02,         //Output (Data,Var,Abs,NWrp,Lin,Pref,NNul,NVol,Bit)	
    0x95, 0x40,         //Report Count (64)									
    0x09, 0x01,         //Usage (Vendor-Defined 1)							
    0xB1, 0x02,         //Feature (Data,Var,Abs,NWrp,Lin,Pref,NNul,NVol,Bit)	
    0xC0                //End Collection											
};

UINT8C DevDesc[] =
{
    0x12,               // bLength
    0x01,               // bDescriptorType
#ifdef DAP_USE_HID_FW_V1
	0x00, 0x02,         // bcdUSB
#else
    0x01, 0x02,         // bcdUSB
#endif
    0x00,
    0x00,
    0x00,
    THIS_ENDP0_SIZE,
    0x28, 
    0x0D, 
    0x04, 
    0x02, 
    0x00, 
    0x01, 
    0x01, 
    0x02,
    0x03, 
    0x01
};

#define USB_DESCSIZE_CONFIG_H 0

// Configuration Descriptor and Interface Descriptor
code const UINT8C CfgDesc[] =
{
/**********************Configuration Descriptor*********************/
	9,					// Length of the descriptor
	0x02,				// Type: Configuration Descriptor
	// Total length of this and following descriptors
#ifdef USE_DAPLINK  // Total L
#ifdef DAP_USE_HID_FW_V1 
    0x6B,	// Actual size of your CfgDesc, set according to your configuration
#else
    0x62,               // Total L
#endif
#else
    0x4B,
#endif
    0x00,               // Total H
#ifdef USE_DAPLINK
	3,		            // Number of interfaces hid+cdc+cdcctrl
#else
    2,                  // Number of interfaces cdc+cdcctrl
 #endif
	0x01, 			    // Value used to select this configuration
	0x00,				// Index of corresponding string descriptor
	0x80,				// Attributes, D7 must be 1, D6 Self-powered, D5 Remote Wakeup, D4-D0=0
	0x32,				// Max current drawn by device, in units of 2mA
    
/**********************HID or WINUSB*********************/
#ifdef USE_DAPLINK
    0x09,
    0x04,
    0x00,
    0x00,
    0x02,
#ifdef DAP_USE_HID_FW_V1
	0x03,               // bInterfaceClass
#else
	0xFF,               // bInterfaceClass
#endif
    0x00,
    0x00,
    0x04,

#ifdef DAP_USE_HID_FW_V1
    // HID Descriptor
    0x09,               // bLength
    0x21,               // bDescriptorType
    0x10, 0x00,         // HID Class Spec
    0x00,               // H/W target country.
    0x01,               // Number of HID class descriptors to follow.
    0x22,       // Descriptor type.
    sizeof(HIDRepDesc) & 0xFF, sizeof(HIDRepDesc) >> 8,  // Total length of report descriptor.
#endif

    //端点描述符
    0x07,
    0x05,
    0x02,
#ifdef DAP_USE_HID_FW_V1 //切换通讯方式，HID属于中断通讯，WinUsb属于块
    0x03,                     	// bmAttributes
#else
    0x02,                     	// bmAttributes
#endif
    0x40,
    0x00,
    0x01,
    
    //端点描述符
    0x07,
    0x05,
    0x83,
#ifdef DAP_USE_HID_FW_V1 //切换通讯方式，HID属于中断通讯，WinUsb属于块
    0x03,                     	// bmAttributes
#else
    0x02,                     	// bmAttributes
#endif
    0x40,
    0x00,
    0x01,
#endif

/************************CDC*************************/
    // Interface Association Descriptor (CDC)
	8,					// Length of the descriptor
	0x0B,				// Type: Interface Association Descriptor (IAD)
	0x02, 				// First interface: 0 in this case, see following ,gw Interface descriptor (CDC)data and ctrl
	0x02, 				// Total number of grouped interfaces
	0x02, 				// bFunctionClass
	0x02, 				// bFunctionSubClass
	0x01, 				// bFunctionProtocol
	0x03, 				// Index of string descriptor describing this function

	// Interface descriptor (CDC) Control
	9,					// Length of the descriptor
	0x04,				// Type: Interface Descriptor
	0x02,				// Interface ID
	0x00,				// Alternate setting
	0x01,				// Number of Endpoints
	0x02,				// Interface class code - Communication Interface Class
	0x02,				// Subclass code - Abstract Control Model
	0x01,				// Protocol code - AT Command V.250 protocol
	0x00,				// Index of corresponding string descriptor (On Windows, it is called "Bus reported device description")

	// Header Functional descriptor (CDC)
	5,					// Length of the descriptor
	0x24,				// bDescriptortype, CS_INTERFACE
	0x00,				// bDescriptorsubtype, HEADER
	0x10,0x01,			// bcdCDC

	// Call Management Functional Descriptor (CDC)
	5,					// Length of the descriptor
	0x24,				// bDescriptortype, CS_INTERFACE
	0x01,				// bDescriptorsubtype, Call Management Functional Descriptor
	0x00,				// bmCapabilities
	// Bit0: Does not handle call management
	// Bit1: Call Management over Comm Class interface
	0x00,				// Data Interfaces

	// Abstract Control Management (CDC)
	4,					// Length of the descriptor
	0x24,				// bDescriptortype, CS_INTERFACE
	0x02,				// bDescriptorsubtype, Abstract Control Management
	0x02,				// bmCapabilities
	// Bit0: CommFeature
	// Bit1: LineStateCoding  Device supports the request combination of
	// Set_Line_Coding,	Set_Control_Line_State, Get_Line_Coding, and the notification Serial_State.
	// Bit2: SendBreak
	// Bit3: NetworkConnection
	// Set_Line_Coding  Set_Control_Line_State  Get_Line_Coding  Serial_State

	// Union Functional Descriptor (CDC)
	5,					// Length of the descriptor
	0x24,				// bDescriptortype, CS_INTERFACE
	0x06,				// bDescriptorSubtype: Union func desc
	0x00,				// bMasterInterface: Communication class interface
	0x01,				// bSlaveInterface0: Data Class Interface

	// EndPoint descriptor (CDC Upload, Interrupt)
	7,					// Length of the descriptor
	0x05,				// Type: Endpoint Descriptor
	0x82,				// Endpoint: D7: 0-Out 1-In, D6-D4=0, D3-D0 Endpoint number
	0x03,				// Attributes:
						// D1:0 Transfer type: 00 = Control 01 = Isochronous 10 = Bulk 11 = Interrupt
						// 			The following only apply to isochronous endpoints. Else set to 0.
						// D3:2 Synchronization Type: 00 = No Synchronization 01 = Asynchronous 10 = Adaptive 11 = Synchronous
						// D5:4	Usage Type: 00 = Data endpoint 01 = Feedback endpoint 10 = Implicit feedback Data endpoint 11 = Reserved
						// D7:6 = 0
	0x10, 0x00,			// Maximum packet size can be handled
	0x40,				// Interval for polling, in units of 1 ms for low/full speed

	// Interface descriptor (CDC) Data
	9,					// Length of the descriptor
	0x04,				// Type: Interface Descriptor
	0x03,				// Interface ID
	0x00,				// Alternate setting
	0x02,				// Number of Endpoints
	0x0a,				// Interface class code
	0x00,				// Subclass code
	0x00,				// Protocol code
	0x00,				// Index of corresponding string descriptor (On Windows, it is called "Bus reported device description")

	// EndPoint descriptor (CDC Upload, Bulk)
	7,					// Length of the descriptor
	0x05,				// Type: Endpoint Descriptor
	0x01,				// Endpoint: D7: 0-Out 1-In, D6-D4=0, D3-D0 Endpoint number
	0x02,				// Attributes:
						// D1:0 Transfer type: 00 = Control 01 = Isochronous 10 = Bulk 11 = Interrupt
						// 			The following only apply to isochronous endpoints. Else set to 0.
						// D3:2 Synchronization Type: 00 = No Synchronization 01 = Asynchronous 10 = Adaptive 11 = Synchronous
						// D5:4	Usage Type: 00 = Data endpoint 01 = Feedback endpoint 10 = Implicit feedback Data endpoint 11 = Reserved
						// D7:6 = 0
	0x40, 0x00,			// Maximum packet size can be handled
	0x00,				// Interval for polling, in units of 1 ms for low/full speed

	// EndPoint descriptor (CDC Upload, Bulk)
	7,				// Length of the descriptor
	0x05,				// Type: Endpoint Descriptor
	0x81,				// Endpoint: D7: 0-Out 1-In, D6-D4=0, D3-D0 Endpoint number
	0x02,				// Attributes:
						// D1:0 Transfer type: 00 = Control 01 = Isochronous 10 = Bulk 11 = Interrupt
						// 			The following only apply to isochronous endpoints. Else set to 0.
						// D3:2 Synchronization Type: 00 = No Synchronization 01 = Asynchronous 10 = Adaptive 11 = Synchronous
						// D5:4	Usage Type: 00 = Data endpoint 01 = Feedback endpoint 10 = Implicit feedback Data endpoint 11 = Reserved
						// D7:6 = 0
	0x40, 0x00,			// Maximum packet size can be handled
	0x00,				// Interval for polling, in units of 1 ms for low/full speed
};

UINT16I USB_STATUS = 0;

UINT8I LineCoding[7] = {0x00, 0xA2, 0x5D, 0x4F, 0x00, 0x00, 0x08}; //初始化波特率为115200，1停止位，无校验，8数据位。

code const UINT8C LangDesc[] = {
	4, 0x03,		// Length = 4 bytes, String Descriptor (0x03) 
	0x09, 0x04	    // 0x0409 English - United States
};

code const UINT8C i_SerialNumber[] = {
	36, 0x03, 	// Length = 20 bytes, String Descriptor (0x03)
	'l', 0, 'c', 0, 'k', 0, 'f', 0, 'b', 0, '.', 0, 'c', 0,
    'o', 0, 'm', 0, '-', 0, 'v', 0, '1', 0, '.', 0, '1', 0, '.', 0, '0', 0, 
    #ifdef DAP_USE_HID_FW_V1
    'H', 0  //表示使用hid协议 兼容win7 mdk5.28一下可以正常识别
    #else
    'W', 0  //表示使用winusb协议 不兼容win7 mdk5.28一下可以正常识别
    #endif
};

code const UINT8C i_Product[] = {
	36, 0x03,   // Length = 36 bytes, String Descriptor (0x03)
    'D', 0, 'A', 0, 'P', 0, 'L', 0, 'i', 0, 'n', 0, 'k', 0, ' ', 0,
    'C', 0, 'M', 0, 'S', 0, 'I', 0, 'S', 0, '-', 0, 'D', 0, 'A', 0,
    'P', 0
};

code const UINT8C i_Manufacturer[] = {
	 8, 0x03, 	// Length = 8 bytes, String Descriptor (0x03)
	'A', 0x00, 'R', 0x00, 'M', 0x00
};

//CDC
code const UINT8C i_CdcString[] =
{
    30, 0x03,
    'D', 0, 'A', 0, 'P', 0, 'L', 0, 'i', 0, 'n', 0, 'k', 0, '-', 0, 'C', 0, 'D', 0, 'C', 0, 'E', 0, 'x', 0, 't', 0
};

// 接口: CMSIS-DAP
UINT8C i_Interface[] =
{
    26,
    0x03,
    'C', 0, 'M', 0, 'S', 0, 'I', 0, 'S', 0, '-', 0, 'D', 0, 'A', 0,
    'P', 0, ' ', 0, 'v', 0, 
    #ifdef DAP_USE_HID_FW_V1
    '1', 0
    #else
    '2', 0
    #endif
};

code const UINT8C* StringDescs[] = {	
	LangDesc,		//[0]
	i_Manufacturer,	//[1]
	i_Product,		//[2]
	i_SerialNumber,	//[3]
    i_Interface,    //[4]
    i_CdcString,    //[5]
};

UINT8C USB_BOSDescriptor[] =
{
    0x05,                                      /* bLength */
    0x0F,                                      /* bDescriptorType */
    0x28, 0x00,                                /* wTotalLength */
    0x02,                                      /* bNumDeviceCaps */
    0x07, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x1C,                                      /* bLength */
    0x10,                                      /* bDescriptorType */
    0x05,                                      /* bDevCapabilityType */
    0x00,                                      /* bReserved */
    0xDF, 0x60, 0xDD, 0xD8,                    /* PlatformCapabilityUUID */
    0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D,
    0x9E, 0x64, 0x8A, 0x9F,
    0x00, 0x00, 0x03, 0x06,    /* >= Win 8.1 *//* dwWindowsVersion*/
    0xAA, 0x00,                                /* wDescriptorSetTotalLength */
    0x20,                                      /* bVendorCode */
    0x00                                       /* bAltEnumCode */
};

UINT8C WINUSB_Descriptor[] =
{
    0x0A, 0x00,                                 /* wLength */
    0x00, 0x00,                                 /* wDescriptorType */
    0x00, 0x00, 0x03, 0x06,                     /* dwWindowsVersion*/
    0xAA, 0x00,                                 /* wDescriptorSetTotalLength */
    /* ... */
    0x08, 0x00,
    0x02, 0x00,
    0x00, 0x00,
    0xA0, 0x00,
    /* ... */
    0x14, 0x00,                                 /* wLength */
    0x03, 0x00,                                 /* wDescriptorType */
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,         /* CompatibleId*/
    0, 0, 0, 0, 0, 0, 0, 0,                     /* SubCompatibleId*/
    0x84, 0x00,                                 /* wLength */
    0x04, 0x00,                                 /* wDescriptorType */
    0x07, 0x00,                                 /* wPropertyDataType */
    0x2A, 0x00,                                 /* wPropertyNameLength */
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
    'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
    0x50, 0x00,                                 /* wPropertyDataLength */
    '{', 0,
    'C', 0, 'D', 0, 'B', 0, '3', 0, 'B', 0, '5', 0, 'A', 0, 'D', 0, '-', 0,
    '2', 0, '9', 0, '3', 0, 'B', 0, '-', 0,
    '4', 0, '6', 0, '6', 0, '3', 0, '-', 0,
    'A', 0, 'A', 0, '3', 0, '6', 0, '-',
    0, '1', 0, 'A', 0, 'A', 0, 'E', 0, '4', 0, '6', 0, '4', 0, '6', 0, '3', 0, '7', 0, '7', 0, '6', 0,
    '}', 0, 0, 0, 0, 0,
};

/*******************************************************************************
* Function Name  : USBDeviceInit()
* Description    : USB设备模式配置,设备模式启动，收发端点配置，中断开启
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void USBDeviceInit()
{
    IE_USB = 0;
    USB_CTRL = 0x00;        // 先设定USB设备模式
    UDEV_CTRL = bUD_PD_DIS; // 禁止DP/DM下拉电阻
#ifndef Fullspeed
    UDEV_CTRL |= bUD_LOW_SPEED; //选择低速1.5M模式
    USB_CTRL |= bUC_LOW_SPEED;
#else
    UDEV_CTRL &= ~bUD_LOW_SPEED; //选择全速12M模式，默认方式
    USB_CTRL &= ~bUC_LOW_SPEED;
#endif

    UEP0_DMA = Ep0Buffer;                                      //端点0数据传输地址
    UEP4_1_MOD &= ~(bUEP4_RX_EN | bUEP4_TX_EN);                //端点0单64字节收发缓冲区
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;                 //OUT事务返回ACK，IN事务返回NAK

    UEP1_DMA = Uart_TxBuff0;									//端点1数据传输地址
    UEP4_1_MOD |= bUEP1_TX_EN | bUEP1_RX_EN;					//端点1发送接收使能
    UEP4_1_MOD |= bUEP1_BUF_MOD;								//端点1收发各双64字节缓冲区
    UEP1_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK; 					//端点1，IN事务返回NAK，OUT返回ACK

    UEP2_DMA = Ep2BufferO;                                     //端点2数据传输地址
    UEP3_DMA = Ep3BufferI;                                     //端点2数据传输地址
    UEP2_3_MOD |= (bUEP3_TX_EN | bUEP2_RX_EN);                   //端点2发送接收使能
    UEP2_3_MOD &= ~(bUEP2_BUF_MOD | bUEP3_BUF_MOD);            //端点2收发各64字节缓冲区
    UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK; //端点2自动翻转同步标志位，IN事务返回NAK，OUT返回ACK
    UEP3_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_NAK;//端点3自动翻转同步标志位，IN事务返回NAK，OUT返回NACK

    UEP4_1_MOD |= bUEP4_TX_EN; // 端点4发送使能

    USB_DEV_AD = 0x00;
    USB_CTRL |= bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN; // 启动USB设备及DMA，在中断期间中断标志未清除前自动返回NAK
    UDEV_CTRL |= bUD_PORT_EN;                              // 允许USB端口
    USB_INT_FG = 0xFF;                                     // 清中断标志
    USB_INT_EN = bUIE_SUSPEND | bUIE_TRANSFER | bUIE_BUS_RST;
    IE_USB = 1;

	UEP1_T_LEN = 0;  //预使用发送长度一定要清空
	UEP2_T_LEN = 0;  //预使用发送长度一定要清空
}
/*******************************************************************************
* Function Name  : DeviceInterrupt()
* Description    : CH559USB中断处理函数
*******************************************************************************/
void DeviceInterrupt(void) interrupt INT_NO_USB using 1 // USB中断服务程序,使用寄存器组1
{
    UINT8 len;
    if (UIF_TRANSFER) // USB传输完成标志
    {
        ////printf("USB_INT_ST:%x\r\n",(USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP)));
        switch (USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
        {
        case UIS_TOKEN_OUT | 2: // endpoint 2# 端点批量下传 DAP-CMD
            if (U_TOG_OK)       // 不同步的数据包将丢弃
            {
                Ep2Oi += 64;
                UEP2_DMA_L = Ep2Oi;
            }
            break;

        case UIS_TOKEN_IN | 3: // endpoint 3# 端点批量上传 DAP_ASK
            Endp3Busy = 0;
            UEP3_T_LEN = 0;                                          //预使用发送长度一定要清空
            UEP3_CTRL = UEP3_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK; //默认应答NAK
            break;

        case UIS_TOKEN_IN | 1: // endpoint 1# 端点批量上传 CDC
            UEP1_T_LEN = 0;
			UEP1_CTRL = UEP1_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;
			USB_CDC_PushData();
            break;
        case UIS_TOKEN_OUT | 1: // endpoint 1# 端点批量下传 CDC
            if (U_TOG_OK)       // 不同步的数据包将丢弃
            {
                USB_CDC_GetData();
            }
            break;

        case UIS_TOKEN_SETUP | 0: // SETUP事务
            len = USB_RX_LEN;
            if (len == (sizeof(USB_SETUP_REQ)))
            {
                SetupLen = UsbSetupBuf->wLengthL;
                if (UsbSetupBuf->wLengthH)
                    SetupLen = 0xFF; // 限制总长度
                len = 0;             // 默认为成功并且上传0长度
                SetupReq = UsbSetupBuf->bRequest;
                switch (UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK)
                {
                case USB_REQ_TYP_STANDARD:
                    switch (SetupReq) //请求码
                    {
                    case USB_GET_DESCRIPTOR:
                        switch (UsbSetupBuf->wValueH)
                        {
                        case 1:               //设备描述符
                            pDescr = DevDesc; //把设备描述符送到要发送的缓冲区
                            len = sizeof(DevDesc);
                            break;
                        case 2:               //配置描述符
                            pDescr = CfgDesc; //把设备描述符送到要发送的缓冲区
                            len = sizeof(CfgDesc);
                            // printf("CfgDesc len:%d\r\n",(UINT16)len);
                            break;
                        case 3: // 字符串描述符
                            if (UsbSetupBuf->wValueL < (sizeof(StringDescs)/sizeof(StringDescs[0])))
                            {
                                pDescr = (UINT8C *)(StringDescs[UsbSetupBuf->wValueL]);
                                len = pDescr[0];
                            }
                            else
                            {
                                len = 0xFF;	//不支持的类型
                            }
                            break;
                        case 15:
                            pDescr = (PUINT8)(&USB_BOSDescriptor[0]);
                            len = sizeof(USB_BOSDescriptor);
                            break;
                        case 0x22:               //报表描述符 GW HID
                            pDescr = HIDRepDesc; //数据准备上传
                            len = sizeof(HIDRepDesc);
                            break;
                        default:
                            len = 0xff; //不支持的命令或者出错
                            break;
                        }
                        break;
                    case USB_SET_ADDRESS:
                        SetupLen = UsbSetupBuf->wValueL; //暂存USB设备地址
                        break;
                    case USB_GET_CONFIGURATION:
                        Ep0Buffer[0] = UsbConfig;
                        if (SetupLen >= 1)
                        {
                            len = 1;
                        }
                        break;
                    case USB_SET_CONFIGURATION:
                        UsbConfig = UsbSetupBuf->wValueL;
                        break;
                    case 0x0A:
                        break;
                    case USB_CLEAR_FEATURE:                                                         // Clear Feature
                        if ((UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) // 端点
                        {
                            switch (UsbSetupBuf->wIndexL)
                            {
                            case 0x82:
                                UEP2_CTRL = UEP2_CTRL & ~(bUEP_T_TOG | MASK_UEP_T_RES) | UEP_T_RES_NAK;
                                break;
                            case 0x81:
                                UEP1_CTRL = UEP1_CTRL & ~(bUEP_T_TOG | MASK_UEP_T_RES) | UEP_T_RES_NAK;
                                break;
                            case 0x02:
                                UEP2_CTRL = UEP2_CTRL & ~(bUEP_R_TOG | MASK_UEP_R_RES) | UEP_R_RES_ACK;
                                break;
                            default:
                                len = 0xFF; // 不支持的端点
                                break;
                            }
                        }
                        else
                        {
                            len = 0xFF; // 不是端点不支持
                        }
                        break;
                    case USB_SET_FEATURE:                               /* Set Feature */
                        if ((UsbSetupBuf->bRequestType & 0x1F) == 0x00) /* 设置设备 */
                        {
                            if ((((UINT16)UsbSetupBuf->wValueH << 8) | UsbSetupBuf->wValueL) == 0x01)
                            {
                                if (CfgDesc[7] & 0x20)
                                {
                                    /* 设置唤醒使能标志 */
                                }
                                else
                                {
                                    len = 0xFF; /* 操作失败 */
                                }
                            }
                            else
                            {
                                len = 0xFF; /* 操作失败 */
                            }
                        }
                        else if ((UsbSetupBuf->bRequestType & 0x1F) == 0x02) /* 设置端点 */
                        {
                            if ((((UINT16)UsbSetupBuf->wValueH << 8) | UsbSetupBuf->wValueL) == 0x00)
                            {
                                switch (((UINT16)UsbSetupBuf->wIndexH << 8) | UsbSetupBuf->wIndexL)
                                {
                                case 0x82:
                                    UEP2_CTRL = UEP2_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL; /* 设置端点2 IN STALL */
                                    break;
                                case 0x02:
                                    UEP2_CTRL = UEP2_CTRL & (~bUEP_R_TOG) | UEP_R_RES_STALL; /* 设置端点2 OUT Stall */
                                    break;
                                case 0x81:
                                    UEP1_CTRL = UEP1_CTRL & (~bUEP_T_TOG) | UEP_T_RES_STALL; /* 设置端点1 IN STALL */
                                    break;
                                default:
                                    len = 0xFF; /* 操作失败 */
                                    break;
                                }
                            }
                            else
                            {
                                len = 0xFF; /* 操作失败 */
                            }
                        }
                        else
                        {
                            len = 0xFF; /* 操作失败 */
                        }
                        break;
                    case USB_GET_STATUS:
                        pDescr = (PUINT8)&USB_STATUS;
                        if (SetupLen >= 2)
                        {
                            len = 2;
                        }
                        else
                        {
                            len = SetupLen;
                        }
                        break;
                    default:
                        len = 0xff; //操作失败
                        break;
                    }

                    break;
                case USB_REQ_TYP_CLASS: /*HID类命令*/
                    if ((UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_INTERF)
                    {
                        switch (SetupReq)
                        {
                        case 0x01:                           // GetReport
                            //pDescr = UserEp2Buf;             //控制端点上传输据
                            if (SetupLen >= THIS_ENDP0_SIZE) //大于端点0大小，需要特殊处理
                            {
                                len = THIS_ENDP0_SIZE;
                            }
                            else
                            {
                                len = SetupLen;
                            }
                            break;
                        case 0x02: // GetIdle
                            break;
                        case 0x03: // GetProtocol
                            break;
                        case 0x09: // SetReport
                            break;
                        case 0x0A: // SetIdle
                            break;
                        case 0x0B: // SetProtocol
                            break;
						case 0x20://Configure
                            break;
                        case 0x21://currently configured
                            pDescr = LineCoding;
                            len = sizeof(LineCoding);
                            break;
                        case 0x22://generates RS-232/V.24 style control signals
                            break;
                        default:
                            len = 0xFF; /*命令不支持*/
                            break;
                        }
                    }
                    break;
                case USB_REQ_TYP_VENDOR:
                    if ((UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                    {
                        switch (SetupReq)
                        {
                        case 0x20: // GetReport
                            if (UsbSetupBuf->wIndexL == 0x07)
                            {
                                pDescr = WINUSB_Descriptor; //把设备描述符送到要发送的缓冲区
                                len = sizeof(WINUSB_Descriptor);
                            }
                            break;
                        default:
                            len = 0xFF; /*命令不支持*/
                            break;
                        }
                    }
                    break;
                default:
                    len = 0xFF;
                    break;
                }
                if (len != 0 && len != 0xFF)
                {
                    if (SetupLen > len)
                    {
                        SetupLen = len; //限制总长度
                    }
                    len = SetupLen >= THIS_ENDP0_SIZE ? THIS_ENDP0_SIZE : SetupLen; //本次传输长度
                    memcpy(Ep0Buffer, pDescr, len);                                 //加载上传数据
                    SetupLen -= len;
                    pDescr += len;
                }
            }
            else
            {
                len = 0xff; //包长度错误
            }
            if (len == 0xff)
            {
                SetupReq = 0xFF;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL; // STALL
            }
            else if (len <= THIS_ENDP0_SIZE) //上传数据或者状态阶段返回0长度包
            {
                UEP0_T_LEN = len;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; //默认数据包是DATA1，返回应答ACK
            }
            else
            {
                UEP0_T_LEN = 0;                                                      //虽然尚未到状态阶段，但是提前预置上传0长度数据包以防主机提前进入状态阶段
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK; //默认数据包是DATA1,返回应答ACK
            }
            break;
        case UIS_TOKEN_IN | 0: // endpoint0 IN
            switch (SetupReq)
            {
            case USB_GET_DESCRIPTOR:
            case 0x20:
                len = SetupLen >= THIS_ENDP0_SIZE ? THIS_ENDP0_SIZE : SetupLen; //本次传输长度
                memcpy(Ep0Buffer, pDescr, len);                                 //加载上传数据
                SetupLen -= len;
                pDescr += len;
                UEP0_T_LEN = len;
                UEP0_CTRL ^= bUEP_T_TOG; //同步标志位翻转
                break;
            case USB_SET_ADDRESS:
                USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | SetupLen;
                UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                break;
            default:
                UEP0_T_LEN = 0; //状态阶段完成中断或者是强制上传0长度数据包结束控制传输
                UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                break;
            }
            break;
        case UIS_TOKEN_OUT | 0: // endpoint0 OUT
            len = USB_RX_LEN;
            if (SetupReq == 0x20) // Set Line Coding.
            {
                if (U_TOG_OK)
                {
                    memcpy(LineCoding, UsbSetupBuf, USB_RX_LEN);
					Config_Uart0(LineCoding);
                    UEP0_T_LEN = 0;
                    UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_ACK;  // 准备上传0包
                }
            }
            else if (SetupReq == 0x09)
            {
                if (Ep0Buffer[0])
                {
                }
                else if (Ep0Buffer[0] == 0)
                {
                }
            }
            UEP0_CTRL ^= bUEP_R_TOG; //同步标志位翻转
            break;
        default:
            break;
        }
        UIF_TRANSFER = 0; //写0清空中断
    }
    if (UIF_BUS_RST) //设备模式USB总线复位中断
    {
        UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP1_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK;
        UEP2_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        USB_DEV_AD = 0x00;
        UIF_SUSPEND = 0;
        UIF_TRANSFER = 0;
        Endp3Busy = 0;
        UIF_BUS_RST = 0; //清中断标志
    }
    if (UIF_SUSPEND) // USB总线挂起/唤醒完成
    {
        UIF_SUSPEND = 0;
        if (USB_MIS_ST & bUMS_SUSPEND) //挂起
        {
        }
    }
    else
    {
        //意外的中断,不可能发生的情况
        USB_INT_FG = 0xFF; //清中断标志
    }
}

typedef void( *goISP)( void );
goISP ISP_ADDR=0x3800;	
UINT16 LED_Timer;

void main(void)
{
    UINT8 Uart_Timeout = 0;

    CfgFsys();   //CH559时钟选择配置
    mDelaymS(5); //修改主频等待内部晶振稳定,必加
    UART_Setup(); // 串口初始化
    USBDeviceInit(); //USB设备模式初始化
    
    EA = 1;          //允许单片机中断
    UEP1_T_LEN = 0;  //预使用发送长度一定要清空
    UEP2_T_LEN = 0;  //预使用发送长度一定要清空
    Ep2Oi = 0;
    Ep2Oo = 0;
    Ep3Ii = 0;
    Ep3Io = 0;
    Endp3Busy = 0;
    DAP_LED_BUSY = 0;
    LED_Timer = 0;
    P1_MOD_OC = P1_MOD_OC & ~(1 << 4);
    P1_DIR_PU = P1_DIR_PU | (1 << 4);
    P1_MOD_OC = P1_MOD_OC & ~(1 << 5);
    P1_DIR_PU = P1_DIR_PU | (1 << 5);

    while (!UsbConfig) {;};

    while (1)
    {
        DAP_Thread();

        if (Endp3Busy != 1 && Ep3Ii != Ep3Io)
        {
            Endp3Busy = 1;

            #ifdef DAP_USE_HID_FW_V1
            UEP3_T_LEN = 64;//Ep3Io>>6];
            #else
            UEP3_T_LEN = Ep3Is[0];//Ep3Io>>6];
            #endif

            UEP3_DMA_L = Ep3Io;
            UEP3_CTRL = UEP3_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK; //有数据时上传数据并应答ACK
            Ep3Io += 64;
            
        }

        LED = !(XBUS_AUX & (bUART0_TX | bUART0_RX));

        if(CLOCK_CFG&bRST)//isp
        {
            USB_CTRL=0;
            UDEV_CTRL=0x80;
            mDelaymS(10);			
            (ISP_ADDR)();
        }		     
    }
}


