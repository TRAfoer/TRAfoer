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
int ReadMalodyJson(const char *FileName, struct Note **output, int *count, float *BPM) {
    FILE *f = fopen(FileName, "rb");
    if (f == NULL) return 1;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    if (buffer == NULL) { fclose(f); return 1; }
    size_t read_len = fread(buffer, 1, length, f);
    buffer[read_len] = '\0';
    fclose(f);

    pJsonObjectNode p = JSONParse(buffer);
    if (p == NULL) { free(buffer); return 1; }

    pJsonArrayNode pArr = NULL;
    JSONObjectGetArrayAttr(p, "note", &pArr);
    if (pArr == NULL) {
        free(buffer);
        JSONDestroy((pJsonNode)p);
        return 2;
    }

    int leng = JSONArrayGetLength(pArr);
    // JSONPrint((pJsonNode)pArr);
    // *output = malloc(sizeof(struct Note) * leng);
    // if (*output == NULL) {
    //     free(buffer);
    //     JSONDestroy((pJsonNode)p);
    //     return 3;
    // }
    *count = leng;

    // BPM：time[0] 是对象，取其中的 "bpm"
    pJsonArrayNode timeArr = NULL;
    JSONObjectGetArrayAttr(p, "time", &timeArr);
    if (timeArr != NULL && JSONArrayGetLength(timeArr) > 0) {
        pJsonObjectNode timeObj = (pJsonObjectNode)JSONArrayGetNode(timeArr, 0);
        double bpm = 0;
        JSONObjectGetNumberAttr(timeObj, "bpm", &bpm);
        *BPM = bpm;
    }

    for (int i = 0; i < leng; ++i) {
        pJsonObjectNode nds = (pJsonObjectNode)JSONArrayGetNode(pArr, i);

        // beat
        pJsonArrayNode beatArr = NULL;
        JSONObjectGetArrayAttr(nds, "beat", &beatArr);
        if (beatArr == NULL || JSONArrayGetLength(beatArr) < 3) continue;

        double b, a, c;
        JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(beatArr, 0), &b);
        JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(beatArr, 1), &a);
        JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(beatArr, 2), &c);
        double beats = b + a / c;
        
        // endbeat：独立变量，先置 NULL
        double HoldBeat = -1;
        pJsonArrayNode endArr = NULL;
        JSONObjectGetArrayAttr(nds, "endbeat", &endArr);
        if (endArr != NULL && JSONArrayGetLength(endArr) >= 3) {
            JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(endArr, 0), &b);
            JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(endArr, 1), &a);
            JSONNodeGetNumberValue((pJsonNumberNode)JSONArrayGetNode(endArr, 2), &c);
            HoldBeat = (b + a / c)-beats;
        }
        
        // column 是数字，不是对象
        double col = 0;
        JSONObjectGetNumberAttr(nds, "column", &col);
        
        (*output)[i].index = (int)col;
        (*output)[i].Beats = beats;
        (*output)[i].HoldBeat = HoldBeat;
        // printf("%.1f %.1f %.1f ",b,a,c);
        // printf("%d %lf %lf \n",(int)col,beats,HoldBeat);

    }

    free(buffer);
    JSONDestroy((pJsonNode)p);
    return 0;
}



// int main(){
//     int count,Bpm;
//     struct Note *out;
//     ReadMalodyJson("Test.json",&out,&count,&Bpm);
//     for(int i=0;i<count;++i){
//         printf("%d %lf %lf \n",out[i].index,out[i].Beats,out[i].HoldBeat);
//     }
// }