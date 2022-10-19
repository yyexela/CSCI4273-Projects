#include <stdio.h>
#include <string.h>

int main(){
    FILE * file_in = fopen("testfile_read.txt", "r");
    FILE * file_out = fopen("testfile_write.txt", "w+");

    char c;

    while((c = fgetc(file_in)) != EOF){
        fputc(c, file_out);
    }
}