/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "boot.h"
#include "video.h"
#include "MenuInits.h"
#include "lib/cromwell/cromString.h"
#include "lib/cromwell/cromSystem.h"
#include "lib/time/timeManagement.h"
#include "lib/LPCMod/xblastDebug.h"
#include "LEDMenuActions.h"
#include "xblast/settings/xblastSettings.h"
#include <stddef.h>

void AdvancedMenu(void *textmenu)
{
    TextMenu((TEXTMENU*)textmenu, NULL);
}

void DrawChildTextMenu(void* menu)
{
    TEXTMENU* menuPtr = (TEXTMENU*)menu;
    TextMenu(menuPtr, menuPtr->firstMenuItem);
    //freeTextMenuAllocMem(menuPtr);
}

void ResetDrawChildTextMenu(TEXTMENU* menu)
{
    debugSPIPrint(DEBUG_GENERAL_UI, "Drawing menu %s\n", menu->szCaption);
    TextMenu(menu, menu->firstMenuItem);
    debugSPIPrint(DEBUG_GENERAL_UI, "Exiting menu %s\n", menu->szCaption);
    freeTextMenuAllocMem(menu);
    debugSPIPrint(DEBUG_GENERAL_UI, "Returning to previous menu\n");
}

void dynamicDrawChildTextMenu(void* menuInitFct)
{
    TEXTMENU* (*fctPtr)(void) = menuInitFct;
    if(NULL == menuInitFct)
    {
        return;
    }

    TEXTMENU* menu = (*fctPtr)();
    debugSPIPrint(DEBUG_GENERAL_UI, "Generated menu %s\n", menu->szCaption);
    ResetDrawChildTextMenu(menu);
}

void DrawLargeHDDTextMenu(unsigned char drive)
{
    breakOutOfMenu = 1;
    LargeHDDMenuDynamic((void *)&drive);
    //Memory allocation freeing is done in ResetDrawChildTextMenu which is called by LargeHDDMenuInit.
}

void freeTextMenuAllocMem(TEXTMENU* menu)
{
    TEXTMENUITEM* currentItem = menu->firstMenuItem;
    TEXTMENUITEM* nextItem;
    int itemCount = 0;

    if(menu != NULL)
    {
        debugSPIPrint(DEBUG_GENERAL_UI, "freeing menu %s\n", menu->szCaption);
        while(currentItem != NULL)
        {
            nextItem = currentItem->nextMenuItem;
            debugSPIPrint(DEBUG_GENERAL_UI, "free menu item : %s\n", currentItem->szCaption);
            if(currentItem->functionDataPtr != NULL && currentItem->dataPtrAlloc)
            {
                debugSPIPrint(DEBUG_GENERAL_UI, "free alloc param\n");
                free(currentItem->functionDataPtr);
            }
            free(currentItem);
            currentItem = nextItem;
        }

        //Finally free menuPtr since it no longer points to an allocated item entry.
        free(menu);
        menu = NULL;
    }
}

void UiHeader(char *title)
{
    BootVideoClearScreen(&jpegBackdrop, 0, 0xffff);
    VIDEO_ATTR=0xffffef37;
    printk("\n\n\2       %s\2\n\n\n", title);
}

void UIFooter(void)
{
    VIDEO_ATTR=0xffc8c8c8;
    printk("\n\n           Press Button 'B' or 'Back' to return.");
    while(cromwellLoop())
    {
        if(risefall_xpad_BUTTON(TRIGGER_XPAD_KEY_B) == 1 || risefall_xpad_STATE(XPAD_STATE_BACK) == 1)
        {
            break;
        }
    }
    initialSetLED(LPCmodSettings.OSsettings.LEDColor);
}
