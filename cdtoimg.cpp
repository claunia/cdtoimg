/*
CDToImg v1.02.
13 Oct 2013.
Written by Truman.

Modified by Natalia Portillo <natalia@claunia.com>

Was language and type: MS Visual Studio .NET 2002, Visual C++ v7, mixed C and C++,
Is: GNU Compiller Collection, mixed C and C++
Was application type : Win32 console.
Is: UNIX console
Will be: Win32 console (MinGW or Visual C++ yet to see)

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
- libcdio as to cross-platform-esque send MMC commands to CD/DVD/BD drives.
- The SCSI codes used in this source were taken from the draft documents MMC1,
  SPC1 and SAM1.
- MMC1 Read CD command (0xBE, CDB 12) to read sectors (2048 user bytes mode).
- Determine errors, retrieve and decode a few sense data.

Read readme.txt for more info.
*/

#include <stdio.h>
#include <malloc.h>
#include "FloatUtils.h"

// CDIO
#include <cdio/cdio.h>
#include <cdio/mmc.h>

#include <string.h> // For memset()
#include <stdlib.h> // For atoi()

//Global variables..
unsigned char *data_buf;  //Buffer for holding transfer data from or to drive.

// Opens device using libcdio
CdIo_t *open_volume(char *drive_letter)
{
	return cdio_open (drive_letter, DRIVER_DEVICE);
}

