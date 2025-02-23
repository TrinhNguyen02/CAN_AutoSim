/*
 * kalman_filter.h
 *
 *  Created on: Feb 16, 2025
 *      Author: tantr
 */

#ifndef INC_KALMAN_FILTER_H_
#define INC_KALMAN_FILTER_H_

typedef struct
{
    float _err_measure;
    float _err_estimate;
    float _q;
    float _current_estimate;
    float _last_estimate;
    float _kalman_gain;
} kalman_t;

kalman_t * create_kalman (float mea_e, float est_e, float q);
float update_kalman (kalman_t * kalman, float value);



#endif /* INC_KALMAN_FILTER_H_ */
