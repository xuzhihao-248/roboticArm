#include "main.h"

extern uint8_t vofa_char[64];
void copy_to_vofa(uint8_t c[4],uint8_t start);//将一个要发送的数据复制到vofa缓存
void float_to_char(float f,unsigned char *s);//小数转换成字符串，上位机调试用
float char_to_float(uint8_t *s);//////数组转换成小数


