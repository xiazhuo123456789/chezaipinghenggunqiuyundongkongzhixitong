#ifndef __KEY_H
#define __KEY_H
#include "init.h"   /* 引脚宏来自 init.h */
#ifdef __cplusplus
extern "C" {
#endif

void Key_Init(void);
uint8_t Key_Read(void);
void    Key_Tick(void);
uint8_t Key_GetNum(void);

#ifdef __cplusplus
}
#endif
#endif
