/*
 * ssd1306_udf.c
 *
 *  Created on: Feb 26, 2025
 *      Author: tantr
 */
#include <string.h>
#include "main.h"
#include "math.h"
#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_udf.h"
#include "ssd1306_graphic.h"

static uint8_t tmp_toggle = 0;

static int needle_angle_speed = 45;
static int needle_angle_rpm = 45;


/* needle speed parameter */
static uint16_t needle_speed_start_x;
static uint16_t needle_speed_start_y;
static uint16_t needle_speed_end_x;
static uint16_t needle_speed_end_y;
static uint16_t needle_speed_center_x = 59;
static uint16_t needle_speed_center_y = 43;
static uint16_t needle_speed_big = 20;
static uint16_t needle_speed_small = 5;

/* needle rpm parameter */
static uint16_t needle_rpm_start_x;
static uint16_t needle_rpm_start_y;
static uint16_t needle_rpm_end_x;
static uint16_t needle_rpm_end_y;
static uint16_t needle_rpm_center_x = 16;
static uint16_t needle_rpm_center_y = 52;
static uint16_t needle_rpm_big = 10;
//	static uint16_t needle_rpm_small = 5;

void disp_msg_7x10(char* msg, uint8_t x, uint8_t y){
	ssd1306_SetCursor(x, y);
	ssd1306_WriteString(msg, Font_7x10, White);
	ssd1306_UpdateScreen();
}

void disp_msg_11x18(char* msg, uint8_t x, uint8_t y){
	ssd1306_SetCursor(x, y);
	ssd1306_WriteString(msg, Font_11x18, White);
	ssd1306_UpdateScreen();
}

void disp_msg_uint(uint16_t number, uint8_t x, uint8_t y)
{
	char c_number[4];
	sprintf(c_number,"%3d",number);
	disp_msg_11x18(c_number, x, y);
}

void disp_draw_dashboard()
{
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_DrawBitmap(0, 0, dashboard_128x64, 128, 64, White);

    needle_angle_speed = 45;
	needle_speed_start_x = needle_speed_big * -sin(needle_angle_speed * 3.14 / 180) + needle_speed_center_x;
	needle_speed_start_y = needle_speed_big * cos(needle_angle_speed * 3.14/ 180) + needle_speed_center_y;

	needle_speed_end_x = needle_speed_small * -sin((needle_angle_speed + 180) * 3.14 / 180) + needle_speed_center_x;
	needle_speed_end_y = needle_speed_small * cos((needle_angle_speed + 180)* 3.14/ 180) + needle_speed_center_y;
	ssd1306_Line(needle_speed_start_x, needle_speed_start_y, needle_speed_end_x, 	needle_speed_end_y, White);

	needle_angle_rpm = 45;
	needle_rpm_start_x = needle_rpm_big * -sin(needle_angle_rpm * 3.14 / 180) + needle_rpm_center_x;
	needle_rpm_start_y = needle_rpm_big * cos(needle_angle_rpm * 3.14/ 180) + needle_rpm_center_y;

	ssd1306_Line(needle_rpm_start_x, needle_rpm_start_y, needle_rpm_center_x, 	needle_rpm_center_y, White);

}

void disp_turn_signal_handle (enum light_mode_t mode)
{
	switch (mode)
	{
	case light_hazard:
		disp_toggle_turn_signal(hazard_blinker);
		break;

	case light_left:
		disp_toggle_turn_signal(left_blinker);
		break;

	case light_right:
		disp_toggle_turn_signal(right_blinker);
		break;

	case stop_light:
		ssd1306_DrawBitmap(0, 0, left_blinker , 128, 64, Black);
		ssd1306_DrawBitmap(0, 0, right_blinker , 128, 64, Black);
		break;

	default:
		break;
	}
}

void disp_toggle_turn_signal (const unsigned char * bitmap)
{
	tmp_toggle = !tmp_toggle;
	ssd1306_DrawBitmap(0, 0, bitmap, 128, 64, tmp_toggle ? White : Black);
}

void disp_off_turn_signal ()
{
	ssd1306_DrawBitmap(0, 0, hazard_blinker , 128, 64, Black);
	ssd1306_DrawBitmap(0, 0, left_blinker , 128, 64, Black);
	ssd1306_DrawBitmap(0, 0, right_blinker , 128, 64, Black);
	tmp_toggle = 0;
}

void disp_error_motor ()
{
	ssd1306_DrawBitmap(0, 0, error_blinker, 128, 64, White);
}

void disp_needle_speed (uint16_t speed, uint16_t rpm)
{
	static uint16_t pre_speed = 0;
	static uint16_t pre_rpm = 0;

	if (abs(speed - pre_speed) > 2)
	{
		ssd1306_Line(needle_speed_start_x, needle_speed_start_y, needle_speed_end_x, 	needle_speed_end_y, Black);
		needle_angle_speed = speed * 270/150 + 45;
		needle_speed_start_x = needle_speed_big * -sin(needle_angle_speed * 3.14 / 180) + needle_speed_center_x;
		needle_speed_start_y = needle_speed_big * cos(needle_angle_speed * 3.14/ 180) + needle_speed_center_y;

		needle_speed_end_x = needle_speed_small * -sin((needle_angle_speed + 180) * 3.14 / 180) + needle_speed_center_x;
		needle_speed_end_y = needle_speed_small * cos((needle_angle_speed + 180)* 3.14/ 180) + needle_speed_center_y;

		ssd1306_Line(needle_speed_start_x, needle_speed_start_y, needle_speed_end_x, 	needle_speed_end_y, White);
		pre_speed = speed;
	}
	if (abs(rpm - pre_rpm) > 2)
	{
		ssd1306_Line(needle_rpm_start_x, needle_rpm_start_y, needle_rpm_center_x, 	needle_rpm_center_y, Black);
		needle_angle_rpm = rpm * 270/250 + 45;
		needle_rpm_start_x = needle_rpm_big * -sin(needle_angle_rpm * 3.14 / 180) + needle_rpm_center_x;
		needle_rpm_start_y = needle_rpm_big * cos(needle_angle_rpm * 3.14/ 180) + needle_rpm_center_y;

//		needle_rpm_end_x = needle_rpm_small * -sin((needle_angle_deg + 180) * 3.14 / 180) + needle_rpm_center_x;
//		needle_rpm_end_y = needle_rpm_small * cos((needle_angle_deg + 180)* 3.14/ 180) + needle_rpm_center_y;

		ssd1306_Line(needle_rpm_start_x, needle_rpm_start_y, needle_rpm_center_x, 	needle_rpm_center_y, White);
		pre_rpm = rpm;
	}
}

