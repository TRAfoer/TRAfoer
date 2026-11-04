#include "JsonLib/jsonLib.h"
#include "NoteDef.h"

int ReadRaw(const char *FileName,struct Note* output,int* count){
    FILE *f=fopen(FileName,"r");
    if(f==NULL) return 1;
    char buffer[256];
    int ccount=0;
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        struct Note n;
        int ifn=sscanf(buffer,"%d %f %f",&n.index,&n.Beats,&n.HoldBeat);
        if(ifn==3){
            output[ccount]=n;
            ccount++;
            printf("loadNote %d %f %f",n.index,n.Beats,n.HoldBeat);
        }
    }
    *count=ccount;
    fclose(f);
}