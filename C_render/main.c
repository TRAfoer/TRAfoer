#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>  // 包含sleep函数的头文件
#include <string.h>
#include<time.h>
#include<math.h>
#include<stdint.h>

#define OLIVEC_IMPLEMENTATION
#include "olive.c"
#include "openPPMp6.c"

#define WIDTH 220
#define HIGHT 114
#define SOILD_BLOCK_HALF_UP "▀"//

// #define OLIVEC_RED(color)   (((color)&0x000000FF)>>(8*0))
// #define OLIVEC_GREEN(color) (((color)&0x0000FF00)>>(8*1))
// #define OLIVEC_BLUE(color)  (((color)&0x00FF0000)>>(8*2))
// #define OLIVEC_ALPHA(color) (((color)&0xFF000000)>>(8*3))
// #define OLIVEC_RGBA(r, g, b, a) ((((r)&0xFF)<<(8*0)) | (((g)&0xFF)<<(8*1)) | (((b)&0xFF)<<(8*2)) | (((a)&0xFF)<<(8*3)))
uint32_t pixels[WIDTH*HIGHT+1];
uint32_t *pxptr=&pixels[0];

int RenderPx(){
    static char out_buf[WIDTH * HIGHT * 20]; // 预分配足够大的缓冲区
    size_t pos = 0;
    uint32_t last_up = 0, last_dw = 0;
    bool first_pixel = true;
    pos += sprintf(out_buf + pos,"\x1b[H");
    for(int j=0;j<HIGHT;j+=2){
        for(int i=0;i<WIDTH;++i){
            if(j+1<HIGHT){//前景色作为满色，背景色作为空色
            uint32_t upColor = pixels[ j   * WIDTH + i];
            uint32_t dwColor = pixels[(j+1)* WIDTH + i];
            if (first_pixel || upColor != last_up || dwColor != last_dw) {
                pos += sprintf(out_buf + pos,
                    "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm", // 合并为一个 SGR
                    OLIVEC_RED(upColor), OLIVEC_GREEN(upColor), OLIVEC_BLUE(upColor),
                    OLIVEC_RED(dwColor), OLIVEC_GREEN(dwColor), OLIVEC_BLUE(dwColor));
                last_up = upColor;
                last_dw = dwColor;
                first_pixel = false;
            }
            memcpy(out_buf + pos, SOILD_BLOCK_HALF_UP, strlen(SOILD_BLOCK_HALF_UP));
            pos += strlen(SOILD_BLOCK_HALF_UP);
            }

        }
        pos += sprintf(out_buf + pos,"\n");
    }
    pos += sprintf(out_buf + pos,"\033[0m");
    fwrite(out_buf, 1, pos, stdout);
    return pos;
}
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
    data=malloc(1024*1024);
    atexit (my_exit);
    srand((uint32_t)time(NULL));
    Olivec_Canvas canvas=olivec_canvas(pixels, WIDTH, HIGHT, WIDTH);

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
    while (1)
    {
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
        clock_t c=clock();
        int pos=RenderPx();
        clock_t e=clock();
        double loadcostms=(double)(e-c)/CLOCKS_PER_SEC*1000;
        fprintf(stdout,"RenderTake %-8.5lf ms | Render length %d",loadcostms,pos);
        usleep(16*1000);
    }
}