#include "test.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int number(const char *text,unsigned *out) {
    char *end;unsigned long value;errno=0;value=strtoul(text,&end,10);
    if(errno||!*text||*end||text[0]=='-'||value>4294967295UL) return 0;
    *out=(unsigned)value;return 1;
}
int main(int argc,char **argv) {
    int i,smoke=0,scenarios=0,verbose=0,selected=-1,failures=0;
    unsigned seed=2026,runs=1,parsed,run;
    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--smoke")) smoke=1;
        else if(!strcmp(argv[i],"--scenarios")) scenarios=1;
        else if(!strcmp(argv[i],"--verbose")) verbose=1;
        else if((!strcmp(argv[i],"--seed")||!strcmp(argv[i],"--runs")||!strcmp(argv[i],"--scenario")) && i+1<argc) {
            const char *option=argv[i++];if(!number(argv[i],&parsed)) {fprintf(stderr,"Invalid number: %s\n",argv[i]);return 2;}
            if(!strcmp(option,"--seed")) seed=parsed;
            else if(!strcmp(option,"--runs")) {if(!parsed||parsed>1000) return 2;runs=parsed;}
            else {if(parsed>=SH_SCENARIOS) return 2;selected=(int)parsed;scenarios=1;}
        } else {fprintf(stderr,"Usage: %s [--smoke] [--scenarios | --scenario 0..8] [--seed N] [--runs 1..1000] [--verbose]\n",argv[0]);return 2;}
    }
    if(!smoke&&!scenarios) smoke=scenarios=1;
    if(smoke) failures+=smoke_tests();
    if(scenarios) for(run=0;run<runs;run++) for(i=0;i<SH_SCENARIOS;i++) if(selected<0||selected==i) {
        char path[120];snprintf(path,sizeof path,"build/scenario-%02d-seed-%u.log",i,seed+run);
        failures+=run_scenario(i,seed+run,verbose,path);
    }
    printf("\n%s: %d failed check(s).\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