/* Displays sense error information. */
void disp_sense(CdIo_t *p_cdio)
{
    cdio_mmc_request_sense_t *pp_sense;
    
    int cmd_ret;
    
    cmd_ret =  mmc_last_cmd_sense(p_cdio, &pp_sense);
    
    if(cmd_ret < 0)
    	printf(" - Error reading last MMC sense.");
    else if(cmd_ret == 0)
    	printf(" - No additional sense info.");
    else
    {
	    printf("Sense data, key:ASC:ASCQ: %02X:%02X:%02X", pp_sense->sense_key, pp_sense->asc, pp_sense->ascq);

	    //Decode sense key:ASC:ASCQ.
	    //It's a very short list - I'm just trying to show you how to decode into text.
	    //You really need to look into MMC document and change this into an exhaustive list from
	    //the sense error table that is found in there.
	    if(pp_sense->sense_key==CDIO_MMC_SENSE_KEY_NO_SENSE)
	    {
	        if(pp_sense->asc==0x00)
	        {
	            if(pp_sense->ascq==0x00)
	            {
	                printf(" - No additional sense info.");  //No errors
	            }
	        }
	    }
	    else
	    if(pp_sense->sense_key==CDIO_MMC_SENSE_KEY_NOT_READY)
	    {
    	    if(pp_sense->asc==0x3A)
	        {
        	    if(pp_sense->ascq==0x00)
            	{
    	            printf(" - Medium not present.");
	            }
            	else
        	    if(pp_sense->ascq==0x01)
    	        {
	                printf(" - Medium not present-tray closed.");
            	}
        	    else
    	        if(pp_sense->ascq==0x02)
	            {
            	    printf(" - Medium not present-tray open.");
        	    }
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
driver_return_code_t test_unit_ready(CdIo_t *p_cdio)
{
    mmc_cdb_t cdb = {{0, }};

    //CDB with values for Test Unit Ready CDB6 command.
    //The values were taken from SPC1 draft paper.
    cdb.field[0]=0x00;  //Code for Test Unit Ready CDB6 command.
    cdb.field[1]=0;
    cdb.field[2]=0;
    cdb.field[3]=0;
    cdb.field[4]=0;
    cdb.field[5]=0;
    cdb.field[6]=0;
    cdb.field[7]=0;
    cdb.field[8]=0;
    cdb.field[9]=0;
    cdb.field[10]=0;
    cdb.field[11]=0;
    cdb.field[12]=0;
    cdb.field[13]=0;
    cdb.field[14]=0;
    cdb.field[15]=0;

	return mmc_run_cmd(p_cdio, 108000000, &cdb, SCSI_MMC_DATA_NONE, 0, NULL);
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for MMC1 set CD speed command.
    3. Send the request to the drive.
*/
// claunia: There is a direct libcdio command for this, should we use it?
driver_return_code_t set_cd_speed(CdIo_t *p_cdio,
                  unsigned short int in_read_speed,
                  unsigned short int in_write_speed)
{
    mmc_cdb_t cdb = {{0, }};

    //CDB with values for set cd speed command.
    //The values were taken from MMC1 draft paper.
    cdb.field[0]=0xBB;  //Code for set cd speed command.
    cdb.field[1]=0;
    cdb.field[2]=(unsigned char)(in_read_speed>>8);
    cdb.field[3]=(unsigned char)in_read_speed;
    cdb.field[4]=(unsigned char)(in_write_speed>>8);
    cdb.field[5]=(unsigned char)in_write_speed;
    cdb.field[6]=0;
    cdb.field[7]=0;
    cdb.field[8]=0;
    cdb.field[9]=0;
    cdb.field[10]=0;
    cdb.field[11]=0;
    cdb.field[12]=0;
    cdb.field[13]=0;
    cdb.field[14]=0;
    cdb.field[15]=0;

	return mmc_run_cmd(p_cdio, 108000000, &cdb, SCSI_MMC_DATA_NONE, 0, NULL);
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for MMC1 read TOC/PMA/ATIP command.
    3. Send the request to the drive.
*/
driver_return_code_t read_TOC_PMA_ATIP(CdIo_t *p_cdio,
                       unsigned char in_format,
                       unsigned char in_trk_sess_no,
                       unsigned short int in_data_trans_len)
{
    mmc_cdb_t cdb = {{0, }};

    //CDB with values for READ TOC/PMA/ATIP CDB10 command.
    //The values were taken from MMC draft paper.
    cdb.field[0]=0x43;  //Code for READ TOC/PMA/ATIP CDB10 command.
    cdb.field[1]=0;
    cdb.field[2]=in_format;  //Format code.
    cdb.field[3]=0;
    cdb.field[4]=0;
    cdb.field[5]=0;
    cdb.field[6]=in_trk_sess_no;
    cdb.field[7]=(unsigned char)(in_data_trans_len >> 8);  //MSB of max length of bytes to receive.
    cdb.field[8]=(unsigned char)in_data_trans_len;  //LSB of max length of bytes to receive.
    cdb.field[9]=0;
    cdb.field[10]=0;
    cdb.field[11]=0;
    cdb.field[12]=0;
    cdb.field[13]=0;
    cdb.field[14]=0;
    cdb.field[15]=0;

    memset(data_buf, 0, in_data_trans_len);

	return mmc_run_cmd(p_cdio, 108000000, &cdb, SCSI_MMC_DATA_READ, in_data_trans_len, (void *)data_buf);
}

/*
    1. Set up the sptd values.
    2. Set up the CDB for MMC1 read CD (0xBE, CDB12) command.
    3. Send the request to the drive.
*/
driver_return_code_t read_cd_2048(CdIo_t *p_cdio,
                  long int MMC_LBA_sector,
                  unsigned long int n_sectors,
                  unsigned char subch_sel_bits)
{
    mmc_cdb_t cdb = {{0, }};
    long int MMC_LBA_sector2;
    unsigned long int n_sectors2;

    //CDB with values for Read CD command.  The values were taken from MMC1 draft paper.
    cdb.field[0]=0xBE;  //Code for Read CD command.
    cdb.field[1]=0;
    
    //Fill in starting MMC sector (CDB[2] to CDB[5])..
    cdb.field[5]=(unsigned char)MMC_LBA_sector;   //Least sig byte of LBA sector no. to read from CD.
    MMC_LBA_sector2=MMC_LBA_sector>>8;
    cdb.field[4]=(unsigned char)MMC_LBA_sector2;  //2nd byte.
    MMC_LBA_sector2=MMC_LBA_sector2>>8;
    cdb.field[3]=(unsigned char)MMC_LBA_sector2;  //3rd byte.
    MMC_LBA_sector2=MMC_LBA_sector2>>8;
    cdb.field[2]=(unsigned char)MMC_LBA_sector2;  //Most significant byte.

    //Fill in no. of sectors to read (CDB[6] to CDB[8])..
    cdb.field[8]=(unsigned char)n_sectors;  //No. of sectors to read from CD byte 0 (LSB).
    n_sectors2=n_sectors>>8;
    cdb.field[7]=(unsigned char)n_sectors2;  //No. of sectors to read from CD byte 1.
    n_sectors2=n_sectors2>>8;
    cdb.field[6]=(unsigned char)n_sectors2;  //No. of sectors to read from CD byte 2 (MSB).

    cdb.field[9]=0x10;  //Read user data only, 2048 bytes per sector from CDROM.
    cdb.field[10]=subch_sel_bits;  //Sub-channel selection bits.
    cdb.field[11]=0;
    cdb.field[12]=0;
    cdb.field[13]=0;
    cdb.field[14]=0;
    cdb.field[15]=0;

    memset(data_buf, 0, 2048*n_sectors);

	return mmc_run_cmd(p_cdio, 108000000, &cdb, SCSI_MMC_DATA_READ, 2048*n_sectors, (void *)data_buf);    
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

driver_return_code_t verified_set_cd_speed(CdIo_t *p_cdio,
                           unsigned short int in_read_speed,
                           unsigned short int in_write_speed)
{
    driver_return_code_t success;

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
    success=set_cd_speed(p_cdio, in_read_speed, in_write_speed);
    printf("done.\n");
    if(success==DRIVER_OP_SUCCESS)
    {
        //Do nothing.
    }
    else
    {
        if(success==DRIVER_OP_MMC_SENSE_DATA)
        {
            disp_sense(p_cdio);
        }
        else
        {
            printf("Command sent but returned with an unhandled status code: %02X\n", success);
        }
    }

    return success;
}

/* Sends Read TOC/PMA/ATIP command to read TOC, check & display errors and return the success state. */
driver_return_code_t verified_read_TOC(CdIo_t *p_cdio,
                       unsigned long int data_buffer_size)
{
    driver_return_code_t success;
    unsigned short int alloc_len=0;

    printf("Sending read TOC command..");
    //Sends MMC1 READ TOC/PMA/ATIP command to drive to get 4 byte header.
    success=read_TOC_PMA_ATIP(p_cdio, 0, 0, 4);
    if(success==DRIVER_OP_SUCCESS)
    {
        alloc_len=data_buf[0] << 8;
        alloc_len=alloc_len | data_buf[1];
    }
    else
    {
        printf("done.\n");
        if(success==DRIVER_OP_MMC_SENSE_DATA)
        {
            disp_sense(p_cdio);
        }
        else
        {
            printf("Command sent but returned with an unhandled status code: %02X\n", success);
        }
    }

    if(success==DRIVER_OP_SUCCESS && (alloc_len>0))
    {
        //Limit alloc len to maximum allowed by size of data transfer buffer length.
        if((alloc_len+2)>data_buffer_size)
        {
            alloc_len=data_buffer_size-2;
        }

        //Sends MMC1 READ TOC/PMA/ATIP command to drive to get full data.
        success=read_TOC_PMA_ATIP(p_cdio, 0, 0, alloc_len+2);
        printf("done.\n");
        if(success==DRIVER_OP_SUCCESS)
        {
            alloc_len=data_buf[0] << 8;
            alloc_len=alloc_len | data_buf[1];
        }
        else
        {
            if(success==DRIVER_OP_MMC_SENSE_DATA)
            {
                disp_sense(p_cdio);
            }
            else
            {
                printf("Command sent but returned with an unhandled status code: %02X\n", success);
            }
        }
    }
    else
    {
        printf("failed.\n");
        printf("Could only return 4 byte Read TOC header.\n");
    }

    return success;
}

/* Sends Test Unit Ready command 3 times, check for errors & display error info. */
driver_return_code_t verified_test_unit_ready3(CdIo_t *p_cdio)
{
    unsigned char i=3;
    driver_return_code_t success;

    /*
    Before sending the required command, here we clear any pending sense info from the drive
    which may interfere by sending Test Unit Ready command at least 3 times if neccessary.
    ----------------------------------------------------------------------------------------*/
    do
    {
        printf("Sending SPC1 Test Unit CDB6 command..");
        //Sends SPC1 Test Unit Ready command to drive
        success=test_unit_ready(p_cdio);
        printf("done.\n");
        if(success==DRIVER_OP_SUCCESS)
        {
            printf("Returned good status.\n");
            i=1;
        }
        else
        {
            if(success==DRIVER_OP_MMC_SENSE_DATA)
            {
                disp_sense(p_cdio);
            }
            else
            {
                printf("Command sent but returned with an unhandled status code: %02X\n", success);
            }
        }
        i--;
    }while(i>0);

    return success;
}

//Find lead-out from read TOC data.
bool find_leadout_from_TOC(unsigned char *data_buf,
                           unsigned char &out_n_tracks,
                           unsigned long int &out_n_sectors)
{
    unsigned long int i;
    unsigned long int sector;
    unsigned short int alloc_len;
    unsigned short int TOC_response_data_len;
    bool found_leadout;

    alloc_len=data_buf[0] << 8;
    alloc_len=alloc_len | data_buf[1];
    TOC_response_data_len=alloc_len+2;
    found_leadout=false;
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

                    found_leadout=true;
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
bool read_cd_to_image(char *drive_letter, char *file_pathname, unsigned short int x_speed, unsigned long int data_buffer_size)
{
    CdIo_t *p_cdio;
    bool cmd_ret;
    driver_return_code_t success;
    unsigned char n_tracks;  //Total tracks.
    unsigned long int n_sectors;  //Total sectors on CD.
    FILE *file_ptr;
    unsigned long int LBA_i;  //For counting "from" LBA (starts from 0).
    unsigned long int LBA_i2;  //For calculating "to" LBA.
    unsigned long int n_sectors_to_read;  //No. of sectors to read per read command.
    unsigned short int speed_kbytes;  //Write speed in kbytes.

    p_cdio = open_volume(drive_letter);
    if (p_cdio != NULL)
    {
        printf("\n");
        success=verified_test_unit_ready3(p_cdio);
        printf("\n");
        if(success==DRIVER_OP_SUCCESS)
        {
            //Get TOC from CD.
            success=verified_read_TOC(p_cdio, data_buffer_size);
            if(success==DRIVER_OP_SUCCESS)
            {
                if(find_leadout_from_TOC(data_buf, n_tracks, n_sectors))
                {
                    printf("Total user tracks : %u\n", n_tracks);
                    printf("Total sectors     : %lu\n", n_sectors);

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
                        success=verified_set_cd_speed(p_cdio, speed_kbytes, 0xFFFF);
                    }
// claunia: no need for this
/*                    else
                    {
                        //No need to set CD speed.
                        success=true;
                    }*/
                    if(success==DRIVER_OP_SUCCESS)
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
                                printf("Reading sector %lu to %lu (total: %lu, progress: %.1f%%)\n", LBA_i, LBA_i2, n_sectors, (double)LBA_i2/n_sectors*100);
                                if(read_cd_2048(p_cdio, LBA_i, n_sectors_to_read, 0)==DRIVER_OP_SUCCESS)
                                {
                                    if(success==DRIVER_OP_SUCCESS)
                                    {
                                        fwrite(data_buf, 2048*n_sectors_to_read, 1, file_ptr);
                                        if(ferror(file_ptr))
                                        {
                                            printf("Write file error!\n");
                                            printf("Aborting process.\n");
                                            cmd_ret=false;
                                            break;  //Stop while loop.
                                        }
                                    }
                                    else
                                    {
                                        if(success==DRIVER_OP_MMC_SENSE_DATA)
                                        {
                                            disp_sense(p_cdio);
                                        }
                                        else
                                        {
                                            printf("Command sent but returned with an unhandled status code: %02X\n", success);
                                        }

                                        printf("Aborting process.\n");
                                        break;  //Stop while loop.
                                    } 
                                }
                                else
                                {
                                    if(success==DRIVER_OP_MMC_SENSE_DATA)
                                    {
                                        disp_sense(p_cdio);
                                    }
                                    else
                                    {
                                        printf("Command sent but returned with an unhandled status code: %02X\n", success);
                                    }

                                    printf("Aborting process.\n");
                                    break;  //Stop while loop.
                                } 

                                LBA_i=LBA_i+n_sectors_to_read;
                            }

                            fclose(file_ptr);
                            cmd_ret=true;
                        }
                        else
                        {
                            printf("Could not create file!\n");
                            printf("Aborting process.\n");
                            cmd_ret = false;
                        }
                    }
                    else
                    {
                        printf("Could not set read speed!\n");
                        printf("Aborting process.\n");
                        cmd_ret = false;
                    }
                }
                else
                {
                    printf("Could not find lead-out entry in TOC to determine total sectors on CD!\n");
                    printf("Aborting process.\n");
                    cmd_ret = false;
                }
            }
            else
            {
                printf("Could not read TOC!\n");
                printf("Aborting process.\n");
                cmd_ret = false;
            }
        }
        else
        {
            printf("Drive is not ready!\n");
            printf("Aborting process.\n");
            cmd_ret = false;
        }

        cdio_destroy(p_cdio);  //Win32 function.
    }
    else
    {
        return false;
    }

    return cmd_ret;
}

void usage()
{
    printf("CDToImg v1.02. 13 Oct 2013.\n");
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
        if(read_cd_to_image(argv[1], argv[2], x_speed, 65536))
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