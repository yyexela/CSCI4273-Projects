#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(){
    printf("Timer running\n");
    clock_t start = clock();
    sleep(20); // NOTE: sleep pauses the clock too :(
    clock_t end = clock();
    printf("Time in s %f\n", (double) ((end - start + 0.0)/CLOCKS_PER_SEC));
}