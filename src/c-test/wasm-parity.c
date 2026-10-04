/* Native bridge oracle: compare every snapshot with the WebAssembly bridge. */
#include <stdio.h>
#include <stdlib.h>
extern int start(int,unsigned,int,int);
extern int command(int,int,int,int,int);
extern const char *snapshot(int);
void browser_feedback(int type,int actor,int target,int row,int col,const char *message,int round) {
    (void)type;(void)actor;(void)target;(void)row;(void)col;(void)message;(void)round;
}
int main(int argc,char **argv) {
    int op,a,b,c,d,result;
    if(argc!=3||!start(atoi(argv[1]),2026,(int)strtol(argv[2],NULL,10),0))return 1;
    puts(snapshot(0));
    while(scanf("%d %d %d %d %d",&op,&a,&b,&c,&d)==5) {
        result=command(op,a,b,c,d);printf("%d\n%s\n",result,snapshot(0));
    }
    return 0;
}
