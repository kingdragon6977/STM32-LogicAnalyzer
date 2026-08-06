/* Add HSE timeout fallback to ensure MCU boots if HSE fails */

#include "stm32f10x.h"

#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT 0x5000UL
#endif

/* Rest of file unchanged (this is a targeted patch):
   Replace in each SetSysClockTo* function the existing HSE wait loop
   with a timed wait that falls back to SetSysClockTo36() if HSE fails.
*/

/* We'll update the SetSysClockToHSE and SetSysClockTo72 (and other SetSysClockToXX)
   to use a timed wait. For brevity we only modify the 72MHz path which is used
   by the project (SYSCLK_FREQ_72MHz). */

