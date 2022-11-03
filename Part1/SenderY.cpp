//============================================================================
//
//% Student Name 1: Siddhesh Sawant
//% Student 1 #: 301326234
//% Student 1 userid (ssawant): stu1 (ssawant@sfu.ca)
//
//% Student Name 2: Namsakhi Kumar
//% Student 2 #: 301336727
//% Student 2 userid (nka47): stu2 (namsakhi_kumar@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P1_<userid1>_<userid2>" (eg. P1_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : SenderY.cpp
// Version     : September 3rd, 2021
// Description : Starting point for ENSC 351 Project
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "SenderY.h"

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <errno.h>
#include <fcntl.h>  // for O_RDWR
#include <sys/stat.h>

#include "myIO.h"

using namespace std;

SenderY::
SenderY(vector<const char*> iFileNames, int d)
:PeerY(d),
 bytesRd(-1), 
 fileNames(iFileNames),
 fileNameIndex(0),
 blkNum(0)
{
}

//-----------------------------------------------------------------------------

/* generate a block (numbered 0) with filename and filesize */
//void SenderY::genStatBlk(blkT blkBuf, const char* fileName)
//void SenderY::printBlk(uint8_t blkBuf[0]){
//    cout<<"----------START-----------"<<endl;
//    for(int i = 0; i<3; i++){
//        cout<<blkBuf[i];
//    }
//    cout<<endl;
//    cout<<"-----------END------------"<<endl;
//}

void SenderY::genStatBlk(uint8_t blkBuf[BLK_SZ_CRC], const char* fileName)
{
    // ********* additional code must be written ***********

                   blkBuf[0] = SOH; //blkBuf[1] = abc.txt
                   blkBuf[1] = blkNum;
                   blkBuf[2] =  ~(blkNum);
                   int base_index;
                  // std::string FileName(fileName);
                  //fileName = FileName.substr(FileName.find_last_of("/") + 1);
                   for(int i = strlen(fileName); i>=0 ; i--)
                   {
                       if(fileName[i] == '/')
                       {
                           base_index = i+1;
                           break;
                       }
                   }
                   int baseNameLen = strlen(fileName) - base_index;

                   memcpy(&blkBuf[DATA_POS], &fileName[base_index], baseNameLen+1);// blkBuf[]

                   struct stat st;
                   stat(fileName, &st);


                   string file_sz =to_string(st.st_size);

                  for(unsigned int i = 0; i <  file_sz.length() ; i++)
                  {
                      blkBuf[DATA_POS+ baseNameLen +1+i] = file_sz[i];
                  }

                  blkBuf[DATA_POS+ baseNameLen+file_sz.length()+1] = '\0';

                  memset(&blkBuf[DATA_POS+ baseNameLen+ file_sz.length()+2],CTRL_Z, 130 -(DATA_POS+ baseNameLen+ file_sz.length()+2) );


               /* calculate and add CRC in network byte order */
               // ********* The next couple lines need to be changed ***********
               uint16_t myCrc16ns;
               crc16ns(&myCrc16ns, &blkBuf[3]);
               blkBuf[131] = myCrc16ns>>8;
               blkBuf[132] = myCrc16ns; // not sure about this line
}

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block
was prepared or if the input file is empty (i.e. has 0 length).
*/
//void SenderY::genBlk(blkT blkBuf)
void SenderY::genBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
    // ********* The next line needs to be changed ***********
    if (-1 == (bytesRd = myRead(transferringFileD, &blkBuf[0], CHUNK_SZ )))
        ErrorPrinter("myRead(transferringFileD, &blkBuf[0], CHUNK_SZ )", __FILE__, __LINE__, errno);
    // ********* and additional code must be written ***********
    uint8_t padByte = CHUNK_SZ - bytesRd;
    void* paddingPtr = blkBuf + DATA_POS + bytesRd;
    if(bytesRd ==0){
        // empty file
        //****additional code must be written here******
        memset(paddingPtr, CTRL_Z, padByte);
    }
    //if not empty file
    if(bytesRd>0){
        blkBuf[0] = SOH;
        blkBuf[SOH_OH] = blkNum+1;
        blkBuf[BLK_NUM_AND_COMP_OH] = ~(blkNum+1);



        if(bytesRd<CHUNK_SZ){
            memset(paddingPtr, CTRL_Z, padByte);
        }
    }
        /* calculate and add CRC in network byte order */
        // ********* The next couple lines need to be changed ***********

        uint16_t myCrc16ns;
        crc16ns(&myCrc16ns, &blkBuf[3]);
        blkBuf[131] = myCrc16ns>>8;
        blkBuf[132] = myCrc16ns; // not sure about this line
    // ...
}

void SenderY::cans()
{
    // ********* send CAN_LEN of CAN characters ***********
            sendByte(CAN);
            sendByte(CAN);
}

//uint8_t SenderY::sendBlk(blkT blkBuf)
void SenderY::sendBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
    // ********* fill in some code here to send a block (write to mediumD) ***********
    myWrite(mediumD, blkBuf, BLK_SZ_CRC);
}

void SenderY::statBlk(const char* fileName)
{
    blkNum = 0;
    // assume 'C' received from receiver to enable sending with CRC
    genStatBlk(blkBuf, fileName); // prepare 0eth block
    sendBlk(blkBuf); // send 0eth block
    // assume sent block will be ACK'd
}

void SenderY::sendFiles()
{
     for (unsigned fileNameIndex = 0; fileNameIndex < fileNames.size(); ++fileNameIndex) {
        // const char* fileName = fileNames[fileNameIndex];
        const char* fileName = fileNames[fileNameIndex];
        transferringFileD = myOpen(fileName, O_RDWR, 0);
        if(transferringFileD == -1) {
            // ********* fill in some code here to write 8 CAN characters ***********
            cans();
            cout /* cerr */ << "Error opening input file named: " << fileName << endl;
            result = "OpenError";
            return;
        }
        else {
            cout << "Sender will send " << fileName << endl;

            // do the protocol, and simulate a receiver that positively acknowledges every
            //  block that it receives.

            statBlk(fileName);

            // assume 'C' received from receiver to enable sending with CRC
            genBlk(blkBuf); // prepare 1st block
            while (bytesRd)
            {

                blkNum ++; // 1st block about to be sent or previous block was ACK'd

                sendBlk(blkBuf); // send block

                // assume sent block will be ACK'd
                genBlk(blkBuf); // prepare next block
                // assume sent block was ACK'd
            };
            // finish up the file transfer, assuming the receiver behaves normally and there are no transmission errors
            // ********* fill in some code here ***********
            sendByte(EOT);
            sendByte(EOT);
            //(myClose(transferringFileD));
            if (-1 == myClose(transferringFileD))
                ErrorPrinter("myClose(transferringFileD)", __FILE__, __LINE__, errno);
        }
    }
    // indicate end of the batch.
    statBlk("");

    result = "Done";
}
