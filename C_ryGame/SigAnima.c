#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
// #define SIGANIMA_IMPLEMENTATION
#ifdef SIGANIMA_IMPLEMENTATION
#define CLAMP(val,dw,up) val<dw?dw:(val>up?up:val)

typedef struct SIGAnimation
{
    float startTime;
    float duration;
    float power;

}SIGAnimation;
typedef struct SigAnimaGroup
{
    struct SIGAnimation* Animas;
    int count;
    int limit;
}SigAnimaGroup;
static SigAnimaGroup InitAnimaGroup(int initCount){
    SigAnimaGroup gp;
    gp.Animas=malloc(initCount*sizeof(SIGAnimation));
    gp.count=0;
    gp.limit=initCount;
}
static void AddAnima(SigAnimaGroup gp,SIGAnimation anima){
    if(gp.limit-gp.count<1){
        gp.limit*=2;
        gp.Animas=realloc(gp.Animas,gp.limit*sizeof(SIGAnimation));
        if(gp.Animas==NULL)printf("ERRORRRRRRRRRRRRRRRRRRR!");
    }
    gp.Animas[gp.count]=anima;
    gp.count+=1;
}
#define lerp(i,a,b) (i-a)/(b-a)
static float Evaluate(SIGAnimation anima,float time){
    if(time-anima.startTime<=0)return 0;
    if(time-anima.startTime>=anima.duration)return 1;
    return pow(lerp(time,anima.startTime,anima.startTime+anima.duration),anima.power);
}











#endif