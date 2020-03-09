// Troi-Ryan Stoeffler
// Corbin Harrell
// TA: Begum Kasap

// Standard includes
#include <string.h>
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

// Common interface includes
#include "uart_if.h"
#include "i2c_if.h"
#include "pin_mux_config.h"

// GFX stuff includes
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "test.h"
#include "pwm.h"

#include "map.h"

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
};

struct Baddie {
    int x;
    int y;
    int color;
    char dir;
};
// static function prototypes
static void updatePacLoc(struct Pac *Pac, int *xVel, int *yVel);
static int adjustVel(int vel, const int *velFactor);
static void gameLoop(void);
static void BoardInit(void);

// main function definition
void main() {

    // intilize the
    BoardInit();

    // mux UART and SPI lines
    PinMuxConfig();

    InitPWMModules();

//    int iLoopCnt = 0;
//
//    while(1) {
//        for(iLoopCnt = 2000; iLoopCnt < 5000; iLoopCnt+=100) {
//            generateFrequency(iLoopCnt);
//            MAP_UtilsDelay(80000000 / 2);
//        }
//
//    }

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

    // Initialize adafruit, then call the game loop
    Adafruit_Init();
    gameLoop();
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

// function that updates the Pac's location accordingly and keeps it within bounds
static void updatePacLoc(struct Pac *pac, int *xVel, int *yVel) {

    // Boarder check
    if((*pac).x + *xVel >= WIDTH - PAC_SIZE) {
        (*pac).x = WIDTH - 1 - PAC_SIZE;
        *xVel = 0;
    }
    if((*pac).x + *xVel < 0) {
        (*pac).x = 0;
        *xVel = 0;
    }
    if((*pac).y + *yVel >= HEIGHT - PAC_SIZE) {
        (*pac).y = HEIGHT - 1 - PAC_SIZE;
        *yVel = 0;
    }
    if((*pac).y + *yVel < 0) {
        (*pac).y = 0;
        *yVel = 0;
    }

    // Choose direction
    bool yGreater = abs(*yVel) > abs(*xVel);
    if(yGreater) {
        *xVel = 0;
        if(*yVel > MAX_VEL) *yVel = MAX_VEL;
        if(*yVel < -MAX_VEL) *yVel = -MAX_VEL;
    } else {
        *yVel = 0;
        if(*xVel > 2) *xVel = 2;
        if(*xVel < -2) *xVel = -2;
    }


    if (*yVel == 0 && *xVel == 0) return;

    // Wall check
    const int blockSize = WIDTH / MAP_SIZE;
    int blockX, blockY;
    if (*xVel < 0 || *yVel < 0)
    {
        blockX = ((*pac).x + *xVel) / blockSize;
        blockY = ((*pac).y + *yVel) / blockSize;
    } else {
        blockX = ((*pac).x + *xVel + PAC_SIZE - 1) / blockSize;
        blockY = ((*pac).y + *yVel + PAC_SIZE - 1) / blockSize;
    }
//    int xLim = (blockX < MAP_SIZE - 4 ? blockX + 4 : MAP_SIZE - 1), yLim = (blockY < MAP_SIZE - 4 ? blockY + 4 : MAP_SIZE - 1);
//    int xStart = blockX > 0 ? blockX : 0, yStart = blockY > 0 ? blockY : 0;
    if (map[blockY][blockX] == 1) {
        if (yGreater) {
            if (*yVel < 0) {
                (*pac).y = (blockY + 1) * 4;
                (*pac).x = blockX * 4;
            } else {
                (*pac).y = (blockX - 1) * 4;
                (*pac).x = (blockX + 1) * 4;
            }
        } else {
            if (*xVel < 0) {
                (*pac).x = (blockX + 1) * 4;
                (*pac).y = blockY * 4;
            } else {
                (*pac).x = (blockX - 1) * 4;
                (*pac).y = (blockY + 1) * 4;
            }
        }
        *xVel = 0;
        *yVel = 0;
    }
    /*
    for (blockX = xStart; blockX <= xLim; blockX++) {
        for (blockY = yStart; blockY <= yLim; blockY++) {
            int bX = blockX * blockSize, bY = blockY * blockSize;
            if (map[blockY][blockX] > 0 && (*pac).x + *xVel >= bX - PAC_SIZE && (*pac).x + *xVel <= bX + blockSize + PAC_SIZE &&
                                           (*pac).y + *yVel <= bY + blockSize + PAC_SIZE && (*pac).y + *yVel >= bY - PAC_SIZE) {
                if ((*pac).x > bX + blockSize + PAC_SIZE) {
                   (*pac).x = bX + blockSize + PAC_SIZE + 1;
                   *xVel = 0;
                }
                if ((*pac).x < bX - PAC_SIZE) {
                    (*pac).x = bX - PAC_SIZE - 1;
                    *xVel = 0;
                }
                if ((*pac).y > bY + blockSize + PAC_SIZE) {
                    (*pac).y = bY + blockSize + PAC_SIZE + 1;
                    *yVel = 0;
                }
                if ((*pac).y < bY - PAC_SIZE) {
                    (*pac).y = bY - PAC_SIZE - 1;
                    *yVel = 0;
                }
            }
        }
    } */

    (*pac).x += *xVel;
    (*pac).y += *yVel;
}

// function to return the adjusted two's complement representation of what was returned
// by the accelerometer.  Also scale it accordingly to the velocity factor (max vel essentially)
static int adjustVel(int vel, const int *velFactor) {
    if (vel > 255 / 2) { // essentially check if it wrapped around to 255 (negative)
        vel -= 255;
    }
    return -(vel * (*velFactor) / (255 / 2)); // adjust the velocity accordingly
}

static void gameLoop(void) {
    struct Pac pac; // structure that keeps track of the pac's loc
    struct Baddie badGuys[4] = {
                                 { 0, 0, RED},
                                 { 0, 0, CYAN},
                                 { 0, 0, GREEN},
                                 { 0, 0, MAGENTA}
                                };
    unsigned char ACCDEV = 0x18, xREG = 0x3, yREG = 0x5; // device and registers for accel
    const int velFactor = 15; // max velocity

    unsigned char dataBuf; // buffer that holds what was returned from a register
    unsigned int color = YELLOW; // starting color of the pac
    int xVel = 0; // velocities of the pac
    int yVel = 0;
    
    fillScreen(0x0000); // first clear the screen
    const int blockSize = WIDTH / MAP_SIZE;
    int initBaddie = 0;
    int i, j;
    for (i = 0; i < MAP_SIZE; i++) {
        for (j = 0; j < MAP_SIZE; j++) {
            if (map[j][i] == 1) {
                fillRect(i * blockSize, j * blockSize, blockSize, blockSize, BLUE);
            } else if (map[j][i] == 2) { // point pac
                fillCircle(i * blockSize + blockSize / 3, j * blockSize + blockSize / 3, blockSize / 3, RED);
            } else if (map[j][i] == 3) { // start loc player
                pac.y = j*4;
                pac.x = i*4;
            } else if (map[j][i] == 4) { // start loc baddies
                if (initBaddie >= 4) continue;
                badGuys[initBaddie].y = j*4;
                badGuys[initBaddie].x = i*4;
            }
        }
    }

    while (1) {
        fillRect(pac.x, pac.y, PAC_SIZE, PAC_SIZE,  0x0000);  // erase the old location of the pac
        I2C_IF_Write(ACCDEV, &xREG, 1, 0); // get the x and y accelerometer information using i2c
        I2C_IF_Read(ACCDEV, &dataBuf, 1);  // and adjust the velocities accordingly.
        yVel = adjustVel((int) dataBuf, &velFactor);
        I2C_IF_Write(ACCDEV, &yREG, 1, 0);  // the x and y values from the registers are flipped
        I2C_IF_Read(ACCDEV, &dataBuf, 1);   // since we found that they changed the wrong axis
        xVel = adjustVel((int) dataBuf, &velFactor);
        updatePacLoc(&pac, &xVel, &yVel); // update the pac's location
        fillRect(pac.x, pac.y, PAC_SIZE, PAC_SIZE, color); // draw new ball on the screen
        delay(3);
    }
}
