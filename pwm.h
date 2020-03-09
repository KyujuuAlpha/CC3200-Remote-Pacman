/*
 * pwm.h
 *
 *  Created on: Mar 8, 2020
 *      Author: troir
 */

#ifndef PWM_H_
#define PWM_H_

void generateFrequency(unsigned long frequency);
void stopFrequencyGenerator(void);

void InitPWMModules();
void DeInitPWMModules();

void frequencyGenerator(void);
static void Tick_Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, unsigned long ulValue);

#endif /* PWM_H_ */
