/*
 * kalman_filter.c
 *
 *  Created on: Feb 16, 2025
 *      Author: tantr
 */
#include "kalman_filter.h"

kalman_t * create_kalman (float mea_e, float est_e, float q)
{
    kalman_t * kalman_instance = (kalman_t *)malloc(sizeof(kalman_t));
    kalman_instance->_current_estimate = 0;
    kalman_instance->_last_estimate = 0;
    kalman_instance->_kalman_gain = 0;
    kalman_instance->_err_measure = mea_e;
    kalman_instance->_err_estimate = est_e;
    kalman_instance->_q = q;
    return kalman_instance;
}

float update_kalman (kalman_t * kalman, float value)
{
    kalman->_kalman_gain = kalman->_err_estimate / (kalman->_err_estimate + kalman->_err_measure);
    kalman->_current_estimate = kalman->_last_estimate + kalman->_kalman_gain * (value - kalman->_last_estimate);
    kalman->_err_estimate = (1.0f - kalman->_kalman_gain) * kalman->_err_estimate + fabsf(kalman->_last_estimate - kalman->_current_estimate) * kalman->_q;
    kalman->_last_estimate = kalman->_current_estimate;

    return kalman->_current_estimate;
}

