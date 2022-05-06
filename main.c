#define SDRAM_BASE 0xC0000000
#define FPGA_ONCHIP_BASE 0xC8000000
#define FPGA_CHAR_BASE 0xC9000000

/* Cyclone V FPGA devices */
#define LEDR_BASE 0xFF200000
#define HEX3_HEX0_BASE 0xFF200020
#define HEX5_HEX4_BASE 0xFF200030
#define SW_BASE 0xFF200040
#define KEY_BASE 0xFF200050
#define TIMER_BASE 0xFF202000
#define PIXEL_BUF_CTRL_BASE 0xFF203020
#define CHAR_BUF_CTRL_BASE 0xFF203030

/* VGA colors */
#define WHITE 0xFFFF
#define YELLOW 0xFFE0
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define GREY 0xC618
#define PINK 0xFC18
#define ORANGE 0xFC00
#define BLACK 0x0

#define ABS(x) (((x) > 0) ? (x) : -(x))

/* Screen size. */
#define RESOLUTION_X 320
#define RESOLUTION_Y 240

/* Constants for animation */
#define BOX_LEN 2
#define NUM_BOXES 8

#define FALSE 0
#define TRUE 1

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int led = 0;
volatile int *ledr = (int *)0xFF200000;
volatile int *KeyEdgeReg = (int *)(0xFF20005C);
volatile int *SwReg = (int *)SW_BASE;

volatile int *pixel_ctrl_ptr = (int *)0xFF203020;
volatile int pixel_buffer_start;

int tileWidth = 20;
int tileHeight = 20;
int tankWidth = 14;
int tankHeight = 14;
int bulletSpeed = 3;
int maxBulletNum = 10;
int wallNum = 45;
int currentBulletCount = 0;

bool startScreen = true;
bool pauseScreen = false;
bool gameRunning = false;
bool p1Victory = false;
bool isFirstMap = true;
bool gamePaused = false;

typedef struct point2d {
    int x;
    int y;
} coord;

typedef struct player {
    int lifeLeft;
    int xDir;
    int yDir;
    coord position;
    bool stop;
    short int playerColor;
    coord lastDirection;
} player;

typedef struct bullet {
    coord position;
    coord direction;
    bool render;
    bool belongToP1;
} bullet;

void swap(int *first, int *second) {
    int temp = *first;
    *first = *second;
    *second = temp;
}

