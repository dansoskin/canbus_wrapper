/*
 * can.c
 *
 *  Created on: Jan 4, 2026
 *      Author: dans
 */

#include <can.h>

FDCAN_TxHeaderTypeDef TxHeader;
FDCAN_RxHeaderTypeDef RxHeader;

uint8_t txData[8];
uint8_t rxData[8];


void setup_can(myCAN_t * myCAN, FDCAN_HandleTypeDef * can_handler)
{
	myCAN->phfdcan = can_handler;

	if(HAL_FDCAN_Start(myCAN->phfdcan) != HAL_OK)
	{
	  Error_Handler();
	}

	if(HAL_FDCAN_ActivateNotification(myCAN->phfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
	{
	  Error_Handler();
	}

	TxHeader.Identifier = 0x123;
	TxHeader.IdType = FDCAN_STANDARD_ID;
	TxHeader.TxFrameType = FDCAN_DATA_FRAME;
	TxHeader.DataLength = FDCAN_DLC_BYTES_8;
	TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
	TxHeader.FDFormat = FDCAN_CLASSIC_CAN;

	TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	TxHeader.MessageMarker = 0;

}


void send_can(myCAN_t * myCAN, uint8_t *payload, uint8_t payload_length)
{
	if(payload_length > 8)
		return;


	TxHeader.DataLength = payload_length;

	memcpy(txData, payload, payload_length);


	HAL_FDCAN_AddMessageToTxFifoQ(myCAN->phfdcan, &TxHeader, txData);

}


void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
	{
		if(HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, rxData)!= HAL_OK)
		{
			Error_Handler();
		}

		if(HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
		{
		  Error_Handler();
		}
	}
}
