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

// macros for some constants
#define SPI_IF_BIT_RATE  100000
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
    if((*ball).x + *xVel >= WIDTH - BALL_RADIUS) { // check if in bounds on the right
        (*ball).x = WIDTH - 1 - BALL_RADIUS;
        *xVel = 0;
    }
    if((*ball).x + *xVel < 0 + BALL_RADIUS) { // check if in bounds on the left
        (*ball).x = 0 + BALL_RADIUS;
        *xVel = 0;
    }
    if((*ball).y + *yVel >= HEIGHT - BALL_RADIUS) { // check if in bounds on the bottom
        (*ball).y = HEIGHT - 1 - BALL_RADIUS;
        *yVel = 0;
    }
    if((*ball).y + *yVel < 0 + BALL_RADIUS) { // check if in bounds on the top
        (*ball).y = 0 + BALL_RADIUS;
        *yVel = 0;
    }
    (*ball).x += *xVel; // update the location accordingly
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
    struct Ball ball = {WIDTH / 2, HEIGHT / 2}; // structure that keeps rack of the ball's loc
    unsigned char ACCDEV = 0x18, xREG = 0x3, yREG = 0x5; // device and registers for accel
    const int velFactor = 15; // max velocity

    unsigned char dataBuf; // buffer that holds what was returned from a register
    unsigned int color = 0xffff; // starting color of the ball
    int xVel = 0; // velocities of the ball
    int yVel = 0;
    
    fillScreen(0x0000); // first clear the screen
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
