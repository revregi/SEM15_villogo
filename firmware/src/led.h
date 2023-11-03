/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file led.h
*
* \brief Soft-PWM LED driver
*
* \author Hekk_Elek
*
**********************************************************************************************************/
#ifndef LED_H
#define LED_H

/***************************************< Includes >**************************************/


/***************************************< Definitions >**************************************/
#define LEDS_NUM               (7u)  //!< Number of LEDs driven by this driver


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
extern DATA U8 gau8LEDBrightness[ LEDS_NUM ];


/***************************************< Public functions >**************************************/
void LED_Init( void );
void LED_Interrupt( void );


#endif /* LED_H */

/***************************************< End of file >**************************************/
