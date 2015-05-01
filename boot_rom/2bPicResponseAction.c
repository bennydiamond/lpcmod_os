/*
 * I2C-related code
 * AG 2002-07-27
 */

 /***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "2bload.h"


// ----------------------------  I2C -----------------------------------------------------------
//
// get a value from a given device address
// errors will have b31 set, ie, will be negative, otherwise fetched byte in LSB of return

int I2CTransmitByteGetReturn(u8 bPicAddressI2cFormat, u8 bDataToWrite)
{
    int nRetriesToLive=400;

    //if(IoInputWord(I2C_IO_BASE+0)&0x8000) {  }
    while(IoInputWord(I2C_IO_BASE+0)&0x0800) ;  // Franz's spin while bus busy with any master traffic

    while(nRetriesToLive--) {

        IoOutputByte(I2C_IO_BASE+4, (bPicAddressI2cFormat<<1)|1);
        IoOutputByte(I2C_IO_BASE+8, bDataToWrite);
        IoOutputWord(I2C_IO_BASE+0, 0xffff); // clear down all preexisting errors
        IoOutputByte(I2C_IO_BASE+2, 0x0a);

        {
            u8 b=0x0;
            while( (b&0x36)==0 ) { b=IoInputByte(I2C_IO_BASE+0); }

            if(b&0x24) {
                //bprintf("I2CTransmitByteGetReturn error %x\n", b);
            }
            if(!(b&0x10)) {
                //bprintf("I2CTransmitByteGetReturn no complete, retry\n");
            } else {
                return (int)IoInputByte(I2C_IO_BASE+6);
            }
        }
    }

    return ERR_I2C_ERROR_BUS;
}

// transmit a word, no returned data from I2C device

int I2CTransmitWord(u8 bPicAddressI2cFormat, u16 wDataToWrite)
{
    int nRetriesToLive=400;

    while(IoInputWord(I2C_IO_BASE+0)&0x0800) ;  // Franz's spin while bus busy with any master traffic

    while(nRetriesToLive--) {
        IoOutputByte(I2C_IO_BASE+4, (bPicAddressI2cFormat<<1)|0);

        IoOutputByte(I2C_IO_BASE+8, (u8)(wDataToWrite>>8));
        IoOutputByte(I2C_IO_BASE+6, (u8)wDataToWrite);
        IoOutputWord(I2C_IO_BASE+0, 0xffff);  // clear down all preexisting errors
        IoOutputByte(I2C_IO_BASE+2, 0x1a);

        {
            u8 b=0x0;
            while( (b&0x36)==0 ) { b=IoInputByte(I2C_IO_BASE+0); }

            if(b&0x24) {
                //bprintf("I2CTransmitWord error %x\n", b);
            }
            if(!(b&0x10)) {
                //bprintf("I2CTransmitWord no complete, retry\n");
            } else {
                return ERR_SUCCESS;
            }
        }
    }
    return ERR_I2C_ERROR_BUS;
}

// ----------------------------  PIC challenge/response -----------------------------------------------------------
//
// given four bytes, returns a u16
// LSB of return is the 'first' byte, MSB is the 'second' response byte

u16 BootPicManipulation(u8 bC, u8 bD, u8 bE,    u8 bF) {
    int n=4;
    u8
        b1 = 0x33,
        b2 = 0xed,
        b3 = ((bC<<2) ^ (bD +0x39) ^ (bE >>2) ^ (bF +0x63)),
        b4 = ((bC+0x0b) ^ (bD>>2) ^ (bE +0x1b))
    ;

    while(n--) {
        b1 += b2 ^ b3;
        b2 += b1 ^ b4;
    }

    return (u16) ((((u16)b2)<<8) | b1);
}

// actual business of getting I2C data from PIC and reissuing munged version
// returns zero if all okay, else error code

int BootPerformPicChallengeResponseAction()
{
    u8 bC, bD, bE, bF;

    bC=I2CTransmitByteGetReturn( 0x10, 0x1c );
    bD=I2CTransmitByteGetReturn( 0x10, 0x1d );
    bE=I2CTransmitByteGetReturn( 0x10, 0x1e );
    bF=I2CTransmitByteGetReturn( 0x10, 0x1f );
    if ((bC==0) && (bD==0) && (bE==0) && (bF==0)) I2CTransmitWord(0x10, 0x0240);

    {
        u16 w=BootPicManipulation(bC, bD, bE, bF);

        I2CTransmitWord( 0x10, 0x2000 | (w&0xff));
        I2CTransmitWord( 0x10, 0x2100 | (w>>8) );
        I2CTransmitWord( 0x10, 0x0100 );
    }

    // continues as part of video setup....
    return ERR_SUCCESS;
}
#if 0
extern int I2cSetFrontpanelLed(u8 b)
{
    I2CTransmitWord( 0x10, 0x800 | b);  // sequencing thanks to Jarin the Penguin!
    I2CTransmitWord( 0x10, 0x701);
    return ERR_SUCCESS;
}
#endif
