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
#include "RefCode_F54_FullRawCapLimit.h"

unsigned char ImageBuffer[CFG_F54_TXCOUNT*CFG_F54_RXCOUNT*2];
signed short ImageArray[CFG_F54_TXCOUNT] [CFG_F54_RXCOUNT] ;

unsigned char F54_FullRawCap(int mode, int option)
// mode - 0:For sensor, 1:For FPC, 2:CheckTSPConnection, 3:Baseline, 4:Delta image
// option - 0:Default, 1:Test purpose
{
   signed short temp=0;
   int Result = 0;
   int TSPCheckLimit=700;
	short Flex_LowerLimit = -100;
	short Flex_UpperLimit = 500;

	int i, j, k;
	unsigned short length;

	unsigned char command;

	length = numberOfTx * numberOfRx* 2;

	if(mode == 0 || mode == 1)
	{
		command = 0x00;
		writeRMI(F54_CBCSettings, &command, 1);
		writeRMI(F54_CBCSettings_0D, &command, 1);
	}
	else if(mode == 3 || mode == 4)
	{
/*		// Disable CBC
		command = 0x00;
		writeRMI(F54_CBCSettings, &command, 1);
		writeRMI(F54_CBCSettings_0D, &command, 1);
*/
	}

	// No sleep
	command = 0x04;
	writeRMI(F01_Ctrl_Base, &command, 1);

	// Set report mode to to run the AutoScan
	if(mode >= 0 && mode < 4)	command = 0x03;
	if(mode == 4)				command = 0x02;
	writeRMI(F54_Data_Base, &command, 1);

	if(mode == 0 || mode == 1)
	{
		command = 0x01;
		writeRMI(NoiseMitigation, &command, 1);
	}
	else if(mode == 3 || mode == 4)
	{
/*		//NoCDM4
		command = 0x01;
		writeRMI(NoiseMitigation, &command, 1);
*/
	}

	if(mode != 3 && mode != 4)
	{
		// Force update & Force cal
		command = 0x06;
		writeRMI(F54_Command_Base, &command, 1);

		do {
			delayMS(1); //wait 1ms
			readRMI(F54_Command_Base, &command, 1);
		} while (command != 0x00);
	}

	// Enabling only the analog image reporting interrupt, and turn off the rest
	command = 0x08;
	writeRMI(F01_Cmd_Base+1, &command, 1);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	// Set the GetReport bit to run the AutoScan
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	// Wait until the command is completed
	do {
	 delayMS(1); //wait 1ms
		readRMI(F54_Command_Base, &command, 1);
   } while (command != 0x00);

	readRMI(F54_Data_Buffer, &ImageBuffer[0], length);

	switch(mode)
	{
		case 0:
		case 1:
			printk("#ofTx\t%d\n", numberOfTx);
			printk("#ofRx\t%d\n", numberOfRx);

			printk("\n#3.03	Full raw capacitance Test\n");

			k = 0;
			for (i = 0; i < numberOfTx; i++)
			{
				printk("%d\t", i);
				for (j = 0; j < numberOfRx; j++)
				{
						temp = (short)(ImageBuffer[k] | (ImageBuffer[k+1] << 8));
						if(CheckButton[i][j] != 1)
						{
							if(mode==0)
							{
								if ((temp >= (int)(Limit[i][j*2])) && (temp <= (int)(Limit[i][j*2+1])))
								{
									Result++;
									printk("%d\t", temp); //It's for getting log for limit set
								}
								else{
									//printk("%d (*)\t", temp); //It's for getting log for limit set
									printk("%d\t", temp); //It's for getting log for limit set
								}
							}
							else
							{
								if ((temp >= Flex_LowerLimit) && (temp <= Flex_UpperLimit))
								{
									Result++;
									printk("%d\t", temp); //It's for getting log for limit set
								}
								else
								{
									//printk("%d (*)\t", temp); //It's for getting log for limit set
									printk("%d\t", temp); //It's for getting log for limit set
								}
							}
						}
						else{
							Result++;
							//printk("%d (O)\t", temp); //It's for getting log for limit set
							printk("%d\t", temp); //It's for getting log for limit set
						}

				  k = k + 2;
				}
				printk("\n"); //It's for getting log for limit set
			}
			printk("#3.04	Full raw capacitance Test END\n");

			printk("\n#3.01	Full raw capacitance Test Limit\n");
			// Print Capacitance Imgae for getting log for Excel format
			printk("\t");
			for (j = 0; j < numberOfRx; j++) printk("%d-Min\t%d-Max\t", j, j);
			printk("\n");

			for (i = 0; i < numberOfTx; i++)
			{
				printk("%d\t", i);
				for (j = 0; j < numberOfRx; j++)
				{
					if(mode==0)
						printk("%d\t%d\t", Limit[i][j*2], Limit[i][j*2+1]);
					else
						printk("%d\t%d\t", Flex_LowerLimit, Flex_UpperLimit);
				}
				printk("\n");
			}
			printk("#3.02	Full raw cap Limit END\n");
			break;

		case 2:
			k = 0;
			for (i = 0; i < numberOfTx; i++)
			{
				for (j = 0; j < numberOfRx; j++)
				{
						temp = (short)(ImageBuffer[k] | (ImageBuffer[k+1] << 8));
						   if (temp > TSPCheckLimit)
						   Result++;
				  k = k + 2;
				}
			}
			break;

		case 3:
		case 4:
			k = 0;
			for (i = 0; i < numberOfTx; i++)
			{
				for (j = 0; j < numberOfRx; j++)
				{
						ImageArray[i][j] = (short)(ImageBuffer[k] | (ImageBuffer[k+1] << 8));
				  k = k + 2;
				}
			}

			if(option == 0)
			{
				 // Print Capacitance Imgae for getting log for Excel format
				 printk("\n\t");
				 for (j = 0; j < numberOfRx; j++) printk("RX%d\t", RxChannelUsed[j]);
				 printk("\n");

				 for (i = 0; i < numberOfTx; i++)
				 {
					 printk("TX%d\t", TxChannelUsed[i]);
					 for (j = 0; j < numberOfRx; j++)
					 {
						 if(mode == 3)		printk("%d\t", (ImageArray[i][j]));
						 else if(mode == 4)	printk("%d\t", ImageArray[i][j]);
					  }
				  printk("\n");
				 }
			}
			break;

		default:
			break;
	}

	if(mode != 3 && mode != 4)
	{
		//Reset
		command= 0x01;
		writeRMI(F01_Cmd_Base, &command, 1);
		delayMS(200);
		readRMI(F01_Data_Base+1, &command, 1); //Read Interrupt status register to Interrupt line goes to high
	}

	if(mode >= 0 && mode < 2)
	{
		if(Result == numberOfTx * numberOfRx)
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
	else if(mode == 2)
	{
		if(Result == 0) return 0; //TSP Not connected
		else return 1; //TSP connected
	 }
	else
	{
		return 0;
	}
 }
#endif

