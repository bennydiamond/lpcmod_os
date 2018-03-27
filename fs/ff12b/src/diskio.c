/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#define _FILE_OFFSET_BITS 64
#define __USE_LARGEFILE

#include "diskio.h"		/* FatFs lower layer API */
#include <stdio.h>
#include <errno.h>


typedef struct {
    DSTATUS status;
    WORD sz_sector;
    QWORD n_sectors;
    HANDLE h_drive;
} STAT;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    DSTATUS status = tsaHarddiskInfo[pdrv].m_fDriveExists ? RES_OK : STA_NOINIT;
    status |= tsaHarddiskInfo[pdrv].m_fAtapi ? STA_PROTECT : RES_OK;
	return status;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DRESULT disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    disk_status(pdrv);
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
#define DEFAULT_RETRY_COUNT 3
    int returnValue;

	return returnValue;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
#define DEFAULT_RETRY_COUNT 3
    int returnValue = BootIdeWriteMultiple(pdrv, buff, sector, count, DEFAULT_RETRY_COUNT);

    return returnValue;
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
    DRESULT res = RES_PARERR;
    switch (cmd) {
    case CTRL_SYNC:         /* Nothing to do */
        res = BootIdeFlushCache(pdrv);
        break;

    case GET_SECTOR_COUNT:  /* Get number of sectors on the drive */
        *(DWORD*)buff = BootIdeGetSectorCount(pdrv);
        res = RES_OK;
        break;

    case GET_SECTOR_SIZE:   /* Get size of sector for generic read/write */
        *(WORD*)buff = BootIdeGetSectorSize(pdrv);
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:    /* Get internal block size in unit of sector */
        // TODO: Fetch block size from device in case flash device are ever supported in XBlast OS.
        *(DWORD*)buff = SZ_BLOCK;
        res = RES_OK;
        break;

    }

    return res;

}

