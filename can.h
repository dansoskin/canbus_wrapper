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

/* Depth of the software RX ring, in whole CAN packets. A frame arriving while
 * the ring is full is discarded whole and counted in stats.rx_dropped, so size
 * this for the longest stall of the loop that calls can_get_from_rbbuffer().
 * Define it in the build to override. */
#ifndef CAN_RX_BUFFER_PACKETS
#define CAN_RX_BUFFER_PACKETS 32u
#endif

typedef struct __attribute__((packed))
{
	FDCAN_RxHeaderTypeDef _rx_header;
	uint8_t _rx_data[8];
}can_rx_packet;

/* lwrb always keeps one byte free, hence the +1 to fit whole packets. */
#define CAN_RX_BUFFER_SIZE (CAN_RX_BUFFER_PACKETS * sizeof(can_rx_packet) + 1u)


/* No TX buffer lives here on purpose: HAL_FDCAN_AddMessageToTxFifoQ() copies
 * the header and payload into the peripheral's own Message RAM before it
 * returns, so can_send() builds the frame on the stack. Keeping a caller-side
 * buffer alive would only waste RAM -- and a stack frame is re-entrant, which
 * a shared buffer is not. (Contrast HAL_UART_Transmit_DMA, which does retain
 * the pointer, and whose buffer must therefore outlive the call.) */

/* Link diagnostics, all updated from interrupt context. A silently dead CAN
 * link is the hardest thing to debug on this bus, so poll these from the
 * application: any non-zero rx_dropped/rx_fifo_lost means frames were lost,
 * and a rising bus_off means the peer is unpowered, unterminated, or set to a
 * different baud rate. */
typedef struct
{
	uint32_t rx_dropped;     /* software ring was full: frame discarded whole */
	uint32_t rx_fifo_lost;   /* hardware FIFO0 overran before we drained it   */
	uint32_t rx_hal_error;   /* HAL_FDCAN_GetRxMessage() failed               */
	uint32_t tx_complete;    /* frames transmitted and acknowledged by a peer */
	uint32_t bus_off;        /* bus-off entries (recovery is automatic)       */
	uint32_t error_passive;  /* error-passive transitions                     */
	uint32_t error_warning;  /* error-warning transitions                     */
}can_stats_t;

typedef struct
{
	FDCAN_HandleTypeDef* _phfdcan;
	FDCAN_FilterTypeDef _sFilterConfig;

	lwrb_t _rx_lwrb;
	uint8_t _rx_buffer[CAN_RX_BUFFER_SIZE];

	uint8_t can_array_idx;

	volatile can_stats_t stats;

}myCAN_t;

HAL_StatusTypeDef can_setup(myCAN_t * myCAN, FDCAN_HandleTypeDef * can_handler);
HAL_StatusTypeDef can_send(myCAN_t * myCAN, uint8_t *payload, uint8_t payload_length, uint32_t node_id, uint8_t is_request);
uint32_t can_data_in_buffer(myCAN_t * myCAN);
size_t can_get_from_rbbuffer(myCAN_t * myCAN, can_rx_packet* new_packet);

#endif /* CAN_CAN_H_ */
