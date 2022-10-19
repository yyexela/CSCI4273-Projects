#include <stdio.h>
#include <time.h>

#define TIMER_TIME 3

int main(int argc, char * argv[]){
    struct timespec start, finish;
    printf("start initialized to (%lu) seconds:\n", start.tv_sec);
    printf("Starting timer for %d seconds:\n", TIMER_TIME);
    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    long diff = finish.tv_sec - start.tv_sec;
    while(diff <= (time_t) TIMER_TIME){
        clock_gettime(CLOCK_MONOTONIC, &finish);
        diff = finish.tv_sec - start.tv_sec;
    }
    printf("TIMED OUT\n");
}
