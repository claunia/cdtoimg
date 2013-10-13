/*
CDToImg v1.01.
22 Oct 2006.
Written by Truman.

Language and type: MS Visual Studio .NET 2002, Visual C++ v7, mixed C and C++,
Application type : Win32 console.

A program that reads an entire CD-ROM (mode 1 or mode 2 form 1) 2048 bytes per
sector and writes to a file - this would be an .ISO file. The CD must be CD-ROM
mode 1 or mode 2 and non-multisession. Note, it does not check if the CD is
valid.  TAO writing mode creates link blocks and this program does not detect
them so if encountered will be considered as a normal sector or unreadable.
Unreadable sectors are not supported and will terminate the process. Only up to
80 minutes reading is supported for the moment. Note that some drives will
ignore certain read speeds, e.g. my Plextor 755A can only be set to a minimum
of 4x read speed. On some drives, reading the last few sectors or ejecting the
disc when reading in 2048 byte/sector mode results in a hang (e.g.: LG CDROM
drive: HL-DT-ST CDROM GCR-8485B 1.05).

As always I do not take any responsibilities if this tool destroys your drive or
even anything else.

This code uses:
- Win32 IOCTL function with SCSI_PASS_THROUGH_DIRECT.
- The SCSI codes used in this source were taken from the draft documents MMC1,
  SPC1 and SAM1.
- MMC1 Read CD command (0xBE, CDB 12) to read sectors (2048 user bytes mode).
- Determine errors, retrieve and decode a few sense data.

Normally you need the ntddscsi.h file from Microsoft DDK CD, but I shouldn't
distribute it, so instead I have written my own my_ntddscsi.h.

If you don't have windows.h some of the define constants are listed as comments.

This is a Win32 console program and only runs in a DOS prompt under Windows
NT4/2K/XP/2003 with appropriate user rights, i.e. you need to log in as
administrator.

Read readme.txt for more info.
*/

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include "my_ntddscsi.h"
#include "sam.h"
#include "FloatUtils.h"

//Global variables..
T_SPDT_SBUF sptd_sb;  //Includes sense buffer
unsigned char *data_buf;  //Buffer for holding transfer data from or to drive.

/*
    1. Check the drive type to see if it's identified as a CDROM drive

    2. Uses Win32 CreateFile function to get a handle to a drive
       that is specified by cDriveLetter.

   If you don't have windows.h, some of the define constants are
   listed as comments.
*/
HANDLE open_volume(char drive_letter)
{
    HANDLE hVolume;
    UINT uDriveType;
    char szVolumeName[8];
    char szRootName[5];
    
    //Make drive_letter as a null terminated string in the form of e.g.: d:\.
    szRootName[0]=drive_letter;
    szRootName[1]=':';
    szRootName[2]='\\';
    szRootName[3]='\0';

    uDriveType = GetDriveType(szRootName);

    //#define GENERIC_READ 0x80000000L
    //#define GENERIC_WRITE 0x40000000L
    switch(uDriveType)
    {
        case DRIVE_CDROM:
        {
            printf("Drive type is recognised as CDROM/DVD.\n");
            break;
        }
        default:
        {
            printf("Drive type is not CDROM/DVD, aborting.\n");
            return INVALID_HANDLE_VALUE;  //#define INVALID_HANDLE_VALUE (long int *)-1
        }
    }

    //Make drive_letter as a null terminated string in the form of e.g.: \\.\d:.
    szVolumeName[0]='\\';
    szVolumeName[1]='\\';
    szVolumeName[2]='.';
    szVolumeName[3]='\\';
    szVolumeName[4]=drive_letter;
    szVolumeName[5]=':';
    szVolumeName[6]='\0';

    //#define FILE_SHARE_READ 0x00000001
    //#define FILE_SHARE_WRITE 0x00000002
    //#define OPEN_EXISTING 3
    //#define FILE_ATTRIBUTE_NORMAL 0x00000080
    //This will only work for CD/DVD device on W2K and higher.
    hVolume = CreateFile(szVolumeName,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);

    if(hVolume == INVALID_HANDLE_VALUE)
    {
        //Try again for NT4..
        hVolume = CreateFile(szVolumeName,
                             GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);

        if(hVolume == INVALID_HANDLE_VALUE)
        {
            printf("Could not create handle for CD/DVD device.\n");
        }
    }

    return hVolume;
}

