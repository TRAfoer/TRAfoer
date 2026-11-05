#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<time.h>
#include<math.h>
#include<stdint.h>
#include<stdbool.h>

#define TIGR_IMPLEMENTATION
#define OLIVEC_IMPLEMENTATION
#define SIGANIMA_IMPLEMENTATION
#include "tigr.h"
#include "olive.c"
#include "SigAnima.c"
#include "openPPMp6.c"
#include "miniaudio.h"
#include "ChatReader.c"
#include "NoteDef.h"
#include "timeTicker.h"


#define WIDTH 800
#define HIGHT 600
#define PI 3.1415926

#define PTX 15
#define PTY 15
#define PTZ 15
#define PTMUL 160

uint32_t *ImageData;
uint32_t *TextureData;
Olivec_Canvas textureCanvas;

void my_exit(void)
{
    int _=system("clear");
    printf("Exit with Clear \n");

}
#define CLAMP(val,dw,up) val<dw?dw:(val>up?up:val)
#define INDEX(i, j) i *HIGHT + j

Olivec_Canvas canvas;
ma_engine engine;
ma_sound  sound;
bool engineisstop=true;

char Keys[] = {'D', 'F', 'J', 'K'};
uint32_t Cols[]={OLIVEC_RGBA(220,20,60,255),OLIVEC_RGBA(255,69,0,255),
                 OLIVEC_RGBA(240,230,140,255),OLIVEC_RGBA(250,250,210,255)};
SIGAnimation TrackAnimas[4];
bool pressState[4] = {
    false,
    false,
    false,
    false};

int sigWidth = 130;
float downMul=0.9;
float speedMul=450.0;
float TrackOffset=25;
float BPM=170;
float offset=0;


double CurTime=-2;
float AnimaVals[4];

void InitNotes(struct Note *nts,int *count){
    ReadMalodyJson("Test.json",&nts,count,&BPM);
}
void DrawNotes(struct Note *nts,int count,float CurB){
    float valdelta[6];
    valdelta[0] =0;//-AnimaVals[0]             ;
    valdelta[1] =0;//+AnimaVals[0]-AnimaVals[1];
    valdelta[2] =0;//+AnimaVals[1]-AnimaVals[2];
    valdelta[3] =0;//+AnimaVals[2]-AnimaVals[3];
    valdelta[4] =0;//+AnimaVals[3]             ;
    const int NtThickness=20;
    for(int i =0;i<count;++i){
        uint32_t col=Cols[nts[i].index];
        int posY=(HIGHT*downMul)-((nts[i].Beats-CurB)*speedMul)-(NtThickness/2);
        if(nts[i].Beats<CurB)col=OLIVEC_RGBA(OLIVEC_RED(col),OLIVEC_GREEN(col),OLIVEC_BLUE(col),50);
        if(posY<0||posY>HIGHT)continue;
        int posX=((nts[i].index)*sigWidth)+(WIDTH/2-2*sigWidth);
        olivec_rect(canvas,posX+valdelta[nts[i].index],posY,sigWidth-+valdelta[nts[i].index]+valdelta[nts[i].index+1],NtThickness,col);
        if(nts[i].HoldBeat<=0)continue;

        int HoldLong=(HIGHT*downMul)-(((nts[i].Beats+nts[i].HoldBeat)-CurB)*speedMul)-(NtThickness);
        olivec_rect(canvas,posX+valdelta[nts[i].index]+5,HoldLong,sigWidth-valdelta[nts[i].index]+valdelta[nts[i].index+1]-10,posY-HoldLong,col/2);

}}
void DrawLiness(){
    int liness[]={
        (WIDTH/2-2*sigWidth)/*-AnimaVals[0]             */,
        (WIDTH/2-sigWidth  )/*+AnimaVals[0]-AnimaVals[1]*/,
        (WIDTH/2           )/*+AnimaVals[1]-AnimaVals[2]*/,
        (WIDTH/2+sigWidth  )/*+AnimaVals[2]-AnimaVals[3]*/,
        (WIDTH/2+2*sigWidth)/*+AnimaVals[3]             */,
    };
    
    for(int i=0;i<4;++i){
        uint32_t Upcol = pressState[i]?0xff222222:0xff000000;
        // olivec_blend_color(&Upcol, Cols[i] * AnimaVals[i]);
        olivec_triangle3c(canvas, liness[i] + 1, 0, liness[i] + 1, (HIGHT - 1) * downMul, liness[i + 1] - 1, (HIGHT - 1) * downMul, Upcol, Cols[i] / 2, Cols[i] / 2);
        olivec_triangle3c(canvas, liness[i + 1] - 1, (HIGHT - 1) * downMul, liness[i + 1] - 1, 0, liness[i] + 1, 0, Cols[i] / 2, Upcol, Upcol);
    }
    olivec_line(canvas,0,HIGHT*downMul,WIDTH-1,HIGHT*downMul,0xffffffff);
}
void InputDetect(Tigr *screen, TimeTicker *ticker, double now){
    if(tigrKeyDown(screen,TK_SPACE)){
        TimeTickerSetRunning(ticker, !ticker->running, now);
    }
    if(tigrKeyDown(screen,TK_UP)){
        TimeTickerSetTime(ticker, TimeTickerGetTime(ticker, now) + 0.1, now);
    }
    if(tigrKeyDown(screen,TK_DOWN)){
        TimeTickerSetTime(ticker, TimeTickerGetTime(ticker, now) - 0.1, now);
    }
    CurTime = TimeTickerGetTime(ticker, now);
    int grad=rand()%100;
    for(int i=0;i<4;++i){
        if(tigrKeyDown(screen,Keys[i])){
            pressState[i]=true;
            TrackAnimas[i].startTime=CurTime;
            // printf("%c was down at %d\n",Keys[i],grad);
        }
        else if (pressState[i] &&!tigrKeyHeld(screen,Keys[i])){
            pressState[i] = false;
            TrackAnimas[i].startTime = CurTime;
        }
    }
}

