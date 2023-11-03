/*! *******************************************************************************************************
* Copyright (c) 2022 Hekk_Elek
*
* \file rgbled.c
*
* \brief RGB LED driver using current-mode pulse control
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/***************************************< Includes >**************************************/
#include "stc8g.h"
#include <string.h>

// Own includes
#include "types.h"
#include "rgbled.h"


/***************************************< Definitions >**************************************/
#define COLOR_LEVELS   (16u)  //!< Number of brightness levels per color
#define PIN_S          (P55)  //!< GPIO pin for "S" LEDs
#define PIN_E          (P54)  //!< GPIO pin for "E" LEDs
#define PIN_1          (P37)  //!< GPIO pin for "1" LEDs
#define PIN_5          (P33)  //!< GPIO pin for "5" LEDs


/***************************************< Types >**************************************/


/***************************************< Constants >**************************************/


/***************************************< Global variables >**************************************/
//! \brief Global array for RGB LED color values
//! \note  Value set is between [0; COLOR_LEVELS)
volatile U8 gau8RGBLEDs[ NUM_RGBLED_COLORS ];


/***************************************< Static function definitions >**************************************/
static void SPulseDelay( void );
static void EPulseDelay( void );
static void OnePulseDelay( void );
static void FivePulseDelay( void );


/***************************************< Private functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for red LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void SPulseDelay( void )
{
  // Wait for 3 usec
	unsigned char i;

	i = 22 + 100;
	while (--i);
}

//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for green LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void EPulseDelay( void )
{
  // Wait for 1 usec
	unsigned char i;

	i = 22 + 100;
	while (--i);
}

//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for blue LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void OnePulseDelay( void )
{
  // Wait for 1 usec
	unsigned char i;

	i = 60;
	while (--i);
}

//----------------------------------------------------------------------------
//! \brief  Delay for generating current pulse for blue LED
//! \param  -
//! \return -
//! \global -
//! \note   Assumes 24 MHz clock!
//-----------------------------------------------------------------------------
static void FivePulseDelay( void )
{
  // Wait for 1 usec
	unsigned char i;

	i = 22 + 100;
	while (--i);
}


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initialize hardware and software layer
//! \param  -
//! \return -
//! \global gau8RGBLEDs
//-----------------------------------------------------------------------------
void RGBLED_Init( void )
{
  memset( gau8RGBLEDs, 0, NUM_RGBLED_COLORS );
  
  // Initialize GPIO pins
	// NOTE: Pin modes are set by PxM0 and PxM1 registers
	// ------------+------+---------------
	// Bit in PxM0 | PxM1 | Pin mode
	//          0  |   0  | Bidirectional
	//          0  |   1  | Input
	//          1  |   0  | Output
	//          1  |   1  | Open-drain
	// ------------+------+---------------
  // Red and green LEDs (P5.4, P5.5)
  PIN_S = 1;
  PIN_E = 1;
  P5M0 |=  (1u<<4u) |  (1u<<5u);  // push-pull
  P5M1 &= ~(1u<<4u) & ~(1u<<5u);
  // Blue LED (P3.3, P3.7)
  PIN_1 = 1;
  PIN_5 = 1;
  P3M0 |=  (1u<<3u) |  (1u<<7u);  // push-pull
  P3M1 &= ~(1u<<3u) & ~(1u<<7u);
}

//----------------------------------------------------------------------------
//! \brief  Interrupt routine for pulse-controlled RGB LED driver
//! \param  -
//! \return -
//! \global gau8RGBLEDs
//! \note   Should be called from periodic timer interrupt routine.
//-----------------------------------------------------------------------------
void RGBLED_Interrupt( void )
{
  static volatile u8Cnt = 0u;
  
  if( gau8RGBLEDs[ 0 ] > u8Cnt )  // Red
  {
    PIN_S = 0;
    SPulseDelay();
    PIN_S = 1;
  }
  if( gau8RGBLEDs[ 1 ] > u8Cnt )  // Green
  {
    PIN_E = 0;
    EPulseDelay();
    PIN_E = 1;
  }
  if( gau8RGBLEDs[ 2 ] > u8Cnt )  // Blue
  {
    PIN_1 = 0;
    OnePulseDelay();
    PIN_1 = 1;
  }
  if( gau8RGBLEDs[ 3 ] > u8Cnt )  // Blue
  {
    PIN_5 = 0;
    FivePulseDelay();
    PIN_5 = 1;
  }
  u8Cnt++;
  if( COLOR_LEVELS <= u8Cnt )
  {
    u8Cnt = 0u;
  }
}


/***************************************< End of file >**************************************/