/* Displays sense error information. */
void disp_sense()
{
    unsigned char key;
    unsigned char ASC;
    unsigned char ASCQ;

    key=sptd_sb.SenseBuf[2] & 0x0F;  //Sense key is only the lower 4 bits
    ASC=sptd_sb.SenseBuf[12];
    ASCQ=sptd_sb.SenseBuf[13];

    printf("Sense data, key:ASC:ASCQ: %02X:%02X:%02X", key, ASC, ASCQ);

    //Decode sense key:ASC:ASCQ.
    //It's a very short list - I'm just trying to show you how to decode into text.
    //You really need to look into MMC document and change this into an exhaustive list from
    //the sense error table that is found in there.
    if(key==SEN_KEY_NO_SEN)
    {
        if(ASC==0x00)
        {
            if(ASCQ==0x00)
            {
                printf(" - No additional sense info.");  //No errors
            }
        }
    }
    else
    if(key==SEN_KEY_NOT_READY)
    {
        if(ASC==0x3A)
        {
            if(ASCQ==0x00)
            {
                printf(" - Medium not present.");
            }
            else
            if(ASCQ==0x01)
            {
                printf(" - Medium not present-tray closed.");
            }
            else
            if(ASCQ==0x02)
            {
                printf(" - Medium not present-tray open.");
            }
        }
    }
    
    printf("\n");
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for SPC1 test unit ready command.
    3. Send the request to the drive.
*/
BOOL test_unit_ready(HANDLE hVolume)
{
    DWORD dwBytesReturned;

    sptd_sb.sptd.Length=sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd_sb.sptd.PathId=0;    //SCSI card ID will be filled in automatically.
    sptd_sb.sptd.TargetId=0;  //SCSI target ID will also be filled in.
    sptd_sb.sptd.Lun=0;       //SCSI lun ID will also be filled in.
    sptd_sb.sptd.CdbLength=6;  //CDB size.
    sptd_sb.sptd.SenseInfoLength=MAX_SENSE_LEN;  //Maximum length of sense data to retrieve.
    sptd_sb.sptd.DataIn=SCSI_IOCTL_DATA_UNSPECIFIED; //There will be no buffer data to/from drive.
    sptd_sb.sptd.DataTransferLength=0;  //Size of buffer transfer data.
    sptd_sb.sptd.TimeOutValue=108000;  //SCSI timeout value (max 108000 sec = time 30 min).
    sptd_sb.sptd.DataBuffer=(PVOID)data_buf;
    sptd_sb.sptd.SenseInfoOffset=sizeof(SCSI_PASS_THROUGH_DIRECT);

    //CDB with values for Test Unit Ready CDB6 command.
    //The values were taken from SPC1 draft paper.
    sptd_sb.sptd.Cdb[0]=0x00;  //Code for Test Unit Ready CDB6 command.
    sptd_sb.sptd.Cdb[1]=0;
    sptd_sb.sptd.Cdb[2]=0;
    sptd_sb.sptd.Cdb[3]=0;
    sptd_sb.sptd.Cdb[4]=0;
    sptd_sb.sptd.Cdb[5]=0;
    sptd_sb.sptd.Cdb[6]=0;
    sptd_sb.sptd.Cdb[7]=0;
    sptd_sb.sptd.Cdb[8]=0;
    sptd_sb.sptd.Cdb[9]=0;
    sptd_sb.sptd.Cdb[10]=0;
    sptd_sb.sptd.Cdb[11]=0;
    sptd_sb.sptd.Cdb[12]=0;
    sptd_sb.sptd.Cdb[13]=0;
    sptd_sb.sptd.Cdb[14]=0;
    sptd_sb.sptd.Cdb[15]=0;

    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive.
    return DeviceIoControl(hVolume,
                           IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           &dwBytesReturned,
                           NULL);
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for MMC1 set CD speed command.
    3. Send the request to the drive.
*/
BOOL set_cd_speed(HANDLE hVolume,
                  unsigned short int in_read_speed,
                  unsigned short int in_write_speed)
{
    DWORD dwBytesReturned;

    sptd_sb.sptd.Length=sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd_sb.sptd.PathId=0;    //SCSI card ID will be filled in automatically.
    sptd_sb.sptd.TargetId=0;  //SCSI target ID will also be filled in.
    sptd_sb.sptd.Lun=0;       //SCSI lun ID will also be filled in.
    sptd_sb.sptd.CdbLength=12;  //CDB size
    sptd_sb.sptd.SenseInfoLength=MAX_SENSE_LEN;  //Return sense buffer length.
    sptd_sb.sptd.DataIn=SCSI_IOCTL_DATA_UNSPECIFIED; //There will be no buffer data to/from drive.
    sptd_sb.sptd.DataTransferLength=0;  //Size of buffer transfer data.
    sptd_sb.sptd.TimeOutValue=108000;  //SCSI timeout value (max 108000 sec = time 30 min).
    sptd_sb.sptd.DataBuffer=(PVOID)data_buf;
    sptd_sb.sptd.SenseInfoOffset=sizeof(SCSI_PASS_THROUGH_DIRECT);

    //CDB with values for set cd speed command.
    //The values were taken from MMC1 draft paper.
    sptd_sb.sptd.Cdb[0]=0xBB;  //Code for set cd speed command.
    sptd_sb.sptd.Cdb[1]=0;
    sptd_sb.sptd.Cdb[2]=(unsigned char)(in_read_speed>>8);
    sptd_sb.sptd.Cdb[3]=(unsigned char)in_read_speed;
    sptd_sb.sptd.Cdb[4]=(unsigned char)(in_write_speed>>8);
    sptd_sb.sptd.Cdb[5]=(unsigned char)in_write_speed;
    sptd_sb.sptd.Cdb[6]=0;
    sptd_sb.sptd.Cdb[7]=0;
    sptd_sb.sptd.Cdb[8]=0;
    sptd_sb.sptd.Cdb[9]=0;
    sptd_sb.sptd.Cdb[10]=0;
    sptd_sb.sptd.Cdb[11]=0;
    sptd_sb.sptd.Cdb[12]=0;
    sptd_sb.sptd.Cdb[13]=0;
    sptd_sb.sptd.Cdb[14]=0;
    sptd_sb.sptd.Cdb[15]=0;

    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive
    return DeviceIoControl(hVolume,
                           IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           &dwBytesReturned,
                           NULL);
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for MMC1 read TOC/PMA/ATIP command.
    3. Send the request to the drive.
*/
BOOL read_TOC_PMA_ATIP(HANDLE hVolume,
                       unsigned char in_format,
                       unsigned char in_trk_sess_no,
                       unsigned short int in_data_trans_len)
{
    DWORD dwBytesReturned;

    sptd_sb.sptd.Length=sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd_sb.sptd.PathId=0;    //SCSI card ID will be filled in automatically.
    sptd_sb.sptd.TargetId=0;  //SCSI target ID will also be filled in.
    sptd_sb.sptd.Lun=0;       //SCSI lun ID will also be filled in.
    sptd_sb.sptd.CdbLength=10;  //CDB size.
    sptd_sb.sptd.SenseInfoLength=MAX_SENSE_LEN;  //Maximum length of sense data to retrieve.
    sptd_sb.sptd.DataIn=SCSI_IOCTL_DATA_IN;  //There will be data from drive.
    sptd_sb.sptd.DataTransferLength=in_data_trans_len;  //Size of input data from drive.
    sptd_sb.sptd.TimeOutValue=108000;  //SCSI timeout value (max 108000 sec = time 30 min).
    sptd_sb.sptd.DataBuffer=(PVOID)data_buf;
    sptd_sb.sptd.SenseInfoOffset=sizeof(SCSI_PASS_THROUGH_DIRECT);

    //CDB with values for READ TOC/PMA/ATIP CDB10 command.
    //The values were taken from MMC draft paper.
    sptd_sb.sptd.Cdb[0]=0x43;  //Code for READ TOC/PMA/ATIP CDB10 command.
    sptd_sb.sptd.Cdb[1]=0;
    sptd_sb.sptd.Cdb[2]=in_format;  //Format code.
    sptd_sb.sptd.Cdb[3]=0;
    sptd_sb.sptd.Cdb[4]=0;
    sptd_sb.sptd.Cdb[5]=0;
    sptd_sb.sptd.Cdb[6]=in_trk_sess_no;
    sptd_sb.sptd.Cdb[7]=(unsigned char)(in_data_trans_len >> 8);  //MSB of max length of bytes to receive.
    sptd_sb.sptd.Cdb[8]=(unsigned char)in_data_trans_len;  //LSB of max length of bytes to receive.
    sptd_sb.sptd.Cdb[9]=0;
    sptd_sb.sptd.Cdb[10]=0;
    sptd_sb.sptd.Cdb[11]=0;
    sptd_sb.sptd.Cdb[12]=0;
    sptd_sb.sptd.Cdb[13]=0;
    sptd_sb.sptd.Cdb[14]=0;
    sptd_sb.sptd.Cdb[15]=0;

    ZeroMemory(data_buf, in_data_trans_len);
    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive.
    return DeviceIoControl(hVolume,
                           IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           &dwBytesReturned,
                           NULL);
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for MMC1 read CD (0xBE, CDB12) command.
    3. Send the request to the drive.
*/
BOOL read_cd_2048(HANDLE hVolume,
                  long int MMC_LBA_sector,
                  unsigned long int n_sectors,
                  unsigned char subch_sel_bits)
{
    DWORD dwBytesReturned;
    long int MMC_LBA_sector2;
    unsigned long int n_sectors2;

    sptd_sb.sptd.Length=sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd_sb.sptd.PathId=0;    //SCSI card ID will be filled in automatically.
    sptd_sb.sptd.TargetId=0;  //SCSI target ID will also be filled in.
    sptd_sb.sptd.Lun=0;       //SCSI lun ID will also be filled in.
    sptd_sb.sptd.CdbLength=12;  //CDB size.
    sptd_sb.sptd.SenseInfoLength=MAX_SENSE_LEN;  //Maximum length of sense data to retrieve.
    sptd_sb.sptd.DataIn=SCSI_IOCTL_DATA_IN;  //There will be data coming from the drive.
    sptd_sb.sptd.DataTransferLength=2048*n_sectors;  //Size of read data.
    sptd_sb.sptd.TimeOutValue=108000;  //SCSI timeout value (max 108000 sec = time 30 min).
    sptd_sb.sptd.DataBuffer=(PVOID)data_buf;
    sptd_sb.sptd.SenseInfoOffset=sizeof(SCSI_PASS_THROUGH_DIRECT);

    //CDB with values for Read CD command.  The values were taken from MMC1 draft paper.
    sptd_sb.sptd.Cdb[0]=0xBE;  //Code for Read CD command.
    sptd_sb.sptd.Cdb[1]=0;
    
    //Fill in starting MMC sector (CDB[2] to CDB[5])..
    sptd_sb.sptd.Cdb[5]=(unsigned char)MMC_LBA_sector;   //Least sig byte of LBA sector no. to read from CD.
    MMC_LBA_sector2=MMC_LBA_sector>>8;
    sptd_sb.sptd.Cdb[4]=(unsigned char)MMC_LBA_sector2;  //2nd byte.
    MMC_LBA_sector2=MMC_LBA_sector2>>8;
    sptd_sb.sptd.Cdb[3]=(unsigned char)MMC_LBA_sector2;  //3rd byte.
    MMC_LBA_sector2=MMC_LBA_sector2>>8;
    sptd_sb.sptd.Cdb[2]=(unsigned char)MMC_LBA_sector2;  //Most significant byte.

    //Fill in no. of sectors to read (CDB[6] to CDB[8])..
    sptd_sb.sptd.Cdb[8]=(unsigned char)n_sectors;  //No. of sectors to read from CD byte 0 (LSB).
    n_sectors2=n_sectors>>8;
    sptd_sb.sptd.Cdb[7]=(unsigned char)n_sectors2;  //No. of sectors to read from CD byte 1.
    n_sectors2=n_sectors2>>8;
    sptd_sb.sptd.Cdb[6]=(unsigned char)n_sectors2;  //No. of sectors to read from CD byte 2 (MSB).

    sptd_sb.sptd.Cdb[9]=0x10;  //Read user data only, 2048 bytes per sector from CDROM.
    sptd_sb.sptd.Cdb[10]=subch_sel_bits;  //Sub-channel selection bits.
    sptd_sb.sptd.Cdb[11]=0;
    sptd_sb.sptd.Cdb[12]=0;
    sptd_sb.sptd.Cdb[13]=0;
    sptd_sb.sptd.Cdb[14]=0;
    sptd_sb.sptd.Cdb[15]=0;

    ZeroMemory(data_buf, 2048*n_sectors);
    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive.
    return DeviceIoControl(hVolume,
                           IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           (PVOID)&sptd_sb, (DWORD)sizeof(sptd_sb),
                           &dwBytesReturned,
                           NULL);        
}

/*
Calculating data rate of digital stereo (2 channels) audio at 44.1 KHz with 16 BITs per channel.
------------------------------------------------------------------------------------------------
Number of bytes taken up by 1 sample of audio:
16 BIT = 1 word = 2 bytes, but there are 2 channels, so 1 sample takes up
2*2=4 bytes.

Data rate in kilobytes/sec will be:
     s=Number of samples per second
     b=Number of bytes per sample

     s*b
     ---- = data rate kb/s
     1000
So data rate of the digital audio is:
     44100*4
     ------- = 176.4 kb/s
     1000
This data rate is used by CDROM/CD writers as 1X speed. This will of course have to be rounded to
a whole number: 176 kb/s, making prog. easier. Note, kb is in units of 1000.
*/
unsigned short int kbytes_2_x_speed(unsigned short int speed_kbytes)
{
    return RoundDouble(speed_kbytes/176.4, 0);
}
unsigned short int x_2_kbytes_speed(unsigned short int x_speed)
{
    return RoundDouble(x_speed*176.4, 0);
}

BOOL verified_set_cd_speed(HANDLE hVolume,
                           unsigned short int in_read_speed,
                           unsigned short int in_write_speed)
{
    BOOL success;

    printf("Sending MMC1 CD speed command ");
    if(in_read_speed==0xFFFF)
    {
        printf("(read: max speed, ");
    }
    else
    {
        printf("(read: %ukbytes (%ux)", in_read_speed, kbytes_2_x_speed(in_read_speed));
    }
    if(in_write_speed==0xFFFF)
    {
        printf("write: max speed)..");
    }
    else
    {
        printf("write: %ukbytes (%ux))..", in_write_speed, kbytes_2_x_speed(in_write_speed));
    }

    //Sends MMC1 set CD speed command to drive.
    success=set_cd_speed(hVolume, in_read_speed, in_write_speed);
    if(success)
    {
        printf("done.\n");
        if(sptd_sb.sptd.ScsiStatus==STATUS_GOOD)
        {
            //Do nothing.
        }
        else
        {
            success=FALSE;
            if(sptd_sb.sptd.ScsiStatus==STATUS_CHKCOND)
            {
                disp_sense();
            }
            else
            {
                printf("Command sent but returned with an unhandled status code: %02X\n", sptd_sb.sptd.ScsiStatus);
            }
        }
    }
    else
    {
        printf("failed.\n");
        printf("DeviceIOControl returned with failed status.\n");
    }

    return success;
}

/* Sends Read TOC/PMA/ATIP command to read TOC, check & display errors and return the success state. */
BOOL verified_read_TOC(HANDLE hVolume,
                       unsigned long int data_buffer_size)
{
    BOOL success;
    unsigned short int alloc_len=0;

    printf("Sending read TOC command..");
    //Sends MMC1 READ TOC/PMA/ATIP command to drive to get 4 byte header.
    success=read_TOC_PMA_ATIP(hVolume, 0, 0, 4);
    if(success)
    {
        if(sptd_sb.sptd.ScsiStatus==STATUS_GOOD)
        {
            alloc_len=data_buf[0] << 8;
            alloc_len=alloc_len | data_buf[1];
        }
        else
        {
            success=FALSE;
            printf("done.\n");
            if(sptd_sb.sptd.ScsiStatus==STATUS_CHKCOND)
            {
                disp_sense();
            }
            else
            {
                printf("Command sent but returned with an unhandled status code: %02X\n", sptd_sb.sptd.ScsiStatus);
            }
        }
    }
    else
    {
        printf("failed.\n");
        printf("Could not return 4 byte Read TOC header.\n");
    }

    if(success && (alloc_len>0))
    {
        //Limit alloc len to maximum allowed by size of data transfer buffer length.
        if((alloc_len+2)>data_buffer_size)
        {
            alloc_len=data_buffer_size-2;
        }

        //Sends MMC1 READ TOC/PMA/ATIP command to drive to get full data.
        success=read_TOC_PMA_ATIP(hVolume, 0, 0, alloc_len+2);
        if(success)
        {
            printf("done.\n");
            if(sptd_sb.sptd.ScsiStatus==STATUS_GOOD)
            {
                alloc_len=data_buf[0] << 8;
                alloc_len=alloc_len | data_buf[1];
            }
            else
            {
                success=FALSE;
                if(sptd_sb.sptd.ScsiStatus==STATUS_CHKCOND)
                {
                    disp_sense();
                }
                else
                {
                    printf("Command sent but returned with an unhandled status code: %02X\n", sptd_sb.sptd.ScsiStatus);
                }
            }
        }
        else
        {
            printf("failed.\n");
            printf("Could only return 4 byte Read TOC header.\n");
        }
    }

    return success;
}

/* Sends Test Unit Ready command 3 times, check for errors & display error info. */
BOOL verified_test_unit_ready3(HANDLE hVolume)
{
    unsigned char i=3;
    BOOL success;

    /*
    Before sending the required command, here we clear any pending sense info from the drive
    which may interfere by sending Test Unit Ready command at least 3 times if neccessary.
    ----------------------------------------------------------------------------------------*/
    do
    {
        printf("Sending SPC1 Test Unit CDB6 command..");
        //Sends SPC1 Test Unit Ready command to drive
        success=test_unit_ready(hVolume);
        if(success)
        {
            printf("done.\n");
            if(sptd_sb.sptd.ScsiStatus==STATUS_GOOD)
            {
                printf("Returned good status.\n");
                i=1;
            }
            else
            {
                if(sptd_sb.sptd.ScsiStatus==STATUS_CHKCOND)
                {
                    disp_sense();
                }
                else
                {
                    printf("Command sent but returned with an unhandled status code: %02X\n", sptd_sb.sptd.ScsiStatus);
                }

                success=FALSE;
            }
        }
        else
        {
            printf("failed.\n");
            printf("DeviceIOControl returned with failed status.\n");
        }
        i--;
    }while(i>0);

    return success;
}

//Find lead-out from read TOC data.
BOOL find_leadout_from_TOC(unsigned char *data_buf,
                           unsigned char &out_n_tracks,
                           unsigned long int &out_n_sectors)
{
    unsigned long int i;
    unsigned long int sector;
    unsigned short int alloc_len;
    unsigned short int TOC_response_data_len;
    BOOL found_leadout;

    alloc_len=data_buf[0] << 8;
    alloc_len=alloc_len | data_buf[1];
    TOC_response_data_len=alloc_len+2;
    found_leadout=FALSE;
    out_n_tracks=0;
    //Iterate thru TOC entries to find track 0xAA (leadout track)..
    for(i=4;i<TOC_response_data_len;i=i+8)
    {
        //Check if we have a complete 8 byte TOC track descriptor page.
        if((i+7)<TOC_response_data_len)
        {
            //Look for ADR Q mode 1 entries only.
            if((data_buf[i+1] & 0xF0)==0x10)
            {
                //Look for track no. 0xAA (lead-out track).
                if(data_buf[i+2]==0xAA)
                {
                    //Get the starting sector of track no. 0xAA.
                    sector=data_buf[i+4];
                    sector=sector<<8;
                    sector=sector | data_buf[i+5];
                    sector=sector<<8;
                    sector=sector | data_buf[i+6];
                    sector=sector<<8;
                    sector=sector | data_buf[i+7];

                    //Check if sector is 0.
                    if(sector>0)
                    {
                        //Total sectors before lead-out.
                        out_n_sectors=sector;
                    }
                    else
                    {
                        //Error, this should never happen.
                        out_n_sectors=0;
                    }

                    out_n_tracks--;  //Don't include this track.

                    found_leadout=TRUE;
                }

                out_n_tracks++;
            }
        }
    }

    return found_leadout;
}

/*
Main loop of reading the CD and writing to image file.
Various error checking are also done here.
*/
BOOL read_cd_to_image(char drive_letter, char *file_pathname, unsigned short int x_speed, unsigned long int data_buffer_size)
{
    HANDLE hVolume;
    BOOL success;
    unsigned char n_tracks;  //Total tracks.
    unsigned long int n_sectors;  //Total sectors on CD.
    FILE *file_ptr;
    unsigned long int LBA_i;  //For counting "from" LBA (starts from 0).
    unsigned long int LBA_i2;  //For calculating "to" LBA.
    unsigned long int n_sectors_to_read;  //No. of sectors to read per read command.
    unsigned short int speed_kbytes;  //Write speed in kbytes.

    hVolume = open_volume(drive_letter);
    if (hVolume != INVALID_HANDLE_VALUE)
    {
        printf("\n");
        success=verified_test_unit_ready3(hVolume);
        printf("\n");
        if(success)
        {
            //Get TOC from CD.
            success=verified_read_TOC(hVolume, data_buffer_size);
            if(success)
            {
                if(find_leadout_from_TOC(data_buf, n_tracks, n_sectors))
                {
                    printf("Total user tracks : %u\n", n_tracks);
                    printf("Total sectors     : %u\n", n_sectors);

                    //Do we need to set CD speed?
                    if(x_speed!=0)
                    {
                        if(x_speed==0xFFFF)
                        {
                            speed_kbytes=0xFFFF;  //Use maximum write speed.
                        }
                        else
                        {
                            //Convert x speed to kbytes speed.
                            speed_kbytes=x_2_kbytes_speed(x_speed);
                        }

                        //Attempt to set the cd read speed to specified and write speed to max.
                        success=verified_set_cd_speed(hVolume, speed_kbytes, 0xFFFF);
                    }
                    else
                    {
                        //No need to set CD speed.
                        success=TRUE;
                    }
                    if(success)
                    {
                        file_ptr=fopen(file_pathname, "wb");
                        if(file_ptr!=NULL)
                        {
                            n_sectors_to_read=data_buffer_size / 2048;  //Block size: E.g.: 65536 / 2048 = 32.
                            LBA_i=0;  //Starting LBA address.
                            while(LBA_i<n_sectors)
                            {
                                //Check if block size is suitable for the remaining sectors.
                                if(n_sectors_to_read>(n_sectors-LBA_i))
                                {
                                    //Alter to the remaining sectors.
                                    n_sectors_to_read=n_sectors-LBA_i;
                                }

                                LBA_i2=LBA_i+n_sectors_to_read-1;
                                printf("Reading sector %u to %u (total: %u, progress: %.1f%%)\n", LBA_i, LBA_i2, n_sectors, (double)LBA_i2/n_sectors*100);
                                if(read_cd_2048(hVolume, LBA_i, n_sectors_to_read, 0))
                                {
                                    if(sptd_sb.sptd.ScsiStatus==STATUS_GOOD)
                                    {
                                        fwrite(data_buf, 2048*n_sectors_to_read, 1, file_ptr);
                                        if(ferror(file_ptr))
                                        {
                                            printf("Write file error!\n");
                                            printf("Aborting process.\n");
                                            success=FALSE;
                                            break;  //Stop while loop.
                                        }
                                    }
                                    else
                                    {
                                        if(sptd_sb.sptd.ScsiStatus==STATUS_CHKCOND)
                                        {
                                            disp_sense();
                                        }
                                        else
                                        {
                                            printf("Command sent but returned with an unhandled status code: %02X\n", sptd_sb.sptd.ScsiStatus);
                                        }

                                        printf("Aborting process.\n");
                                        success=FALSE;
                                        break;  //Stop while loop.
                                    } 
                                }

                                LBA_i=LBA_i+n_sectors_to_read;
                            }

                            fclose(file_ptr);
                        }
                        else
                        {
                            printf("Could not create file!\n");
                            printf("Aborting process.\n");
                            success=FALSE;
                        }
                    }
                    else
                    {
                        printf("Could not set read speed!\n");
                        printf("Aborting process.\n");
                    }
                }
                else
                {
                    printf("Could not find lead-out entry in TOC to determine total sectors on CD!\n");
                    printf("Aborting process.\n");
                    success=FALSE;
                }
            }
            else
            {
                printf("Could not read TOC!\n");
                printf("Aborting process.\n");
            }
        }
        else
        {
            printf("Drive is not ready!\n");
            printf("Aborting process.\n");
        }

        CloseHandle(hVolume);  //Win32 function.
    }
    else
    {
        return FALSE;
    }

    return success;
}

void usage()
{
    printf("CDToImg v1.01. 22 Oct 2006.\n");
    printf("Usage: cdtoimg <drive letter> <output file> [x read speed]\n");
    printf("x speed is one of the following:\n");
    printf("  - Enter CD x speed value.\n");
    printf("  - Ommit or enter 0 to use currently set speed.\n");
    printf("  - Enter m for max speed.\n");
    return ;  //Exit program here.
}

int main(int argc, char *argv[])
{
    unsigned short int x_speed;

    if((argc == 3) || (argc == 4))
    {
        //65536 data transfer buffer.
        data_buf=(unsigned char *)malloc(65536);

        if(argc == 3)
        {
            x_speed=0;  //Use currently set write speed.
        }
        else
        {
            if(x_speed=argv[3][0]=='m')
            {
                x_speed=0xFFFF;  //Use maximum write speed.
            }
            else
            {
                x_speed=atoi(argv[3]);  //Use specified write speed.
            }
        }
        if(read_cd_to_image(argv[1][0], argv[2], x_speed, 65536))
        {
            printf("Process finished.");
        }
        else
        {
            printf("Could not create image from drive %c.", argv[1][0]);
        }

        free(data_buf);

	return 0;
    }
    else
    {
        usage();
        return -1;  //Exit program here.
    }
}
