/*
 * pwm.h
 *
 *  Created on: Mar 8, 2020
 *      Author: Troi-Ryan Stoeffler
 */

#ifndef SOUND_H_
#define SOUND_H_

// 0 - 9 (2000 Hz to 5000 Hz) for each character
// each char lasts 33 ms
#define BEEP  "34"
#define DEATH "8888666611111100000000"

#define ZERO_CHAR '0'

void playSound(char *newSong);
void stopSound(void);
int isSoundPlaying(void);
void generateFrequency(unsigned long frequency);
void stopFrequencyGenerator(void);

void InitSoundModules();
void DeInitSoundModules();

void updateSoundModules(void);

void frequencyGenerator(void);
static void Tick_Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, unsigned long ulValue);

#endif /* SOUND_H_ */
