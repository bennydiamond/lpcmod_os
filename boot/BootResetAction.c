/*
 * Sequences the necessary post-reset actions from as soon as we are able to run C
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************
 */

#include "boot.h"
#include "BootEEPROM.h"
#include "BootFlash.h"
#include "BootFATX.h"
#include "lib/LPCMod/BootLPCMod.h"
#include "xbox.h"
#include "cpu.h"
#include "config.h"
#include "video.h"
#include "memory_layout.h"
#include "lpcmod_v1.h"
#include "lib/LPCMod/BootLPCMod.h"
#include "lib/scriptEngine/xblastScriptEngine.h"

const char *xbox_mb_rev[8] = {
        "DevKit",
        "DebugKit",
        "1.0",
        "1.1",
        "1.2/1.3",
        "1.4/1.5",
        "1.6/1.6b",
        "Unknown"
    };

JPEG jpegBackdrop;

int nTempCursorMbrX, nTempCursorMbrY;

extern volatile int nInteruptable;

volatile CURRENT_VIDEO_MODE_DETAILS vmode;
//extern KNOWN_FLASH_TYPE aknownflashtypesDefault[];

void ClearScreen (void) {
    BootVideoClearScreen(&jpegBackdrop, 0, 0xffff);
}

void printMainMenuHeader(OBJECT_FLASH *of, char *modName, bool fHasHardware, u32 cpuSpeed){
    //Length of array is set depending on how many revision can be uniquely identified.
    //Modify this enum if you modify the "XBOX_REVISION" enum in boot.h

    ClearScreen();

    VIDEO_CURSOR_POSX=(vmode.xmargin/*+64*/)*4;
    VIDEO_CURSOR_POSY=vmode.ymargin;

    printk("\n\n");
    if (cromwell_config==XROMWELL) {
        printk("           \2"PROG_NAME" (XBE) v" VERSION "\n\n\2");
    } else if (cromwell_config==CROMWELL) {
        printk("           \2"PROG_NAME" (ROM) v" VERSION "\n\n\2");
    }

    VIDEO_ATTR=0xff00ff00;

    VIDEO_CURSOR_POSX=(vmode.xmargin/*+64*/)*4;
    VIDEO_CURSOR_POSY=vmode.ymargin+64;


    VIDEO_ATTR=0xff00ff00;
#if DEV_FEATURES
    printk("           Modchip: %s    DEBUG_fHasHardware: 0x%02x\n",modName, fHasHardware);
    VIDEO_ATTR=0xffc8c8c8;
    printk("           THIS IS A WIP BUILD, flash manID= %x  devID= %x\n", of->m_bManufacturerId, of->m_bDeviceId);
#else
    printk("           Modchip: %s\n",modName);
#endif
    VIDEO_ATTR=0xff00ff00;


   printk("           Xbox revision: %s ", xbox_mb_rev[mbVersion]);
   if (xbox_ram > 64) {
        VIDEO_ATTR=0xff00ff00;
   } else {
        VIDEO_ATTR=0xffffa20f;
   }
   printk("  CPU: %uMHz   RAM: %dMiB\n", cpuSpeed, xbox_ram);

    VIDEO_CURSOR_POSX=(vmode.xmargin/*+64*/)*4;
#ifndef SILENT_MODE
    // capture title area
    VIDEO_ATTR=0xffc8c8c8;
    printk("           Encoder: ");
    VIDEO_ATTR=0xffc8c800;
    printk("%s  ", VideoEncoderName());
    VIDEO_ATTR=0xffc8c8c8;
    printk("Cable: ");
    VIDEO_ATTR=0xffc8c800;
    printk("%s  ", AvCableName());

    if (I2CGetTemperature(&n, &nx)) {
        VIDEO_ATTR=0xffc8c8c8;
        printk("CPU Temp: ");
        VIDEO_ATTR=0xffc8c800;
        printk("%doC  ", n);
        VIDEO_ATTR=0xffc8c8c8;
        printk("M/b Temp: ");
        VIDEO_ATTR=0xffc8c800;
        printk("%doC  ", nx);
    }

    printk("\n");
    nTempCursorX=VIDEO_CURSOR_POSX;
    nTempCursorY=VIDEO_CURSOR_POSY;
#endif

    VIDEO_ATTR=0xffffffff;

}

