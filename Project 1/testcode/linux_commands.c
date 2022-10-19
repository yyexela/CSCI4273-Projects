#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char * argv[]){
    if(argc < 2){
        printf("Usage: ./linux_commands.exe [ \"ls\" | \"delete [FILE]\" ]\n");
        exit(0);
    }

    FILE * pipe; // pipe to read from after executing command

    if(strcmp(argv[1], "ls") == 0){
        if(argc != 2){
            printf("Usage: ./linux_commands.exe [ \"ls\" | \"delete [FILE]\" ]\n");
            exit(0);
        }

        char buf[1000]; // BUFFER for output of the command
        memset(buf, 0, 1000); // Clear the buffer first

        if((pipe = popen(argv[1], "r")) == NULL){
            printf("ERROR calling \"popen\" with command \"%s\"\n", argv[1]);
        }

        uint32_t i = 0; // How many characters is in the output
        char c;
        while((c = fgetc(pipe)) != EOF){
            buf[i] = c;
            i++;
        }
        buf[i+1] = '\0'; // Set null-terminating

        for(int j = 0; j < i+i; j++){
            printf("%c", buf[j]);
        }
        printf("\n");
        //printf("%s\n", buf);
    } else if(strcmp(argv[1], "delete") == 0){
        if(argc != 3){
            printf("Usage: ./linux_commands.exe [ \"ls\" | \"delete [FILE]\" ]\n");
            exit(0);
        }

        char cmd[1000]; // We'll construct the command in here, since we need to concatenate "rm" with "[FILE]"
        memset(cmd, 0, 1000); // Clear the char array first

        // Concatenate "rm " with "[FILE]"
        strcpy(cmd, "rm ");
        strncpy(cmd + 3, argv[2], strlen(argv[2]));
        printf("Executing \"%s\", 5 secs to abort\n", cmd);
        sleep(5);

        if((pipe = popen(cmd, "r")) == NULL){
            printf("ERROR calling \"popen\" with command \"%s\"\n", cmd);
        }
    }


    pclose(pipe);
}