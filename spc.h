/*
   SCSI-3 Primary Control definitions (SPC). 09 Oct 2006.
   Definitions taken from SCSI-3 SPC1 draft.
   Computer Programming Language: MS Visual Studio NET 2002 C/C++.
   Author: Truman
*/
#ifndef SPC_H
#define SPC_H

#define MAX_SENSE_LEN 18 //Sense data max length
//Sense key codes..
#define SEN_KEY_NO_SEN      0x00 //No sense key info.
#define SEN_KEY_NOT_READY   0x02 //Device not ready error.
#define SEN_KEY_ILLEGAL_REQ 0x05 //Illegal request, error/s in parameters or cmd.

//CDB for test unit ready command
typedef struct
{
    unsigned char cmd;
    unsigned char reserved1;
    unsigned char reserved2;
    unsigned char reserved3;
    unsigned char reserved4;
    unsigned char control;
}T_test_unit_ready;

//Request sense return data format
typedef struct
{
    unsigned char response_code;
    unsigned char segment_no;
    unsigned char flags_sensekey;
    unsigned char info0;
    unsigned char info1;
    unsigned char info2;
    unsigned char info3;
    unsigned char add_len;
    unsigned char com_spec_info0;
    unsigned char com_spec_info1;
    unsigned char com_spec_info2;
    unsigned char com_spec_info3;
    unsigned char ASC;
    unsigned char ASCQ;
    unsigned char field_rep_ucode;
    unsigned char sen_key_spec15;
    unsigned char sen_key_spec16;
    unsigned char sen_key_spec17;
    unsigned char add_sen_bytes;
}T_sense_data;

#endif
