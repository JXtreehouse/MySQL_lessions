/*
 * @Author: your name
 * @Date: 2021-10-27 10:27:47
 * @LastEditTime: 2021-10-27 10:32:02
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /MySQL_lessions/docs/unsigned.c
 */
#include <stdio.h>
int main() {
    unsigned int a;
    unsigned int b;
    a = 1;
    b = 2;
    printf("a - b:   %d\n", a -b );
    printf("a - b:   %u\n", a -b );
    return 1;
}