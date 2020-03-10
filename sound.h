/*
 * pwm.h
 *
 *  Created on: Mar 8, 2020
 *      Author: Troi-Ryan Stoeffler
 */

#ifndef SOUND_H_
#define SOUND_H_

void generateFrequency(unsigned long frequency);
void stopFrequencyGenerator(void);

void InitSoundModules();
void DeInitSoundModules();

void frequencyGenerator(void);
static void Tick_Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, unsigned long ulValue);

#endif /* SOUND_H_ */
