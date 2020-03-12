// Troi-Ryan Stoeffler
// Corbin Harrell
// TA: Begum Kasap

// Standard includes
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

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
#include "json.h"

// macros for some constants
#define SPI_IF_BIT_RATE  800000
#define TR_BUFF_SIZE     100
#define PAC_SIZE         4
#define MAX_VEL          1

#define START_STATE  0
#define GAME_STATE   1
#define GOVER_STATE  2
#define TITLE_SCREEN 3

#define ENABLE_SERVER 1

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

// pac struct
struct Pac {
    int x;
    int y;
    int score;
};

struct Baddie {
    int id;
    int x;
    int y;
    int velX;
    int velY;
    int color;
    char dirQueue[8];
    bool ready;
    bool validMoves[4];
};
// static function prototypes
static void updatePacLoc(struct Pac *Pac, int *xVel, int *yVel); // updates pac loc based on vel
static int adjustVel(int vel, const int *velFactor); // adjusts raw readout from accelerometer to usable vel
static long dropFrame(long frameCount); // tracks number of dropped frames
static char *integerToString(int i);
static char *coordsToString(int i, int j);
static unsigned long getCurrentSysTimeMS(void);
void decideVelocities(struct Baddie *bad); // decides next move direction of bad based on queue & valid moves
static void determineValidMoves(struct Baddie *bad); // sets valid moves of bads
static void BoardInit(void);

static void mainGameLogic(void); // game logic for playing
static void gameOverLogic(void); // logic for reseting
static void startScreenLogic(void); // logic for init
static void gameLoop(void); // loop over logics based on state

int pellet_counter = 0;

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

#if ENABLE_SERVER == 1
    // connect to the network
    networkConnect();
#endif

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

// some function definitions

int abs(int val) {
    if (val < 0) return -val;
    return val;
}

static char *integerToString(int i) {
    static char stringBufA[20] = "";
    sprintf(stringBufA, "%d", i); // prints int to buffer
    return stringBufA;
}

static char *coordsToString(int i, int j) {
    static char stringBufB[20] = "";
    sprintf(stringBufB, "%d %d", i, j); // prints coords to buff
    return stringBufB;
}


// MAIN GAME LOOP STUFF
static int frameDrop = 0;
static long dropFrame(long delay) {
    frameDrop++;
    return (delay += 888800) < 0 ? dropFrame(delay) : delay;
}

static unsigned long getCurrentSysTimeMS(void) {
    // slow clock keeps track of real time, used to establish connection and seed random
    // convert PRCMSlowClkCtrGet ticks to time
    return (unsigned long) ((unsigned long long) PRCMSlowClkCtrGet() / 32768.0 * 1000.0);
}

