/*
 * pwm.h
 *
 *  Created on: Mar 8, 2020
 *      Author: Troi-Ryan Stoeffler
 */

#ifndef SOUND_H_
#define SOUND_H_

#define BEEP "33336688"

#define ZERO_CHAR '0'

void playSound(char *newSong);
void generateFrequency(unsigned long frequency);
void stopFrequencyGenerator(void);

void InitSoundModules();
void DeInitSoundModules();

void updateSoundModules(void);

void frequencyGenerator(void);
static void Tick_Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, unsigned long ulValue);

#endif /* SOUND_H_ */
