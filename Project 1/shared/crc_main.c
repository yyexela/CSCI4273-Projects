/**********************************************************************
 *
 * Filename:    main.c
 *
 * Description: Main file
 *
 * Notes:
 *
 *
 *
 * Copyright (c) 2014 Francisco Javier Lana Romero
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc.h"

int main() 
{

    int8_t A_Texto[40];
    crc CRC_Result = 0;

	printf("CRC32 calculator\r\n");
	printf("For other CRCs change #define CRC_32 value in the crc.h file\r\n");
	printf("Insert value and press enter:");

	fgets((void *) A_Texto, 40, stdin);
    
	
    F_CRC_InicializaTabla();
    CRC_Result = F_CRC_CalculaCheckSum((void *) A_Texto, strlen((char *) A_Texto)-1);
	printf("Result: 0x%X", CRC_Result);
	getchar(); 
    return 0;
}

