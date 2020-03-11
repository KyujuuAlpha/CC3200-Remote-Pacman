// Troi-Ryan Stoeffler
// Corbin Harrell
// TA: Begum Kasap

// Standard includes
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"
#include "timer.h"

#include "pin.h"

// Common interface includes
#include "uart_if.h"
#include "i2c_if.h"
#include "pin_mux_config.h"

// GFX stuff includes
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "test.h"
#include "map.h"
#include "sound.h"
#include "aws_if.h"

// macros for some constants
#define SPI_IF_BIT_RATE  800000
#define TR_BUFF_SIZE     100
#define PAC_SIZE         4
#define MAX_VEL          1

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

// ball struct
struct Pac {
    int x;
    int y;
    int score;
};

struct Baddie {
    int x;
    int y;
    int color;
    char dir;
    char prevDir;
};
// static function prototypes
static void updatePacLoc(struct Pac *Pac, int *xVel, int *yVel);
static int adjustVel(int vel, const int *velFactor);
static long dropFrame(long frameCount);
static char *encodeCoords(int *coords, int size);
static char *integerToString(int i);
static unsigned long getCurrentSysTimeMS(void);
static void gameInit(void);
static void BoardInit(void);

// main function definition
void main() {

    // intilize the
    BoardInit();

    // mux UART and SPI lines
    PinMuxConfig();

    // init sound system
    InitSoundModules();

    // I2C Init
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    // enable spi clock
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    // reset the spi peripheral
    MAP_PRCMPeripheralReset(PRCM_GSPI);

    // reset spi
    MAP_SPIReset(GSPI_BASE);

    // spi configuration
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));

    // enable spi for communication
    MAP_SPIEnable(GSPI_BASE);

    // connect to the network
    networkConnect();

    // Initialize adafruit, then call the game loop
    Adafruit_Init();
    while(1) gameInit();
}

// Board initialization function
static void BoardInit(void) {
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

// Function definitions
int abs(int val) {
    if (val < 0) return -val;
    return val;
}

static bool yCollision(int x, int y, int yVel) {
    // Wall check
    const int blockSize = WIDTH / MAP_SIZE;
    int blockX, blockY;
    blockX = (x + 2) / blockSize;
    if (yVel < 0)
    {
        blockY = (y + yVel) / blockSize;
    } else {
        blockY = (y + yVel + PAC_SIZE - 1) / blockSize;
    }
    return map[blockY][blockX] == 1;
}

static bool xCollision(int x, int y, int xVel) {
    // Wall check
    const int blockSize = WIDTH / MAP_SIZE;
    int blockX, blockY;
    blockY = (y+2) / blockSize;
    if (xVel < 0)
    {
        blockX = (x + xVel) / blockSize;
    } else {
        blockX = (x + xVel + PAC_SIZE - 1) / blockSize;
    }
    return map[blockY][blockX] == 1;
}

void gameOver(struct Pac* pac) {
    fillRect(0, 0, WIDTH - 1, HEIGHT - 1, 0x0000);
    setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 16);
    Outstr("GAME OVER");
    setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 8);
    Outstr("Score: ");
    Outstr(integerToString(pac->score));
    delay(100);
}

bool enemyHit(struct Pac* pac, struct Baddie* bad) {
    return ((int)pac->x/4 == (int)bad->x/4 && (int)pac->y/4 == (int)bad->y/4);
}

static void updateBaddieLoc(struct Baddie* bad) {
    int velX = 0;
    int velY = 0;
    switch (bad->dir) {
    case 'U':
        velY = -1;
        break;
    case 'D':
        velY = 1;
        break;
    case 'L':
        velX = -1;
        break;
    case 'R':
        velX = 1;
        break;
    }
    if(!yCollision(bad->x, bad->y, velY)) {
        bad->y += velY;
    }
    if(!xCollision(bad->x, bad->y, velX)) {
        bad->x += velX;
    }
}

