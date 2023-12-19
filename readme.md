## 代码来源

基于梁山派 [daplink]() 改了串口的代码，修复了原版串口数据偶尔异常的问题，把原版一直闪烁的led灯改为了串口指示灯。其中串口的代码来源于 abgelehnt 的[Tiny-DAPLink](https://github.com/abgelehnt/Tiny-DAPLink)

## 功能说明

- 支持SWD接口
- 支持虚拟串口
- 仅支持winusb通讯

## 硬件说明



## 软件说明

自行使用keil编译，或直接使用我编译好的固件烧录即可

## 文件说明

DAP
│  CH552.H		 //标准头文件
│  DAP.c			 //DAP源码
│  DAP.h			 //DAP头文件
│  Debug.c		 //芯片配置
│  Debug.h		 //芯片配置头文件
│  main.c	     //主程序与USB中断服务函数
│  readme.md
│  SW_DP.c		 //SWD源码
│  Uart.c		   //串口
│  Uart.h		   //串口头文件
├─firmware		 //固件
│      CH55x_DAPLink.hex.hex	
├─image	       //图片
├─KEIL	
│  │  CH55x_DAPLink.uvproj	//keil4工程文件
│ 
│
└─tool
        CMSIS_DAP.dll		//5.28winusb无法识别补丁
        keil4.exe			     //keil4安装包
        WCHISPTool_Setup.exe//下载工具

## 鸣谢

[https://gitee.com/lcsc/lspiclink](https://gitee.com/lcsc/lspiclink)

[https://github.com/abgelehnt/Tiny-DAPLink](https://github.com/abgelehnt/Tiny-DAPLink)



