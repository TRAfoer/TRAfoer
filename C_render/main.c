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
struct point
{
    float x;
    float y;
    float z;
    uint32_t Col;
};
signed main()
{
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
    struct point pts[11][11][11];
    for (int i = 0; i < 10; ++i)
        for(int j=0;j<10;++j)
            for (int k = 0; k < 10; ++k){
                struct point *p=&pts[i][j][k];
                p->x = (i - 5) * 40 + rand() % 10;
                p->y = (j - 5) * 30 + rand() % 10;
                p->z = (k - 5) * 30 + rand() % 10;
                p->Col = OLIVEC_RGBA(rand() % 255, rand() % 255, rand() % 255, 150);
            }
    system("clear");
    while (!tigrClosed(screen))
    {
        if (tigrKeyHeld(screen, TK_CONTROL) && tigrKeyDown(screen, 'Q'))
        {
            printf("12121");
            break;
        }
        const float CAMERA_DIST = 400.0f; // 必须 > 物体最大 |z| (150) + 余量
        const float FOCAL = 400.0f;       // 焦距，越大透视越弱
        const float NEAR = 1.0f;

        timeDt += 50;
        const float alp = (float)timeDt / 5000.0f;
        const float cosA = cosf(alp);
        const float sinA = sinf(alp);

        olivec_fill(canvas, 0xff000000);

        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < 10; ++j)
                for (int k = 0; k < 10; ++k)
                {
                    struct point p = pts[i][j][k]; // 从原始位置旋转（不累积）

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

                    olivec_circle(canvas, sx, sy, 3, p.Col);
                }
        // memcpy(, pixels, WIDTH * HIGHT * sizeof(TPixel));
        tigrUpdate(screen);
    }
    
    tigrFree(screen);
    return 0;
}