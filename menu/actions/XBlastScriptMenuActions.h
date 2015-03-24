#ifndef _RUNSCRIPTMENUACTIONS_H_
#define _RUNSCRIPTMENUACTIONS_H_
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

void loadRunScript(void *fname);

void saveScriptToFlash(void *fname);
void loadScriptFromFlash(void * ignored);
void toggleRunBootScript(void * itemStr);
void toggleRunBankScript(void * itemStr);

#endif
