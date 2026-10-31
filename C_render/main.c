#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<time.h>
#include<math.h>
#include<stdint.h>

#define TIGR_IMPLEMENTATION
#define OLIVEC_IMPLEMENTATION
#include "tigr.h"
#include "olive.c"
#include "openPPMp6.c"

#define WIDTH 800
#define HIGHT 600
#define SOILD_BLOCK_HALF_UP "▀"//

uint64_t timeDt=0;
uint32_t *data;
#define RandomCol 0
void my_exit(void)
{
    system("clear");
    printf("Exit with Clear \n");
    if(data!=NULL)free(data);
}
signed main(){
    data=malloc(256*256*sizeof(uint32_t));
    atexit (my_exit);
    srand((uint32_t)time(NULL));
    Tigr *screen = tigrWindow(WIDTH, HIGHT, "位图显示", TIGR_FIXED);
    Olivec_Canvas canvas=olivec_canvas((uint32_t*)(screen->pix), WIDTH, HIGHT, WIDTH);
    
    //图片读取部分
    int pw,ph;
    int err=openPPMp6("test.ppm",data,&pw,&ph);
    if(err)printf("ERR!");
    Olivec_Canvas image;
    image=olivec_canvas(data, pw, ph, pw);
    if(RandomCol){
        for(int i=0;i<ph;++i)
        for(int j=0;j<pw;++j)
        // OLIVEC_PIXEL(image, i, i)
        image.pixels[   i*pw+j]=OLIVEC_RGBA(rand()%255,rand()%255,rand()%255,rand()%255);
    }
    
    
    system("clear");
    while (!tigrClosed(screen))
    {
        if (tigrKeyDown(screen, TK_CONTROL) && tigrKeyDown(screen, 'Q'))
        {
            printf("12121");
           
        }
        timeDt+=50;
        olivec_fill(canvas, 0xFF000000);                        // 清背景（不透明黑）
        olivec_line(canvas,0,0,WIDTH-1,HIGHT-1,0xFF00AAFF);
        olivec_circle(canvas,WIDTH/2,HIGHT/2,20.0+(sinf(((float)timeDt)/200)*15),0x55FF00FF);

        olivec_triangle3c(canvas,0,0,WIDTH-1,HIGHT/2,0,HIGHT-1,0xFF00FF00,0X00FF0000,0XFF0000FF);
        olivec_sprite_blend(canvas,5,5,5+pw,5+ph,image);
        float z1=2+sin((double)timeDt/1000.0);
        float z3=2-sin((double)timeDt/1000.0);
        olivec_triangle3z(
            canvas,
            (WIDTH/2+15)/z1,15/z1,
            WIDTH/2+50,15,
            (WIDTH/2+15)/z3,100/z3,
            z1,1,z3
        );
        olivec_triangle3uv(canvas,100,50
            ,125,100
            ,60,113,
            0,0
            ,1,0.5
            ,0,1,
            1,1,1,image);
        olivec_text(canvas, "mann",(timeDt/50)%WIDTH,rand()%5+HIGHT/4,olivec_default_font,1,0xFFFFFFFF);
        // memcpy(, pixels, WIDTH * HIGHT * sizeof(TPixel));
        tigrUpdate(screen);
        _sleep(16);
    }
    
    tigrFree(screen);
    return 0;
}