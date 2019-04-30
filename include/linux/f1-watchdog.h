/*----------------------------------------------------------------------------

        Written by Dean Matsen
        Copyright (C) 2013 Honeywell
        All Rights Reserved.

----------------------------------------------------------------------------*/

#ifndef _F1_WATCHDOG_H
#define _F1_WATCHDOG_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define	F1WD_IOCTL_BASE	'W'

#pragma pack(1)

struct f1wd_control
{
      /* F1WDF_* flags
       */
  __u32 flags;

      /* Hardware timeout, in milliseconds.  Ignored unless
       * F1WDF_SET_HW_TIMEOUT is set in flags
       */
  __u32 hw_timeout_sec;

      /* Software timeout, in seconds.  Ignored unless F1WDF_SET_SW_TIMEOUT
       * is set in flags
       */
  __u32 sw_timeout_sec;

      /* A simple mod-256 checksup of the contents of this structure
       */
  unsigned char chksum;
};

#pragma pack()

/* Pet the soft watchdog (resets the timer that pets the hardware
 * watchdog)
 *
 * NOTE: The hardware watchdog cannot be directly pet
 */
#define F1WDF_PET_SW_WDOG      0x00000001

/* Set the hardware timeout from the "hw_timeout_ms" variable
 *
 * NOTE: The hardware watchdog cannot be directly pet.  The software timer
 * pets it at a one second rate
 */
#define F1WDF_SET_HW_TIMEOUT   0x00000002

/* Set the software timeout from the "sw_timeout_sec" variable
 */
#define F1WDF_SET_SW_TIMEOUT   0x00000004

/* Reset (via watchdog) ASAP
 */
#define F1WDF_FORCE_RESET      0x00000008

/* Enable one second software timer that pets the hardware watchdog.  It
 * will stop petting the hardware watchdog after the software timeout
 * expires, unless it is pet with the F1WDF_PET_SW_WDOG bit
 */
#define F1WDF_SW_TIMER_ON      0x00000010

/* Disable the one second timer.  The hardware watchdog is guaranteed to
 * time out in this mode, after the specified hardware timeout.  This is
 * useful for a delayed reset.
 *
 * NOTE: The hardware watchdog cannot be directly pet
 */
#define F1WDF_SW_TIMER_OFF     0x00000020


#define F1WDIOC_CONTROL        _IOW(F1WD_IOCTL_BASE, 0, struct f1wd_control)

#ifdef __KERNEL__

long f1_wdt_control ( struct f1wd_control *cmddata );
void f1_wdt_induce_hard_reset ( void );

#endif	/* __KERNEL__ */

#endif  /* _F1_WATCHDOG_H */
