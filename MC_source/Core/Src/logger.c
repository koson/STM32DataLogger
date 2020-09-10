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

sTrig Trigger = {
	.eTrigStatus = TRIG_IDLE,
	.idxChannel = 0,
	.fTrigVal = 0,
	.smplCh = 0,
	.numSmplCh = 0
};

enum eData Datahandler = DATA_IDLE;

uint16_t BuffAna_q11[SAMPLES];
uint8_t BuffDig_q11[SAMPLES];
uint16_t BuffIdx = 0;
uint16_t SendBuffIdx = 0;

void TrigHandler(void) {
	switch (Trigger.eTrigStatus) {
		case TRIG_IDLE:
			break;
		case TRIG_ANA_POS:
			if (V_U_Analog[Trigger.idxChannel] >= Trigger.fTrigVal) {
				Trigger.eTrigStatus = TRIG_REC;
			}
			break;
		case TRIG_ANA_NEG:
			if (V_U_Analog[Trigger.idxChannel] <= Trigger.fTrigVal) {
				Trigger.eTrigStatus = TRIG_REC;
			}
			break;
		case TRIG_REC:
			// Add Analog channels if used
			if ((Trigger.smplCh & 0x01) && (BuffIdx < SAMPLES)) {
				BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[0]);
				BuffIdx++;
			}
			if ((Trigger.smplCh & 0x02) && (BuffIdx < SAMPLES)) {
				BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[1]);
				BuffIdx++;
			}
			if ((Trigger.smplCh & 0x04) && (BuffIdx < SAMPLES)) {
				BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[2]);
				BuffIdx++;
			}
			if ((Trigger.smplCh & 0x08) && (BuffIdx < SAMPLES)) {
				BuffAna_q11[BuffIdx] = encode_q11(V_U_Analog[3]);
				BuffIdx++;
			}
			// Add Digital channels if used

			if (BuffIdx == SAMPLES) {
				BuffIdx = 0;
				Trigger.eTrigStatus = TRIG_IDLE;
				SendBuffIdx = 0;
				Datahandler = DATA_SEND;
			}
		case TRIG_SENDDATA:
			break;
		default:
			break;
	}
}

void DataHandler(void) {
	switch (Datahandler) {
		case DATA_IDLE:
			break;
		case DATA_SEND:
			if (SendBuffIdx < 10 /*SAMPLES*/) {
				uint16_t tmp3[10] = {0x4142,0x4344,0x4546,0x4748,0x4950,0x5152,0x5354,0x5556,0x5758,0x5960};
				//uint8_t *tmp2 = (uint8_t*)&tmp[0];
				HAL_UART_Transmit_IT(&huart3, &BuffAna_q11[0], sizeof(BuffAna_q11));
				SendBuffIdx = 10;
			} else {
				SendBuffIdx = 0;
				Datahandler = DATA_IDLE;
				BuffIdx = 0;
				Trigger.eTrigStatus = TRIG_IDLE;
			}
			break;
		default:
			break;
	}
}

uint16_t encode_q11(float Val) {
	return (uint16_t)((Val * 2048) + 0.5);
}

float decode_q11(uint16_t Val) {
	return (Val / 2048.0);
}
