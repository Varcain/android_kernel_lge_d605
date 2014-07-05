/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

#include "RefCode.h"
#include "RefCode_PDTScan.h"

#ifdef _F54_TEST_
unsigned char F54_TxOpenReport(void)
{
   unsigned char ImageBuffer[CFG_F54_TXCOUNT];
   unsigned char ImageArray[CFG_F54_TXCOUNT];
   unsigned char Result = 0;
   //unsigned char Result[CFG_F54_TXCOUNT];

   int i, k;
   int shift;

   unsigned char command;

   printk("\nBin #: 6		Name: Transmitter Open Test\n");
   for (i = 0; i < 32; i++)
		ImageArray[i] = 1;

   // Set report mode
   command = 0x0F;
   writeRMI(F54_Data_Base, &command, 1);

   command = 0x00;
   writeRMI(F54_Data_LowIndex, &command, 1);
   writeRMI(F54_Data_HighIndex, &command, 1);

   // Set the GetReport bit
   command = 0x01;
   writeRMI(F54_Command_Base, &command, 1);

   // Wait until the command is completed
   do {
	   delayMS(1); //wait 1ms
      readRMI(F54_Command_Base, &command, 1);
   } while (command != 0x00);

   readRMI(F54_Data_Buffer, &ImageBuffer[0], 4);

//	printk("Buffer : 0x%x 0x%x 0x%x 0x%x \n", ImageBuffer[0], ImageBuffer[1], ImageBuffer[2], ImageBuffer[3]);

	// One bit per transmitter channel
	k = 0;
	for (i = 0; i < 32; i++)
	{
		 k = i / 8;
		 shift = i % 8;
		 if(!(ImageBuffer[k] & (1 << shift))) ImageArray[i] = 0;
	}

   printk("Column:\t");
   for (i = 0; i < numberOfTx; i++)
   {
	   printk("Tx%d,\t", TxChannelUsed[i]);
   }
   printk("\n");

   printk("0:\t");
   for (i = 0; i < numberOfTx; i++)
   {
	   if(!ImageArray[TxChannelUsed[i]])
	   {
		   Result++;
		   printk("%d,\t", ImageArray[TxChannelUsed[i]]);
	   }
	   else
	   {
		   printk("%d(*),\t", ImageArray[TxChannelUsed[i]]);
	   }
   }
   printk("\n");

   /*
   // Check against test limits
   printk("\nTx-Tx short test result:\n");
   for (i = 0; i < numberOfTx; i++)
   {
	    if (ImageArray[i]== TxTxLimit)
			Result[i] = 'P'; //Pass
		else
		    Result[i] = 'F'; //Fail
		printk("Tx[%d] = %c\n", TxChannelUsed[i], Result[i]);
   }
   */

   //enable all the interrupts
//	SetPage(0x00);
   //Reset
	command= 0x01;
	writeRMI(F01_Cmd_Base, &command, 1);
	delayMS(200);
	readRMI(F01_Data_Base+1, &command, 1); //Read Interrupt status register to Interrupt line goes to high

	if(Result == numberOfTx)
	{
		printk("Test Result: Pass\n");
		return 1; //Pass
	}
	else
	 {
		 printk("Test Result: Fail\n");
		 return 0; //Fail
	 }
}
#endif

