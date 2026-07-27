/*
 * can.c
 *
 *  Created on: Jan 4, 2026
 *      Author: dans
 */

#include <can.h>
#define CAN_ARR_NUM 5

static myCAN_t * can_ptr_arr[CAN_ARR_NUM] = {0};
static uint8_t can_ptr_array_idx = 0;

/*
 * \brief: map a HAL handle back to the wrapper that owns it
 *
 * Returns NULL for a handle that was never registered. Every caller is an
 * interrupt callback and must tolerate that: calling Error_Handler() here
 * would disable interrupts and spin forever, taking the whole application
 * down over a transient bus condition.
 */
static myCAN_t * can_from_handle(const FDCAN_HandleTypeDef * hfdcan)
{
	for (uint8_t i = 0; i < can_ptr_array_idx; i++)
	{
		if (can_ptr_arr[i]->_phfdcan == hfdcan)
			return can_ptr_arr[i];
	}
	return NULL;
}

HAL_StatusTypeDef can_setup(myCAN_t * myCAN, FDCAN_HandleTypeDef * can_handler)
{
	if(myCAN == NULL || can_handler == NULL)
		return HAL_ERROR;

	if(can_ptr_array_idx >= CAN_ARR_NUM)
		return HAL_ERROR;

	memset(myCAN, 0, sizeof(myCAN_t));

	myCAN->_phfdcan = can_handler;

	/* Ring buffer first, and registration before the peripheral starts: the
	 * first frame can land the instant HAL_FDCAN_Start() returns, and an
	 * unregistered handle or an uninitialised ring would silently drop it. */
	if(!lwrb_init(&myCAN->_rx_lwrb, myCAN->_rx_buffer, sizeof(myCAN->_rx_buffer)))
		return HAL_ERROR;

	myCAN->can_array_idx = can_ptr_array_idx;
	can_ptr_arr[can_ptr_array_idx++] = myCAN;

	/* MESSAGE_LOST and the error-status trio are what turn a dead link into
	 * something visible: without them an unpowered or unterminated peer just
	 * wedges the TX FIFO and the node sits bus-off with no diagnostic. */
	if(HAL_FDCAN_ActivateNotification(myCAN->_phfdcan,
			FDCAN_IT_RX_FIFO0_NEW_MESSAGE |
			FDCAN_IT_RX_FIFO0_MESSAGE_LOST |
			FDCAN_IT_BUS_OFF |
			FDCAN_IT_ERROR_PASSIVE |
			FDCAN_IT_ERROR_WARNING, 0) != HAL_OK)
		return HAL_ERROR;

	if(HAL_FDCAN_ActivateNotification(myCAN->_phfdcan, FDCAN_IT_TX_COMPLETE,
			0xFFFFFFFF) != HAL_OK)
		return HAL_ERROR;

	if(HAL_FDCAN_Start(myCAN->_phfdcan) != HAL_OK)
		return HAL_ERROR;

	return HAL_OK;
}

/*
 *\brief 		send CAN packet
 *\param[in]	myCAN: 				CAN library struct.
 *\param[in]	payload: 			packet payload 8 bytes of uint8_t.
 *\param[in]	payload_lentgh:		packet payload length 8 bytes max.
 *\param[in]	node_id:			destination node Id. 11 bits in standard mode.
 *\param[in]	is_request: 		RTR bit.
 */
HAL_StatusTypeDef can_send(myCAN_t * myCAN, uint8_t *payload, uint8_t payload_length, uint32_t node_id, uint8_t is_request)
{
	if(myCAN == NULL)
		return HAL_ERROR;

	if(payload_length > 8u)
		return HAL_ERROR;

	/* Standard ids are 11 bits. The HAL shifts this left by 18 into the same
	 * register as the XTD/RTR bits, so a larger value would silently change
	 * the frame type instead of being rejected. */
	if(node_id > 0x7FFu)
		return HAL_ERROR;

	/* Fields that never vary for a classic standard-id frame. const keeps this
	 * in flash, and copying from it means the invariants are stated once.
	 * MessageMarker is a user-defined tag (0-255) copied into the Tx Event
	 * FIFO, which is disabled here, so it is unused. */
	static const FDCAN_TxHeaderTypeDef tx_template = {
		.IdType              = FDCAN_STANDARD_ID,
		.ErrorStateIndicator = FDCAN_ESI_ACTIVE,
		.BitRateSwitch       = FDCAN_BRS_OFF,
		.FDFormat            = FDCAN_CLASSIC_CAN,
		.TxEventFifoControl  = FDCAN_NO_TX_EVENTS,
		.MessageMarker       = 0,
	};

	/* Both live on the stack: HAL_FDCAN_AddMessageToTxFifoQ() copies header and
	 * payload into the peripheral's Message RAM and only then sets TXBAR, so
	 * neither is referenced once the call returns. Keeping them local also
	 * makes can_send() re-entrant, which a shared header would not be. */
	FDCAN_TxHeaderTypeDef tx_header = tx_template;
	uint8_t tx_data[8] = {0};

	tx_header.Identifier  = node_id;
	tx_header.TxFrameType = is_request ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
	tx_header.DataLength  = (uint32_t)payload_length;

	/* A remote frame carries no data, so payload may legitimately be NULL. */
	if(payload != NULL && payload_length > 0u)
		memcpy(tx_data, payload, payload_length);

	return HAL_FDCAN_AddMessageToTxFifoQ(myCAN->_phfdcan, &tx_header, tx_data);
}

