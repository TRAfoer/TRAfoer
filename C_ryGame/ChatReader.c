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

int ReadMalodyJson(char *FileName,struct Note *output,int *count,int *BPM){//未完成
    FILE *f = fopen(FileName, "rb");
    char *buffer;
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length + 1);
        if (buffer)
        {
            size_t read_len = fread(buffer, 1, length, f);
            buffer[read_len] = '\0';
        }
        fclose(f);
    }
    pJsonObjectNode p = JSONParse(buffer);
    if (p == NULL)
        return 1;
    pJsonArrayNode pArr = NULL;
    JSONObjectGetArrayAttr(p, "note", &pArr);
    if (pArr == NULL)
        return 2;
    int leng = JSONArrayGetLength(pArr);
    output=malloc(sizeof(struct Note)*(leng+1));
    *count=leng;

    pJsonArrayNode timeArr;
    JSONObjectGetArrayAttr(p, "time", timeArr);
    JSONNodeGetNumberValue(JSONArrayGetNode(timeArr,0),BPM);
    
    for (int i = 0; i < leng; ++i)
    {
        pJsonObjectNode nds = JSONArrayGetNode(pArr,i);
        pJsonArrayNode subarr;
        JSONObjectGetArrayAttr(nds,"beat",subarr);
        double b,a,c;
        JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(subarr, 0), &b);
        JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(subarr, 1), &a);
        JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(subarr, 2), &c);
        double beats=b+(a/c);

        JSONObjectGetArrayAttr(nds, "endbeat", subarr);
        double HoldBeat=-1;
        if(subarr!=NULL){
            JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(subarr, 0), &b);
            JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(subarr, 1), &a);
            JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(subarr, 2), &c);
            HoldBeat=b+(a/c);
        }
        double index;
        pJsonObjectNode idx;
        JSONObjectGetObjectAttr(nds, "column", &idx);
        JSONNodeGetNumberValue((pJsonNumberNode)idx, &index);
        struct Note n;
        printf("%d %.5f %.5f",index ,beats,HoldBeat);
        n.Beats=beats;
        n.HoldBeat=HoldBeat;
        n.index=(int)index;
    }
        
    
    
    
    
    
    free(buffer);
    JSONDestroy((pJsonNode)p);
}



int main(){
    int count,Bpm;
    ReadMalodyJson("Test.mc",NULL,&count,&Bpm);
}