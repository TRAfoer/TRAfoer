#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>  // 包含sleep函数的头文件
#include <string.h>
#include<time.h>
#include<math.h>

#define OLIVEC_IMPLEMENTATION
#include "olive.c"

#define WIDTH 220
#define HIGHT 114
#define SOILD_BLOCK_HALF_UP "@"//

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
signed main(){
    srand((uint32_t)time(NULL));
    Olivec_Canvas canvas=olivec_canvas(pixels, WIDTH, HIGHT, WIDTH);
    system("clear");
    while (1)
    {
        int dtw=(sinf((double)timeDt/1000.0))*WIDTH/2;
        int dth=(sinf((double)timeDt/1000.0))*HIGHT/2;
        timeDt+=50;
        olivec_fill(canvas, OLIVEC_RGBA(rand()%10,rand()%10,rand()%10,0xFF));                        // 清背景（不透明黑）
        olivec_line(canvas,WIDTH/2+dtw,0,WIDTH/2-dtw,HIGHT-1,0xFF00AAFF);
        olivec_line(canvas,0,HIGHT/2-dth,WIDTH-1,HIGHT/2+dth,0xFF00AAFF);
        olivec_circle(canvas,WIDTH/2,HIGHT/2,20.0+(sinf(((float)timeDt)/200)*15),0x55FF00FF);
        olivec_text(canvas, "mann",rand()%WIDTH/2+WIDTH/4,rand()%HIGHT/2+HIGHT/4,olivec_default_font,1,0xFFFFFFFF);
        clock_t c=clock();
        int pos=RenderPx();
        clock_t e=clock();
        double loadcostms=(double)(e-c)/CLOCKS_PER_SEC*1000;
        fprintf(stdout,"RenderTake %-8.5lf ms | Render length %d",loadcostms,pos);
        usleep(16*1000);
    }
}