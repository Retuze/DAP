/********************************** (C) COPYRIGHT *******************************
* File Name          : DEBUG.C
* Author             : WCH
* Version            : V1.0
* Date               : 2017/01/20
* Description        : CH552 DEBUG Interface
                     (1)������0�����ӡ��Ϣ�������ʿɱ�;
*******************************************************************************/

#include "CH552.H"
#include "DEBUG.h"

/*******************************************************************************
* Function Name  : CfgFsys( )
* Description    : CH552ʱ��ѡ������ú���,Ĭ��ʹ���ڲ�����12MHz�����������FREQ_SYS����
                   ����PLL_CFG��CLOCK_CFG���õõ�����ʽ���£�
                   Fsys = (Fosc * ( PLL_CFG & MASK_PLL_MULT ))/(CLOCK_CFG & MASK_SYS_CK_DIV);
                   ����ʱ����Ҫ�Լ�����
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void CfgFsys()
{
#ifdef OSC_EN_XT
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    CLOCK_CFG |= bOSC_EN_XT;   //ʹ���ⲿ����
    CLOCK_CFG &= ~bOSC_EN_INT; //�ر��ڲ�����
#endif
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
#if FREQ_SYS == 24000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x06; // 24MHz
#endif
#if FREQ_SYS == 16000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x05; // 16MHz
#endif
#if FREQ_SYS == 12000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x04; // 12MHz
#endif
#if FREQ_SYS == 6000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x03; // 6MHz
#endif
#if FREQ_SYS == 3000000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x02; // 3MHz
#endif
#if FREQ_SYS == 750000
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x01; // 750KHz
#endif
#if FREQ_SYS == 187500
    CLOCK_CFG = CLOCK_CFG & ~MASK_SYS_CK_SEL | 0x00; // 187.5KHz
#endif
	//GLOBAL_CFG = GLOBAL_CFG | bLDO3V3_OFF;//3.3V LDO OFF 
    SAFE_MOD = 0xFF;
}

/*******************************************************************************
* Function Name  : mDelayus(UNIT16 n)
* Description    : us��ʱ����
* Input          : UNIT16 n
* Output         : None
* Return         : None
*******************************************************************************/
void mDelayuS(UINT16 n) // ��uSΪ��λ��ʱ
{
#ifdef FREQ_SYS
#if FREQ_SYS <= 6000000
    n >>= 2;
#endif
#if FREQ_SYS <= 3000000
    n >>= 2;
#endif
#if FREQ_SYS <= 750000
    n >>= 4;
#endif
#endif
    while (n)
    {
        // total = 12~13 Fsys cycles, 1uS @Fsys=12MHz
        ++SAFE_MOD; // 2 Fsys cycles, for higher Fsys, add operation here
#ifdef FREQ_SYS
#if FREQ_SYS >= 14000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 16000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 18000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 20000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 22000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 24000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 26000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 28000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 30000000
        ++SAFE_MOD;
#endif
#if FREQ_SYS >= 32000000
        ++SAFE_MOD;
#endif
#endif
        --n;
    }
}

/*******************************************************************************
* Function Name  : mDelayms(UNIT16 n)
* Description    : ms��ʱ����
* Input          : UNIT16 n
* Output         : None
* Return         : None
*******************************************************************************/
void mDelaymS(UINT16 n) // ��mSΪ��λ��ʱ
{
    while (n)
    {
#ifdef DELAY_MS_HW
        while ((TKEY_CTRL & bTKC_IF) == 0)
            ;
        while (TKEY_CTRL & bTKC_IF)
            ;
#else
        mDelayuS(1000);
#endif
        --n;
    }
}
