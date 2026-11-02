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

#define PTX 15
#define PTY 15
#define PTZ 15
#define PTMUL 160
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
struct point
{
    float x;
    float y;
    float z;
    uint32_t Col;
};
#define CLAMP(val,dw,up) val<dw?dw:(val>up?up:val)
#define INDEX(i, j) i *HIGHT + j
signed main()
{
    data=malloc(256*256*sizeof(uint32_t));
    atexit (my_exit);
    srand((uint32_t)time(NULL));
    Tigr *screen = tigrWindow(WIDTH, HIGHT, "TEST-ONE", TIGR_FIXED);
    Olivec_Canvas canvas=olivec_canvas((uint32_t*)(screen->pix), WIDTH, HIGHT, WIDTH);
    uint32_t *originDat;
    originDat = (uint32_t *)malloc(HIGHT * WIDTH * sizeof(uint32_t) + 1);

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
    struct point pts[PTX + 1][PTY + 1][PTZ+1];
    float muls[PTX + 1][PTY + 1][PTZ + 1];
    for (int i = 0; i < PTX; ++i) for (int j = 0; j < PTY; ++j) for (int k = 0; k < PTZ; ++k)
    {
        struct point *p = &pts[i][j][k];
        p->x = (i - PTX / 2) * PTMUL + rand() % 100;
        p->y = (j - PTY / 2) * PTMUL + rand() % 100;
        p->z = (k - PTZ / 2) * PTMUL + rand() % 100;
        p->Col = 0xffffffff;//OLIVEC_RGBA(rand() % 255, rand() % 255, rand() % 255, 150);
        muls[i][j][k]=(float)((rand()%200)-100)/100.0;
    }
    Olivec_Canvas oc = {
        .pixels = originDat,
        .width = canvas.width,
        .height = canvas.height,
        .stride = canvas.stride,
    };
    system("clear");
    while (!tigrClosed(screen))
    {
        time_t start=clock();
        if (tigrKeyHeld(screen, TK_CONTROL) && tigrKeyDown(screen, 'Q'))
        {
            printf("12121");
            break;
        }
        const float CAMERA_DIST = 400.0f; // 必须 > 物体最大 |z| (150) + 余量
        const float FOCAL = 400.0f;       // 焦距，越大透视越弱
        const float NEAR = 1.0f;

        timeDt += 50;
        
        olivec_fill(canvas, 0xff000000);
        
        for (int i = 0; i < PTX; ++i)
        for (int j = 0; j < PTY; ++j)
        for (int k = 0; k < PTZ; ++k)
        {
            struct point p = pts[i][j][k]; // 从原始位置旋转（不累积）
            float alp = ((float)timeDt / 5000.0f) * muls[i][j][k];
            float cosA = cosf(alp);
            float sinA = sinf(alp);
            // ---- 绕 Y 轴旋转：用临时变量，避免读到修改后的 p.x ----
            float rx = p.x * cosA - p.z * sinA;
            float rz = p.x * sinA + p.z * cosA;
            float ry = p.y;

            // ---- 透视投影 ----
            float z_view = rz + CAMERA_DIST;
            if (z_view < NEAR)
                z_view = NEAR;

            float scale = FOCAL / z_view;
            int sx = (int)(rx * scale + WIDTH / 2);
            int sy = (int)(-ry * scale + HIGHT / 2); // y 轴翻转
            // uint32_t col = OLIVEC_RGBA(OLIVEC_RED(p.Col), OLIVEC_GREEN(p.Col), OLIVEC_BLUE(p.Col), (int)((CAMERA_DIST - z_view) / CAMERA_DIST * 255));
            olivec_circle(canvas, sx, sy, (int)(7.0+ 5.0 * fabs(sin((float)(timeDt) / 500.0))), p.Col);
        }

        memcpy(originDat, canvas.pixels, HIGHT * WIDTH * sizeof(uint32_t));
        for (size_t y = 0; y < canvas.height; ++y)
        {
            for (size_t x = 0; x < canvas.width; ++x)
            {
                size_t i = ((x)+rand()%5) % canvas.width;
                size_t j = ((y)+rand()%5) % canvas.height;
                uint32_t col = OLIVEC_PIXEL(oc, i, j);

                uint32_t imagecol = OLIVEC_PIXEL(image, x / 5 % image.width, y / 5 % image.height);
                imagecol = OLIVEC_RGBA(OLIVEC_RED(imagecol), OLIVEC_GREEN(imagecol), OLIVEC_BLUE(imagecol), OLIVEC_RED(col));
                olivec_blend_color(&OLIVEC_PIXEL(canvas, x, y), imagecol);
            }
        }
        time_t end=clock();
        float dlt = ((float)(end - start)) / CLOCKS_PER_SEC*1000;
        char str[128];
        sprintf(str,"render time take: %5.4lf ms -- ctrl+q to quit",dlt);
        olivec_text(canvas, str, 0+2, HIGHT - 128-2, olivec_default_font, 4, 0xff000000);
        olivec_text(canvas, str, 0, HIGHT - 128, olivec_default_font, 4, 0xffffffff);
        tigrUpdate(screen);
    }
    free(originDat);
    tigrFree(screen);
    return 0;
}