signed main()
{
    
    TextureData=malloc(128*128*sizeof(uint32_t));
    ImageData=malloc(256*256*sizeof(uint32_t));
    atexit (my_exit);
    srand((uint32_t)time(NULL));
    Tigr *screen = tigrWindow(WIDTH, HIGHT, "TEST-ONE", TIGR_FIXED);
    canvas=olivec_canvas((uint32_t*)(screen->pix), WIDTH, HIGHT, WIDTH);
    for(int i=0;i<4;++i){
        TrackAnimas[i].startTime=-1;
        TrackAnimas[i].power=0.2;
        TrackAnimas[i].duration=0.05;
    }
    
    //图片读取部分
    int pw,ph;
    int err=openPPMp6("test.ppm",ImageData,&pw,&ph);
    if(err)printf("ERR when1!");
    int tw,th;
    err=openPPMp6("TEXTURE.ppm",TextureData,&tw,&th);
    if(err)printf("ERR when2!");

    if (ma_engine_init(NULL, &engine) != MA_SUCCESS)
    {
        return -1;
    }
    if (ma_sound_init_from_file(&engine, "World Vanquisher.wav", 0, NULL, NULL, &sound) != MA_SUCCESS)
    {
        return -1;
    }

        Olivec_Canvas image;
    image=olivec_canvas(ImageData, pw, ph, pw);
    for(int i=0;i<pw;++i)
    for(int j=0;j<ph;++j){
        int ctrX=i-pw/2;
        int ctrY=j-ph/2;
        if(ctrX*ctrX+ctrY*ctrY>((pw/2+ph/2)/2)*((pw/2+ph/2)/2)){
            OLIVEC_PIXEL(image,i,j)=0x00000000;
    }}
    textureCanvas=olivec_canvas(TextureData,tw,th,tw);

    int Notecount=0;
    struct Note *nts=malloc(sizeof(struct Note)*25565);
    InitNotes(nts,&Notecount);
    
    double TpB=60.0/BPM;

    if(sizeof(Keys)/sizeof(char)<4){
        return 1;
    }
    TimeTicker ticker;
    double lastFrameTime = ClockNow();
    TimeTickerInit(&ticker, CurTime + offset, lastFrameTime);
    while (!tigrClosed(screen)&&!tigrKeyHeld(screen,'Q'))
    {
        double now = ClockNow();
        double Dt = now - lastFrameTime;
        lastFrameTime = now;
        InputDetect(screen, &ticker, now);
        if(ticker.running && CurTime>=0 && engineisstop){
            engineisstop=false;
            ma_sound_start(&sound);
        }else if(!ticker.running && !engineisstop){
            engineisstop=true;
            ma_sound_stop(&sound);
        }
        double Bts=CurTime/TpB;
        for(int i=0;i<4;++i)
            AnimaVals[i] = !pressState[i] ? (1.0 - Evaluate(TrackAnimas[i], CurTime)) : ( Evaluate(TrackAnimas[i], CurTime));
        float all=0;
        for(int i=0;i<4;++i) all+=CurTime<0?0:AnimaVals[i];

        uint32_t addCol=OLIVEC_RGBA((int)(255*all/8),(int)(255*all/8),(int)(255*all/8),50);
        olivec_blend_color(&addCol,0xaa181818);
        olivec_fill(canvas,addCol);
       
        {
            float triangle_angle = 0.5f * PI * CurTime;

            float ps[4][2];
            float uvs[4][2] = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1},
            };
            float len = WIDTH / 16 + (all + (pow(1.0-fmod(Bts+1000, 1.0), 3))) * 10;
            for (size_t i = 0; i < 4; ++i)
            {
                ps[i][0] = WIDTH / 14 + cosf(PI / 2 * i + triangle_angle) * len;
                ps[i][1] = HIGHT / 8 + 10 + sinf(PI / 2 * i + triangle_angle) * len;
            }
        for (size_t i = 0; i < 2; ++i) {
            int i1 = (i*2 + 0)%4;
            int i2 = (i*2 + 1)%4;
            int i3 = (i*2 + 2)%4;
            olivec_triangle3uv(
                canvas,
                ps[i1][0], ps[i1][1],
                ps[i2][0], ps[i2][1],
                ps[i3][0], ps[i3][1],
                uvs[i1][0], uvs[i1][1],
                uvs[i2][0], uvs[i2][1],
                uvs[i3][0], uvs[i3][1],
                1, 1, 1,
                image
            );
       }
        
        }
        
        DrawLiness();
        DrawNotes(nts,Notecount,Bts);

        char str1[32];
        char str2[32];
        sprintf(str1,"%d %7.5lf %7.5lf",Notecount,CurTime,Bts);
        sprintf(str2,"dt equ %7.5lf  q 2 quit",Dt);
        olivec_text(canvas,str1,10,HIGHT-100,olivec_default_font,2,0xffFFBF00);
        olivec_text(canvas,str2,10,HIGHT-100+20,olivec_default_font,2,0xffFFBF00);
        const char ct[]="space 2 ctn and q 2 qt";
        const char ct2[]="olivec is fucking good";
        if(!ticker.running){
            olivec_rect(canvas,0,0,WIDTH-1,HIGHT-1,0xaa000000);
            olivec_text(canvas,ct,WIDTH/2-((sizeof(ct)/sizeof(char))*olivec_default_font.width/2)*4,HIGHT/2-100,olivec_default_font,4,0xffffffff);
            olivec_text(canvas,ct2,WIDTH/2-((sizeof(ct2)/sizeof(char))*olivec_default_font.width/2)*4,HIGHT/2,olivec_default_font,4,0xffffffff);
            // TPixel p;p.a=255,p.r=255,p.g=255,p.b=255;
            // tigrPrint(screen,tfont,0,0,p,ct);
        }
        tigrUpdate(screen);
    }
    ma_engine_uninit(&engine);
    tigrFree(screen);
    return 0;
}
