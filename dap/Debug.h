#ifndef __DEBUG_H__
#define __DEBUG_H__

//���庯������ֵ
#ifndef SUCCESS
#define SUCCESS 0
#endif
#ifndef FAIL
#define FAIL 0xFF
#endif

//���嶨ʱ����ʼ
#ifndef START
#define START 1
#endif
#ifndef STOP
#define STOP 0
#endif

#define FREQ_SYS 24000000 //ϵͳ��Ƶ24MHz

void CfgFsys();                         //CH552ʱ��ѡ�������
void mDelayuS(UINT16 n);                // ��uSΪ��λ��ʱ
void mDelaymS(UINT16 n);                // ��mSΪ��λ��ʱ

#endif
