// Troi-Ryan Stoeffler
// Corbin Harrell
// TA: Begum Kasap

// Standard includes
#include <string.h>

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

// Common interface includes
#include "uart_if.h"
#include "i2c_if.h"
#include "pin_mux_config.h"

// GFX stuff includes
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "test.h"

#include "map.h"

// macros for some constants
#define SPI_IF_BIT_RATE  800000
#define TR_BUFF_SIZE     100
#define BALL_RADIUS      2

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

// ball struct
struct Ball {
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
static void updateBallLoc(struct Ball *ball, int *xVel, int *yVel);
static int adjustVel(int vel, const int *velFactor);
static void gameLoop(void);
static void BoardInit(void);

// main function definition
void main() {

    // intilize the
    BoardInit();

    // mux UART and SPI lines
    PinMuxConfig();

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

// function that updates the ball's location accordingly and keeps it within bounds
static void updateBallLoc(struct Ball *ball, int *xVel, int *yVel) {
    if((*ball).x + *xVel >= WIDTH - BALL_RADIUS) {
        (*ball).x = WIDTH - 1 - BALL_RADIUS;
        *xVel = 0;
    }
    if((*ball).x + *xVel < 0 + BALL_RADIUS) {
        (*ball).x = 0 + BALL_RADIUS;
        *xVel = 0;
    }
    if((*ball).y + *yVel >= HEIGHT - BALL_RADIUS) {
        (*ball).y = HEIGHT - 1 - BALL_RADIUS;
        *yVel = 0;
    }
    if((*ball).y + *yVel < 0 + BALL_RADIUS) {
        (*ball).y = 0 + BALL_RADIUS;
        *yVel = 0;
    }
    const int blockSize = WIDTH / MAP_SIZE;
    int blockX = ((*ball).x + *xVel) / blockSize - 2, blockY = ((*ball).y + *yVel) / blockSize - 2;
    int xLim = (blockX < MAP_SIZE - 4 ? blockX + 4 : MAP_SIZE - 1), yLim = (blockY < MAP_SIZE - 4 ? blockY + 4 : MAP_SIZE - 1);
    int xStart = blockX > 0 ? blockX : 0, yStart = blockY > 0 ? blockY : 0;
    for (blockX = xStart; blockX <= xLim; blockX++) {
        for (blockY = yStart; blockY <= yLim; blockY++) {
            int bX = blockX * blockSize, bY = blockY * blockSize;
            if (map[blockY][blockX] > 0 && (*ball).x + *xVel >= bX - BALL_RADIUS && (*ball).x + *xVel <= bX + blockSize + BALL_RADIUS &&
                                           (*ball).y + *yVel <= bY + blockSize + BALL_RADIUS && (*ball).y + *yVel >= bY - BALL_RADIUS) {
                if ((*ball).x > bX + blockSize + BALL_RADIUS) {
                   (*ball).x = bX + blockSize + BALL_RADIUS + 1;
                   *xVel = 0;
                }
                if ((*ball).x < bX - BALL_RADIUS) {
                    (*ball).x = bX - BALL_RADIUS - 1;
                    *xVel = 0;
                }
                if ((*ball).y > bY + blockSize + BALL_RADIUS) {
                    (*ball).y = bY + blockSize + BALL_RADIUS + 1;
                    *yVel = 0;
                }
                if ((*ball).y < bY - BALL_RADIUS) {
                    (*ball).y = bY - BALL_RADIUS - 1;
                    *yVel = 0;
                }
            }
        }
    }
    (*ball).x += *xVel;
    (*ball).y += *yVel;
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
    struct Ball ball; // structure that keeps track of the ball's loc
    struct Baddie badGuys[4] = {
                                 { 0, 0, RED},
                                 { 0, 0, CYAN},
                                 { 0, 0, GREEN},
                                 { 0, 0, MAGENTA}
                                };
    unsigned char ACCDEV = 0x18, xREG = 0x3, yREG = 0x5; // device and registers for accel
    const int velFactor = 15; // max velocity

    unsigned char dataBuf; // buffer that holds what was returned from a register
    unsigned int color = YELLOW; // starting color of the ball
    int xVel = 0; // velocities of the ball
    int yVel = 0;
    
    fillScreen(0x0000); // first clear the screen
    const int blockSize = WIDTH / MAP_SIZE;
    int initBaddie = 0;
    int i, j;
    for (i = 0; i < MAP_SIZE; i++) {
        for (j = 0; j < MAP_SIZE; j++) {
            if (map[j][i] == 1) {
                fillRect(i * blockSize, j * blockSize, blockSize, blockSize, BLUE);
            } else if (map[j][i] == 2) { // point ball
                fillCircle(i * blockSize + blockSize / 3, j * blockSize + blockSize / 3, blockSize / 3, RED);
            } else if (map[j][i] == 3) { // start loc player
                ball.y = j*4;
                ball.x = i*4;
            } else if (map[j][i] == 4) { // start loc baddies
                if (initBaddie >= 4) continue;
                badGuys[initBaddie].y = j*4;
                badGuys[initBaddie].x = i*4;
            }
        }
    }

    while (1) {
        fillCircle(ball.x, ball.y, BALL_RADIUS, 0x0000);  // erase the old location of the ball
        I2C_IF_Write(ACCDEV, &xREG, 1, 0); // get the x and y accelerometer information using i2c
        I2C_IF_Read(ACCDEV, &dataBuf, 1);  // and adjust the velocities accordingly.
        yVel = adjustVel((int) dataBuf, &velFactor);
        I2C_IF_Write(ACCDEV, &yREG, 1, 0);  // the x and y values from the registers are flipped
        I2C_IF_Read(ACCDEV, &dataBuf, 1);   // since we found that they changed the wrong axis
        xVel = adjustVel((int) dataBuf, &velFactor);
        updateBallLoc(&ball, &xVel, &yVel); // update the ball's location
        fillCircle(ball.x, ball.y, BALL_RADIUS, color); // draw new ball on the screen
        delay(1);
        if (color <= 0x0005) { // keep shifting the color of the ball until it gets
            color = 0xffff;    // too dark
        }
        color -= 0x0002;
    }
}
