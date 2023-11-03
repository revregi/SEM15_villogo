/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file led.c
*
* \brief Soft-PWM LED driver
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/***************************************< Includes >**************************************/
// Own includes
#include "stc8g.h"
#include "types.h"
#include "led.h"


/***************************************< Definitions >**************************************/
#define PWM_LEVELS      (16u)  //!< PWM levels implemented: [0; PWM_LEVELS)

// Pin definitions
#define LED0            (P17)  //!< Pin of LED0
#define LED1            (P16)  //!< Pin of LED1
#define LED2            (P11)  //!< Pin of LED2
#define LED3            (P10)  //!< Pin of LED3
#define LED4            (P35)  //!< Pin of LED4
#define LED5            (P34)  //!< Pin of LED5
#define LED6            (P32)  //!< Pin of LED6


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
DATA U8 gau8LEDBrightness[ LEDS_NUM ];  //!< Array for storing individual brightness levels
DATA U8 gu8PWMCounter;                  //!< Counter for the base of soft-PWM
DATA BIT gbitSide;                      //!< Stores which side of the panel is active


/***************************************< Static function definitions >**************************************/


/***************************************< Private functions >**************************************/


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initialize all IO pins associated with LEDs
//! \param  -
//! \return -
//! \global gau8LEDBrightness[], gu8PWMCounter
//! \note   Should be called in the init block
//-----------------------------------------------------------------------------
void LED_Init( void )
{
  U8 u8Index;
  
  // Init globals
  gu8PWMCounter = 0;
  for( u8Index = 0; u8Index < LEDS_NUM; u8Index++ )
  {
    gau8LEDBrightness[ u8Index ] = 0;
  }
  
  // Starting with MPX1
  gbitSide = 0;
  
	// NOTE: Pin modes are set by PxM0 and PxM1 registers
	// ------------+------+---------------
	// Bit in PxM0 | PxM1 | Pin mode
	//          0  |   0  | Bidirectional
	//          0  |   1  | Input
	//          1  |   0  | Output
	//          1  |   1  | Open-drain
	// ------------+------+---------------
  // P3.2, P3.4, P3.5
  P3M0 |= (1u<<2u) | (1u<<4u) | (1u<<5u);  // push-pull
  P3M1 &= ~(1u<<2u) & ~(1u<<4u) & ~(1u<<5u);
  // P1.0, P1.1, P1.6, P1.7
  P1M0 |= (1u<<0u) | (1u<<1u) | (1u<<6u) | 1u<<7u;  // push-pull
  P1M1 &= ~(1u<<0u) & ~(1u<<1u) & ~(1u<<6u) & ~(1u<<7u);
}

//----------------------------------------------------------------------------
//! \brief  Interrupt routine to implement soft-PWM
//! \param  -
//! \return -
//! \global gau8LEDBrightness[], gu8PWMCounter
//! \note   Should be called from periodic timer interrupt routine.
//-----------------------------------------------------------------------------
void LED_Interrupt( void )
{
  static BIT bDrive = 0;
  static U8   u8DriveCounter = 0u;
  
  u8DriveCounter++;
  if( u8DriveCounter == 5u )
  {
    bDrive = 1;
    u8DriveCounter = 0u;
  }
  else
  {
    bDrive = 0;
  }
  
  if( ( 1 == bDrive ) )
  {
    gu8PWMCounter++;
    if( gu8PWMCounter == PWM_LEVELS )
    {
      gu8PWMCounter = 0;
    }
  }
  //NOTE: unfortunately SFRs cannot be put in an array, so this cannot be implented as a for cycle
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 0u ] > gu8PWMCounter )  // D??
  {
    LED0 = 0;
  }
  else
  {
    LED0 = 1;
  }
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 1u ] > gu8PWMCounter )  // D??
  {
    LED1 = 0;
  }
  else
  {
    LED1 = 1;
  }
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 2u ] > gu8PWMCounter )  // D??
  {
    LED2 = 0;
  }
  else
  {
    LED2 = 1;
  }
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 3u ] > gu8PWMCounter )  // D??
  {
    LED3 = 0;
  }
  else
  {
    LED3 = 1;
  }
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 4u ] > gu8PWMCounter )  // D??
  {
    LED4 = 0;
  }
  else
  {
    LED4 = 1;
  }
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 5u ] > gu8PWMCounter )  // D??
  {
    LED5 = 0;
  }
  else
  {
    LED5 = 1;
  }
  if( ( 1 == bDrive ) && gau8LEDBrightness[ 6u ] > gu8PWMCounter )  // D??
  {
    LED6 = 0;
  }
  else
  {
    LED6 = 1;
  }
}


/***************************************< End of file >**************************************/