void plot_pixel(int x, int y, short int line_color) {
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

void clear_screen() {
    for (int x = 0; x < 320; x++) {
        for (int y = 0; y < 240; y++) {
            plot_pixel(x, y, BLACK);
        }
    }
}

void draw_line(int x0, int y0, int x1, int y1, short int color) {
    bool is_steep = ABS(y1 - y0) > ABS(x1 - x0);

    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    int delta_x = x1 - x0;
    int delta_y = ABS(y1 - y0);
    int error = -(delta_x / 2);
    int y = y0;
    int y_step = y0 < y1 ? 1 : -1;

    for (int x = x0; x <= x1; x++) {
        if (is_steep) {
            plot_pixel(y, x, color);
        } else {
            plot_pixel(x, y, color);
        }
        error += delta_y;
        if (error > 0) {
            y += y_step;
            error -= delta_x;
        }
    }
}

void wait_for_vsync() {
    volatile int *status_reg = (int *)0xFF20302C;
    volatile int *buffer_reg = (int *)0xFF203020;
    int status = *(status_reg);  // get the current status from status register
    int sBitStatus = status & 0x01;

    *buffer_reg = 1;

    do {
        sBitStatus = *status_reg & 0x01;
    } while (sBitStatus);
}

void clearMainScreen() {
    for (int x = 0; x < 12 * tileWidth; x++) {
        for (int y = 0; y < 240; y++) {
            plot_pixel(x, y, BLACK);
        }
    }
    draw_line(12 * tileWidth, 0, 12 * tileWidth, RESOLUTION_Y - 1, ORANGE);
}

void drawBox(int startX, int startY, int endX, int endY, short int color, bool fill) {
    if (!fill) {
        draw_line(startX, startY, endX, startY, color);
        draw_line(startX, endY, endX, endY, color);
        draw_line(startX, startY, startX, endY, color);
        draw_line(endX, startY, endX, endY, color);
    } else {
        int deltaY = endY - startY;
        for (int iterator = 0; iterator < deltaY; iterator++) {
            draw_line(startX, startY + iterator, endX, startY + iterator, color);
        }
    }
}

void textOnHex() {
    int *hexBase = (int *)HEX3_HEX0_BASE;
    int *hex4Base = (int *)HEX5_HEX4_BASE;

    if (p1Victory) {
        *hex4Base = 0b0111001100000110;
        *hexBase = 0b0111110001111100011111100110111;
    } else {
        *hex4Base = 0b0111001101011011;
        *hexBase = 0b0111110001111100011111100110111;
    }
}

void drawStartScreen() {
    // main window
    drawBox(2 * tileWidth, 2 * tileHeight, 10 * tileWidth, 5 * tileHeight, WHITE, false);
    drawBox(2 * tileWidth, 8 * tileHeight, 4 * tileWidth, 10 * tileHeight, WHITE, false);
    drawBox(8 * tileWidth, 8 * tileHeight, 10 * tileWidth, 10 * tileHeight, WHITE, false);

    draw_line(2 * tileWidth, 6 * tileHeight, 10 * tileWidth, 7 * tileHeight, WHITE);
    draw_line(2 * tileWidth, 7 * tileHeight, 10 * tileWidth, 6 * tileHeight, WHITE);

    // splitter
    draw_line(12 * tileWidth, 0, 12 * tileWidth, RESOLUTION_Y - 1, ORANGE);

    // status pane
    draw_line(12 * tileWidth, 4 * tileHeight, 16 * tileWidth, 4 * tileHeight, ORANGE);
    draw_line(12 * tileWidth, 6 * tileHeight, 16 * tileWidth, 6 * tileHeight, ORANGE);
    draw_line(12 * tileWidth, 8 * tileHeight, 16 * tileWidth, 8 * tileHeight, ORANGE);

    // upper small tanks
    drawBox(13 * tileWidth - tankWidth / 2, tileHeight + (tileHeight - tankHeight) / 2, 13 * tileWidth + tankWidth / 2,
            tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, BLUE, true);

    drawBox(14 * tileWidth - tankWidth / 2, tileHeight + (tileHeight - tankHeight) / 2, 14 * tileWidth + tankWidth / 2,
            tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, BLUE, true);

    drawBox(15 * tileWidth - tankWidth / 2, tileHeight + (tileHeight - tankHeight) / 2, 15 * tileWidth + tankWidth / 2,
            tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, BLUE, true);

    // upper big tank
    drawBox(13 * tileWidth + (tileWidth - tankWidth), 2 * tileHeight + (tileHeight - tankHeight), 15 * tileWidth - (tileWidth - tankWidth),
            4 * tileHeight - (tileHeight - tankHeight), BLUE, true);

    // lower small tanks
    drawBox(13 * tileWidth - tankWidth / 2, 9 * tileHeight + (tileHeight - tankHeight) / 2, 13 * tileWidth + tankWidth / 2,
            9 * tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, RED, true);

    drawBox(14 * tileWidth - tankWidth / 2, 9 * tileHeight + (tileHeight - tankHeight) / 2, 14 * tileWidth + tankWidth / 2,
            9 * tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, RED, true);

    drawBox(15 * tileWidth - tankWidth / 2, 9 * tileHeight + (tileHeight - tankHeight) / 2, 15 * tileWidth + tankWidth / 2,
            9 * tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, RED, true);

    // lower big tank
    drawBox(13 * tileWidth + (tileWidth - tankWidth), 10 * tileHeight + (tileHeight - tankHeight), 15 * tileWidth - (tileWidth - tankWidth),
            12 * tileHeight - (tileHeight - tankHeight), RED, true);
}

bool hitBoundaryBullet(coord pos) {
    return (pos.x <= 0 || pos.x >= 12 * tileWidth || pos.y <= 2 || pos.y >= 12 * tileHeight);
}

bool withInBox(coord pos, coord boxStart, int boxWidth, int boxHeight) {
    if ((pos.x >= boxStart.x && pos.x <= (boxStart.x + boxWidth)) && (pos.y >= boxStart.y && pos.y <= boxStart.y + boxHeight)) {
        return true;
    }
    return false;
}

bool hitPlayer(bullet b, player p1, player p2) {
    if (b.belongToP1) {
        if (withInBox(b.position, p2.position, tankWidth, tankHeight)) {
            return true;
        }
    } else {
        if (withInBox(b.position, p1.position, tankWidth, tankHeight)) {
            return true;
        }
    }
    return false;
}

void displayPlayerLife(player p) {
    int ledValue = *ledr;
    if (p.playerColor == BLUE) {
        for (int life = 0; life < (3 - p.lifeLeft); life++) {
            drawBox((13 + life) * tileWidth - tankWidth / 2, tileHeight + (tileHeight - tankHeight) / 2, (13 + life) * tileWidth + tankWidth / 2,
                    tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, BLACK, true);
        }
        *ledr = (p.lifeLeft | ledValue);
    } else {
        for (int life = 0; life < (3 - p.lifeLeft); life++) {
            drawBox((13 + life) * tileWidth - tankWidth / 2, 9 * tileHeight + (tileHeight - tankHeight) / 2, (13 + life) * tileWidth + tankWidth / 2,
                    9 * tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, BLACK, true);
        }
        *ledr = ((p.lifeLeft << 8) | ledValue);
    }
}

void drawVictoryScreen() {
    if (p1Victory) {
        drawBox(2 * tileWidth, 3 * tileHeight, 10 * tileWidth, 9 * tileHeight, BLUE, true);
    } else {
        drawBox(2 * tileWidth, 3 * tileHeight, 10 * tileWidth, 9 * tileHeight, RED, true);
    }
    textOnHex();
}

void drawBullets(bullet bullets[], int currentBulletCount) {
    for (int bulletIte = 0; bulletIte < currentBulletCount; bulletIte++) {
        if (bullets[bulletIte].render) {
            draw_line(bullets[bulletIte].position.x, bullets[bulletIte].position.y,
                      bullets[bulletIte].position.x - bullets[bulletIte].direction.x * 2, bullets[bulletIte].position.y - bullets[bulletIte].direction.y * 2,
                      bullets[bulletIte].belongToP1 ? BLUE : RED);
        }
    }
}

void drawWalls(int x, int y) {
    coord pos;
    pos.x = x;
    pos.y = y;
    drawBox(pos.x, pos.y, pos.x + tileWidth, pos.y + tileHeight, ORANGE, true);
    drawBox(pos.x + 5, pos.y + 5, pos.x + tileWidth - 5, pos.y + tileHeight - 5, GREY, true);
}

void drawPlayer(player p) {
    drawBox(p.position.x, p.position.y, p.position.x + tankWidth, p.position.y + tankHeight, p.playerColor, true);
}

void loadFirstGameMap(coord *walls) {
    int wallIte = 0;
    for (; wallIte < 4; wallIte++) {
        drawWalls(tileWidth + tileWidth * wallIte, tileHeight);
        walls[wallIte].x = (tileWidth + tileWidth * wallIte);
        walls[wallIte].y = tileHeight;
    }

    for (; wallIte < 4 + 8; wallIte++) {
        drawWalls(tileWidth, 2 * tileHeight + tileHeight * (wallIte - 4));
        walls[wallIte].x = (tileWidth);
        walls[wallIte].y = 2 * tileHeight + tileHeight * (wallIte - 4);
    }

    for (; wallIte < (12 + 7); wallIte++) {
        drawWalls(3 * tileWidth + tileWidth * (wallIte - 12), 3 * tileHeight);
        walls[wallIte].x = 3 * tileWidth + tileWidth * (wallIte - 12);
        walls[wallIte].y = 3 * tileHeight;
    }

    for (; wallIte < (19 + 4); wallIte++) {
        drawWalls(8 * tileWidth + tileWidth * (wallIte - 19), tileHeight);
        walls[wallIte].x = 8 * tileWidth + tileWidth * (wallIte - 19);
        walls[wallIte].y = tileHeight;
    }

    for (; wallIte < (23 + 3); wallIte++) {
        drawWalls(9 * tileWidth, tileHeight * 4 + tileHeight * (wallIte - 23));
        walls[wallIte].x = 9 * tileWidth;
        walls[wallIte].y = tileHeight * 4 + tileHeight * (wallIte - 23);
    }

    for (; wallIte < (26 + 4); wallIte++) {
        drawWalls(7 * tileWidth + (wallIte - 26) * tileWidth, 7 * tileHeight);
        walls[wallIte].x = 7 * tileWidth + (wallIte - 26) * tileWidth;
        walls[wallIte].y = 7 * tileHeight;
    }

    for (; wallIte < (30 + 2); wallIte++) {
        drawWalls(2 * tileWidth + (wallIte - 30) * tileWidth, 9 * tileHeight);
        walls[wallIte].x = 2 * tileWidth + (wallIte - 30) * tileWidth;
        walls[wallIte].y = 9 * tileHeight;
    }

    for (; wallIte < (32 + 3); wallIte++) {
        drawWalls(3 * tileWidth, 5 * tileHeight + (wallIte - 32) * tileHeight);
        walls[wallIte].x = 3 * tileWidth;
        walls[wallIte].y = 5 * tileHeight + (wallIte - 32) * tileHeight;
    }

    for (; wallIte < (35 + 2); wallIte++) {
        drawWalls(4 * tileWidth + (wallIte - 35) * tileWidth, 7 * tileHeight);
        walls[wallIte].x = 4 * tileWidth + (wallIte - 35) * tileWidth;
        walls[wallIte].y = 7 * tileHeight;
    }

    for (; wallIte < (37 + 3); wallIte++) {
        drawWalls(8 * tileWidth + (wallIte - 37) * tileWidth, 10 * tileHeight);
        walls[wallIte].x = 8 * tileWidth + (wallIte - 37) * tileWidth;
        walls[wallIte].y = 10 * tileHeight;
    }

    for (; wallIte < (40 + 2); wallIte++) {
        drawWalls(5 * tileWidth + (wallIte - 40) * tileWidth, 10 * tileHeight);
        walls[wallIte].x = 5 * tileWidth + (wallIte - 40) * tileWidth;
        walls[wallIte].y = 10 * tileHeight;
    }

    drawWalls(6 * tileWidth, tileHeight);
    walls[wallIte].x = 6 * tileWidth;
    walls[wallIte].y = tileHeight;

    drawWalls(6 * tileWidth, 5 * tileHeight);
    walls[wallIte + 1].x = 6 * tileWidth;
    walls[wallIte + 1].y = 5 * tileHeight;

    drawWalls(10 * tileWidth, 9 * tileHeight);
    walls[wallIte + 2].x = 10 * tileWidth;
    walls[wallIte + 2].y = 9 * tileHeight;

    // drawWalls(6 * tileWidth, 11 * tileHeight);
}

void loadSecondGameMap(coord *walls) {
    int wallIte = 0;
    for (; wallIte < 4; wallIte++) {
        drawWalls(tileWidth + wallIte * tileWidth, tileHeight);
        walls[wallIte].x = tileWidth + wallIte * tileWidth;
        walls[wallIte].y = tileHeight;
    }

    for (; wallIte < (4 + 3); wallIte++) {
        drawWalls(tileWidth, 2 * tileHeight + tileHeight * (wallIte - 4));
        walls[wallIte].x = tileWidth;
        walls[wallIte].y = 2 * tileHeight + tileHeight * (wallIte - 4);
    }

    for (; wallIte < (7 + 2); wallIte++) {
        drawWalls(tileWidth * 8 + tileWidth * (wallIte - 7), 0);
        walls[wallIte].x = tileWidth * 8 + tileWidth * (wallIte - 7);
        walls[wallIte].y = 0;
    }

    for (; wallIte < (9 + 3); wallIte++) {
        drawWalls(4 * tileWidth + (wallIte - 9) * tileWidth, 3 * tileHeight);
        walls[wallIte].x = 4 * tileWidth + (wallIte - 9) * tileWidth;
        walls[wallIte].y = 3 * tileHeight;
    }

    for (; wallIte < (12 + 3); wallIte++) {
        drawWalls(3 * tileWidth + (wallIte - 12) * tileWidth, 4 * tileHeight);
        walls[wallIte].x = 3 * tileWidth + (wallIte - 12) * tileWidth;
        walls[wallIte].y = 4 * tileHeight;
    }

    for (; wallIte < (15 + 2); wallIte++) {
        drawWalls(3 * tileWidth + (wallIte - 15) * tileWidth, 5 * tileHeight);
        walls[wallIte].x = 3 * tileWidth + (wallIte - 15) * tileWidth;
        walls[wallIte].y = 5 * tileHeight;
    }

    for (; wallIte < (17 + 2); wallIte++) {
        drawWalls(10 * tileWidth, 2 * tileHeight + (wallIte - 17) * tileHeight);
        walls[wallIte].x = 10 * tileWidth;
        walls[wallIte].y = 2 * tileHeight + (wallIte - 17) * tileHeight;
    }

    for (; wallIte < (19 + 3); wallIte++) {
        drawWalls(9 * tileWidth + (wallIte - 19) * tileWidth, 4 * tileHeight);
        walls[wallIte].x = 9 * tileWidth + (wallIte - 19) * tileWidth;
        walls[wallIte].y = 4 * tileHeight;
    }

    for (; wallIte < (22 + 2); wallIte++) {
        drawWalls(7 * tileWidth + (wallIte - 22) * tileWidth, 6 * tileHeight);
        walls[wallIte].x = 7 * tileWidth + (wallIte - 22) * tileWidth;
        walls[wallIte].y = 6 * tileHeight;
    }

    for (; wallIte < (24 + 2); wallIte++) {
        drawWalls(7 * tileWidth + (wallIte - 24) * tileWidth, 7 * tileHeight);
        walls[wallIte].x = 7 * tileWidth + (wallIte - 24) * tileWidth;
        walls[wallIte].y = 7 * tileHeight;
    }

    for (; wallIte < (26 + 2); wallIte++) {
        drawWalls(0, 8 * tileHeight + (wallIte - 26) * tileHeight);
        walls[wallIte].x = 0;
        walls[wallIte].y = 8 * tileHeight + (wallIte - 26) * tileHeight;
    }

    for (; wallIte < (28 + 2); wallIte++) {
        drawWalls(tileWidth + (wallIte - 28) * tileWidth, 11 * tileHeight);
        walls[wallIte].x = tileWidth + (wallIte - 28) * tileWidth;
        walls[wallIte].y = 11 * tileHeight;
    }

    for (; wallIte < (30 + 2); wallIte++) {
        drawWalls(2 * tileWidth + (wallIte - 30) * tileWidth, 9 * tileHeight);
        walls[wallIte].x = 2 * tileWidth + (wallIte - 30) * tileWidth;
        walls[wallIte].y = 9 * tileHeight;
    }

    for (; wallIte < (32 + 2); wallIte++) {
        drawWalls(5 * tileWidth + (wallIte - 32) * tileWidth, 10 * tileHeight);
        walls[wallIte].x = 5 * tileWidth + (wallIte - 32) * tileWidth;
        walls[wallIte].y = 10 * tileHeight;
    }

    for (; wallIte < (34 + 3); wallIte++) {
        drawWalls(8 * tileWidth + (wallIte - 34) * tileWidth, 10 * tileHeight);
        walls[wallIte].x = 8 * tileWidth + (wallIte - 34) * tileWidth;
        walls[wallIte].y = 10 * tileHeight;
    }

    for (; wallIte < (37 + 2); wallIte++) {
        drawWalls(5 * tileWidth, 7 * tileHeight + (wallIte - 37) * tileHeight);
        walls[wallIte].x = 5 * tileWidth;
        walls[wallIte].y = 7 * tileHeight + (wallIte - 37) * tileHeight;
    }

    drawWalls(6 * tileWidth, tileHeight);
    walls[wallIte].x = 6 * tileWidth;
    walls[wallIte].y = tileHeight;

    wallIte++;
    drawWalls(tileWidth, 6 * tileHeight);
    walls[wallIte].x = tileWidth;
    walls[wallIte].y = 6 * tileHeight;

    wallIte++;
    drawWalls(10 * tileWidth, 7 * tileHeight);
    walls[wallIte].x = 10 * tileWidth;
    walls[wallIte].y = 7 * tileHeight;

    wallIte++;
    drawWalls(6 * tileWidth, 11 * tileHeight);
    walls[wallIte].x = 6 * tileWidth;
    walls[wallIte].y = 11 * tileHeight;

    wallIte++;
    drawWalls(10 * tileWidth, 9 * tileHeight);
    walls[wallIte].x = 10 * tileWidth;
    walls[wallIte].y = 9 * tileHeight;

    wallIte++;
    drawWalls(3 * tileWidth, 8 * tileHeight);
    walls[wallIte].x = 3 * tileWidth;
    walls[wallIte].y = 8 * tileHeight;

    // drawBox(2 * tileWidth, 2 * tileHeight, 10 * tileWidth, 10 * tileHeight, WHITE, false);
}

void drawLowerSmallTank() {
    for (int life = 0; life < 3; life++) {
        drawBox((13 + life) * tileWidth - tankWidth / 2, 9 * tileHeight + (tileHeight - tankHeight) / 2, (13 + life) * tileWidth + tankWidth / 2,
                9 * tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, RED, true);
    }
}

void drawUpperSmallTank() {
    for (int life = 0; life < 3; life++) {
        drawBox((13 + life) * tileWidth - tankWidth / 2, tileHeight + (tileHeight - tankHeight) / 2, (13 + life) * tileWidth + tankWidth / 2,
                tileHeight + (tileHeight - tankHeight) / 2 + tankHeight, BLUE, true);
    }
}

void drawSideLabel() {
    draw_line(12 * tileWidth, 4 * tileHeight, 16 * tileWidth, 6 * tileHeight, WHITE);
    draw_line(12 * tileWidth, 6 * tileHeight, 16 * tileWidth, 4 * tileHeight, WHITE);

    draw_line(12 * tileWidth, 6 * tileHeight, 16 * tileWidth, 8 * tileHeight, WHITE);
    draw_line(12 * tileWidth, 8 * tileHeight, 16 * tileWidth, 6 * tileHeight, WHITE);
}

void clearPlayerTrace(player p1, player p2, bullet bullets[]) {
    int startX = p1.position.x - 5;
    int startY = p1.position.y - 5;
    if (startX < 0) {
        startX = 0;
    }
    if (startY < 0) {
        startY = 0;
    }
    drawBox(startX, startY, startX + tileWidth, startY + tileHeight, BLACK, true);

    startX = p2.position.x - 5;
    startY = p2.position.y - 5;
    if (startX < 0) {
        startX = 0;
    }
    if (startY < 0) {
        startY = 0;
    }
    drawBox(startX, startY, startX + tileWidth, startY + tileHeight, BLACK, true);

    // splitters
    draw_line(12 * tileWidth, 0, 12 * tileWidth, RESOLUTION_Y - 1, ORANGE);
    draw_line(12 * tileWidth, 4 * tileHeight, 16 * tileWidth, 4 * tileHeight, ORANGE);
    draw_line(12 * tileWidth, 6 * tileHeight, 16 * tileWidth, 6 * tileHeight, ORANGE);
    draw_line(12 * tileWidth, 8 * tileHeight, 16 * tileWidth, 8 * tileHeight, ORANGE);

    for (int bulletIte = 0; bulletIte < currentBulletCount; bulletIte++) {
        draw_line(bullets[bulletIte].position.x, bullets[bulletIte].position.y,
                  bullets[bulletIte].position.x - bullets[bulletIte].direction.x * 3, bullets[bulletIte].position.y - bullets[bulletIte].direction.y * 3,
                  BLACK);
    }

    // side labels
    drawSideLabel();

    // big tank
    drawBigTank(p1);
    drawBigTank(p2);
}

void drawPauseScreen() {
    drawBox(2 * tileWidth, 3 * tileHeight, 10 * tileWidth, 9 * tileHeight, PINK, true);
}

void drawBigTank(player p) {
    if (p.playerColor == BLUE) {
        if (p.lastDirection.x == 0 && p.lastDirection.y == 1) {
        } else if (p.lastDirection.x == 0 && p.lastDirection.y == -1) {
        } else if (p.lastDirection.x == 1 && p.lastDirection.y == 0) {
        } else if (p.lastDirection.x == 1 && p.lastDirection.y == -1) {
        } else if (p.lastDirection.x == 1 && p.lastDirection.y == 1) {
        } else if (p.lastDirection.x == -1 && p.lastDirection.y == 0) {
        } else if (p.lastDirection.x == -1 && p.lastDirection.y == -1) {
        } else if (p.lastDirection.x == -1 && p.lastDirection.y == 1) {
        }
    } else {
        if (p.lastDirection.x == 0 && p.lastDirection.y == 1) {
        } else if (p.lastDirection.x == 0 && p.lastDirection.y == -1) {
        } else if (p.lastDirection.x == 1 && p.lastDirection.y == 0) {
        } else if (p.lastDirection.x == 1 && p.lastDirection.y == -1) {
        } else if (p.lastDirection.x == 1 && p.lastDirection.y == 1) {
        } else if (p.lastDirection.x == -1 && p.lastDirection.y == 0) {
        } else if (p.lastDirection.x == -1 && p.lastDirection.y == -1) {
        } else if (p.lastDirection.x == -1 && p.lastDirection.y == 1) {
        }
    }
}

int main(void) {
    *(pixel_ctrl_ptr + 1) = 0xC8000000;
    wait_for_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen();
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    clear_screen();

    // draw start screen
    drawStartScreen();
    drawSideLabel();
    wait_for_vsync();
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
    drawStartScreen();
    drawSideLabel();

    // pool for sw0 and key 0
    // if sw0 = 0 and key 0 pressed, map 1 is selected
    while (startScreen) {
        if (*KeyEdgeReg & 0x1) {  // if bit 0 is set to 1
            if ((*SwReg & 0b1) == 1) {
                isFirstMap = false;
            }
            startScreen = false;   // breaks out of start screen
            gameRunning = true;    // set game running flag to true
            *KeyEdgeReg = 0b1111;  // reset all keys
        }
    }

    clearMainScreen();
    wait_for_vsync();
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
    clearMainScreen();

    player p1;
    p1.stop = false;
    p1.lifeLeft = 3;
    p1.position.x = 222;
    p1.position.y = 220;
    p1.xDir = 0;
    p1.yDir = 0;
    p1.playerColor = BLUE;

    player p2;
    p2.stop = false;
    p2.lifeLeft = 3;
    p2.position.x = 2;
    p2.position.y = 4;
    p2.xDir = 0;
    p2.yDir = 0;
    p2.playerColor = RED;

    coord walls[wallNum];

    bullet bullets[maxBulletNum];
    while (1) {
        if (!gamePaused) {
            if (startScreen) {
                drawStartScreen();
                wait_for_vsync();
                pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
                drawStartScreen();
            }

            while (startScreen) {
                if (*KeyEdgeReg & 0x1) {  // if bit 0 is set to 1
                    if ((*SwReg & 0b1) == 1) {
                        isFirstMap = false;
                    } else if((*SwReg & 0b1) == 0){
                        isFirstMap = true;
                    }
                    startScreen = false;   // breaks out of start screen
                    gameRunning = true;    // set game running flag to true
                    *KeyEdgeReg = 0b1111;  // reset all keys
                }
                // reset player 1 data
                p1.stop = false;
                p1.lifeLeft = 3;
                p1.position.x = 222;
                p1.position.y = 220;
                p1.xDir = 0;
                p1.yDir = 0;
                p1.playerColor = BLUE;
                p1.lastDirection.x = 0;
                p1.lastDirection.y = 0;

                // reset player 2 data
                p2.stop = false;
                p2.lifeLeft = 3;
                p2.position.x = 2;
                p2.position.y = 4;
                p2.xDir = 0;
                p2.yDir = 0;
                p2.playerColor = RED;
                p2.lastDirection.x = 0;
                p2.lastDirection.y = 0;

                // reset bullets
                for (int bulletIte = 0; bulletIte < currentBulletCount; bulletIte++) {
                    bullets[bulletIte].render = false;
                }

                // reset walls
                for (int wallIte; wallIte < wallNum; wallIte++) {
                    walls[wallIte].x = 0;
                    walls[wallIte].y = 0;
                }
            }

            clearMainScreen();
            drawUpperSmallTank();
            drawLowerSmallTank();

            wait_for_vsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer

            clearMainScreen();
            drawUpperSmallTank();
            drawLowerSmallTank();

            int *hexBase = (int *)HEX3_HEX0_BASE;
            int *hex4Base = (int *)HEX5_HEX4_BASE;
            *hexBase = 0b0;
            *hex4Base = 0b0;

            while (gameRunning) {
                if (*KeyEdgeReg & 0b10) {  // press key 1 to pause game
                    gamePaused = true;
                    *KeyEdgeReg = 0b10;  // reset key 1
                    break;
                }

                clearPlayerTrace(p1, p2, bullets);
                if (isFirstMap) {
                    loadFirstGameMap(walls);
                } else {
                    loadSecondGameMap(walls);
                }

                int SwValue = *SwReg;
                // handle player1 movement input
                p1.yDir = (SwValue & 0b1000) ? -1 : (SwValue & 0b100) ? 1
                                                                      : 0;
                p1.xDir = (SwValue & 0b10) ? -1 : (SwValue & 0b1) ? 1
                                                                  : 0;

                if ((SwValue & 0b1000) && (SwValue & 0b100)) {
                    p1.yDir = 0;
                }
                if ((SwValue & 0b10) && (SwValue & 0b1)) {
                    p1.xDir = 0;
                }

                if (p1.xDir != 0 || p1.yDir != 0) {
                    p1.lastDirection.x = p1.xDir;
                    p1.lastDirection.y = p1.yDir;
                }

                // handle player 2 movement input
                p2.yDir = (SwValue & 0b1000000000) ? -1 : (SwValue & 0b100000000) ? 1
                                                                                  : 0;
                p2.xDir = (SwValue & 0b10000000) ? -1 : (SwValue & 0b1000000) ? 1
                                                                              : 0;

                if ((SwValue & 0b1000000000) && (SwValue & 0b100000000)) {
                    p2.yDir = 0;
                }
                if ((SwValue & 0b10000000) && (SwValue & 0b1000000)) {
                    p2.xDir = 0;
                }

                if (p2.xDir != 0 || p2.yDir != 0) {
                    p2.lastDirection.x = p2.xDir;
                    p2.lastDirection.y = p2.yDir;
                }

                // handle player1's position
                bool moveInX = false;
                bool moveInY = false;
                if ((p1.position.x != 0 && p1.position.x + tankWidth != 12 * tileWidth) ||
                    (p1.position.x == 0 && p1.xDir == 1) ||
                    (p1.position.x == 12 * tileWidth - tankWidth && p1.xDir == -1)) {
                    moveInX = true;
                }
                if ((p1.position.y != 0 && p1.position.y + tankHeight != 12 * tileHeight) ||
                    (p1.position.y == 0 && p1.yDir == 1) ||
                    (p1.position.y == 12 * tileHeight - tankHeight && p1.yDir == -1)) {
                    moveInY = true;
                }

                // handles wall collision for p1
                for (int wallIte = 0; wallIte < wallNum; wallIte++) {
                    coord corner1 = p1.position;
                    coord corner2 = p1.position;
                    coord corner3 = p1.position;
                    coord corner4 = p1.position;

                    corner2.x += tankWidth;
                    corner3.y += tankHeight;
                    corner4.x += tankWidth;
                    corner4.y += tankHeight;
                    if (withInBox(corner1, walls[wallIte], tileWidth, tileHeight) ||
                        withInBox(corner2, walls[wallIte], tileWidth, tileHeight) ||
                        withInBox(corner3, walls[wallIte], tileWidth, tileHeight) ||
                        withInBox(corner4, walls[wallIte], tileWidth, tileHeight)) {
                        if (corner1.y == walls[wallIte].y + tileHeight) {
                            if (corner1.x < (walls[wallIte].x + tileWidth) && corner2.x > walls[wallIte].x) {
                                if (p1.yDir == -1) {
                                    moveInY = false;
                                }
                            }
                        } else if (corner3.y == walls[wallIte].y) {
                            if (corner1.x < (walls[wallIte].x + tileWidth) && corner2.x > walls[wallIte].x) {
                                if (p1.yDir == 1) {
                                    moveInY = false;
                                }
                            }
                        } else if (corner1.x == walls[wallIte].x + tileWidth) {
                            if (corner1.y < walls[wallIte].y + tileHeight && corner3.y > walls[wallIte].y) {
                                if (p1.xDir == -1) {
                                    moveInX = false;
                                }
                            }
                        } else if (corner2.x == walls[wallIte].x) {
                            if (corner1.y < walls[wallIte].y + tileHeight && corner3.y > walls[wallIte].y) {
                                if (p1.xDir == 1) {
                                    moveInX = false;
                                }
                            }
                        }
                    }
                }

                if (moveInX) {
                    p1.position.x += p1.xDir;
                }
                if (moveInY) {
                    p1.position.y += p1.yDir;
                }

                // handle player2's position
                moveInX = false;
                moveInY = false;
                if ((p2.position.x != 0 && p2.position.x + tankWidth != 12 * tileWidth) ||
                    (p2.position.x == 0 && p2.xDir == 1) ||
                    (p2.position.x == 12 * tileWidth - tankWidth && p2.xDir == -1)) {
                    moveInX = true;
                }
                if ((p2.position.y != 0 && p2.position.y + tankHeight != 12 * tileHeight) ||
                    (p2.position.y == 0 && p2.yDir == 1) ||
                    (p2.position.y == 12 * tileHeight - tankHeight && p2.yDir == -1)) {
                    moveInY = true;
                }

                // handles wall collision for p2
                for (int wallIte = 0; wallIte < wallNum; wallIte++) {
                    coord corner1 = p2.position;
                    coord corner2 = p2.position;
                    coord corner3 = p2.position;
                    coord corner4 = p2.position;

                    corner2.x += tankWidth;
                    corner3.y += tankHeight;
                    corner4.x += tankWidth;
                    corner4.y += tankHeight;
                    if (withInBox(corner1, walls[wallIte], tileWidth, tileHeight) ||
                        withInBox(corner2, walls[wallIte], tileWidth, tileHeight) ||
                        withInBox(corner3, walls[wallIte], tileWidth, tileHeight) ||
                        withInBox(corner4, walls[wallIte], tileWidth, tileHeight)) {
                        if (corner1.y == walls[wallIte].y + tileHeight) {
                            if (corner1.x < (walls[wallIte].x + tileWidth) && corner2.x > walls[wallIte].x) {
                                if (p2.yDir == -1) {
                                    moveInY = false;
                                }
                            }
                        } else if (corner3.y == walls[wallIte].y) {
                            if (corner1.x < (walls[wallIte].x + tileWidth) && corner2.x > walls[wallIte].x) {
                                if (p2.yDir == 1) {
                                    moveInY = false;
                                }
                            }
                        } else if (corner1.x == walls[wallIte].x + tileWidth) {
                            if (corner1.y < walls[wallIte].y + tileHeight && corner3.y > walls[wallIte].y) {
                                if (p2.xDir == -1) {
                                    moveInX = false;
                                }
                            }
                        } else if (corner2.x == walls[wallIte].x) {
                            if (corner1.y < walls[wallIte].y + tileHeight && corner3.y > walls[wallIte].y) {
                                if (p2.xDir == 1) {
                                    moveInX = false;
                                }
                            }
                        }
                    }
                }

                if (moveInX) {
                    p2.position.x += p2.xDir;
                }
                if (moveInY) {
                    p2.position.y += p2.yDir;
                }

                // handles bullets fire
                // use do while(0) to break out of if statement
                do {
                    // key 0 is pressed, p1 shoots bullets
                    if (*KeyEdgeReg & 0b1) {
                        if (p1.lastDirection.x == 0 && p1.lastDirection.y == 0) {
                            break;
                        }
                        if (currentBulletCount == maxBulletNum) {
                            for (int ite = 0; ite < currentBulletCount; ite++) {
                                if (!bullets[ite].render) {
                                    bullets[ite].direction = p1.lastDirection;
                                    bullets[ite].direction.x *= bulletSpeed;
                                    bullets[ite].direction.y *= bulletSpeed;
                                    bullets[ite].position = p1.position;
                                    bullets[ite].render = true;
                                    bullets[ite].belongToP1 = true;

                                    bullets[ite].position.x += tankWidth / 2;
                                    bullets[ite].position.y += tankHeight / 2;
                                    break;
                                }
                            }
                        } else {
                            bullets[currentBulletCount].direction = p1.lastDirection;
                            bullets[currentBulletCount].direction.x *= bulletSpeed;
                            bullets[currentBulletCount].direction.y *= bulletSpeed;
                            bullets[currentBulletCount].position = p1.position;
                            bullets[currentBulletCount].render = true;
                            bullets[currentBulletCount].belongToP1 = true;

                            bullets[currentBulletCount].position.x += tankWidth / 2;
                            bullets[currentBulletCount].position.y += tankHeight / 2;
                            currentBulletCount++;
                        }
                        *KeyEdgeReg = 0b1;              // reset key 0
                    } else if (*KeyEdgeReg & 0b1000) {  // key 3 is pressed
                        if (p2.lastDirection.x == 0 && p2.lastDirection.y == 0) {
                            break;
                        }
                        if (currentBulletCount == maxBulletNum) {
                            for (int ite = 0; ite < currentBulletCount; ite++) {
                                if (!bullets[ite].render) {
                                    bullets[ite].direction = p2.lastDirection;
                                    bullets[ite].direction.x *= bulletSpeed;
                                    bullets[ite].direction.y *= bulletSpeed;
                                    bullets[ite].position = p2.position;
                                    bullets[ite].render = true;
                                    bullets[ite].belongToP1 = false;

                                    bullets[ite].position.x += tankWidth / 2;
                                    bullets[ite].position.y += tankHeight / 2;
                                    break;
                                }
                            }
                        } else {
                            bullets[currentBulletCount].direction = p2.lastDirection;
                            bullets[currentBulletCount].direction.x *= bulletSpeed;
                            bullets[currentBulletCount].direction.y *= bulletSpeed;
                            bullets[currentBulletCount].position = p2.position;
                            bullets[currentBulletCount].render = true;
                            bullets[currentBulletCount].belongToP1 = false;

                            bullets[currentBulletCount].position.x += tankWidth / 2;
                            bullets[currentBulletCount].position.y += tankHeight / 2;
                            currentBulletCount++;
                        }
                        *KeyEdgeReg = 0b1000;  // reset key 3
                    }
                } while (0);

                // check if bullet has hit any thing or the player, disable render if so
                for (int bulletIte = 0; bulletIte < currentBulletCount; bulletIte++) {
                    if (!bullets[bulletIte].render) {
                        continue;
                    }
                    if (hitBoundaryBullet(bullets[bulletIte].position)) {
                        bullets[bulletIte].render = false;
                        *ledr = 0b1;
                        continue;
                    } else if (hitPlayer(bullets[bulletIte], p1, p2)) {
                        if (bullets[bulletIte].belongToP1) {
                            p2.lifeLeft--;
                        } else {
                            p1.lifeLeft--;
                        }
                        bullets[bulletIte].render = false;
                        *ledr = 0b10;
                        continue;
                    }

                    // handles bullets wall collision
                    bool hitWall = false;
                    for (int wallIte = 0; wallIte < wallNum; wallIte++) {
                        if (withInBox(bullets[bulletIte].position, walls[wallIte], tileWidth, tileHeight)) {
                            hitWall = true;
                            bullets[bulletIte].render = false;
                            break;
                        }
                    }

                    if (!hitWall) {
                        bullets[bulletIte].position.x += bullets[bulletIte].direction.x;
                        bullets[bulletIte].position.y += bullets[bulletIte].direction.y;
                        *ledr = 0b0;
                    }
                }

                drawPlayer(p1);
                drawPlayer(p2);
                drawBullets(bullets, currentBulletCount);

                displayPlayerLife(p1);
                displayPlayerLife(p2);

                if (p1.lifeLeft == 0) {
                    gameRunning = false;
                    p1Victory = false;
                } else if (p2.lifeLeft == 0) {
                    gameRunning = false;
                    p1Victory = true;
                }

                wait_for_vsync();
                pixel_buffer_start = *(pixel_ctrl_ptr + 1);
            }

            if (!gamePaused) {
                for (int bulletIte = 0; bulletIte < currentBulletCount; bulletIte++) {
                    bullets[bulletIte].render = false;
                }
            }

            while (!gameRunning && !gamePaused) {
                drawVictoryScreen();

                if (*KeyEdgeReg & 0b1) {  // press key 0 to restart
                    startScreen = false;
                    gameRunning = true;
                    break;
                } else if (*KeyEdgeReg & 0b10) {  // press key1 to go main
                    startScreen = true;
                    break;
                }

                wait_for_vsync();
                pixel_buffer_start = *(pixel_ctrl_ptr + 1);
            }

            *KeyEdgeReg = 0b1111;

            if (!gamePaused) {
                p1.stop = false;
                p1.lifeLeft = 3;
                p1.position.x = 222;
                p1.position.y = 220;
                p1.xDir = 0;
                p1.yDir = 0;
                p1.playerColor = BLUE;
                p1.lastDirection.x = 0;
                p1.lastDirection.y = 0;

                p2.stop = false;
                p2.lifeLeft = 3;
                p2.position.x = 2;
                p2.position.y = 4;
                p2.xDir = 0;
                p2.yDir = 0;
                p2.playerColor = RED;
                p2.lastDirection.x = 0;
                p2.lastDirection.y = 0;
            }

            if (!gamePaused) {
                clearMainScreen();
                wait_for_vsync();
                pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
                clearMainScreen();
            }
        } else {
            // clearMainScreen();
            drawPauseScreen();

            if (*KeyEdgeReg & 0b1) {  // press key 0 to restart
                startScreen = false;
                gameRunning = true;
                gamePaused = false;
            } else if (*KeyEdgeReg & 0b10) {
                startScreen = true;
                gameRunning = false;
                gamePaused = false;
            }

            if (!gamePaused && startScreen && !gameRunning) {
                *KeyEdgeReg = 0b1111;
                clearMainScreen();
                wait_for_vsync();
                pixel_buffer_start = *(pixel_ctrl_ptr + 1);
                clear_screen();
            }

            wait_for_vsync();
            pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        }
    }
    return 0;
}