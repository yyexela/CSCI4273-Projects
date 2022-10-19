#include <stdio.h>
#include <stdlib.h>

int main(int argc, char * argv[]){
    printf("Running DFS_folders.sh\n");
    int ret = system("./DFS_folders.sh");
    if (ret != 0){
        printf("Error with ./DFS_folders.sh\n");
    }
    return 0;
}