// function that updates the Pac's location accordingly and keeps it within bounds
static void updatePacLoc(struct Pac *pac, int *xVel, int *yVel) {

    bool yGreater = abs(*yVel) > abs(*xVel);
    if(*yVel > MAX_VEL) *yVel = MAX_VEL;
    if(*yVel < -MAX_VEL) *yVel = -MAX_VEL;
    if(*xVel > MAX_VEL) *xVel = MAX_VEL;
    if(*xVel < -MAX_VEL) *xVel = -MAX_VEL;
    if (*yVel == 0 && *xVel == 0) return;

    if(yCollision(pac->x, pac->y, *yVel) && xCollision(pac->x, pac->y, *xVel)) { // there's a collision in both directions
        *xVel = 0;
        *yVel = 0;
        return;
        // don't move
    }
    if(yGreater) { // y is greater
        if (!yCollision(pac->x, pac->y, *yVel)) { // no collision moving in y
            (*pac).y += *yVel;
            (*pac).x = (((*pac).x + 2) / 4) * 4;
            return;
            // move y, position x on rails
        }
        yGreater = false;
    }
    if(!yGreater) { // either x is greater or y collided
        if (!xCollision(pac->x, pac->y, *xVel)) { // no collision moving in y
            (*pac).x += *xVel;
            (*pac).y = (((*pac).y + 2) / 4) * 4;
            return;
        }
        if (!yCollision(pac->x, pac->y, *yVel)) { // no collision moving in y
            (*pac).y += *yVel;
            (*pac).x = (((*pac).x + 2) / 4) * 4;
            return;
            // move y, position x on rails
        }
    }
}

// function to return the adjusted two's complement representation of what was returned
// by the accelerometer.  Also scale it accordingly to the velocity factor (max vel essentially)
static int adjustVel(int vel, const int *velFactor) {
    if (vel > 255 / 2) { // essentially check if it wrapped around to 255 (negative)
        vel -= 255;
    }
    return -(vel * (*velFactor) / (255 / 2)); // adjust the velocity accordingly
}

static int frameDrop = 0;

static long dropFrame(long frameCount) {
    frameDrop++;
    return (frameCount += 888800) < 0 ? dropFrame(frameCount) : frameCount;
}

static char translation[32] = {'0', '1', '2', '3', '4', '5', '6',
                               '7', '8', '9', 'a', 'b', 'c', 'd',
                               'e', 'f', 'g', 'h', 'i', 'j', 'k',
                               'l', 'm', 'n', 'o', 'p', 'q', 'r',
                               's', 't', 'u', 'v'};

static char *encodeCoords(int *coords, int size) {
    static char encode[] = "";
    int i;
    for (i = 0; i < size; i++) {
        sprintf(encode, "%s%c", encode, translation[coords[i]]);
    }
    return encode;
}

static char *integerToString(int i) {
    static char stringBuf[] = "";
    sprintf(stringBuf, "%d", i);
    return stringBuf;
}

static unsigned long getCurrentSysTimeMS(void) {
    return (unsigned long) ((unsigned long long) PRCMSlowClkCtrGet() / 32768.0 * 1000.0);
}

void drawScore(struct Pac pac) {
    fillRect(12, 4, 16, 8, 0x0000);
    setCursor(12,4);
    Outstr(integerToString(pac.score));
}

