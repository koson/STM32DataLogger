/*
 * logger.c
 *
 *  Created on: 08.09.2020
 *      Author: philzo
 */

#include <stdint.h>
#include <stdlib.h>
#include "main.h"
#include "usart.h"
#include "logger.h"
#include "adc.h"

/* global variables ----------------------------------------------------------*/
uint8_t rxData[6];

sTrig Trigger = {
	.eTrigStatus = TRIG_IDLE,
	.idxChannel = 0,
	.fTrigVal = 0,
	.smplCh = 0,
	.numSmplCh = 0
};

enum eData Datahandler = DATA_IDLE;


float V_U_TrigAnalog_old = 0;
uint8_t V_D_TrigDigital_old = 0;

uint16_t BuffAna_q11[SAMPLES];
uint8_t BuffDig_q11[SAMPLES];
uint32_t BuffIdx = 0;
uint32_t StartSendIdx = 0;
uint8_t triggered = 0;
uint8_t sendChunk = 0;		// send data is divided in 4 chunks. first 2 chunks for analog signals are send, then 2 for digital signals are send

uint16_t CntSampleRemaining = 0;

// Trigger FSM
void TrigHandler(void) {
	switch (Trigger.eTrigStatus) {
		case TRIG_IDLE:
			break;
		case TRIG_ANA_POS:
			getSample();
			if ((V_U_TrigAnalog_old < Trigger.fTrigVal) && (V_U_Analog[Trigger.idxChannel] >= Trigger.fTrigVal)) {
				SignalTriggered();
			} else {
				V_U_TrigAnalog_old = V_U_Analog[Trigger.idxChannel];
			}
			break;
		case TRIG_ANA_NEG:
			getSample();
			if ((V_U_TrigAnalog_old > Trigger.fTrigVal) && (V_U_Analog[Trigger.idxChannel] <= Trigger.fTrigVal)) {
				SignalTriggered();
			} else {
				V_U_TrigAnalog_old = V_U_Analog[Trigger.idxChannel];
			}
			break;
		case TRIG_DIG_POS:
			getSample();
			if ((V_D_TrigDigital_old == 0) && (V_D_Input[Trigger.idxChannel] == 1)) {
				SignalTriggered();
			} else {
				V_D_TrigDigital_old = V_D_Input[Trigger.idxChannel];
			}
			break;
		case TRIG_DIG_NEG:
			getSample();
			if ((V_D_TrigDigital_old == 1) && (V_D_Input[Trigger.idxChannel] == 0)) {
				SignalTriggered();
			} else {
				V_D_TrigDigital_old = V_D_Input[Trigger.idxChannel];
			}
			break;
		case TRIG_ENDREC:
			// Triggerd. get last 50% Samples
			getSample();
			break;
		default:
			break;
	}
}

void SignalTriggered(void) {
	Trigger.eTrigStatus = TRIG_ENDREC;
	if (BuffIdx > (SAMPLES / 2)) {
		StartSendIdx = BuffIdx - SAMPLES / 2;
	} else {
		StartSendIdx = BuffIdx + SAMPLES / 2;
	}
	triggered = 1;
	CntSampleRemaining = SAMPLES / 2; // Trigger fixed at 50%
}

void getSample(void) {
	// Add Channels if used
	if (Trigger.smplCh & 0x01) {
		BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[0]);
		BuffDig_q11[BuffIdx] = V_D_Input[0];
		BuffIdx++;
	}
	if (Trigger.smplCh & 0x02) {
		BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[1]);
		BuffDig_q11[BuffIdx] = V_D_Input[1];
		BuffIdx++;
	}
	if (Trigger.smplCh & 0x04) {
		BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[2]);
		BuffDig_q11[BuffIdx] = V_D_Input[2];
		BuffIdx++;
	}
	if (Trigger.smplCh & 0x08) {
		BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[3]);
		BuffDig_q11[BuffIdx] = V_D_Input[3];
		BuffIdx++;
	}

	if (BuffIdx == SAMPLES) BuffIdx = 0;
	if (triggered) {
		CntSampleRemaining -= Trigger.numSmplCh/2;
	}
	if ((CntSampleRemaining == 0) && triggered) {
		triggered = 0;
		Trigger.eTrigStatus = TRIG_IDLE;
		BuffIdx = 0;
		Datahandler = DATA_SEND;
	}

}
void DataHandler(void) {
	switch (Datahandler) {
		case DATA_IDLE:
			break;
		case DATA_SEND:
			HAL_UART_Transmit_IT(&huart3, &BuffAna_q11[StartSendIdx], (SAMPLES-StartSendIdx)*2);
			sendChunk = 1;
			Datahandler = DATA_IDLE;
			break;
		default:
			break;
	}
}

//Set Trigger from UART info
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	Trigger.eTrigStatus = rxData[0] & 0x0F;
	Trigger.idxChannel = rxData[1] & 0x0F;
	uint16_t tmp = (rxData[2] << 8) + rxData[3];
	Trigger.fTrigVal = decode_q11(tmp);
	Trigger.smplCh = rxData[4];
	Trigger.numSmplCh = rxData[5] & 0x0F;

	// set old values to detect flag
	V_U_TrigAnalog_old = V_U_Analog[Trigger.idxChannel];
	V_D_TrigDigital_old = V_D_Input[Trigger.idxChannel];

	// Rearm interrupt
	HAL_UART_Receive_IT(&huart3, rxData, sizeof(rxData));
}

// necessery to send remaining Buffer Chunks
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	if (sendChunk == 1) {
		HAL_UART_Transmit_IT(&huart3, &BuffAna_q11[0], StartSendIdx*2);
		sendChunk = 2;
	} else if (sendChunk == 2) {
		HAL_UART_Transmit_IT(&huart3, &BuffDig_q11[StartSendIdx], (SAMPLES-StartSendIdx));
		sendChunk = 3;
	} else if (sendChunk == 3) {
		HAL_UART_Transmit_IT(&huart3, &BuffDig_q11[0], StartSendIdx);
		sendChunk = 0;
	}
}

// float to uint16 fixed point with 11 decimal places conversion
uint16_t encode_q11(float Val) {
	return (uint16_t)((Val * 2048) + 0.5);
}

// uint16 fixed point with 11 decimal places to float conversion
float decode_q11(uint16_t Val) {
	return (Val / 2048.0);
}
