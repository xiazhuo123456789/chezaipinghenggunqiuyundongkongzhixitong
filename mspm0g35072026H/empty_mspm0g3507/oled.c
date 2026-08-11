/*
 * oled.c — SSD1306 0.96" OLED, SDA=PA0 SCL=PA1
 * 引脚宏来自 init.h (OLED_SDA_PORT, OLED_SDA_PIN, OLED_SCL_PORT, OLED_SCL_PIN 等)
 */
#include "oled.h"
#include <string.h>

static uint8_t buf[128*8];

#define sda_1()  do{DL_GPIO_enableOutput(OLED_SDA_PORT,OLED_SDA_PIN);DL_GPIO_setPins(OLED_SDA_PORT,OLED_SDA_PIN);}while(0)
#define sda_0()  do{DL_GPIO_enableOutput(OLED_SDA_PORT,OLED_SDA_PIN);DL_GPIO_clearPins(OLED_SDA_PORT,OLED_SDA_PIN);}while(0)
#define scl_1()  do{DL_GPIO_setPins(OLED_SCL_PORT,OLED_SCL_PIN);}while(0)
#define scl_0()  do{DL_GPIO_clearPins(OLED_SCL_PORT,OLED_SCL_PIN);}while(0)
#define sda_r()  (DL_GPIO_readPins(OLED_SDA_PORT,OLED_SDA_PIN)?1:0)

#define D delay_cycles(600)

static void start(void) { sda_1();D;scl_1();D;sda_0();D;scl_0();D; }
static void stop(void)  { sda_0();D;scl_1();D;sda_1();D; }
static void wb(uint8_t b) {
    for(uint8_t m=0x80;m;m>>=1){ if(b&m)sda_1();else sda_0();D;scl_1();D;scl_0();D; }
    /* ACK: 切输入读, 不检查 */
    DL_GPIO_disableOutput(OLED_SDA_PORT,OLED_SDA_PIN); D; scl_1(); D; scl_0(); D;
    sda_0();
}
static void cmd(uint8_t c){ start();wb(OLED_I2C_ADDR<<1);wb(0x00);wb(c);stop(); }

void OLED_Init(void)
{
    DL_GPIO_initDigitalOutput(OLED_SDA_IOMUX); /* PA0 */
    DL_GPIO_initDigitalOutput(OLED_SCL_IOMUX); /* PA1 */
    DL_GPIO_enableOutput(OLED_SDA_PORT, OLED_SDA_PIN|OLED_SCL_PIN);
    DL_GPIO_setPins(OLED_SDA_PORT, OLED_SDA_PIN|OLED_SCL_PIN);
    delay_cycles(32000UL*200);

    static const uint8_t init[]={
        0xAE, 0xD5,0x80, 0xA8,0x3F, 0xD3,0x00, 0x40,
        0x8D,0x14, 0x20,0x00, 0xA1, 0xC8, 0xDA,0x12,
        0x81,0xCF, 0xD9,0xF1, 0xDB,0x40, 0xA4, 0xA6, 0xAF
    };
    for(uint8_t i=0;i<sizeof(init);i++) cmd(init[i]);

    OLED_Clear();
}

void OLED_Clear(void) { memset(buf,0,sizeof(buf)); OLED_Refresh(); }

void OLED_Refresh(void)
{
    for(uint8_t p=0;p<8;p++) {
        cmd(0xB0|p); cmd(0x21); cmd(0); cmd(127); /* horizontal: col 0..127 */
        start(); wb(OLED_I2C_ADDR<<1); wb(0x40);
        for(uint16_t c=0;c<128;c++) wb(buf[(uint16_t)p*128+c]);
        stop();
    }
}

/* 把 6x8 字符的 6 列写入 page 的 buf 中 */
static void put_char_buf(uint8_t page, uint8_t col, char ch)
{
    if(ch<' '||ch>'~') return;
    const uint8_t *f = asc2_0806[ch-' '];
    for(uint8_t i=0;i<6;i++)
        buf[(uint16_t)page*128 + col + i] = f[i];
}

void OLED_ShowString(uint8_t x, uint8_t page, const char *s, uint8_t sz)
{
    (void)sz;
    while(*s) {
        put_char_buf(page, x, *s);
        x += 6; if(x>122){x=0;page++;} s++;
    }
}

void OLED_ShowNum(uint8_t x, uint8_t page, int32_t n, uint8_t len, uint8_t sz)
{
    char b[12]; uint8_t i=0;
    if(n<0){b[i++]='-';n=-n;}
    uint8_t j=i; do{b[i++]='0'+n%10;n/=10;}while(n);
    while(i-j<len)b[i++]=' ';
    for(uint8_t a=j,z2=i-1;a<z2;a++,z2--){char t=b[a];b[a]=b[z2];b[z2]=t;}
    b[i]=0;
    OLED_ShowString(x,page,b,sz);
}

void OLED_ShowPage(uint8_t pg)
{
    OLED_Clear();
    switch(pg){
    case 1:
        OLED_ShowString(0, 0, "Hello World!", 0);
        break;
    case 2:
        OLED_ShowString(0, 0, "TRACKING", 0);
        OLED_ShowString(0, 2, "K2:GO  K3:STOP", 0);
        break;
    case 3: OLED_ShowString(0, 0, "SERVO TEST", 0); break;
    case 4: OLED_ShowString(0, 0, "OPENLOOP", 0); break;
    case 5: OLED_ShowString(0, 0, "VISION", 0);
            OLED_ShowString(0, 2, "K2:GO  K3:STOP", 0); break;
    }
    OLED_Refresh();
}
