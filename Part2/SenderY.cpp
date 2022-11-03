//============================================================================
//
//% Student Name 1: student1
//% Student 1 #: 123456781
//% Student 1 userid (email): stu1 (stu1@sfu.ca)
//
//% Student Name 2: student2
//% Student 2 #: 123456782
//% Student 2 userid (email): stu2 (stu2@sfu.ca)
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
//% * Your group name should be "P2_<userid1>_<userid2>" (eg. P2_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : SenderY.cpp
// Version     : September 23rd, 2021
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "SenderY.h"

#include <iostream>
#include <experimental/filesystem> // for C++14
#include <filesystem>
#include <stdio.h> // for snprintf()
#include <stdint.h> // for uint8_t
#include <string.h> // for memset(), and memcpy() or strncpy()
#include <errno.h>
#include <fcntl.h>	// for O_RDWR
#include "AtomicCOUT.h" // FOR COUT
#include <sys/stat.h>

#include "myIO.h"
#include "VNPE.h"

using namespace std;
using namespace std::filesystem; // C++17
//using namespace experimental::filesystem; // C++14

SenderY::
SenderY(vector<const char*> iFileNames, int d)
:PeerY(d),
 bytesRd(-1), 
 fileNames(iFileNames),
// fileNameIndex(0),
 blkNum(0)
{
}

//-----------------------------------------------------------------------------

// get rid of any characters that may have arrived from the medium.
void SenderY::dumpGlitches()
{
	const int dumpBufSz = 20;
	char buf[dumpBufSz];
	int bytesRead;
	while (dumpBufSz == (bytesRead = PE(myReadcond(mediumD, buf, dumpBufSz, 0, 0, 0))));
}

// Send the block, less the block's last byte, to the receiver.
// Returns the block's last byte.

uint8_t SenderY::sendMostBlk(blkT blkBuf)
//uint8_t SenderY::sendMostBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
	const int mostBlockSize = (BLK_SZ_CRC) - 1;
	PE_NOT(myWrite(mediumD, blkBuf, mostBlockSize), mostBlockSize);
	return *(blkBuf + mostBlockSize);
}

// Send the last byte of a block to the receiver
void
SenderY::
sendLastByte(uint8_t lastByte)
{
	PE(myTcdrain(mediumD)); // wait for previous part of block to be completely drained from the descriptor
	dumpGlitches();			// dump any received glitches

	PE_NOT(myWrite(mediumD, &lastByte, sizeof(lastByte)), sizeof(lastByte));
}

/* generate a block (numbered 0) with filename and filesize */
void SenderY::genStatBlk(blkT blkBuf, const char* fileName)
//void SenderY::genStatBlk(uint8_t blkBuf[BLK_SZ_CRC], const char* fileName)
{
    blkBuf[SOH_OH] = 0;
    blkBuf[SOH_OH + 1] = ~0;
    int index = DATA_POS;
    if (*fileName) { // (0 != strcmp("", fileName)) { // (strlen(fileName) > 0)
        const auto myBasename = path( fileName ).filename().string();
        auto c_basename = myBasename.c_str();
        int fileNameLengthPlus1 = strlen(c_basename) + 1;
        // check for fileNameLengthPlus1 greater than 127.
        if (fileNameLengthPlus1 + 1 > CHUNK_SZ) { // need at least one decimal digit to store st.st_size below
            cout /* cerr */ << "Ran out of space in file info block!  Need block with 1024 bytes of data." << endl;
            exit(-1);
        }

        // On Linux: The maximum length for a file name is 255 bytes. The maximum combined length of both the file name and path name is 4096 bytes.
        memcpy(&blkBuf[index], c_basename, fileNameLengthPlus1);
        //strncpy(&blkBuf[index], c_basename, 12X);
        index += fileNameLengthPlus1;
        struct stat st;
        PE(stat(fileName, &st));
        int spaceAvailable = CHUNK_SZ + DATA_POS - index;
        int spaceNeeded = snprintf((char*)&blkBuf[index], spaceAvailable, "%ld", st.st_size); // check the value of CHUNK_SZ + DATA_POS - index
        if (spaceNeeded > spaceAvailable) {
            cout /* cerr */ << "Ran out of space in file info block!  Need block with 1024 bytes of data." << endl;
            exit(-1);
        }
        index += spaceNeeded + 1;

        blkNum = 0 - 1; // initialize blkNum for the data blocks to come.
    }
    uint8_t padSize = CHUNK_SZ + DATA_POS - index;
    memset(blkBuf+index, 0, padSize);

    // check here if index is greater than 128 or so.
    blkBuf[0] = SOH; // can be pre-initialized for efficiency if no 1K blocks allowed

    /* calculate and add CRC in network byte order */
    crc16ns((uint16_t*)&blkBuf[PAST_CHUNK], &blkBuf[DATA_POS]);
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
	//read data and store it directly at the data portion of the buffer
	bytesRd = PE(myRead(transferringFileD, &blkBuf[DATA_POS], CHUNK_SZ ));
	if (bytesRd>0) {
		blkBuf[0] = SOH; // can be pre-initialized for efficiency
		//block number and its complement
		uint8_t nextBlkNum = blkNum + 1;
		blkBuf[SOH_OH] = nextBlkNum;
		blkBuf[SOH_OH + 1] = ~nextBlkNum;

				//pad ctrl-z for the last block
				uint8_t padSize = CHUNK_SZ - bytesRd;
				memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);

			/* calculate and add CRC in network byte order */
			crc16ns((uint16_t*)&blkBuf[PAST_CHUNK], &blkBuf[DATA_POS]);
	}
}