void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
	UNUSED(BufferIndexes);

	myCAN_t * myCAN = can_from_handle(hfdcan);

	if(myCAN == NULL)
		return;

	myCAN->stats.tx_complete++;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{

	/* attach interrupt to actual can bus*/
	myCAN_t* myCAN = can_from_handle(hfdcan);

	if(myCAN == NULL)
		return;

	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != 0u)
		myCAN->stats.rx_fifo_lost++;

	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0u)
		return;

	/* Drain the FIFO rather than taking one frame per interrupt:
	 * HAL_FDCAN_IRQHandler clears the new-message flag BEFORE calling us, so a
	 * frame arriving while we are in here raises no further interrupt and
	 * would sit unread until some later frame happened to show up. That
	 * strands one-shot replies (an RTR getter) and looks like a timeout. */
	while(HAL_FDCAN_GetRxFifoFillLevel(myCAN->_phfdcan, FDCAN_RX_FIFO0) > 0u)
	{
		/* Zeroed: the HAL copies only DLC-many bytes, so anything past the
		 * payload would otherwise be whatever this ISR left on the stack. */
		can_rx_packet new_packet = {0};

		/* can_rx_packet is packed, so &new_packet._rx_header is potentially
		 * unaligned and must not be handed to the HAL. Receive into an aligned
		 * temporary and copy it in. */
		FDCAN_RxHeaderTypeDef temp_header;

		if(HAL_FDCAN_GetRxMessage(myCAN->_phfdcan, FDCAN_RX_FIFO0,
				&temp_header, new_packet._rx_data) != HAL_OK)
		{
			myCAN->stats.rx_hal_error++;
			return;
		}

		memcpy(&new_packet._rx_header, &temp_header, sizeof(temp_header));

		/* All-or-nothing. lwrb_write() copies as much as fits and reports the
		 * short count, so an unchecked write on a full ring stores a partial
		 * packet and permanently desynchronises every later read. */
		if(lwrb_get_free(&myCAN->_rx_lwrb) < sizeof(new_packet))
		{
			myCAN->stats.rx_dropped++;
			continue;   /* keep draining the hardware FIFO regardless */
		}

		lwrb_write(&myCAN->_rx_lwrb, &new_packet, sizeof(new_packet));
	}
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
	myCAN_t * myCAN = can_from_handle(hfdcan);

	if(myCAN == NULL)
		return;

	if((ErrorStatusITs & FDCAN_IT_ERROR_WARNING) != 0u)
		myCAN->stats.error_warning++;

	if((ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE) != 0u)
		myCAN->stats.error_passive++;

	if((ErrorStatusITs & FDCAN_IT_BUS_OFF) != 0u)
	{
		/* This interrupt fires on entering AND on leaving bus-off, so read the
		 * actual status rather than counting both edges as a fault. */
		FDCAN_ProtocolStatusTypeDef status = {0};
		HAL_FDCAN_GetProtocolStatus(myCAN->_phfdcan, &status);

		if(status.BusOff != 0u)
		{
			myCAN->stats.bus_off++;

			/* Bus-off sets CCCR.INIT in hardware and takes the node off the
			 * bus for good. Clearing it starts the 129 x 11 recessive-bit
			 * recovery, so the link heals itself once the peer comes back.
			 * HAL_FDCAN_Start() cannot do this: it only acts in state READY
			 * and we are BUSY. */
			CLEAR_BIT(myCAN->_phfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
		}
	}
}


/*
 * \brief: return the number of bytes in the buffer
 *\param[in]: myCAN: pointer to CAN struct
 */

uint32_t can_data_in_buffer(myCAN_t * myCAN)
{
	return lwrb_get_full(&myCAN->_rx_lwrb);
}

/*
 * \brief: reads the data from the lwrb and store inside the new_packet
 * \param[in]: 	myCAN: 		pointer to CAN struct
 * \param[out: 	new_packet: pointer to rx_packet struct to store the data
 */
size_t can_get_from_rbbuffer(myCAN_t * myCAN, can_rx_packet* new_packet)
{
    return lwrb_read(&myCAN->_rx_lwrb, new_packet, sizeof(can_rx_packet));
}