static void gameInit(void) {
    struct Pac pac; // structure that keeps track of the pac's loc
    struct Baddie badGuys[4] = {
                                 { 0, 0, BAD_1_COLOR },
                                 { 0, 0, BAD_2_COLOR },
                                 { 0, 0, BAD_3_COLOR },
                                 { 0, 0, BAD_4_COLOR }
                                };
    unsigned char ACCDEV = 0x18, xREG = 0x3, yREG = 0x5; // device and registers for accel
    const int velFactor = 15; // max velocity

    unsigned char dataBuf; // buffer that holds what was returned from a register
    unsigned int color = PLAYER_COLOR; // starting color of the pac
    int xVel = 0; // velocities of the pac
    int yVel = 0;
    int initBaddie = 0;
    
    fillScreen(0x0000); // first clear the screen

    // intial stuff
    const int blockSize = WIDTH / MAP_SIZE;
    int i, j, k;
    for (i = 0; i < MAP_SIZE; i++) {
        for (j = 0; j < MAP_SIZE; j++) {
            if (map[j][i] == WALL) {
                fillRect(i * blockSize, j * blockSize, blockSize, blockSize, WALL_COLOR);
            } else if (map[j][i] == POINT) { // point pac
                k =  blockSize / 2;
                fillRect(i * blockSize + blockSize / 2 - k / 2, j * blockSize + blockSize / 2 - k / 2, k, k, POINT_COLOR);
            } else if (map[j][i] == SPAWN) { // start loc player
                pac.y = j*4;
                pac.x = i*4;
                pac.score = 0;
                drawScore(pac);
            } if (map[j][i] == 4) { // start loc baddies
                if (initBaddie >= 4) continue;
                badGuys[initBaddie].y = j*4;
                badGuys[initBaddie].x = i*4;
                badGuys[initBaddie].dir = 'L';
                initBaddie++;
            }
        }
    }

    // game loop
    unsigned long prevTime = getCurrentSysTimeMS();
    long newDelay = 0;
    unsigned char tickTimer = 0, tickCounter = 0;
    while (1) {
        do {
            if (tickTimer >= 2) { // get new data 10 times a second
                tickTimer = 0;
                I2C_IF_Write(ACCDEV, &xREG, 1, 0); // get the x and y accelerometer information using i2c
                I2C_IF_Read(ACCDEV, &dataBuf, 1);  // and adjust the velocities accordingly.
                yVel = adjustVel((int) dataBuf, &velFactor);
                I2C_IF_Write(ACCDEV, &yREG, 1, 0);  // the x and y values from the registers are flipped
                I2C_IF_Read(ACCDEV, &dataBuf, 1);   // since we found that they changed the wrong axis
               xVel = adjustVel((int) dataBuf, &velFactor);

                if (tickCounter > 100) { // send a get request every 10 seconds to check for new baddies
                    //playSound(DEATH);
                    tickCounter = 0;
                    buildRequest("pac_x", integerToString(pac.x));
                    buildRequest("pac_y", integerToString(pac.y));
                    sendRequest();

                    receiveString();
                } else {
                    tickCounter++;
                }
            } else {
                tickTimer++;
            }
            if (xVel != 0 || yVel != 0) {
                fillRect(pac.x, pac.y, PAC_SIZE, PAC_SIZE, 0x0000);  // erase the old location of the pac
            }
            updatePacLoc(&pac, &xVel, &yVel); // update the pac's location
            fillRect(pac.x, pac.y, PAC_SIZE, PAC_SIZE, color); // draw new ball on the screen
            int bad;
            for (bad = 0; bad < 4; bad++) {
                fillRect(badGuys[bad].x, badGuys[bad].y, PAC_SIZE, PAC_SIZE, 0x0000);
                updateBaddieLoc(&badGuys[bad]);
                fillRect(badGuys[bad].x, badGuys[bad].y, PAC_SIZE, PAC_SIZE, badGuys[bad].color);
                if(enemyHit(&pac, &badGuys[bad])) {
                    gameOver(&pac);
                    return;
                }
            }
            if(map[pac.y/blockSize][pac.x/blockSize] == 2) {
                map[pac.y/blockSize][pac.x/blockSize] = 0;
                pac.score++;
                drawScore(pac);
            }
        } while (frameDrop-- > 0);
        updateSoundModules();

        // timing handling
        newDelay = (long)(33.33 - (((long) getCurrentSysTimeMS() - (long) prevTime)) * 26666.67);
        frameDrop = 0;
        if (newDelay < 0) { // dropped a frame
            newDelay = dropFrame(newDelay);
        }
        MAP_UtilsDelay((unsigned long) newDelay);
        prevTime = getCurrentSysTimeMS();
    }
}
