/*! *******************************************************************************************************
* Copyright (c) 2021-2022 Hekk_Elek
*
* \file animation.c
*
* \brief Implementation of LED animations
*
* \author Hekk_Elek
*
**********************************************************************************************************/

/*----------------------------------------------------------------------------------------
How it works
============
Animations are implemented on a virtual machine. This machine has opcodes that operate on
the LED brightness state variables. Each animation program runs in a loop. Instructions have
the following format:
  [ LED brightness array -- signed integer ] [ Opcode ] [ Opcode specific operand ]

----------------------------------------------------------------------------------------*/

/***************************************< Includes >**************************************/
// Standard C libraries
#include <string.h>

// Own includes
#include "types.h"
#include "led.h"
#include "rgbled.h"
#include "util.h"
#include "animation.h"
#include "persist.h"


/***************************************< Definitions >**************************************/
#define RIGHT_LEDS_START    (6u)  //!< Index of the first LED on the right side of the board


/***************************************< Types >**************************************/
//! \brief Opcode bits used in animation virtual machine
typedef enum
{
  LOAD      = 0x00u,  //!< Loads the LED brightness array to the PWM driver
  ADD       = 0x01u,  //!< Adds the LED brightness array elements to the current brightness level; if overflows, it sets to zero
  RSHIFT    = 0x02u,  //!< Shifts all the current LED brightness levels clockwise
  LSHIFT    = 0x04u,  //!< Shifts all the current LED brightness levels anticlockwise
//  UMOVE     = 0x04u,  //!< Moves some of the values upwards. Uses saturation logic. Doesn't roll over.
//  DMOVE     = 0x08u,  //!< Moves some of the values downwards. Uses saturation logic. Doesn't roll over.
  DIV       = 0x10u,  //!< Divides the the current LED brightness levels by the given number
  USOURCE   = 0x20u,  //!< Add values to the brightness and if it overflows/underflows then it will be added to the upwards next value. If it overflows/underflows then it will do the same until it reaches the uppper or lower end.
  DSOURCE   = 0x40u,  //!< Add values to the brightness and if it overflows/underflows then it will be added to the downwards next value. If it overflows/underflows then it will do the same until it reaches the uppper or lower end.
  REPEAT    = 0x80u   //!< Do the instruction and repeat by (operand)-times
} E_ANIMATION_OPCODE;

//! \brief Instruction used by the animation state machine -- for normal LEDs
typedef struct
{
  U16 u16TimingMs;                               //!< How long the machine should stay in this state
  U8  au8LEDBrightness[ LEDS_NUM ];              //!< Brightness of each LED
  U8  u8AnimationOpcode;                         //!< Opcode (E_ANIMATION_OPCODE)
  U8  u8AnimationOperand;                        //!< Opcode-specific operand
} S_ANIMATION_INSTRUCTION_NORMAL;

//! \brief Instruction used by the animation state machine -- for the RGB LED
typedef struct
{
  U16 u16TimingMs;                               //!< How long the machine should stay in this state
  U8  au8RGBLEDBrightness[ NUM_RGBLED_COLORS ];  //!< Brightness of each color
  U8  u8AnimationOpcode;                         //!< Opcode (E_ANIMATION_OPCODE)
  U8  u8AnimationOperand;                        //!< Opcode-specific operand
} S_ANIMATION_INSTRUCTION_RGB;

//! \brief Animation structure
typedef struct
{
  U8                                         u8AnimationLengthNormal;  //!< How many instructions this animation has for the normal LEDs
  const S_ANIMATION_INSTRUCTION_NORMAL CODE* psInstructionsNormal;     //!< Pointer to the instructions themselves -- normal LEDs
  U8                                         u8AnimationLengthRGB;     //!< How many instructions this animation has for the RGB LED
  const S_ANIMATION_INSTRUCTION_RGB CODE*    psInstructionsRGB;        //!< Pointer to the instructions themselves -- RGB LED
} S_ANIMATION;


