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
#include "lwrb.h"

#define CAN_RX_BUFFER_SIZE (8 * sizeof(can_rx_packet)) // set the buffer size as the size of 8 CAN packets
#define CAN_TX_BUFFER_SIZE 10// set the buffer size as the size of 10 CAN packets

typedef struct __attribute__((packed))
{
	FDCAN_RxHeaderTypeDef _rx_header;
	uint8_t _rx_data[8];
}can_rx_packet;


typedef struct __attribute__((packed))
{
	FDCAN_TxHeaderTypeDef _tx_header;
	uint8_t _tx_data[8];
}can_tx_packet;

typedef struct
{
	FDCAN_HandleTypeDef* _phfdcan;
	FDCAN_FilterTypeDef _sFilterConfig;

	can_tx_packet _tx_buffer[CAN_TX_BUFFER_SIZE];
	uint32_t tx_buffer_idx;
//	FDCAN_TxHeaderTypeDef _tx_header;
//	uint8_t _tx_data[8];

	lwrb_t _rx_lwrb;
	uint8_t _rx_buffer[CAN_RX_BUFFER_SIZE];

	uint8_t can_array_idx;

}myCAN_t;

HAL_StatusTypeDef can_setup(myCAN_t * myCAN, FDCAN_HandleTypeDef * can_handler);
HAL_StatusTypeDef can_send(myCAN_t * myCAN, uint8_t *payload, uint8_t payload_length, uint32_t node_id, uint8_t is_request);
uint32_t can_data_in_buffer(myCAN_t * myCAN);
size_t can_get_from_rbbuffer(myCAN_t * myCAN, can_rx_packet* new_packet);

#endif /* CAN_CAN_H_ */
