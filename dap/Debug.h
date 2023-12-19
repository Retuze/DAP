#ifndef __DEBUG_H__
#define __DEBUG_H__

//定义函数返回值
#ifndef SUCCESS
#define SUCCESS 0
#endif
#ifndef FAIL
#define FAIL 0xFF
#endif

//定义定时器起始
#ifndef START
#define START 1
#endif
#ifndef STOP
#define STOP 0
#endif

#define FREQ_SYS 24000000 //系统主频24MHz

void CfgFsys();                         //CH552时钟选择和配置
void mDelayuS(UINT16 n);                // 以uS为单位延时
void mDelaymS(UINT16 n);                // 以mS为单位延时

#endif
