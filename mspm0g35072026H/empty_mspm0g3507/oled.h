#ifndef __OLED_H
#define __OLED_H

#include "init.h"       /* 引脚宏来自 init.h */
#include "oledfont.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size);
void OLED_Refresh(void);
void OLED_ShowPage(uint8_t page);   /* 菜单页索引 */

#endif
