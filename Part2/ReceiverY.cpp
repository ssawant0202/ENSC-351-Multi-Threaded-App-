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
// File Name   : ReceiverY.cpp
// Version     : September 24th, 2021
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "ReceiverY.h"

#include <string.h> // for memset()
#include <fcntl.h>
#include <stdint.h>
#include "AtomicCOUT.h" //FOR COUT
#include "myIO.h"
#include "VNPE.h"

using namespace std;

ReceiverY::
ReceiverY(int d)
:PeerY(d),
closedOk(1),
anotherFile(0xFF),
NCGbyte('C'),
goodBlk(false), 
goodBlk1st(false), 
syncLoss(false), // transfer will end when syncLoss becomes true
numLastGoodBlk(0),
BlockZeroRep(0)
{
    blockAlreadyRecieved = false;
}

/* Only called after an SOH character has been received.
The function receives the remaining characters to form a complete
block.
The function will set or reset a Boolean variable,
goodBlk. This variable will be set (made true) only if the
calculated checksum or CRC agrees with the
one received and the received block number and received complement
are consistent with each other.
Boolean member variable syncLoss will only be set to
true when goodBlk is set to true AND there is a
fatal loss of syncronization as described in the XMODEM
specification.
The member variable goodBlk1st will be made true only if this is the first
time that the block was received in "good" condition. Otherwise
goodBlk1st will be made false.
*/

void ReceiverY::getRestBlk()
{
 PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);
  //  goodBlk1st = goodBlk = true;
   uint8_t nextGoodBlk= numLastGoodBlk+1;
   uint16_t* CRC_tot = new uint16_t;
   crc16ns(CRC_tot, &rcvBlk[DATA_POS]);
   if(rcvBlk[1]+rcvBlk[2]!=255)
        {
            goodBlk = false;
            goodBlk1st = false;
            syncLoss = false;
        }
    else
    {
        if((rcvBlk[1]!=nextGoodBlk) && (rcvBlk[1]!=numLastGoodBlk))
        {
            syncLoss = true;
        }
         else
        {
            syncLoss = false;
            if(CRC_tot[0]!=rcvBlk[131] || CRC_tot[1]!=rcvBlk[132])
            {
                 goodBlk = goodBlk1st = false;
            }
            else
            {
                goodBlk = true;
                if(rcvBlk[1]==nextGoodBlk) //1 //2
                {
                        goodBlk1st = true;

                }
                else                       //(rcvBlk[BLK_NUM_BYTE] != numGoodBlk)
                {   if(anotherFile)        // This is then Zeroth Block
                    {    BlockZeroRep++; // 2
                        if(BlockZeroRep >=2)
                        {
                            goodBlk = false; //
                        }
                        else
                            goodBlk = true;

                        if(numLastGoodBlk == 255)
                        {
                            numLastGoodBlk = 0;
                        }
                    }
                    else{     //  this is for all blocks after zeroth block if previous block called again
                        if(rcvBlk[1] == numLastGoodBlk)
                        {
                            blockAlreadyRecieved = true;        // Our ACK likely got corrupted. Resend out ACK
                            goodBlk = false;                    // Indicate this isn't a good block. We don't want to rewrite it to the file
                        }
                        }
                }

            }

        }

    }


}

/*
void ReceiverY::getRestBlk()
{
    // ********* this function must be improved ***********
    PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);
    goodBlk1st = goodBlk = true;


}
*/
//Write chunk (data) in a received block to disk
void ReceiverY::writeChunk()
{
	PE_NOT(myWrite(transferringFileD, &rcvBlk[DATA_POS], CHUNK_SZ), CHUNK_SZ);
}

int
ReceiverY::
openFileForTransfer()
{
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    transferringFileD = myCreat((const char *) &rcvBlk[DATA_POS], mode);
    return transferringFileD;
}

int
ReceiverY::
closeTransferredFile()
{
   // return myClose(transferringFileD);
    closedOk = myClose(transferringFileD);
    return closedOk;
}

//Send CAN_LEN CAN characters in a row to the XMODEM sender, to inform it of
//	the cancelling of a file transfer
void ReceiverY::cans()
{
	// no need to space in time CAN chars coming from receiver
    char buffer[CAN_LEN];
    memset( buffer, CAN, CAN_LEN);
    PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}
uint8_t
ReceiverY::
checkForAnotherFile()
{
    return (anotherFile = rcvBlk[DATA_POS]);
}

