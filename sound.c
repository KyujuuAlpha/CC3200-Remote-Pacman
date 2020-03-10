/*
 * pwm.c
 *
 *  Created on: Mar 8, 2020
 *      Author: Troi-Ryan Stoeffler
 */

#include "sound.h"

#include <stdbool.h>

#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_apps_rcm.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "timer.h"
#include "utils.h"
#include "prcm.h"
#include "timer_if.h"
#include "gpio.h"

static bool freqFlag = false, isGenerating = false;

void generateFrequency(unsigned long frequency) {
    if (!isGenerating) {
        isGenerating = true;
        Tick_Timer_IF_Start(TIMERA0_BASE, TIMER_A, 80000000 / (frequency / 2));
    } else {
        MAP_TimerLoadSet(TIMERA0_BASE, TIMER_A, 80000000 / (frequency / 2));
    }
}

void stopFrequencyGenerator(void) {
    isGenerating = false;
    freqFlag = false;
    GPIOPinWrite(GPIOA0_BASE, 0x1, 0x0);
    Timer_IF_Stop(TIMERA0_BASE, TIMER_A); //stop the timer
}

void InitSoundModules() {
    GPIOPinWrite(GPIOA0_BASE, 0x1, 0x0);
    isGenerating = false;
    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, frequencyGenerator);
}

void DeInitSoundModules() {
    GPIOPinWrite(GPIOA0_BASE, 0x1, 0x0);
    MAP_TimerDisable(TIMERA0_BASE, TIMER_A);
    MAP_PRCMPeripheralClkDisable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
}

void frequencyGenerator(void) {
    Timer_IF_InterruptClear(TIMERA0_BASE); // clear timer interrupt
    if (freqFlag = !freqFlag) {
        GPIOPinWrite(GPIOA0_BASE, 0x1, 0x1);
    } else {
        GPIOPinWrite(GPIOA0_BASE, 0x1, 0x0);
    }
}

static void Tick_Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, unsigned long ulValue) {
    MAP_TimerLoadSet(ulBase,ulTimer,ulValue);
    MAP_TimerEnable(ulBase,ulTimer);
}