static char state;
static bool skipFrameDrop;
static void gameLoop(void) {
    // main game loop
    unsigned long prevTime = getCurrentSysTimeMS();
    long newDelay = 0;
    skipFrameDrop = false;
    state = TITLE_SCREEN; // initial state

    while (1) {
        if (skipFrameDrop) {
            frameDrop = 0;
            skipFrameDrop = false;
        }

        do {
            switch (state) {
                case TITLE_SCREEN:
                    titleScreenLogic();
                case START_STATE:
                    startScreenLogic(); // updates state to game_state
                    break;
                case GAME_STATE: //runs until game over reached
                    mainGameLogic();
                    break;
                case GOVER_STATE: // resets state to start after 5 seconds
                    gameOverLogic();
                    break;
            }

        } while (frameDrop-- > 0); // once a certain number of frames drop, reset frames
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

// INITIAL STATE STUFF
const int blockSize = WIDTH / MAP_SIZE;

static struct Pac pac; // structure that keeps track of the pac's loc
static struct Baddie badGuys[4] = {
                             { 0, -1, -1, 0, 0, BAD_1_COLOR, "", false },
                             { 1, -1, -1, 0, 0, BAD_2_COLOR, "", false },
                             { 2, -1, -1, 0, 0, BAD_3_COLOR, "", false },
                             { 3, -1, -1, 0, 0, BAD_4_COLOR, "", false }
                            };
static int xVel = 0, yVel = 0; // velocities of the pac
static int tickTimer = 0, tickCounter = 0, selectedBaddie = -1;

static void drawScore(void) {
    fillRect(12, 4, 17, 8, 0x0000); // clears current written score
    setCursor(12,4);
    Outstr(integerToString(pac.score)); // prints score in upper right
}

static void startScreenLogic(void) {
    fillScreen(0x0000); // first clear the screen
    int i, j, k, initBaddie = 0;
    for (i = 0; i < 4; i++) { // init all bads to impossible location so program knows they aren't initialized
        badGuys[i].x = -1;
        badGuys[i].y = -1;
    }
    // initial stuff
    // iterate through map, setting info based on values in each entry
    for (i = 0; i < MAP_SIZE; i++) {
        for (j = 0; j < MAP_SIZE; j++) {
            if (map[j][i] == WALL) {
                // draw wall tile
                fillRect(i * blockSize, j * blockSize, blockSize, blockSize, WALL_COLOR);
            } else if (map[j][i] == POINT || map[j][i] == PLACEHOLDER) { // point pac
                // draw point pellet, reset inactive pellets to active
                map[j][i] = POINT;
                pellet_count++;
                k =  blockSize / 2; // middle of tile to draw pellet
                fillRect(i * blockSize + blockSize / 2 - k / 2, j * blockSize + blockSize / 2 - k / 2, k, k, POINT_COLOR);
            } else if (map[j][i] == SPAWN) { // start loc player
                // set pac location
                pac.y = j*4;
                pac.x = i*4;
                drawScore();
            } else if (map[j][i] == ENEMY) { // start loc baddies
                // init next bad
                if (initBaddie >= 4) continue; // do not init more than 4 bads
                badGuys[initBaddie].y = j*4;
                badGuys[initBaddie].x = i*4;
                badGuys[initBaddie].velY = 0;
                badGuys[initBaddie].velX = 0;
                strcpy(badGuys[initBaddie].dirQueue, ""); // clear dir queue
                badGuys[initBaddie].ready = false;
                determineValidMoves(&badGuys[initBaddie]);
                decideVelocities(&badGuys[initBaddie]); // set random move dir
                initBaddie++;
            }
        }
    }
    tickTimer = 0;
    tickCounter = 0;
    selectedBaddie = -1;
    skipFrameDrop = true;
    state = GAME_STATE; // switch to main game state, there is a possibility for a title screen
}

// MAIN GAME STUFF
static bool yCollision(int x, int y, int yVel) {
    // Wall check
    const int blockSize = WIDTH / MAP_SIZE; // set pix size of grid tiles
    int blockX, blockY;
    blockX = (x + 2) / blockSize; // the X coor of grid tile that mid pixel of top/bottom edge lies in
    if (yVel < 0) {
        blockY = (y + yVel) / blockSize; // Y coor of grid tile that mid pixel of top edge lies
    } else {
        blockY = (y + yVel + PAC_SIZE - 1) / blockSize; // Y coor of grid tile that mid pixel of bottom edge lies
    }
    return map[blockY][blockX] == WALL; // check map tile that enemy would move into
}

static bool xCollision(int x, int y, int xVel) {
    // Wall check
    const int blockSize = WIDTH / MAP_SIZE; // set pix size of grid tiles
    int blockX, blockY;
    blockY = (y+2) / blockSize; // the Y coor of grid tile that mid pixel of left/right edge lies in
    if (xVel < 0) {
        blockX = (x + xVel) / blockSize; // X coor of grid tile that mid pixel of left edge lies
    } else {
        blockX = (x + xVel + PAC_SIZE - 1) / blockSize; // X coor of grid tile that mid pixel of right edge lies
    }
    return map[blockY][blockX] == WALL; // check map tile that enemy would move into
}

bool enemyHit(struct Pac* pac, struct Baddie* bad) {
    // check if enemy and pac share grid coords
    return ((int)pac->x/4 == (int)bad->x/4 && (int)pac->y/4 == (int)bad->y/4);
}

void decideVelocities(struct Baddie *bad) {
    char dirChoice = 4, i;
    if (bad->dirQueue[0] != '\0') { // dir queue not empty
        do {
            if (bad->dirQueue[0] == '\0') {
                break;
            }
            dirChoice = bad->dirQueue[0] - '0';
            i = 1;
            while (bad->dirQueue[i - 1] != '\0') {
                bad->dirQueue[i - 1] = bad->dirQueue[i];
                i++;
            }
            if (bad->dirQueue[0] == '\0') {
                bad->ready = true; // set enemy ready to recieve next dir
            }
        } while (!bad->validMoves[dirChoice]); // find first valid move in queue, set that as dir
    } else if(bad->id != selectedBaddie) { // queue empty
        srand((unsigned int) getCurrentSysTimeMS());
        int sanityCheck = 0; // prevents looping forever in case no valid moves
        dirChoice = rand() % 4; // choose ran num 0-3 to start in valid moves
        while (!bad->validMoves[dirChoice] && sanityCheck < 4) {
            dirChoice++; // iterate through dir till valid found
            sanityCheck++;
            if (dirChoice == 4) dirChoice = 0;
        }
    }
    switch (dirChoice) {
        case 0:
            bad->velY = -1; // move up
            break;
        case 1:
            bad->velX = -1; // move left
            break;
        case 2:
            bad->velX = 1; // move down
            break;
        case 3:
            bad->velY = 1; // move right
            break;
    }
}

static void updateBaddieLoc(struct Baddie* bad) {
    if(bad->velY != 0) {
        if (!yCollision(bad->x, bad->y, bad->velY)) { // if there's no collision, move
            bad->y += bad->velY;
            bad->x = ((bad->x + 2) / 4) * 4; // fix pac x to left side of grid tile
        } else {
            bad->velY = 0;
        }
    }
    if(bad->velX != 0) {
        if (!xCollision(bad->x, bad->y, bad->velX)) {
            bad->x += bad->velX;
            bad->y = ((bad->y + 2) / 4) * 4; // fix pac y to top side of grid tile
        } else {
            bad->velX = 0;
        }
    }
    determineValidMoves(bad); // update valid moves based on new position
}

// function that updates the Pac's location accordingly and keeps it within bounds
static void updatePacLoc(struct Pac *pac, int *xVel, int *yVel) {

    bool yGreater = abs(*yVel) > abs(*xVel); // determine greater vel
    if(*yVel > MAX_VEL) *yVel = MAX_VEL; // bind vel to +-1
    if(*yVel < -MAX_VEL) *yVel = -MAX_VEL;
    if(*xVel > MAX_VEL) *xVel = MAX_VEL;
    if(*xVel < -MAX_VEL) *xVel = -MAX_VEL;
    if (*yVel == 0 && *xVel == 0) return; // no movement

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
            // move x, position y on rails
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

static void parseGETRequest(char *request) {
    parseJSON(request);
    char *val;
    if (badGuys[0].dirQueue[0] == '\0' && !badGuys[0].ready) { // badGuy is ready to recieve new commands
        val = getValue("b1_q"); // retireve shadow update
        if (strcmp(val, "ready") != 0) { // there was something to recieve
            strcpy(badGuys[0].dirQueue, val); // copy vals to queue
            selectedBaddie = 0;
        }
    }
    if (badGuys[1].dirQueue[0] == '\0' && !badGuys[1].ready) {
        val = getValue("b2_q");
        if (strcmp(val, "ready") != 0) {
            strcpy(badGuys[1].dirQueue, val);
            selectedBaddie = 1;
        }
    }
    if (badGuys[2].dirQueue[0] == '\0' && !badGuys[2].ready) {
        val = getValue("b3_q");
        if (strcmp(val, "ready") != 0) {
            strcpy(badGuys[2].dirQueue, val);
            selectedBaddie = 2;
        }
    }
    if (badGuys[3].dirQueue[0] == '\0' && !badGuys[3].ready) {
        val = getValue("b4_q");
        if (strcmp(val, "ready") != 0) {
            strcpy(badGuys[3].dirQueue, val);
            selectedBaddie = 3;
        }
    }
}

static unsigned char ACCDEV = 0x18, xREG = 0x3, yREG = 0x5; // device and registers for accel
static const int velFactor = 15; // max velocity;
static bool pollReceiveMode = false, requestFlag = false;
static char pointX[12], pointY[12], pointIndex = 0;

// this is called every 33 ms, barring the that frames are skipped!
static void mainGameLogic(void) {
    if (tickTimer >= 2) { // get new data 10 times a second
        static char *receive;
        unsigned char dataBuf; // buffer that holds what was returned from a register
        tickTimer = 0;
        I2C_IF_Write(ACCDEV, &xREG, 1, 0); // get the x and y accelerometer information using i2c
        I2C_IF_Read(ACCDEV, &dataBuf, 1);  // and adjust the velocities accordingly.
        yVel = adjustVel((int) dataBuf, &velFactor);
        I2C_IF_Write(ACCDEV, &yREG, 1, 0);  // the x and y values from the registers are flipped
        I2C_IF_Read(ACCDEV, &dataBuf, 1);   // since we found that they changed the wrong axis
        xVel = adjustVel((int) dataBuf, &velFactor);

        if (pollReceiveMode) { // retireve shadow update
            receive = networkReceive();
            if (strlen(receive) > 4) {
                if (!requestFlag) {
                    parseGETRequest(receive);
                }
                pollReceiveMode = false;
            }
        }

        if (tickCounter > 10) { // alternate between POST and GET every 2 seconds
            tickCounter = 0;
#if ENABLE_SERVER == 1
            if (!pollReceiveMode) { //only continue if received the response to the old request
                if (requestFlag = !requestFlag) { // if now true
                    buildRequest("pac_loc", coordsToString(pac.x, pac.y));
                    buildRequest("b1_loc", coordsToString(badGuys[0].x, badGuys[0].y));
                    buildRequest("b2_loc", coordsToString(badGuys[1].x, badGuys[1].y));
                    buildRequest("b3_loc", coordsToString(badGuys[2].x, badGuys[2].y));
                    buildRequest("b4_loc", coordsToString(badGuys[3].x, badGuys[3].y));
                    if (badGuys[0].ready) {
                        badGuys[0].ready = false;
                        buildRequest("b1_q", "ready");
                    }
                    if (badGuys[1].ready) {
                        badGuys[1].ready = false;
                        buildRequest("b2_q", "ready");
                    }
                    if (badGuys[2].ready) {
                        badGuys[2].ready = false;
                        buildRequest("b3_q", "ready");
                    }
                    if (badGuys[3].ready) {
                        badGuys[3].ready = false;
                        buildRequest("b4_q", "ready");
                    }
                    receive = sendRequest();
                } else { // if now false
                    receive = receiveString();
                }
                if (strlen(receive) <= 4) {
                    pollReceiveMode = true;
                } else {
                    if (!requestFlag) {
                        parseGETRequest(receive);
                    }
                    pollReceiveMode = false;
                }
            }
#endif
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
    fillRect(pac.x, pac.y, PAC_SIZE, PAC_SIZE, PLAYER_COLOR); // draw new pac on the screen
    int bad;
    pointIndex = 0;
    for (bad = 0; bad < 4; bad++) { // iterate through bads
        // get grid coord of cur bad
        int badGridX = badGuys[bad].x/4;
        int badGridY = badGuys[bad].y/4;
        fillRect(badGuys[bad].x, badGuys[bad].y, PAC_SIZE, PAC_SIZE, 0x0000); // clear bad from screen
        // Check the grid locs around bad and set them to fill them with point pellet to prevent enemy from erasing them
        if (map[badGridY][badGridX] == POINT) {
            pointX[pointIndex] = badGridX;
            pointY[pointIndex++] = badGridY;
        }
        // check position below
        if ((badGuys[bad].y + (blockSize / 2)) / 4 != badGridY && map[badGridY + 1][badGridX] == POINT) {
            pointX[pointIndex] = badGridX;
            pointY[pointIndex++] = badGridY + 1;
        }
        // check position to right
        if ((badGuys[bad].x + (blockSize / 2)) / 4 != badGridX && map[badGridY][badGridX + 1] == POINT) {
            pointX[pointIndex] = badGridX + 1;
            pointY[pointIndex++] = badGridY;
        }
        updateBaddieLoc(&badGuys[bad]); // try to move bad
        // if velocities zero (collision)
        if (badGuys[bad].velX == 0 && badGuys[bad].velY == 0) {
            decideVelocities(&badGuys[bad]); // next move in queue or random valid
        }
        fillRect(badGuys[bad].x, badGuys[bad].y, PAC_SIZE, PAC_SIZE, badGuys[bad].color); // redraw enemy in ne loc
        if(enemyHit(&pac, &badGuys[bad])) { // check if enemy collision with pac
            tickTimer = 0;
            state = GOVER_STATE; // gg u loose
            return;
        }
    }
    int i, k = blockSize / 2;
    // fill all point tiles that were erased by bads
    for (i = 0; i < pointIndex; i++) {
        for (bad = 0; bad < 4; bad++) {
            if (pointX[i] == badGuys[bad].x/4 && pointY[i] == badGuys[bad].y/4) {
                break;
            }
            if (bad == 3) {
                fillRect(pointX[i] * blockSize + blockSize / 2 - k / 2, pointY[i] * blockSize + blockSize / 2 - k / 2, k, k, POINT_COLOR);
            }
        }
    }

    if(map[pac.y/blockSize][pac.x/blockSize] == POINT) {
        // update score if pac has entered a point tile
        map[pac.y/blockSize][pac.x/blockSize] = PLACEHOLDER;
        pac.score++;
        pellet_count--;
        if (pellet_count == 0) {
            state = GOVER_STATE;
        }
        drawScore();
        playSound(BEEP);
    }
}

// GAME OVER STUFF
static void gameOverLogic(void) {
    if (tickTimer == 0 && pellet_count > 0) {
        // clear screen
        fillRect(0, 0, WIDTH, HEIGHT, 0x0000);
        setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 16);
        Outstr("GAME OVER");
        setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 8);
        Outstr("Score: ");
        // draw final score
        Outstr(integerToString(pac.score));
        playSound(DEATH);
    } else if (tickTimer == 0 && pellet_count == 0) {
        // clear screen
        fillRect(0, 0, WIDTH, HEIGHT, 0x0000);
        setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 16);
        Outstr("SCREEN CLEARED");
        setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 8);
        Outstr("Score: ");
        // draw final score
        Outstr(integerToString(pac.score));
        playSound(DEATH);
    }
    if (tickTimer > 30 * 5 && pellet_count > 0) { // wait five seconds (30 frames * 5)
        pollReceiveMode = false;
        requestFlag = false;
        state = TITLE_SCREEN;
    } else if (tickTimer > 30 * 5 && pellet_count == 0) { // wait five seconds (30 frames * 5)
        pollReceiveMode = false;
        requestFlag = false;
        state = START_STATE;
    } else {
        tickTimer++;
    }
}

static void titleScreenLogic() {
    if (tickTimer == 0) {
        fillRect(0, 0, WIDTH, HEIGHT, 0x0000);
        setCursor(WIDTH / 2 - 32, HEIGHT / 2 - 16);
        Outstr("PAC MAN");
    }
    if (tickTimer > 30 * 5) {
        state = START_STATE;
    } else {
        tickTimer++;
    }
    pac.score = 0;
}

static void determineValidMoves(struct Baddie* bad) {
    // U
    bad->validMoves[0] = !yCollision(bad->x, bad->y, -1);
    // L
    bad->validMoves[1] = !xCollision(bad->x, bad->y, -1);
    // R
    bad->validMoves[2] = !xCollision(bad->x, bad->y, 1);
    // D
    bad->validMoves[3] = !yCollision(bad->x, bad->y, 1);
}