//Send CAN_LEN copies of CAN characters in a row to the YMODEM receiver, to inform it of
//	the cancelling of a file transfer
void SenderY::cans()
{
	// No need to space in time CAN chars for Part 2.
	// This function will be more complicated in later parts. 
    char buffer[CAN_LEN];
    memset( buffer, CAN, CAN_LEN);
    PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}

/* While sending the now current block for the first time, prepare the next block if possible.
*/
void SenderY::sendBlkPrepNext()
{
    // **** this function will need to be modified ****
    blkNum ++; // 1st block about to be sent or previous block ACK'd
    uint8_t lastByte = sendMostBlk(blkBuf);
    genBlk(blkBuf); // prepare next block
    sendLastByte(lastByte);
    memcpy(blkBufs[0], blkBufs[1], BLK_SZ_CRC);
    memcpy(blkBufs[1], blkBuf, BLK_SZ_CRC);
}

// Resends the block that had been sent previously to the xmodem receiver.
void SenderY::resendBlk()
{
	// resend the block including the checksum or crc16
	//  ***** You will have to write this simple function *****
    uint8_t lastByte = sendMostBlk(blkBufs[0]); // 0 OR 1
     sendLastByte(lastByte);
}


int
SenderY::
openFileToTransfer(const char* fileName)
{
    transferringFileD = myOpen(fileName, O_RDONLY);
    return transferringFileD;
}

int
SenderY::
closeTransferredFile()
{
    return PE(myClose(transferringFileD));
}