/***************************************< Constants >**************************************/
//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasKITT[ 14u ] = 
{
  {200u, { 0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
  { 100u, { 5,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
  { 100u, {10,  5,  0,  0,  0,  0,  0 }, LOAD,  0u },
  { 100u, {15, 10,  5,  0,  0,  0,  0 }, LOAD,  0u },
  { 100u, {10, 15, 10,  5,  0,  0,  0 }, LOAD,  0u },
  { 100u, { 5, 10, 15, 10,  5,  0,  0 }, LOAD,  0u },
  { 100u, { 0,  5, 10, 15, 10,  5,  0 }, LOAD,  0u },
  { 100u, { 0,  0,  5, 10, 15, 10,  5,}, LOAD,  0u },
	{ 100u, { 0,  0,  5, 10, 10, 15, 10 }, LOAD,  0u },
  { 100u, { 0,  0,  0,  5, 10, 10, 15 }, LOAD,  0u },
  { 100u, { 0,  0,  0,  0,  5, 10, 10 }, LOAD,  0u },
  { 100u, { 0,  0,  0,  0,  0,  5, 10 }, LOAD,  0u },
	{ 100u, { 0,  0,  0,  0,  0,  0,  5 }, LOAD,  0u },
  {200u, { 0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasKITTRGB[ 6u ] = 
{
  { 100u, {  0,  0,  0,  0}, LOAD,         0u },
  { 100u, {  5,  0,  0,  0}, ADD | REPEAT, 2u },
  { 50u, {  0,  5,  0,  0}, ADD | REPEAT, 2u },
  { 50u, {  0,  0,  5,  0}, ADD | REPEAT, 2u },
	{ 50u, {  0,  0,  0,  5}, ADD | REPEAT, 2u },
  {750u, { 15, 15, 15, 15}, LOAD,         0u },
};

//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasAnimation2[ 13u ] = 
{
  {  115u, {  0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
  {  115u, {  3,  3,  3,  3,  3,  3,  3 }, ADD | REPEAT,  4u },
  {  115u, { -3, -3, -3, -3, -3, -3, -3 }, ADD | REPEAT,  4u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasAnimation2RGB[ 6u ] = 
{
  {  115u, {  0,  0,  0,  0}, LOAD,         0u },
  {  115u, {  3,  3,  3,  3}, ADD | REPEAT, 4u },
  {  115u, { -3, -3, -3, -3}, ADD | REPEAT, 4u },
};

//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasAnimation3[ 4u ] = 
{
  {  70u, { 15, 15, 15, 15, 15, 15, 15 }, LOAD,           0u },
  {  70u, { -1, -1, -1, -1, -1, -1, -1 }, ADD | REPEAT,  14u },
  {  70u, {  0,  0,  0,  0,  0,  0,  0 }, LOAD,           0u },
  {  70u, {  1,  1,  1,  1,  1,  1,  1 }, ADD | REPEAT,  14u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasAnimation3RGB[ 4u ] = 
{
  {  70u, { 15, 15,  0,  0}, LOAD,          0u },
  {  70u, { -1, -1,  1,  1}, ADD | REPEAT, 14u },
  {  70u, {  0,  0, 15, 15}, LOAD,          0u },
  {  70u, {  1,  1, -1, -1}, ADD | REPEAT, 14u },
};


//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasAnimation4[ 14u ] = 
{
  {  125u, {  0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
  {  125u, {  0,  0,  3,  0,  0,  0,  0 }, LOAD,  0u },
  {  125u, {  0,  3,  6,  3,  0,  0,  0 }, LOAD,  0u },
  {  125u, {  3,  6,  9,  6,  3,  0,  0 }, LOAD,  0u },
  {  125u, {  6,  9, 12,  9,  6,  3,  0 }, LOAD,  0u },
  {  125u, {  9, 12, 15, 12,  9,  6,  3 }, LOAD,  0u },
  {  125u, { 12, 15, 15, 15, 12,  9,  6 }, LOAD,  0u },
  {  125u, { 15, 15, 12, 15, 15, 12,  9 }, LOAD,  0u },
  {  125u, { 15, 12,  9, 12, 15, 15, 12 }, LOAD,  0u },
  {  125u, { 12,  9,  6,  9, 12, 15, 15 }, LOAD,  0u },
  {  125u, { -3, -3, -3, -3, -3, -3, -3 }, ADD | REPEAT,  1u},
  {  125u, {  3,  0,  0,  0,  3,  6,  9 }, LOAD,  0u },
  {  125u, {  0,  0,  0,  0,  0,  3,  6 }, LOAD,  0u },
  {  125u, {  0,  0,  0,  0,  0,  0,  3 }, LOAD,  0u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasAnimation4RGB[ 10u ] = 
{
  { 250u, {  0,  0,  0,  0}, LOAD,         0u },
  {  125u, {  0,  0,  3,  0}, LOAD,         0u },
  {  125u, {  0,  3,  6,  0}, LOAD,         0u },
  {  125u, {  3,  3,  3,  2}, ADD | REPEAT, 2u },
  {  125u, { 12, 15, 15, 8}, LOAD,         0u },
  {  125u, { 15, 15, 12, 8}, LOAD,         0u },
  {  125u, { 15, 12,  9, 8}, LOAD,         0u },
  {  125u, { -3, -3, -3, -2}, ADD | REPEAT, 2u },
  {  125u, {  3,  0,  0,  0}, LOAD,         0u },
  { 250u, {  0,  0,  0,  0}, LOAD,         0u },
};

//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasAnimation5[ 4u ] = 
{
  {1525u, {  0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
  {  75u, {  3,  3,  3,  3,  3,  3,  3 }, ADD | REPEAT,  4u },
  {  75u, { -3, -3, -3, -3, -3, -3, -3 }, ADD | REPEAT,  4u },
  { 450u, {  0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasAnimation5RGB[ 7u ] = 
{
  {  75u, {  0,  0,  0,  0}, LOAD,         0u },
  {  75u, {  3,  0,  0,  0}, ADD | REPEAT, 4u },
  {  75u, { -3,  0,  0,  0}, ADD | REPEAT, 4u },
  {  75u, {  0,  3,  0,  0}, ADD | REPEAT, 4u },
  {  75u, {  0, -3,  0,  0}, ADD | REPEAT, 4u },
  { 750u, {  0,  0,  0,  0}, LOAD,         0u },
  { 450u, {  0,  0, 15, 15}, LOAD,         0u },
};

//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasAnimation6[ 10u ] = 
{
  {  120u, {  0,  0,  0,  0,  0,  0,  0 }, LOAD,  0u },
  {  120u, {  0,  0,  0,  0,  0,  0,  3 }, LOAD,  0u },
  {  120u, {  0,  0,  0,  0,  0,  3,  6 }, LOAD,  0u },
  {  120u, {  3,  0,  0,  0,  3,  6,  9 }, LOAD,  0u },
  {  120u, {  6,  3,  0,  3,  6,  9, 12 }, LOAD,  0u },
  {  120u, {  9,  6,  3,  6,  9, 12, 15 }, LOAD,  0u },
  {  120u, { 12,  9,  6,  9, 12, 15, 15 }, LOAD,  0u },
  {  120u, { 15, 12,  9, 12, 15, 15, 15 }, LOAD,  0u },
  {  120u, { 15, 15, 12, 15, 15, 15, 15 }, LOAD,  0u },
  { 840u, { 15, 15, 15, 15, 15, 15, 15 }, LOAD,  0u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasAnimation6RGB[ 10u ] = 
{
  {  120u, {  0, 0, 0, 1 }, LOAD,  0u },
  {  120u, {  0, 0, 0, 3 }, LOAD,  0u },
  {  120u, {  0,  0,  0,  3}, LOAD,  0u },
  {  120u, {  0,  0,  3,  6 }, LOAD,  0u },
  {  120u, {  0,  0,  3,  6 }, LOAD,  0u },
  {  120u, {  0,  3,  6,  9}, LOAD,  0u },
  {  120u, { 0,  3,  6,  9}, LOAD,  0u },
  {  120u, { 3, 6,  9, 12}, LOAD,  0u },
  {  120u, { 6, 9, 12, 12}, LOAD,  0u },
  { 840u, { 12, 12, 12, 12}, LOAD,  0u }
};

//--------------------------------------------------------
//! \brief KITT animation -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasAnimation7[ 7u ] = 
{
  {220u, {15, 10,  5,  0,  0,  5, 10 }, LOAD,  0u },
  {220u, {10, 15, 10,  5,  0,  0,  5 }, LOAD,  0u },
  {220u, { 5, 10, 15, 10,  5,  0,  0 }, LOAD,  0u },
  {220u, { 0,  5, 10, 15, 10,  5,  0 }, LOAD,  0u },
  {220u, { 0,  0,  5, 10, 15, 10,  5,}, LOAD,  0u },
	{220u, { 5,  0,  0,  5, 10, 15, 10 }, LOAD,  0u },
  { 110u, {10,  5,  0,  0,  5, 10, 15 }, LOAD,  0u },
};
//! \brief KITT animation -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasAnimation7RGB[ 6u ] = 
{
  { 110u, { 15,  0,  0, 15}, LOAD,         0u },
	{ 110u, {  0,  5,  0, -5}, ADD | REPEAT, 2u },
  { 110u, { -5,  0,  5,  0}, ADD | REPEAT, 2u },
  { 110u, {  0, -5,  0,  5}, ADD | REPEAT, 2u },
  { 110u, {  5,  0, -5,  0}, ADD | REPEAT, 2u },
};

//--------------------------------------------------------
//! \brief All blackness, reached right before going to power down mode -- normal LEDs
CODE const S_ANIMATION_INSTRUCTION_NORMAL gasBlackness[ 1u ] =
{
  {0xFFFFu, { 0,  0,  0,  0,  0,  0,  0}, LOAD, 0u },
};
//! \brief All blackness, reached right before going to power down mode -- RGB LED
CODE const S_ANIMATION_INSTRUCTION_RGB gasBlacknessRGB[ 1u ] =
{
  {0xFFFFu, { 0,  0,  0,  0}, LOAD, 0u },
};

// *******************************************************
//! \brief Table of animations
CODE const S_ANIMATION gasAnimations[ NUM_ANIMATIONS ] = 
{
  {sizeof(gasKITT)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasKITT,             sizeof(gasKITTRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasKITTRGB },

  {sizeof(gasAnimation2)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasAnimation2,             sizeof(gasAnimation2RGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasAnimation2RGB },

  {sizeof(gasAnimation3)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasAnimation3,             sizeof(gasAnimation3RGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasAnimation3RGB },

  {sizeof(gasAnimation4)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasAnimation4,             sizeof(gasAnimation4RGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasAnimation4RGB },

  {sizeof(gasAnimation5)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasAnimation5,             sizeof(gasAnimation5RGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasAnimation5RGB },

  {sizeof(gasAnimation6)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasAnimation6,             sizeof(gasAnimation6RGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasAnimation6RGB },

  {sizeof(gasAnimation7)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),             gasAnimation7,             sizeof(gasAnimation7RGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),             gasAnimation7RGB },

  // Last animation, don't change its location
  {sizeof(gasBlackness)/sizeof(S_ANIMATION_INSTRUCTION_NORMAL),        gasBlackness,        sizeof(gasBlacknessRGB)/sizeof(S_ANIMATION_INSTRUCTION_RGB),        gasBlackness }
};


/***************************************< Global variables >**************************************/
IDATA U16 gu16NormalTimer;                    //!< Ms resolution timer for normal LED animation
IDATA U16 gu16RGBTimer;                       //!< Ms resolution timer for the RGB LED animation
IDATA U16 gu16LastCall;                       //!< The last time the main cycle was called
// Local variables
static IDATA U8 u8LastState = 0xFFu;          //!< Previously executed instruction index for normal LEDs
static IDATA U8 u8RepetitionCounter = 0u;     //!< Instruction repetition counter for normal LEDs
static IDATA U8 u8LastStateRGB = 0xFFu;       //!< Previously executed instruction index for RGB LED
static IDATA U8 u8RepetitionCounterRGB = 0u;  //!< Instruction repetition counter for RGB LED


/***************************************< Static function definitions >**************************************/
static I8 SaturateBrightness( U8* pu8BrightnessVariable );


/***************************************< Private functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Saturates the given brightness variable
//! \param  *pu8BrightnessVariable: pointer to the variable
//! \return How much the variable needed to change
//! \global -
//! \note   Changes the parameter too.
//-----------------------------------------------------------------------------
static I8 SaturateBrightness( U8* pu8BrightnessVariable )
{
  I8 i8Return = 0;
  if( (I8)*pu8BrightnessVariable < 0 )
  {
    i8Return = (I8)*pu8BrightnessVariable;
    *pu8BrightnessVariable = 0u;
  }
  else if( (I8)*pu8BrightnessVariable > 15u )
  {
    i8Return = (I8)*pu8BrightnessVariable - 15;
    *pu8BrightnessVariable = 15u;
  }
  return i8Return;
}


/***************************************< Public functions >**************************************/
//----------------------------------------------------------------------------
//! \brief  Initialize layer
//! \param  -
//! \return -
//! \global All globals in this layer.
//! \note   Should be called in the init block of the firmware.
//-----------------------------------------------------------------------------
void Animation_Init( void )
{
  gu16NormalTimer = 0u;
  gu16RGBTimer = 0u;
  gu16LastCall = Util_GetTimerMs();
}

//----------------------------------------------------------------------------
//! \brief  Check timer and update LED brightnesses based on the animation.
//! \param  -
//! \return -
//! \global -
//! \note   Should be called from main cycle.
//-----------------------------------------------------------------------------
void Animation_Cycle( void )
{
  U8  u8AnimationState;
  U16 u16StateTimer = 0u;
  U16 u16TimeNow = Util_GetTimerMs();
  U8  u8Index, u8InnerIndex;
  U8  u8OpCode;
  U8  u8Temp;
  I8  i8Change;
  
  // Check if time has elapsed since last call
  if( u16TimeNow != gu16LastCall )
  {
    // Increase the synchronized timer with the difference
    DISABLE_IT;
    gu16NormalTimer += ( u16TimeNow - gu16LastCall );
    gu16RGBTimer += ( u16TimeNow - gu16LastCall );
    ENABLE_IT;

    // Make sure not to overindex arrays
    if( gsPersistentData.u8AnimationIndex >= NUM_ANIMATIONS )
    {
      gsPersistentData.u8AnimationIndex = 0u;
    }
    
    // --------------------------------------< For the normal LEDs
    // Calculate the state of the animation
    for( u8AnimationState = 0u; u8AnimationState < gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthNormal; u8AnimationState++ )
    {
      u16StateTimer += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u16TimingMs;
      if( u16StateTimer > gu16NormalTimer )
      {
        break;
      }
    }
    if( u8AnimationState >= gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthNormal )
    {
      // restart animation
      u8AnimationState = 0u;
      DISABLE_IT;
      gu16NormalTimer = 0u;
      gu16RGBTimer = 0u;
      ENABLE_IT;
    }
    if( u8LastState != u8AnimationState )  // next instruction
    {
      u8OpCode = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u8AnimationOpcode;
      // Just a load instruction, nothing more
      if( LOAD == u8OpCode )
      {
        memcpy( gau8LEDBrightness, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness, LEDS_NUM );
        u8LastState = u8AnimationState;
      }
      else  // Other opcodes -- IMPORTANT: the order of operations are fixed!
      {
        // Add operation
        if( ADD & u8OpCode )
        {
          for( u8Index = 0u; u8Index < LEDS_NUM; u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( gau8LEDBrightness[ u8Index ] > 15u )  // overflow/underflow happened
            {
              gau8LEDBrightness[ u8Index ] = 0u;
            }
          }
        }
        // Right shift operation
        if( RSHIFT & u8OpCode )
        {
          u8Temp = gau8LEDBrightness[ LEDS_NUM - 1u ];
          for( u8Index = LEDS_NUM - 1u; u8Index > 0u; u8Index-- )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index - 1u ];
          }
          gau8LEDBrightness[ 0u ] = u8Temp;
        }
        // Left shift operation
        if( LSHIFT & u8OpCode )
        {
          u8Temp = gau8LEDBrightness[ 0u ];
          for( u8Index = 0u; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            gau8LEDBrightness[ u8Index ] = gau8LEDBrightness[ u8Index + 1u ];
          }
          gau8LEDBrightness[ LEDS_NUM - 1u ] = u8Temp;
        }
/*
        // Upward move operation
        if( UMOVE & u8OpCode )
        {
          // Left side
          for( u8Index = 0u; u8Index < (RIGHT_LEDS_START - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] -= i8Change;
            for( u8InnerIndex = u8Index; u8InnerIndex < (RIGHT_LEDS_START - 1u); u8InnerIndex++ )
            {
              i8Change += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
              gau8LEDBrightness[ u8InnerIndex + 1u ] += i8Change;
              i8Change = SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex + 1u ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START - 1u ];
          gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] );
          // Right side
          for( u8Index = LEDS_NUM - 1u; u8Index > RIGHT_LEDS_START; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( (I8)gau8LEDBrightness[ u8Index ] - i8Change < 0u )  // saturation downwards
            {
              gau8LEDBrightness[ u8Index - 1u ] += gau8LEDBrightness[ u8Index ];
            }
            else  // no saturation
            {
              gau8LEDBrightness[ u8Index - 1u ] += i8Change;
            }
            gau8LEDBrightness[ u8Index ] -= i8Change;
            SaturateBrightness( &gau8LEDBrightness[ u8Index ] );
            SaturateBrightness( &gau8LEDBrightness[ u8Index - 1u ] );  // saturate the next LED too
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START ];
          gau8LEDBrightness[ RIGHT_LEDS_START ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START ] );
        }
        // Downward move operation
        if( DMOVE & u8OpCode )
        {
          //TODO: this works for positive move values only!
          // Left side
          for( u8Index = (RIGHT_LEDS_START - 1u); u8Index > 0u ; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( (I8)gau8LEDBrightness[ u8Index ] - i8Change < 0u )  // saturation downwards
            {
              gau8LEDBrightness[ u8Index - 1u ] += gau8LEDBrightness[ u8Index ];
            }
            else  // no saturation
            {
              gau8LEDBrightness[ u8Index - 1u ] += i8Change;
            }
            gau8LEDBrightness[ u8Index ] -= i8Change;
            SaturateBrightness( &gau8LEDBrightness[ u8Index ] );
            SaturateBrightness( &gau8LEDBrightness[ u8Index - 1u ] );  // saturate the next LED too
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ 0u ];
          gau8LEDBrightness[ 0u ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ 0u ] );
          // Right side
          for( u8Index = RIGHT_LEDS_START; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( (I8)gau8LEDBrightness[ u8Index ] - i8Change < 0u )  // saturation downwards
            {
              gau8LEDBrightness[ u8Index + 1u ] += gau8LEDBrightness[ u8Index ];
            }
            else  // no saturation
            {
              gau8LEDBrightness[ u8Index + 1u ] += i8Change;
            }
            gau8LEDBrightness[ u8Index ] -= i8Change;
            SaturateBrightness( &gau8LEDBrightness[ u8Index ] );
            SaturateBrightness( &gau8LEDBrightness[ u8Index + 1u ] );  // saturate the next LED too
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ LEDS_NUM - 1u ];
          gau8LEDBrightness[ LEDS_NUM - 1u ] -= i8Change;
          SaturateBrightness( &gau8LEDBrightness[ LEDS_NUM - 1u ] );
        }
*/
        // Upward source instruction
        if( USOURCE & u8OpCode )
        {
          // Left side
          for( u8Index = 0u; u8Index < (RIGHT_LEDS_START - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = u8Index; u8InnerIndex < (RIGHT_LEDS_START - 1u); u8InnerIndex++ )
            {
              gau8LEDBrightness[ u8InnerIndex + 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START - 1u ];
          gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START - 1u ] );
          // Right side
          for( u8Index = LEDS_NUM - 1u; u8Index > RIGHT_LEDS_START; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = LEDS_NUM - 1u; u8InnerIndex > RIGHT_LEDS_START; u8InnerIndex-- )
            {
              gau8LEDBrightness[ u8InnerIndex - 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ RIGHT_LEDS_START ];
          gau8LEDBrightness[ RIGHT_LEDS_START ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ RIGHT_LEDS_START ] );
        }
        // Downward source instruction
        if( DSOURCE & u8OpCode )
        {
          // Left side
          for( u8Index = (RIGHT_LEDS_START - 1u); u8Index > 0u; u8Index-- )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = u8Index; u8InnerIndex > 0u; u8InnerIndex-- )
            {
              gau8LEDBrightness[ u8InnerIndex - 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ 0u ];
          gau8LEDBrightness[ 0u ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ 0u ] );
          // Right side
          for( u8Index = RIGHT_LEDS_START; u8Index < (LEDS_NUM - 1u); u8Index++ )
          {
            i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            gau8LEDBrightness[ u8Index ] += i8Change;
            for( u8InnerIndex = RIGHT_LEDS_START; u8InnerIndex < (LEDS_NUM - 1u); u8InnerIndex++ )
            {
              gau8LEDBrightness[ u8InnerIndex + 1u ] += SaturateBrightness( &gau8LEDBrightness[ u8InnerIndex ] );
            }
          }
          i8Change = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ LEDS_NUM - 1u ];
          gau8LEDBrightness[ LEDS_NUM - 1u ] += i8Change;
          SaturateBrightness( &gau8LEDBrightness[ LEDS_NUM - 1u ] );
        }
        // Divide instruction
        if( DIV & u8OpCode )
        {
          for( u8Index = 0u; u8Index < LEDS_NUM; u8Index++ )
          {
            u8Temp = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].au8LEDBrightness[ u8Index ];
            if( u8Temp != 0u )
            {
              gau8LEDBrightness[ u8Index ] /= u8Temp;
            }
          }
        }
        // Repeat instruction
        if( REPEAT & u8OpCode )
        {
          // If we're here the first time
          if( 0u == u8RepetitionCounter )
          {
            u8RepetitionCounter = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u8AnimationOperand;
            // Step back in time
            gu16NormalTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u16TimingMs;
          }
          else  // We're already repeating...
          {
            u8RepetitionCounter--;
            if( 0u != u8RepetitionCounter )
            {
              // Step back in time
              gu16NormalTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsNormal[ u8AnimationState ].u16TimingMs;
            }
            else  // No more repeating
            {
             u8LastState = u8AnimationState;
            }
          }
        }
        else  // if there's no repeat opcode
        {
          u8LastState = u8AnimationState;  // save that this operation is finished
        }
      }
    }
    
    // --------------------------------------< For the RGB LED
    // Calculate the state of the animation
    u16StateTimer = 0u;
    for( u8AnimationState = 0u; u8AnimationState < gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthRGB; u8AnimationState++ )
    {
      u16StateTimer += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u16TimingMs;
      if( u16StateTimer > gu16RGBTimer )
      {
        break;
      }
    }
/*
    if( u8AnimationState >= gasAnimations[ gsPersistentData.u8AnimationIndex ].u8AnimationLengthRGB )
    {
      // restart animation
      u8AnimationState = 0u;
      DISABLE_IT;
      gu16SynchronizedTimer = 0;
      ENABLE_IT;
    }
*/
    if( u8LastStateRGB != u8AnimationState )  // next instruction
    {
      u8OpCode = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u8AnimationOpcode;
      // Just a load instruction, nothing more
      if( LOAD == u8OpCode )
      {
        memcpy( gau8RGBLEDs, (void*)gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].au8RGBLEDBrightness, NUM_RGBLED_COLORS );
        u8LastStateRGB = u8AnimationState;
      }
      else  // Other opcodes -- IMPORTANT: the order of operations are fixed!
      {
        // Add operation
        if( ADD & u8OpCode )
        {
          for( u8Index = 0u; u8Index < NUM_RGBLED_COLORS; u8Index++ )
          {
            gau8RGBLEDs[ u8Index ] += gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].au8RGBLEDBrightness[ u8Index ];
            if( gau8RGBLEDs[ u8Index ] > 15u )  // overflow/underflow happened
            {
              gau8RGBLEDs[ u8Index ] = 0u;
            }
          }
        }
        // Right shift operation
        if( RSHIFT & u8OpCode )
        {
          // Not implemented
        }
        // Left shift operation
        if( LSHIFT & u8OpCode )
        {
          // Not implemented
        }
/*
        // Upward move operation
        if( UMOVE & u8OpCode )
        {
          // Not implemented
        }
        // Downward move operation
        if( DMOVE & u8OpCode )
        {
          // Not implemented
        }
*/
        if( USOURCE & u8OpCode )
        {
          // Not implemented
        }
        if( DSOURCE & u8OpCode )
        {
          // Not implemented
        }
        if( DIV & u8OpCode )
        {
          for( u8Index = 0u; u8Index < NUM_RGBLED_COLORS; u8Index++ )
          {
            u8Temp = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].au8RGBLEDBrightness[ u8Index ];
            if( u8Temp != 0u )
            {
              gau8RGBLEDs[ u8Index ] /= u8Temp;
            }
          }
        }
        // Repeat instruction
        if( REPEAT & u8OpCode )
        {
          // If we're here the first time
          if( 0u == u8RepetitionCounterRGB )
          {
            u8RepetitionCounterRGB = gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u8AnimationOperand;
            // Step back in time
            gu16RGBTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u16TimingMs;
          }
          else  // We're already repeating...
          {
            u8RepetitionCounterRGB--;
            if( 0u != u8RepetitionCounterRGB )
            {
              // Step back in time
              gu16RGBTimer -= gasAnimations[ gsPersistentData.u8AnimationIndex ].psInstructionsRGB[ u8AnimationState ].u16TimingMs;
            }
            else  // No more repeating
            {
             u8LastStateRGB = u8AnimationState;
            }
          }
        }
        else  // if there's no repeat opcode
        {
          u8LastStateRGB = u8AnimationState;  // save that this operation is finished
        }
      }
    }    
    // Store the timestamp
    gu16LastCall = u16TimeNow;
  }
}

//----------------------------------------------------------------------------
//! \brief  Set the new animation
//! \param  -
//! \return -
//! \global -
//! \note   Should be called from main cycle only!
//-----------------------------------------------------------------------------
void Animation_Set( U8 u8AnimationIndex )
{
  if( u8AnimationIndex < NUM_ANIMATIONS )
  {
    gsPersistentData.u8AnimationIndex = u8AnimationIndex;
    DISABLE_IT;
    gu16NormalTimer = 0u;
    gu16RGBTimer = 0u;
    ENABLE_IT;
    u8LastState = 0xFFu;
    u8RepetitionCounter = 0u;
    u8LastStateRGB = 0xFFu;
    u8RepetitionCounterRGB = 0u;
  }
}


/***************************************< End of file >**************************************/
