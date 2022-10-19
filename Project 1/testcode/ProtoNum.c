#include<stdio.h>
#include<netdb.h>

int main(int argc, char * argv[]){
	if(argc != 2){
		printf("usage: test <protocol name>\n");
	}
	struct protoent *proto;
	proto = getprotobyname(argv[1]);
	if(proto == NULL){
		printf("ERROR\n");
		return -1;
	}
	printf("Protocol number for %s is %d\n", argv[1], proto->p_proto);
}
