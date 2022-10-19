#include <stdio.h>
#include <string.h>
#include <unistd.h>

int get_md5(char *str, char *ret){
  FILE *cmd_response;

  if((cmd_response = popen(str, "r")) == NULL){
      printf("ERROR calling \"popen\" with command \"%s\"\n", str);
      return -1;
  }

  size_t pos = 0;
  size_t bytes_read = 0;
  size_t bytes_left = 32;
  while(pos != 32){
      if((bytes_read = fread(ret + pos, 1, bytes_left, cmd_response)) < 0){
          return -1;
      }
      pos += bytes_read;
  }

  if(pclose(cmd_response)){
    printf("ERROR with pclose\n");
  }
  return 0;
}

int main(int argc, char * argv[]){
    if(argc != 2 || strlen(argv[1]) > 8192){
        printf("Usage: get_md5 <text>\n");
        printf("<text> cannot exceed 8192 bytes\n");
        return 0;
    }
    
    char str[8192];
    char ret[33];
    memset(ret, '\0', 33);
    strcpy(str, "echo ");
    strcpy(str + 5, argv[1]);
    strcpy(str + 5 + strlen(argv[1]), " | md5sum");

    printf("Running '%s'\n", str);

    get_md5(str, ret);

    printf("%s\n", ret);
}