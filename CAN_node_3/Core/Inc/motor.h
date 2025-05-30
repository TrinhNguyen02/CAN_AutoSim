/*
 * motor.h
 *
 *  Created on: Mar 6, 2025
 *      Author: tantr
 */

#ifndef INC_MOTOR_H_
#define INC_MOTOR_H_


#define 	MAX_SPEED 		144
#define 	PULSES_ENC		22
#define		SAMPLING_ENC	50


typedef struct
{
	uint32_t curr_cnt;
	uint32_t pre_cnt;
	uint16_t actl_speed;
	uint16_t actl_rpm;
	uint8_t _fault_detected;
} motor_t;

int error;


#endif /* INC_MOTOR_H_ */
