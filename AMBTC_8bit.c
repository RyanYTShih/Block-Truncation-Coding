#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define FILENAME_LENGTH 30
#define MAX_PIXEL_VALUE 255
#define BLOCK_WIDTH 4
#define BLOCK_NUM (BLOCK_WIDTH * BLOCK_WIDTH)

static int width;

static int bitPos[16] =
{
    1, 0, 1, 0,
    0, 1, 0, 1,
    1, 0, 1, 0,
    0, 1, 0, 1
};

static int **OriginImage;
static int **image_out;

static float **imageAvg;
static int **bitmap;
static int **avgHigh;
static int **avgLow;

static char *filename, *filename_out;

static float MSE = 0.0;
static float PSNR = 0.0;

void memAlloc();
int readImage(int**);
void calAvg(int**);
void calBitmap(float**);
void decode();
int isInBoundary(int, int);
int outputImage(int**);


int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Argument error\n");
        return 1;
    }
    
    float encodeStart_t, encodeEnd_t, decodeStart_t, decodeEnd_t;
    
    width = atoi(argv[2]);
    
    memAlloc();

    strcpy(filename, argv[1]);
    strcpy(filename_out, "out-");
    strcat(filename_out, filename);
    // printf("%s\n", filename_out);

    if(readImage(OriginImage))
        return 1;
    
    encodeStart_t = clock();
    calAvg(OriginImage);

    calBitmap(imageAvg);
    encodeEnd_t = clock();

    decodeStart_t = clock();
    decode();
    decodeEnd_t = clock();

    if(outputImage(image_out))
        return 1;
    
    printf("\nEncode time:\t%.3f sec\n", (encodeEnd_t - encodeStart_t) / 1000.0);
    printf("Decode time:\t%.3f sec\n", (decodeEnd_t - decodeStart_t) / 1000.0);
    printf("MSE:\t%f\n", MSE);
    printf("PSNR:\t%f\n", PSNR);
    printf("End program.\n");

    return 0;
}

void memAlloc()
{
    int i, j;
    
    filename = (char*) malloc(sizeof(char) * FILENAME_LENGTH);
    filename_out = (char*) malloc(sizeof(char) * (FILENAME_LENGTH + 4));
    

    OriginImage = (int**) malloc(sizeof(int*) * width);
    image_out = (int**) malloc(sizeof(int*) * width);
    bitmap = (int**) malloc(sizeof(int*) * width);
    for (i = 0; i < width; i++)
    {
        OriginImage[i] = (int*) malloc(sizeof(int) * width);
        image_out[i] = (int*) malloc(sizeof(int) * width);
        bitmap[i] = (int*) malloc(sizeof(int) * width);
    }

    imageAvg = (float**) malloc(sizeof(float*) * (width / BLOCK_WIDTH));
    avgHigh = (int**) malloc(sizeof(int*) * (width / BLOCK_WIDTH));
    avgLow = (int**) malloc(sizeof(int*) * (width / BLOCK_WIDTH));
    for(i = 0; i < width / BLOCK_WIDTH; i++)
    {
        imageAvg[i] = (float*) malloc(sizeof(float) * (width / BLOCK_WIDTH));
        avgHigh[i] = (int*) malloc(sizeof(int) * (width / BLOCK_WIDTH));
        avgLow[i] = (int*) malloc(sizeof(int) * (width / BLOCK_WIDTH));
    }
}

int readImage(int **image)
{
    int w, h;
    FILE *fp;
    if((fp = fopen(filename, "rb")) == NULL)
    {
        // printf("Can't open file.\n");
        return 1;
    }
    // else
    //     printf("Success read from \"%s\".\n", filename);

    printf("x=[");
    for (w = 0; w < width; w++)
    {
        if (w > 0)
            printf(";\n");
        for (h = 0; h < width; h++)
        {
            if (h > 0)
                printf(",");
            image[w][h] = (int) fgetc(fp);
            printf("%d", image[w][h]);
        }
    }
    fclose(fp);

    printf("]\n");

    return 0;
}

void calAvg(int **image)
{
    int avgX, avgY, x, y;

    for(avgX = 0; avgX < width / BLOCK_WIDTH; avgX++)
    {
        for(avgY = 0; avgY < width / BLOCK_WIDTH; avgY++)
        {
            int sum = 0;
            for(x = avgX * BLOCK_WIDTH; x < avgX * BLOCK_WIDTH + BLOCK_WIDTH; x++)
            {
                for(y = avgY * BLOCK_WIDTH; y < avgY * BLOCK_WIDTH + BLOCK_WIDTH; y++)
                {
                    sum += image[x][y];
                }
            }
            imageAvg[avgX][avgY] = sum / BLOCK_NUM;
        }
    }
}

