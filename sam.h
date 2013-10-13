/*
   SCSI-3 SCSI Architecture Model definitions (SAM), taken from SCSI-3 SAM1 draft.
   Computer Programming Language: MS Visual Studio NET 2002 C/C++.
   Author: Truman
   Date written: 07 Aug 2005.
*/
#ifndef SAM_H
#define SAM_H

//SCSI return status codes.
#define STATUS_GOOD     0x00  // Status Good
#define STATUS_CHKCOND  0x02  // Check Condition
#define STATUS_CONDMET  0x04  // Condition Met
#define STATUS_BUSY     0x08  // Busy
#define STATUS_INTERM   0x10  // Intermediate
#define STATUS_INTCDMET 0x14  // Intermediate-condition met
#define STATUS_RESCONF  0x18  // Reservation conflict
#define STATUS_COMTERM  0x22  // Command Terminated
#define STATUS_QFULL    0x28  // Queue full
#define STATUS_ACA      0x30  // ACA active

#endif