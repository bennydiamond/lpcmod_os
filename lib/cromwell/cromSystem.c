/*
 * system.c
 *
 *  Created on: Jan 30, 2017
 *      Author: cromwelldev
 */

#include "FlashUi.h"
#include "FlashDriver.h"
#include "lib/time/timeManagement.h"
#include "WebServerOps.h"

unsigned char cromwellLoop(void)
{
    if(currentNetworkState != NetworkState_Idle)
    {
        run_lwip();
    }

    //debugSPIPrint loop
    //LCD loop

    return 1;
}
