#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// 返回 0 成功；非 0 失败
static int openPPMp6(const char *FileName, uint32_t *out_data, int *w, int *h)
{
    FILE *f = fopen(FileName, "rb");        // 二进制模式
    if (!f) { perror("fopen"); return 1; }

    // 1) magic
    char magic[3] = {0};
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P6") != 0) {
        fprintf(stderr, "Not a P6 PPM (magic=%s)\n", magic);
        fclose(f); return 2;
    }

    // 2) width height maxval（顺序：宽 高 最大值）
    int width, height, maxval;
    if (fscanf(f, "%d %d %d", &width, &height, &maxval) != 3) {
        fclose(f); return 3;
    }
    if (maxval != 255) {
        fprintf(stderr, "Only maxval=255 supported, got %d\n", maxval);
        fclose(f); return 4;
    }

    // 3) 吃掉 maxval 后的那一个空白（可能是 \n，也可能是 \r\n）
    int c = fgetc(f);
    if (c == '\r') c = fgetc(f);
    if (c != '\n') { fclose(f); return 5; }

    // 4) 读 RGB 字节
    size_t pixel_count = (size_t)width * height;
    uint8_t *rgb = malloc(pixel_count * 3);
    if (!rgb) { fclose(f); return 6; }

    if (fread(rgb, 3, pixel_count, f) != pixel_count) {
        fprintf(stderr, "Short read\n");
        free(rgb); fclose(f); return 7;
    }
    fclose(f);

    // 5) 转换成 olive 的 RGBA (内存布局 R,G,B,A)
    for (size_t i = 0; i < pixel_count; ++i) {
        uint8_t r = rgb[i*3 + 0];
        uint8_t g = rgb[i*3 + 1];
        uint8_t b = rgb[i*3 + 2];
        out_data[i] = (uint32_t)r | ((uint32_t)g << 8) |
                      ((uint32_t)b << 16) | (255u << 24);
    }
    free(rgb);

    *w = width;
    *h = height;
    return 0;
}