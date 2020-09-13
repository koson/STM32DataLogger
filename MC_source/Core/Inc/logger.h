/*
 * logger.h
 *
 *  Created on: 08.09.2020
 *      Author: philzo
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _LOGGER_H_
#define _LOGGER_H_

/* global macros and defines -------------------------------------------------*/
#define SAMPLES	30000
#define SEND_CHUNK 2

/* typedefs ------------------------------------------------------------------*/
enum eTrig {TRIG_IDLE, TRIG_ANA_POS, TRIG_ANA_NEG, TRIG_DIG_POS, TRIG_DIG_NEG, TRIG_ENDREC};
enum eData {DATA_IDLE, DATA_PREP_SEND, DATA_SEND};

typedef struct {
	enum eTrig eTrigStatus;
	uint8_t idxChannel;			// index of witch channel checks trigger
	float fTrigVal;				// Analog Trigger Value
	uint8_t smplCh;				// which channel will be sampled. first 4 MSB are for digital signal, 4 LSB for analog
	uint8_t numSmplCh;			// number of channels
} sTrig;


/* global variables ----------------------------------------------------------*/
extern uint8_t rxData[6];

extern sTrig Trigger;
extern enum eData Datahandler;

extern float V_U_TrigAnalog_old;
extern uint8_t V_D_TrigDigital_old;

/* global c-function prototypes ----------------------------------------------*/
void getSample(void);
void SignalTriggered(void);

extern void TrigHandler(void);
extern void DataHandler(void) ;
extern uint16_t encode_q11(float Val);
extern float decode_q11(uint16_t Val);

#endif
