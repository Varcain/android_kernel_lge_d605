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
 */

#include "RefCode.h"
#include "RefCode_PDTScan.h"

#ifdef _F54_TEST_
unsigned char F54_HighResistance(void)
{
	unsigned char imageBuffer[6];
	short resistance[3];
	int i, Result=0;
	unsigned char command;

	//float resistanceLimit[3][2] = {{-1000, 450}, {-1000, 450}, {-400, 20}}; //base value * 1000 --HSLEE
	int resistanceLimit[3][2] = {{-1000, 450}, {-1000, 450}, {-400, 20}}; //base value * 1000 --HSLEE

	printk("\nBin #: 12		Name: High Resistance Test\n");

   // Set report mode
   command = 0x04;
   writeRMI(F54_Data_Base, &command, 1);

   // Force update
   command = 0x04;
   writeRMI(F54_Command_Base, &command, 1);

   do {
		delayMS(1); //wait 1ms
        readRMI(F54_Command_Base, &command, 1);
   } while (command != 0x00);

   command = 0x02;
   writeRMI(F54_Command_Base, &command, 1);

   do {
		delayMS(1); //wait 1ms
        readRMI(F54_Command_Base, &command, 1);
   } while (command != 0x00);

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

	readRMI(F54_Data_Buffer, imageBuffer, 6);

	printk("Parameters:\t");
	for(i=0; i<3; i++)
	{
		resistance[i] = (short)((imageBuffer[i*2+1] << 8) | imageBuffer[i*2]);
		//printk("%1.3f,\t\t", (float)(resistance[i])/1000);
		//if((resistance[i]/1000 >= resistanceLimit[i][0]) && (resistance[i]/1000 <= resistanceLimit[i][1]))	Result++;
		printk("%d,\t\t", (resistance[i]));
		if((resistance[i] >= resistanceLimit[i][0]) && (resistance[i] <= resistanceLimit[i][1]))	Result++;
	}
	printk("\n");

	printk("Limits:\t\t");
	for(i=0; i<3; i++)
	{
		//printk("%1.3f,%1.3f\t", resistanceLimit[i][0], resistanceLimit[i][1]);
		printk("%d,%d\t", resistanceLimit[i][0], resistanceLimit[i][1]);
	}
	printk("\n");

   // Set the Force Cal
   command = 0x02;
   writeRMI(F54_Command_Base, &command, 1);

   do {
		delayMS(1); //wait 1ms
        readRMI(F54_Command_Base, &command, 1);
   } while (command != 0x00);

   //enable all the interrupts
   //Reset
   command= 0x01;
   writeRMI(F01_Cmd_Base, &command, 1);
   delayMS(200);
   readRMI(F01_Data_Base+1, &command, 1); //Read Interrupt status register to Interrupt line goes to high

   if(Result == 3)
	{
		printk("Test Result: Pass\n");
		return 1; //Pass
	}
   else
	{
		printk("Test Result: Fail, Result = %d\n", Result);
		return 0; //Fail
	}
}
#endif