//////////////////////////////////////////////////////////////////////
//
//  BootResetAction()

extern void BootResetAction ( void ) {
    bool fMbrPresent=false;
    bool fFirstBoot=false;                    //Flag to indicate first boot since flash update
    int nTempCursorX, nTempCursorY;
    int n, nx, i, returnValue = 255;
    char modName[30] = "Unsupported modchip!";
    u8 * bootScriptBuffer;
    u8 tempFanSpeed = 20;
    int bootScriptSize = -1, res, dcluster;
    u32 cpuSpeed;
    _LPCmodSettings *tempLPCmodSettings;
    OBJECT_FLASH of;
    FATXPartition *partition;
    FATXFILEINFO fileinfo;
    // A bit hacky, but easier to maintain.
    const KNOWN_FLASH_TYPE aknownflashtypesDefault[] = {
        #include "flashtypes.h"
    };

    u8 EjectButtonPressed=0;

#ifdef SPITRACE
    //Required to populate GenPurposeIOs before toggling GPIOs.
    LPCMod_WriteIO(0x4, 0x4); // /CS to '1'
#endif

    debugSPIPrint("XBlast OS is starting.");

    A19controlModBoot=BNKFULLTSOP;        //Start assuming no control over A19 line.

    //Set to NULL as it's not used yet.
    //gobalGenericPtr = NULL;
    scriptSavingPtr = NULL;

    of.m_pbMemoryMappedStartAddress=(u8 *)LPCFlashadress;
    
    xF70ELPCRegister = 0x03;       //Assume no control over the banks but we are booting from bank3
    x00FFLPCRegister = ReadFromIO(XODUS_CONTROL);       //Read A15 and D0 states.
                                                        //Should return 0x04 on normal boot, 0x08 on TSOP recovery.

    TSOPRecoveryMode = 0;
    //TSOPRecoveryMode = (x00FFLPCRegister & 0x08) >> 3;  //If we booted and A15 was already set.
                                                        //It means we are in TSOP recovery. Set to 1.
                                                        //We'll check later if TSOP flash is accessible.


    fHasHardware = 0;

#ifndef SPITRACE        //Do not reset GenPurposeIOs values as they've been updated when "LPCMod_WriteIO(0x4, 0x4)" function was called.
    GenPurposeIOs.GPO3 = 0;
    GenPurposeIOs.GPO2 = 0;
    GenPurposeIOs.GPO1 = 0;
    GenPurposeIOs.GPO0 = 0;
    GenPurposeIOs.GPI1 = 0;
    GenPurposeIOs.GPI0 = 0;
    GenPurposeIOs.A19BufEn = 0;
    GenPurposeIOs.EN_5V = 0;
#endif

    memcpy(&cromwell_config,(void*)(0x03A00000+0x20),4);
    memcpy(&cromwell_retryload,(void*)(0x03A00000+0x24),4);
    memcpy(&cromwell_loadbank,(void*)(0x03A00000+0x28),4);
    memcpy(&cromwell_Biostype,(void*)(0x03A00000+0x2C),4);

    VIDEO_CURSOR_POSX=40;
    VIDEO_CURSOR_POSY=140;
        
    VIDEO_AV_MODE = 0xff;
    nInteruptable = 0;

    // prep our BIOS console print state
    VIDEO_ATTR=0xffffffff;

    // init malloc() and free() structures
    MemoryManagementInitialization((void *)MEMORYMANAGERSTART, MEMORYMANAGERSIZE);

    BootInterruptsWriteIdt();

    // initialize the PCI devices
    //bprintf("BOOT: starting PCI init\n\r");
    BootPciPeripheralInitialization();
    // Reset the AGP bus and start with good condition
    BootAGPBUSInitialization();
    EjectButtonPressed = I2CTransmitByteGetReturn(0x10, 0x03) & 0x01;
    I2CTransmitByteGetReturn(0x10, 0x11);       // dummy Query IRQ
    I2CWriteBytetoRegister(0x10, 0x03,0x00);	// Clear Tray Register
    I2CTransmitWord(0x10, 0x0c01); // close DVD tray
    
    
    I2CTransmitWord(0x10, 0x1901); // no reset on eject
    
    if(cromwell_config==CROMWELL)
        LEDRed();        //Signal the user to press Eject button to avoid Quickboot.
//    if(cromwell_config==CROMWELL){              //Only check if booted from ROM.
    fHasHardware = LPCMod_HW_rev();         //Will output 0xff if no supported modchip detected.
    debugSPIPrint("Modchip hardware ID is: 0x%04X", fHasHardware);
//    }

    if(fHasHardware == SYSCON_ID_V1){
        debugSPIPrint("XBlast Lite V1 detected on LPC bus.");
        sprintf(modName,"%s", "XBlast Lite V1");
        //Check which flash chip is detected by system.
        BootFlashGetDescriptor(&of, (KNOWN_FLASH_TYPE *)&aknownflashtypesDefault[0]);
        if(of.m_bManufacturerId == 0xbf && of.m_bDeviceId == 0x5b){     //If we detected a SST49LF080A
            debugSPIPrint("XBlast Lite V1 flash chip detected. We booted from LPC indeed.");
            //Make sure we'll be reading from OS Bank
            switchOSBank(BNKOS);
        }
        else {  //SST49LF080A flash chip was NOT detected.
            debugSPIPrint("XBlast Lite V1 flash chip NOT detected. Assuming we booted from TSOP");
            fHasHardware = SYSCON_ID_V1_TSOP;
            WriteToIO(XODUS_CONTROL, RELEASED0); //Make sure D0/A15 is not grounded.
        }
        LPCMod_ReadIO(NULL);
        debugSPIPrint("Read XBlast Lite V1 IO status.");
    }
    else if(fHasHardware == SYSCON_ID_XT){
       debugSPIPrint("Aladdin XBlast detected on LPC bus.");
       sprintf(modName,"%s", "Aladdin XBlast");
       //Check which flash chip is detected by system.
       BootFlashGetDescriptor(&of, (KNOWN_FLASH_TYPE *)&aknownflashtypesDefault[0]);
       if(of.m_bManufacturerId == 0xbf && of.m_bDeviceId == 0x5b){     //If we detected a SST49LF080A
           debugSPIPrint("Aladdin XBlast flash chip detected. We booted from LPC indeed.");
           //Make sure we'll be reading from OS Bank
           switchOSBank(BNKOS);
       }
       else {  //SST49LF080A flash chip was NOT detected.
           debugSPIPrint("Aladdin XBlast flash chip NOT detected. Assuming we booted from TSOP");
           fHasHardware = SYSCON_ID_XT_TSOP;
           WriteToIO(XODUS_CONTROL, RELEASED0); //Make sure D0/A15 is not grounded.
       }
    }
    else {
        debugSPIPrint("No XBlast OS compatible hardware found.");
        u32 x3probe = I2CTransmitByteGetReturn(0x51, 0x0);  //Xecuter 3 will send out 0xff
        debugSPIPrint("Probing for X3 EEprom. Result: 0x%08X", x3probe);
        if(x3probe != 0xff && x3probe != 0x80000002){       //Another (hacky) way to detect is to probe SMBus at addresses
            fHasHardware = SYSCON_ID_X3;                    //normally unused by the Xbox. By my own experimentation, address
            debugSPIPrint("Assuming X3 chip detected.");    //0x51 isn't used when X3 is NOT plugged. Then probing the SMBus
        }                                                   //offset 0 of address 0x51 will return either 0xff or 0x80000002.
                                                            //Any other value will be assumed coming from the (encrypted?)
                                                            //X3 eeprom and thus instructing the program that a X3 is detected.
                                                            //More tests will be needed to verify and confirm this theory.
                                                            //Tests have been done on NTSC-U 1.0 and 1.6(a) Xboxes so far.

        if(fHasHardware == SYSCON_ID_XX1 || fHasHardware == SYSCON_ID_XX2)
            sprintf(modName,"%s", "SmartXX V1/V2");
        else if(fHasHardware == SYSCON_ID_XXOPX)
            sprintf(modName,"%s", "SmartXX LT OPX");
        else if(fHasHardware == SYSCON_ID_XX3)
            sprintf(modName,"%s", "SmartXX V3");
        else if(fHasHardware == SYSCON_ID_X3)
            sprintf(modName,"%s", "Xecuter 3(CE)");
        else
            fHasHardware = 0;               //Unknown device, set to 0 to indicate no known hardware.

        currentFlashBank = BNKOS;           //Make sure the system knows we're on the right bank.
        TSOPRecoveryMode = 0;               //Whatever happens, it's not possible to recover TSOP on other modchips.
    }

    LPCmodSettings.OSsettings.migrateSetttings = 0xFF; //Will never be 0xFF
    //Retrieve XBlast OS settings from flash. Function checks if valid device can be read from.
    BootFlashGetOSSettings(&LPCmodSettings);
    //Save a copy of fresly loaded settings from flash.
    //This will be useful to detect settings changes
    memcpy(&LPCmodSettingsOrigFromFlash, &LPCmodSettings, sizeof(_LPCmodSettings));
    debugSPIPrint("Read persistent OS settings from flash.");


    if(LPCmodSettings.OSsettings.migrateSetttings == 0xFF ||
       LPCmodSettings.OSsettings.activeBank > 0x87 ||
       LPCmodSettings.OSsettings.altBank > 0x87 ||
       LPCmodSettings.OSsettings.Quickboot == 0xFF ||
       LPCmodSettings.OSsettings.selectedMenuItem == 0xFF ||
       LPCmodSettings.OSsettings.fanSpeed > 100 ||
       LPCmodSettings.OSsettings.fanSpeed < 10 ||
       LPCmodSettings.OSsettings.bootTimeout > 240 ||
       LPCmodSettings.OSsettings.LEDColor > 4 || 
       LPCmodSettings.OSsettings.TSOPcontrol == 0xFF ||
       LPCmodSettings.OSsettings.TSOPhide == 0xFF ||
       LPCmodSettings.OSsettings.enableNetwork == 0xFF ||
       LPCmodSettings.OSsettings.useDHCP == 0xFF ||
       LPCmodSettings.LCDsettings.migrateLCD == 0xFF ||
       LPCmodSettings.LCDsettings.enable5V == 0xFF ||
       LPCmodSettings.LCDsettings.lcdType > 1 ||        //Because HDD44780 = 0 and KS0073 = 1. No other valid value possible.
       LPCmodSettings.LCDsettings.nbLines > 8 ||
       LPCmodSettings.LCDsettings.lineLength > 40 ||
       LPCmodSettings.LCDsettings.backlight > 100 ||
       LPCmodSettings.LCDsettings.contrast > 100 ||
       LPCmodSettings.LCDsettings.displayMsgBoot == 0xFF ||
       LPCmodSettings.LCDsettings.customTextBoot == 0xFF ||
       LPCmodSettings.LCDsettings.displayBIOSNameBoot == 0xFF ||
       (LPCmodSettings.firstScript.ScripMagicNumber&0xFFF0) != 0xFAF0){
            debugSPIPrint("No persistent OS settings found on flash. Creating default settings.");
            fFirstBoot = true;
            initialLPCModOSBoot(&LPCmodSettings);                //No settings for LPCMod were present in flash.
            //OS sometimes lock on after a fresh flash. Disabling to see if that's causing it.(probably)
            //BootFlashSaveOSSettings();        //Put some initial values in there.
            LEDFirstBoot(NULL);
            LPCmodSettings.OSsettings.bootTimeout = 0;        //No countdown since it's the first boot since a flash update.
                                                            //Configure your device first.
    }

    //Let's set that up right here.
    setCFGFileTransferPtr(&LPCmodSettings, &settingsPtrStruct);

    if(cromwell_config==XROMWELL && fHasHardware != SYSCON_ID_V1 && fHasHardware != SYSCON_ID_XT){	//If coming from XBE and no XBlast Mod is detected
    	tempFanSpeed = I2CGetFanSpeed();
    	if(tempFanSpeed < 10)
    	    tempFanSpeed = 10;
    	else if(tempFanSpeed > 100)
    	    tempFanSpeed = 100;
    	    
    	LPCmodSettings.OSsettings.fanSpeed = tempFanSpeed;		//Get previously set fan speed
    }
    else
    	I2CSetFanSpeed(LPCmodSettings.OSsettings.fanSpeed);		//Else we're booting in ROM mode and have a fan speed to set.
    debugSPIPrint("Fan speed adjustment if needed.");

    if(fHasHardware == SYSCON_ID_V1_TSOP){
    	//LPCmodSettings.OSsettings.TSOPcontrol = (ReadFromIO(XODUS_CONTROL) & 0x20) >> 5;     //A19ctrl maps to bit5
        LPCmodSettings.OSsettings.TSOPcontrol = (u8)GenPurposeIOs.A19BufEn;
        debugSPIPrint("Buffer enable for A19 control : %sabled.", GenPurposeIOs.A19BufEn? "En" : "Dis");
    }

    BootLCDInit();    //Basic init. Do it even if no LCD is connected on the system.
    debugSPIPrint("BootLCDInit done.");
    
    //Stuff to do right after loading persistent settings from flash.
    if(!fFirstBoot){
        if(fHasHardware == SYSCON_ID_V1 ||
           fHasHardware == SYSCON_ID_V1_TSOP ||
           fHasHardware == SYSCON_ID_XX1 ||
           fHasHardware == SYSCON_ID_XX2 ||
           fHasHardware == SYSCON_ID_XXOPX ||
           fHasHardware == SYSCON_ID_XX3 ||
           fHasHardware == SYSCON_ID_X3){
            debugSPIPrint("Check if we need to drive the LCD.");
            assertInitLCD();                            //Function in charge of checking if a init of LCD is needed.
            debugSPIPrint("assertInitLCD done.");
        }
        //further init here.
    }


    // We disable The CPU Cache
    cache_disable();
    // We Update the Microcode of the CPU
    display_cpuid_update_microcode();
    // We Enable The CPU Cache
    cache_enable();
    //setup_ioapic();
    // We look how much memory we have ..
    BootDetectMemorySize();
    debugSPIPrint("Detected RAM size : %uMB.", xbox_ram);

    BootEepromReadEntireEEPROM();
    debugSPIPrint("EEprom read.");
        
    I2CTransmitWord(0x10, 0x1a01); // unknown, done immediately after reading out eeprom data
    I2CTransmitWord(0x10, 0x1b04); // unknown
        
    /* Here, the interrupts are Switched on now */
    BootPciInterruptEnable();
    /* We allow interrupts */
    nInteruptable = 1;
    

#ifndef SILENT_MODE
    printk("           BOOT: start USB init\n");
#endif

    cpuSpeed = getCPUFreq();
    
    BootStartUSB();
    debugSPIPrint("USB init done.");

    mbVersion = I2CGetXboxMBRev();
    debugSPIPrint("Xbox motherboad rev: %s.", xbox_mb_rev[mbVersion]);
    //Load up some more custom settings right before booting to OS.
    if(!fFirstBoot){
        if(LPCmodSettings.OSsettings.runBootScript && cromwell_config==CROMWELL){
            debugSPIPrint("Running boot script.");
            bootScriptSize = fetchBootScriptFromFlash(&bootScriptBuffer);
            if(bootScriptSize > 0){
                i = BNKOS;
                runScript(bootScriptBuffer, bootScriptSize, 1, &i);
                free(bootScriptBuffer);
            }
            debugSPIPrint("Boot script execution done.");
        }
        if((fHasHardware == SYSCON_ID_V1 || fHasHardware == SYSCON_ID_XT) && cromwell_config==CROMWELL){       //Quickboot only if on the right hardware.
            debugSPIPrint("Check any Quickboot or EjectButton boot rule.");
            if(EjectButtonPressed && LPCmodSettings.OSsettings.altBank != BNKOS){              //Xbox was started from eject button and eject button quick boot is enabled.
                debugSPIPrint("Eject button press boot detected.");
                if(LPCmodSettings.OSsettings.altBank > BOOTFROMTSOP){
                    debugSPIPrint("Booting XBlast flash bank");
                    switchBootBank(LPCmodSettings.OSsettings.altBank);
              	}
                else{
                    debugSPIPrint("Booting TSOP flash bank");
                    //WriteToIO(XODUS_CONTROL, RELEASED0);    //Release D0
                    //If booting from TSOP, use of the XODUS_CONTROL register is fine.
                    if(mbVersion == REV1_6 || mbVersion == REVUNKNOWN)
                        switchBootBank(KILL_MOD);    // switch to original bios. Mute modchip.
                    else{
                        switchBootBank(LPCmodSettings.OSsettings.altBank);    // switch to original bios but modchip listen to LPC commands.
                                                                                                        // Lock flash bank control with OSBNKCTRLBIT.
                    }
                }
                I2CTransmitWord(0x10, 0x1b00 + ( I2CTransmitByteGetReturn(0x10, 0x1b) & 0xfb )); // clear noani-bit
                BootStopUSB();
                I2CRebootQuick();
                while(1);	//Hang there.
            }
            EjectButtonPressed = I2CTransmitByteGetReturn(0x10, 0x03) & 0x01;
            I2CTransmitByteGetReturn(0x10, 0x11);       // dummy Query IRQ
            I2CWriteBytetoRegister(0x10, 0x03,0x00);	// Clear Tray Register
            I2CTransmitWord(0x10, 0x0c01); // close DVD tray
            if(!EjectButtonPressed && LPCmodSettings.OSsettings.Quickboot){       //Eject button NOT pressed and Quickboot ON.
                debugSPIPrint("Going to Quickboot.");
                if(LPCmodSettings.OSsettings.activeBank > BOOTFROMTSOP){
                    debugSPIPrint("Booting XBlast flash bank");
                    switchBootBank(LPCmodSettings.OSsettings.activeBank);
              	}
                else{
                    debugSPIPrint("Booting TSOP flash bank");
                    //If booting from TSOP, use of the XODUS_CONTROL register is fine.
                    if(mbVersion == REV1_6 || mbVersion == REVUNKNOWN)
                        switchBootBank(KILL_MOD);    // switch to original bios. Mute modchip.
                    else{
                        switchBootBank(LPCmodSettings.OSsettings.activeBank);    // switch to original bios but modchip listen to LPC commands.
																			     // Lock flash bank control with OSBNKCTRLBIT.
                    }
                }
                I2CTransmitWord(0x10, 0x1b00 + ( I2CTransmitByteGetReturn(0x10, 0x1b) & 0xfb )); // clear noani-bit
                BootStopUSB();
                I2CRebootQuick();
                while(1);
            }
        }
        debugSPIPrint("No Quickboot or EjectButton boot this time.");
        initialSetLED(LPCmodSettings.OSsettings.LEDColor);
    }
    else{
        debugSPIPrint("First boot so no script or bank loading before going to OS at least once.");
    }
    // Load and Init the Background image
    // clear the Video Ram
    memset((void *)FB_START,0x00,0x400000);

    BootVgaInitializationKernelNG((CURRENT_VIDEO_MODE_DETAILS *)&vmode);
    jpegBackdrop.pData =NULL;
    jpegBackdrop.pBackdrop = NULL;
    if(BootVideoInitJPEGBackdropBuffer(&jpegBackdrop))
    { // decode and malloc backdrop bitmap
        extern int _start_backdrop;
        extern int _end_backdrop;
        BootVideoJpegUnpackAsRgb(
            (u8 *)&_start_backdrop,
             &jpegBackdrop,
	    _end_backdrop - _start_backdrop
        );
    }
    // paint the backdrop
    printMainMenuHeader(&of, modName, fHasHardware, cpuSpeed);

    // set Ethernet MAC address from EEPROM
    {
        volatile u8 * pb=(u8 *)0xfef000a8;  // Ethernet MMIO base + MAC register offset (<--thanks to Anders Gustafsson)
        int n;
        for(n=5;n>=0;n--) { *pb++=    eeprom.MACAddress[n]; } // send it in backwards, its reversed by the driver
    }
#ifndef SILENT_MODE
    BootEepromPrintInfo();
#endif

    // init the IDE devices
#ifndef SILENT_MODE
    VIDEO_ATTR=0xffc8c8c8;
    printk("           Initializing IDE Controller\n");
#endif
//    BootIdeWaitNotBusy(0x1f0);
//    wait_ms(100);
#ifndef SILENT_MODE
    printk("           Ready\n");
#endif


    debugSPIPrint("Starting IDE init.");
    BootIdeInit();
    debugSPIPrint("IDE init done.");


    //Load settings from xblast.cfg file if no settings were detected.
    //But first do we have a HDD on Master?
    if(tsaHarddiskInfo[0].m_fDriveExists && !tsaHarddiskInfo[0].m_fAtapi){
        debugSPIPrint("Master HDD exist.");
        //TODO: Load optional JPEG backdrop from HDD here. Maybe fetch skin name from cfg file?
        debugSPIPrint("Trying to load new JPEG from HDD.");
        if(!LPCMod_ReadJPGFromHDD("\\XBlast\\icons.jpg"))
            debugSPIPrint("\"ìcons.jpg\" loaded. Moving on to \"backdrop.jpg\".");
        if(!LPCMod_ReadJPGFromHDD("\\XBlast\\backdrop.jpg")){
            debugSPIPrint("\"backdrop.jpg\" loaded. Repainting.");
            printMainMenuHeader(&of, modName, fHasHardware, cpuSpeed);
    	}

        if(cromwell_config==XROMWELL && fHasHardware != SYSCON_ID_V1 && fHasHardware != SYSCON_ID_XT){
            debugSPIPrint("Trying to load settings from cfg file on HDD.");
            tempLPCmodSettings = (_LPCmodSettings *)malloc(sizeof(_LPCmodSettings));
            returnValue = LPCMod_ReadCFGFromHDD(tempLPCmodSettings, &settingsPtrStruct);
            if(returnValue == 0){
                memcpy(&LPCmodSettings, tempLPCmodSettings, sizeof(_LPCmodSettings));
                //settingsPrintData(NULL);
                I2CSetFanSpeed(LPCmodSettings.OSsettings.fanSpeed);
                initialSetLED(LPCmodSettings.OSsettings.LEDColor);
                //Stuff to do right after loading persistent settings from file.
                if(fHasHardware == SYSCON_ID_V1 ||
                   fHasHardware == SYSCON_ID_V1_TSOP ||
                   fHasHardware == SYSCON_ID_XX1 ||
                   fHasHardware == SYSCON_ID_XX2 ||
                   fHasHardware == SYSCON_ID_XXOPX ||
                   fHasHardware == SYSCON_ID_XX3 ||
                   fHasHardware == SYSCON_ID_X3){
                    assertInitLCD();                            //Function in charge of checking if a init of LCD is needed.
                }
                
                partition = OpenFATXPartition(0, SECTOR_SYSTEM, SYSTEM_SIZE);
	        if(partition != NULL) {
        	    dcluster = FATXFindDir(partition, FATX_ROOT_FAT_CLUSTER, "XBlast");
        	    if((dcluster != -1) && (dcluster != 1)) {
            		dcluster = FATXFindDir(partition, dcluster, "scripts");
        	    }
        	    if((dcluster != -1) && (dcluster != 1)) {
        	    	res = FATXFindFile(partition, "bank.script", FATX_ROOT_FAT_CLUSTER, &fileinfo);
                	if(res == 0 || fileinfo.fileSize == 0) {
                	    LPCmodSettings.OSsettings.runBankScript = 0;
			}
        	    	res = FATXFindFile(partition, "boot.script", FATX_ROOT_FAT_CLUSTER, &fileinfo);
                	if(res == 0 || fileinfo.fileSize == 0) {
                	    LPCmodSettings.OSsettings.runBootScript = 0;
			}
                    }
                    CloseFATXPartition(partition);
                }
                //bootScriptSize should not have changed if we're here.
                if(LPCmodSettings.OSsettings.runBootScript && bootScriptSize <= 0){
                    debugSPIPrint("Running boot script.");
                    if(loadScriptFromHDD("\\XBlast\\scripts\\boot.script", &fileinfo)){
                        i = BNKOS;
                        runScript(bootScriptBuffer, bootScriptSize, 1, &i);
                        free(bootScriptBuffer);
                    }
                    debugSPIPrint("Boot script execution done.");
                }
            }

            free(tempLPCmodSettings);
        }
    }
    
    VIDEO_CURSOR_POSX=nTempCursorX;
    VIDEO_CURSOR_POSY=nTempCursorY;
    VIDEO_CURSOR_POSX=0;
    VIDEO_CURSOR_POSY=0;

    if(mbVersion > REV1_1 && !DEV_FEATURES)
       LPCmodSettings.OSsettings.TSOPcontrol = 0;       //Make sure to not show split TSOP options. Useful if modchip was moved from 1 console to another.

    printk("\n\n\n\n");

    nTempCursorMbrX=VIDEO_CURSOR_POSX;
    nTempCursorMbrY=VIDEO_CURSOR_POSY;


    //Debug routine to (hopefully) identify the i2c eeprom on a Xecuter 3.
//    u8 *videosavepage = malloc(FB_SIZE);
//    memcpy(videosavepage,(void*)FB_START,FB_SIZE);
//    BootVideoClearScreen(&jpegBackdrop, 0, 0xffff);
//    printk("\n\n\n\n");
//    for(i = 0x50; i < 0x54; i++){               //Hopefully they didn't use an obscure eeprom chip
//                                                //and it will respond to top nibble 0b0101. If not we'll bruteforce it.
//            printk("\n                addr:%02x     data:%02x", i, I2CTransmitByteGetReturn(i, 0x0));
//            printk("\n                addr:%02x     data:%02x", i, I2CTransmitByteGetReturn(i, 0x1));
//
//    }
//    while ((risefall_xpad_BUTTON(TRIGGER_XPAD_KEY_A) != 1)) wait_ms(10);
//    memcpy((void*)FB_START,videosavepage,FB_SIZE);
//    free(videosavepage);
    //Remove after success
    

    //Check for unformatted drives.
    for (i=0; i<2; ++i) {
        if (tsaHarddiskInfo[i].m_fDriveExists && !tsaHarddiskInfo[i].m_fAtapi
            && tsaHarddiskInfo[i].m_dwCountSectorsTotal >= (SECTOR_EXTEND - 1)
            && !(tsaHarddiskInfo[i].m_securitySettings&0x0002)) {    //Drive not locked.
            if(tsaHarddiskInfo[i].m_enumDriveType != EDT_XBOXFS){
                debugSPIPrint("No FATX detected on %s HDD.", i ? "Slave" : "Master");
                // We save the complete framebuffer to memory (we restore at exit)
                videosavepage = malloc(FB_SIZE);
                memcpy(videosavepage,(void*)FB_START,FB_SIZE);
                char ConfirmDialogString[50];
                sprintf(ConfirmDialogString, "               Format new drive (%s)?\0", i ? "slave":"master");
                if(!ConfirmDialog(ConfirmDialogString, 1)){
                    debugSPIPrint("Formatting base partitions.");
                    FATXFormatDriveC(i, 0);                     //'0' is for non verbose
                    FATXFormatDriveE(i, 0);
                    FATXFormatCacheDrives(i, 0);
                    FATXSetBRFR(i);
                    //If there's enough sectors to make F and/or G drive(s).
                    if(tsaHarddiskInfo[i].m_dwCountSectorsTotal >= (SECTOR_EXTEND + SECTORS_SYSTEM)){
                        debugSPIPrint("Show user extended partitions format options.");
                        DrawLargeHDDTextMenu(i);//Launch LargeHDDMenuInit textmenu.
                    }
                    if(tsaHarddiskInfo[i].m_fHasMbr == 0)       //No MBR
                        FATXSetInitMBR(i);                      //Since I'm such a nice program, I will integrate the partition table to the MBR.
                    debugSPIPrint("HDD format done.");
                }
                memcpy((void*)FB_START,videosavepage,FB_SIZE);
                free(videosavepage);
            }
        }
    }
    
    
//    printk("i2C=%d SMC=%d, IDE=%d, tick=%d una=%d unb=%d\n", nCountI2cinterrupts, nCountInterruptsSmc, nCountInterruptsIde, BIOS_TICK_COUNT, nCountUnusedInterrupts, nCountUnusedInterruptsPic2);
    IconMenuInit();
    debugSPIPrint("Starting IconMenu.");
    while(IconMenu())
        IconMenuInit();
    //Good practice.
    free(videosavepage);

    //Should never come back here.
    while(1);
}
