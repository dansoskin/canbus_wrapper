/*
 * can.c
 *
 *  Created on: Jan 4, 2026
 *      Author: dans
 */

#include <can.h>
#define CAN_ARR_NUM 5

myCAN_t * can_ptr_arr[CAN_ARR_NUM] = {0};
uint8_t can_ptr_array_idx = 0;
volatile uint32_t msg_counter = 0;

static inline uint32_t get_fdcan_dlc(uint8_t length);

HAL_StatusTypeDef can_setup(myCAN_t * myCAN, FDCAN_HandleTypeDef * can_handler)
{
	if(can_ptr_array_idx >= CAN_ARR_NUM)
		return HAL_ERROR;

	memset(myCAN, 0, sizeof(myCAN_t));

	myCAN->_phfdcan = can_handler;

	myCAN->can_array_idx = can_ptr_array_idx;
	can_ptr_arr[can_ptr_array_idx++] = myCAN;


	if(HAL_FDCAN_Start(myCAN->_phfdcan) != HAL_OK)
	{
	  Error_Handler();
	}

	if(HAL_FDCAN_ActivateNotification(myCAN->_phfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0)!= HAL_OK)
	{
	  Error_Handler();
	}

	if(HAL_FDCAN_ActivateNotification(myCAN->_phfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE|
				FDCAN_IT_TX_COMPLETE
				, 0xFFFFFFFF)!= HAL_OK)
		{
		  Error_Handler();
		}


	uint8_t rsp = lwrb_init(&myCAN->_rx_lwrb, myCAN->_rx_buffer, sizeof(myCAN->_rx_buffer));

	if (rsp)
		return HAL_OK;
	return HAL_ERROR;

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

	if(payload_length > FDCAN_DLC_BYTES_8)
		return HAL_ERROR;

	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.Identifier = node_id;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.IdType = FDCAN_STANDARD_ID;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.TxFrameType = is_request ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.DataLength = (uint32_t)payload_length;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.BitRateSwitch = FDCAN_BRS_OFF;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.FDFormat = FDCAN_CLASSIC_CAN;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header.MessageMarker = 0;		//MessageMarker is a user-defined tag (0–255) that you attach to a TX message.
	

	memcpy(&myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_data, payload, payload_length);

	HAL_StatusTypeDef ret_value = HAL_FDCAN_AddMessageToTxFifoQ(myCAN->_phfdcan,
						&(myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_header),
						myCAN->_tx_buffer[myCAN->tx_buffer_idx]._tx_data);

	myCAN->tx_buffer_idx = (myCAN->tx_buffer_idx + 1) % CAN_TX_BUFFER_SIZE;

	return ret_value;
}

void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
	++msg_counter;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{

	/* attach interrupt to actual can bus*/
	myCAN_t* myCAN = NULL;
	for (uint8_t i = 0; i < can_ptr_array_idx; i++)
	{
		if (can_ptr_arr[i]->_phfdcan == hfdcan)
			myCAN = can_ptr_arr[i];
	}

	if(myCAN == NULL)
		Error_Handler();


	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
	{
		can_rx_packet new_packet;
		FDCAN_RxHeaderTypeDef temp_header;

		if(HAL_FDCAN_GetRxMessage(myCAN->_phfdcan, FDCAN_RX_FIFO0, &temp_header, new_packet._rx_data)!= HAL_OK)
		{

//			new_packet._rx_header = temp_header;
			Error_Handler();
		}

		if(HAL_FDCAN_ActivateNotification(myCAN->_phfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
		{
		  Error_Handler();
		}

		memcpy(&new_packet._rx_header, &temp_header, sizeof(temp_header));
		lwrb_write(&myCAN->_rx_lwrb, &new_packet, sizeof(new_packet));
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



