/*
 * can.h
 *
 *  Created on: Jan 4, 2026
 *      Author: dans
 */

#ifndef CAN_CAN_H_
#define CAN_CAN_H_

#include <main.h>
#include <string.h>

typedef struct
{
	FDCAN_HandleTypeDef * phfdcan;



}myCAN_t;

void setup_can(myCAN_t * myCAN, FDCAN_HandleTypeDef * can_handler);


void send_can(myCAN_t * myCAN, uint8_t *payload, uint8_t payload_length);



#endif /* CAN_CAN_H_ */
