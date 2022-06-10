/*
 * @Author: your name
 * @Date: 2021-10-27 10:37:34
 * @LastEditTime: 2021-10-27 10:37:35
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /MySQL_lessions/docs/unsigned2.c
 */
#include <stdio.h>
int main() {
    unsigned int a;
    unsigned int b;
    a = 1;
    b = 2;
    printf("a - b:   %d, %x\n", a-b , a-b);
    printf("a - b:   %u, %x\n", a-b, a-b );
    return 1;
}