void ReceiverY::receiveFiles()
{
    ReceiverY& ctx = *this; // needed to work with SmartState-generated code

    // ***** improve this member function *****

    // below is just an example template.  You can follow a
    //  different structure if you want.
    ctx.errCnt = 0;
    transferringFileD = -1;
    while (
            sendByte(ctx.NCGbyte),
           // PE_NOT(myRead(mediumD, rcvBlk, 1), 1), // Should be SOH
           // @@@@@@@@@@@@
           PE_NOT(myRead(mediumD, rcvBlk, 1), 1), (rcvBlk[0] == SOH),
            ctx.getRestBlk(), // get block 0 with fileName and filesize


            checkForAnotherFile()) {

        if(!ctx.syncLoss & (ctx.errCnt< errB) & !ctx.goodBlk){
            ctx.errCnt++;
            ctx.sendByte(NAK);
            continue;
        }
        else if( ctx.syncLoss || ctx.errCnt >= errB){
            ctx.cans();
                if (ctx.syncLoss)
                    ctx.result="LossOfSync at Stat Blk";
                else
                    ctx.result="ExcessiveErrors at Stat";
        }
        else if((!ctx.syncLoss) & (ctx.errCnt < errB)) {
            if(BlockZeroRep < 2)                      // goodBlk = true
                {
                    ctx.checkForAnotherFile();
                }
            else{                                //block zero repeated, goodBlk = false
                    ctx.sendByte(ACK);
                    ctx.sendByte(ctx.NCGbyte);
                }
        }
        else{}// do nothing;
        if(!anotherFile){
            break;
        }

// if blockzerorep >=2 then jummp to while if not then execute the lines form 243 to 256
        if(ctx.openFileForTransfer() == -1){ // zeroth block second time
            if(BlockZeroRep < 2){
            cans();
            result = "CreatError"; // include errno or so
            return;
            }
        }
        else {
            if(BlockZeroRep < 2){
            sendByte(ACK); // acknowledge block 0 with fileName.

            // inform sender that the receiver is ready and that the
            //      sender can send the first block
            sendByte(ctx.NCGbyte);
            }

            while(PE_NOT(myRead(mediumD, rcvBlk, 1), 1), (rcvBlk[0] == SOH))
            {
                ctx.getRestBlk();// set all the variables;
                if(ctx.goodBlk1st){
                    ctx.errCnt = 0;
                    ctx.anotherFile = 0;

                }
                else
                    ctx.errCnt++;

                if(!ctx.syncLoss & (ctx.errCnt < errB)){
                   if((blockAlreadyRecieved ==  true) && (!goodBlk))
                   {
                       ctx.sendByte(ACK);
                   }
                   else{
                    if(ctx.goodBlk){
                        ctx.sendByte(ACK);
                        if(ctx.anotherFile){
                            ctx.sendByte(ctx.NCGbyte);
                        }
                    }
                    else   // goodBlk  =  false, but blockAlreadyRecieved = false
                        ctx.sendByte(NAK);
                   }
                }
                if(ctx.goodBlk1st){
                    ctx.writeChunk();
                    numLastGoodBlk++;
                    ctx.goodBlk1st =  false;
                }
               if(ctx.syncLoss || ctx.errCnt >= errB){
                      ctx.cans();
                      ctx.closeTransferredFile();
                if (ctx.syncLoss)
                 ctx.result="LossOfSyncronization";
                else
                 ctx.result="ExcessiveErrors";
               }



                ctx.sendByte(ACK); // assume the expected block was received correctly.
                ctx.writeChunk();
            };

            // assume EOT was just read in the condition for the while loop
            ctx.sendByte(NAK); // NAK the first EOT
            PE_NOT(myRead(mediumD, rcvBlk, 1), 1); // presumably read in another EOT
            if(rcvBlk[0]==EOT){
                if(ctx.closeTransferredFile()==0)
                {
                    ctx.sendByte(ACK);
                    ctx.sendByte(ctx.NCGbyte);
                    ctx.errCnt=0;
                }
                else
                {
                    ctx.cans();
                    ctx.result="CloseError";
                }

            }

            // Check if the file closed properly.  If not, result should be "CloseError".

        }
    }
    if(rcvBlk[0]==CAN){
        PE_NOT(myRead(mediumD, rcvBlk, 1), 1);
        if(rcvBlk[0]==CAN)
        {
            if(transferringFileD != 1){
                ctx.closeTransferredFile();
            }
            ctx.result="SndCancelled";
        }
        else{

            COUT << "Receiver received totally unexpected char # after first can " << rcvBlk[0] << ": " << (char) rcvBlk[0]<<endl;
            exit(EXIT_FAILURE);
            }


    }
    else if(!anotherFile){
        sendByte(ACK); // acknowledge empty block 0.
        ctx.result = "Done";
    }
    else{

        COUT << "Receiver received totally unexpected char #" << rcvBlk[0] << ": " << (char) rcvBlk[0]<<endl;
        exit(EXIT_FAILURE);
        }


}