bool SenderY::checkByte(char *byte) {

    if (*byte != ACK && *byte != NAK && *byte != 'C' && *byte != CAN) {
        this->result = "Unknown Character!";
        cout << "Sender received an unexpected char " << *byte << endl;
        return false;
    }
    return true;
}
void SenderY::sendFiles()
{
    char byteToReceive;
    SenderY& ctx = *this; // needed to work with SmartState-generated code
    bool abortTransfer = false;
    // ***** modify the below code according to the protocol *****
    // below is just a starting point.  You can follow a
    //  different structure if you want.

    //for (auto fileName : fileNames) {
    for (unsigned fileNameIndex = 0; fileNameIndex < fileNames.size(); ++fileNameIndex) {
        const char* fileName = fileNames[fileNameIndex];
        
        uint8_t temp[] = {'C',ACK, 'C', ACK, ACK, ACK, ACK, NAK, ACK, 'C'};
        int i =0;
        ctx.openFileToTransfer(fileName);
	
        if(ctx.transferringFileD != -1) {
            ctx.genStatBlk(blkBuf, fileName); // prepare 0eth block
        }
        if(ctx.transferringFileD == -1) {
                   ctx.cans();
                   ctx.result = "OpenError"; // include errno and so forth in here.
               }
        //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); //reads for c
       byteToReceive = ACK;// assuming get a 'C'
        while(checkByte(&byteToReceive)&& byteToReceive !='C' && errCnt< errB )
        {
            errCnt++;
            if(errCnt>=errB){
                ctx.result ="ExcessiveErrors";
                ctx.cans();
            }
            //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // reading c's
            byteToReceive = temp[i++];
            cout<<byteToReceive<<endl;
        }
        // we know we have a C
        errCnt =0;
        if(byteToReceive == 'C')
            ctx.sendBlkPrepNext(); // send 0th block and prepare 1st block


          //  PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); //reading after first C, should be ack but can be nak
            byteToReceive = temp[i++];
        cout <<byteToReceive<<endl;
        if(byteToReceive == NAK)
            {
                while(checkByte(&byteToReceive) && byteToReceive == NAK && errCnt<errB )
                {
                    resendBlk(); // worry about the 2 cans here
                    //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
                    byteToReceive = temp[i++];
                     cout <<byteToReceive<<endl;
                    errCnt++;
                    if(errCnt>=errB){
                     ctx.result ="ExcessiveErrors";
                     ctx.cans();
                    }
                    //dumpglitches ??
                }
              errCnt = 0;
            }
            else if(byteToReceive == ACK)
            {
               // PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get a 'C'
                byteToReceive = temp[i++];
                 cout <<byteToReceive<<endl;
                if(byteToReceive == 'C')
                {
                    while (ctx.bytesRd && errCnt < errB && !abortTransfer)
                    {
                        //data transfer begins
                      ctx.sendBlkPrepNext();
                      //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // either be ack or nak
                      byteToReceive = temp[i++];
                      cout <<byteToReceive<<endl;
                      if(byteToReceive == ACK){
                          continue;
                      }
                      else if(byteToReceive == NAK){
                          while(checkByte(&byteToReceive) && byteToReceive == NAK && errCnt<errB )
                              {
                                 resendBlk(); // worry about the 2 cans here
                                 //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
                                  byteToReceive = temp[i++];
                                   cout <<byteToReceive<<endl;
                                   errCnt++;
                                   if(errCnt>=errB){
                                   ctx.result ="ExcessiveErrors";
                                    ctx.cans();
                                   }
                                              //dumpglitches ??
                                }
                                        errCnt = 0;
                                        bytesRd = 0;
                      }
                      else
                          ctx.result = "ERROR state";
                    }
                    if(ctx.bytesRd<=0){
                        //send eot1
                        sendByte(EOT);
                       // PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // checkig for nak, assuming ack is never sent from receiver
                        byteToReceive = temp[i++];
                         cout <<byteToReceive<<endl;
                        if(byteToReceive == NAK){
                            //send eot2
                            sendByte(EOT);
                            //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
                            byteToReceive = temp[i++];
                             cout <<byteToReceive<<endl;
                            if(byteToReceive == NAK){
                                int count = 0;
                                while(count <10 && byteToReceive == NAK){
                                    sendByte(EOT);
                                    //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
                                    byteToReceive = temp[i++];
                                    cout <<byteToReceive<<endl;
                                    count++;
                                }
                                if(count >=10)
                                {
                                    ctx.cans();
                                    ctx.result = "EOTS exceeded beyond 10";
                                }


                            }
                            else if(byteToReceive == ACK){
                                //assuming the next line will give us C
                               // PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
                                byteToReceive = temp[i++];
                                cout <<byteToReceive<<endl;
                                if(fileNameIndex >= fileNames.size()){
                                    break; // assuming it exists for loop
                                }
                            }
                        }
                        else
                            ctx.result = "refer to the Craig's doc";
                    }
                }
            }
            else
            {
                // checkByte
            }

	

          //  ctx.sendByte(EOT); // send the first EOT
          //  PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get a NAK
          //  ctx.sendByte(EOT); // send the second EOT
           // PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get an ACK

            //ctx.closeTransferredFile();
    }
    //send null pathname here because no files left
    genStatBlk(blkBuf, "");
    uint8_t lastByte = sendMostBlk(blkBuf);
    sendLastByte(lastByte);
    PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
   /* while(byteToReceive == NAK){
        uint8_t lastByte = sendMostBlk(blkBuf);
        sendLastByte(lastByte);
         PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
    }
    */
    if(byteToReceive==ACK){
        ctx.result="Done";
    }
    else
    {
        ctx.result="Done_NAK"; //his is a case that can be covered by timeouts in future parts
    }





    // indicate end of the batch.
   // ctx.genStatBlk(blkBuf, ""); // prepare 0eth block
    //PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get a 'C'
   // ctx.sendLastByte(ctx.sendMostBlk(blkBuf));
   // PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get an ACK

  //  ctx.result = "Done";
}