void calBitmap(float **imageAvg)
{
    int avgX, avgY, x, y;

    for(avgX = 0; avgX < width / BLOCK_WIDTH; avgX++)
    {
        for(avgY = 0; avgY < width / BLOCK_WIDTH; avgY++)
        {
            int countHigher = 0, count = 0;
            float absolute = 0;
            for(x = avgX * BLOCK_WIDTH; x < avgX * BLOCK_WIDTH + BLOCK_WIDTH; x++)
            {
                for(y = avgY * BLOCK_WIDTH; y < avgY * BLOCK_WIDTH + BLOCK_WIDTH; y++)
                {
                    if(bitPos[count++])
                    {
                        if(OriginImage[x][y] > imageAvg[avgX][avgY])
                        {
                            bitmap[x][y] = 1;
                            countHigher++;
                        }
                        else
                        {
                            bitmap[x][y] = 0;
                        }
                    }
                    else
                    {
                        bitmap[x][y] = -1;
                    }
                    absolute += abs(OriginImage[x][y] - imageAvg[avgX][avgY]);
                }
            }
            absolute /= BLOCK_NUM;
            if(countHigher == 0 || countHigher == 8)
            {
                avgHigh[avgX][avgY] = avgLow[avgX][avgY] = imageAvg[avgX][avgY];
            }
            else
            {
                avgHigh[avgX][avgY] = imageAvg[avgX][avgY] + ((BLOCK_NUM * absolute) / (2 * countHigher));
                avgLow[avgX][avgY] = imageAvg[avgX][avgY] - ((BLOCK_NUM * absolute) / (2 * (BLOCK_NUM - countHigher)));
            }
        }
    }
}

void decode()
{
    int avgX, avgY, x, y;

    for(avgX = 0; avgX < width / BLOCK_WIDTH; avgX++)
    {
        for(avgY = 0; avgY < width / BLOCK_WIDTH; avgY++)
        {
            for(x = avgX * BLOCK_WIDTH; x < avgX * BLOCK_WIDTH + BLOCK_WIDTH; x++)
            {
                for(y = avgY * BLOCK_WIDTH; y < avgY * BLOCK_WIDTH + BLOCK_WIDTH; y++)
                {
                    if(bitmap[x][y] == 1)
                    {
                        image_out[x][y] = avgHigh[avgX][avgY];
                    }
                    else if(bitmap[x][y] == 0)
                    {
                        image_out[x][y] = avgLow[avgX][avgY];
                    }
                }
            }
        }
    }
    for(avgX = 0; avgX < width / BLOCK_WIDTH; avgX++)
    {
        for(avgY = 0; avgY < width / BLOCK_WIDTH; avgY++)
        {
            for(x = avgX * BLOCK_WIDTH; x < avgX * BLOCK_WIDTH + BLOCK_WIDTH; x++)
            {
                for(y = avgY * BLOCK_WIDTH; y < avgY * BLOCK_WIDTH + BLOCK_WIDTH; y++)
                {
                    if(bitmap[x][y] == -1)
                    {
                        int sum = 0, num = 0;
                        if(isInBoundary(x, y - 1))
                        {
                            sum += image_out[x][y - 1];
                            num++;
                        }
                        if(isInBoundary(x, y + 1))
                        {
                            sum += image_out[x][y + 1];
                            num++;
                        }
                        if(isInBoundary(x - 1, y))
                        {
                            sum += image_out[x - 1][y];
                            num++;
                        }
                        if(isInBoundary(x + 1, y))
                        {
                            sum += image_out[x + 1][y];
                            num++;
                        }
                        image_out[x][y] = sum / num;
                    }
                }
            }
        }
    }
}

int isInBoundary(int x, int y)
{
    return x >= 0 && x < width && y >= 0 && y < width;
}

int outputImage(int **image)
{
    FILE *fp;
    if((fp = fopen(filename_out, "wb")) == NULL)
    {
        // printf("Can't open file.\n");
        return 1;
    }
    // else
    //     printf("Success write to \"%s\".\n", filename_out);
    int w, h;

    printf("y=[");
    for(w = 0; w < width; w++)
    {
        if (w > 0)
            printf(";\n");
        for(h = 0; h < width; h++)
        {
            if (h > 0)
                printf(",");
            printf("%d", image[w][h]);
            fputc(image[w][h], fp);
            MSE += pow(OriginImage[w][h] - image[w][h], 2);
            if(OriginImage[w][h] > 255 || OriginImage[w][h] < 0)
            printf("%d ", OriginImage[w][h]);
        }
    }
    fclose(fp);

    printf("]\n");

    MSE /= width * width;
    PSNR = 10 * log10(pow(MAX_PIXEL_VALUE, 2) / MSE);

    return 0;
}