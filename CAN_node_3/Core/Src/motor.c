/*
 * motor.c
 *
 *  Created on: Mar 6, 2025
 *      Author: tantr
 */
#include "main.h"
#include "motor.h"
#include "stm32f1xx_hal.h"

extern motor_t main_motor;
extern uint16_t speed_pulse;


int contraint(int value, int min, int max)
{
    if (value <= min)
    {
        return min;
    }
    else if (value >= max)
    {
        return max;
    }
    return value;
}

int calc_PID(int input, int set_point, float kp, float ki, float kd)
{
	int error = (int)(set_point - input);
	static int pre_set_point = 0;
	static int pre_error = 0;
	int output = (int)(set_point+ kp*error + kd*(error - pre_error));

 	pre_set_point = set_point;
	pre_error = error;
	output = contraint(output, -100, 100);
	return output;
}


