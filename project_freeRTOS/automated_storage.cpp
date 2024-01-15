// labwork1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include<conio.h>
#include<stdlib.h>
#include <string.h>
#include <windows.h>  //for Sleep function
#include <time.h>
extern "C" {
#include <interface.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
}

// MAILBOXES
xQueueHandle mbx_x;		
xQueueHandle mbx_z;		
xQueueHandle mbx_xResend;	
xQueueHandle mbx_zResend;	
xQueueHandle mbx_input;		
xQueueHandle mbx_req;		
xQueueHandle mbx_lights;

// SEMAPHORES
xSemaphoreHandle sem_x_done;
xSemaphoreHandle sem_z_done;
xSemaphoreHandle sem_canPut;

typedef struct
{
	int reference;		// reference of product
	long int entryDate;	// seconds since January 1, 1970
	int expiration;		// time to expire
	bool isEmpty;		//FALSE if in storage coordinates there's a product, TRUE if else

} StoreRequest;

int getBitValue(uInt8 value, uInt8 bit_n)
// given a byte value, returns the value of its bit n
{
	return(value & (1 << bit_n));
}

void setBitValue(uInt8* variable, int n_bit, int new_value_bit)
// given a byte value, set the n bit to value
{
	uInt8  mask_on = (uInt8)(1 << n_bit);
	uInt8  mask_off = ~mask_on;
	if (new_value_bit)* variable |= mask_on;
	else                *variable &= mask_off;
}

void moveXLeft()
// move parking kit to the left
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //  read port 2
	setBitValue(&p, 6, 1);     //  set bit 6 to high level
	setBitValue(&p, 7, 0);      //set bit 7 to low level
	writeDigitalU8(2, p); //  update port 2
	taskEXIT_CRITICAL();
}

void moveXRight()
// move parking kit to the right
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 6, 0);    //  set bit 6 to  low level
	setBitValue(&p, 7, 1);      //set bit 7 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void stopX()
// stop moving x 
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 6, 0);   //  set bit 6 to  low level
	setBitValue(&p, 7, 0);   //set bit 7 to low level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

int getXPos()
// get current position of x
{
	uInt8 p = readDigitalU8(0);   //read port 0
	if (!getBitValue(p, 2))	   //read bit value in position 2
		return 1;
	if (!getBitValue(p, 1))   //read bit value in position 1
		return 2;
	if (!getBitValue(p, 0))   //read bit value in position 0
		return 3;
	return(-1);
}

void gotoX(int x_dest)
// move x to a position chosen (x_dest) 
{
	int x_act = getXPos();		//get position in the x axis
	if (x_act != x_dest) {		//x_dest is position in the x axis wanted
		if (x_act < x_dest)
			moveXRight();   //move right in the x axis
		else if (x_act > x_dest)
			moveXLeft();   //move left in the x axis
		while (getXPos() != x_dest) {
			Sleep(1);
		}
		stopX();   //stop moving in the x axis direction
	}
}

void moveYInside()
// move parking kit inside
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 4, 0);    //  set bit 4 to  low level
	setBitValue(&p, 5, 1);      //set bit 5 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void moveYOutside()
// move parking kit outside
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 5, 0);    //  set bit 5 to  low level
	setBitValue(&p, 4, 1);      //set bit 4 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void stopY()
// stop moving y
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 5, 0);   //  set bit 5 to  low level
	setBitValue(&p, 4, 0);   //set bit 4 to low level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

int getYPos()
// get current position of y
{
	uInt8 p = readDigitalU8(0);   //read port 0
	if (!getBitValue(p, 5))   //read bit value in position 5
		return 1;
	if (!getBitValue(p, 4))   //read bit value in position 4
		return 2;
	if (!getBitValue(p, 3))   //read bit value in position 3
		return 3;
	return(-1);
}

void gotoY(int y_dest)
// move y to a position chosen (y_dest) 
{
	int y_act = getYPos();   //get position in the y axis
	if (y_act != y_dest) {   //y_dest is the y axis destination position
		if (y_act < y_dest)
			moveYInside();   //move inside in the y axis
		else if (y_act > y_dest)
			moveYOutside();   //move outside in the y axis
		while (getYPos() != y_dest) {
			Sleep(1);
		}
		stopY();   //stop moving in the y axis direction
	}
}

int getZPos()
// get current position of z
{
	uInt8 p0 = readDigitalU8(0);   //read port 0
	uInt8 p1 = readDigitalU8(1);   //read port 1
	if (!getBitValue(p1, 3))   //read bit 3 of port 1
		return 1;
	if (!getBitValue(p1, 1))   //read bit 1 of port 1
		return 2;
	if (!getBitValue(p0, 7))   // read bit 7 of port 0
		return 3;
	return(-1);
}

int getZPosUp()
// get current position of z in the up position
{
	uInt8 p0 = readDigitalU8(0);   //read port 0
	uInt8 p1 = readDigitalU8(1);   //read port 1
	if (!getBitValue(p1, 2))   //read bit 2 of port 1
		return 1;
	if (!getBitValue(p1, 0))   //read bit 0 of port 1
		return 2;
	if (!getBitValue(p0, 6))   // read bit 6 of port 0
		return 3;
	return(-1);
}

void moveZUp()
// move parking kit up
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 2, 0);    //  set bit 2 to  low level
	setBitValue(&p, 3, 1);      //set bit 3 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void moveZDown()
// move parking kit down
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 3, 0);    //  set bit 3 to  low level
	setBitValue(&p, 2, 1);      //set bit 2 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void stopZ()
// stop moving z
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 3, 0);   //  set bit 3 to  low level
	setBitValue(&p, 2, 0);   //set bit 2 to low level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void gotoZ(int z_dest)
// move z to a position chosen (z_dest) 
{
	int z_act = getZPos();   //get position in the z axis
	if (z_act != z_dest) {   //z_dest is the z axis destination position
		if (z_act < z_dest)
			moveZUp();   //move up in the z axis
		else if (z_act > z_dest)
			moveZDown();   //move down in the z axis
		while (getZPos() != z_dest) {   //while position is not reached
			Sleep(1);
		}
		stopZ();   //stop moving in the z axis direction
	}
}


void gotoZUp()
// move z to the up sensor of the current position 
{
	int z_up = getZPos();
	if (z_up != -1)
	{
		uInt8 p0 = readDigitalU8(0);   //read port 0
		uInt8 p1 = readDigitalU8(1);   //read port 1
		moveZUp();   //move up in the z axis direction
		
		if (z_up == 1)
			while (getBitValue(p1, 2))   //keep getting the value of bit 2 of port 1 until it has reached the up position
			{
				p1 = readDigitalU8(1);   //read port 1
				Sleep(1);
			}
		else if (z_up == 2)
		{
			while (getBitValue(p1, 0))   //keep getting the value of bit 0 of port 1 until it has reached the up position
			{
				p1 = readDigitalU8(1);   //read port 1
				Sleep(1);
			}
		}
		else if (z_up == 3)
		{
			while (getBitValue(p0, 6))   //keep getting the value of bit 6 of the port 0 until it has reached the up position
			{
				p0 = readDigitalU8(0);   //read port 0
				Sleep(1);
			}
		}
		stopZ();   //stop moving in the z axis direction
	}
}


void gotoZDown()
// move z to the down sensor of the current position 
{	
	int z_up = getZPosUp();

	if (z_up != -1)
	{
		uInt8 p0 = readDigitalU8(0); //read port 0
		uInt8 p1 = readDigitalU8(1); //read port 1
		moveZDown();
		if (z_up == 1)
			while (getBitValue(p1, 3))   //keep getting the value of bit 3 of the port 1 until it has reached the down position
			{
				p1 = readDigitalU8(1); //read port 1
				Sleep(1);
			}
		else if (z_up == 2)
		{
			while (getBitValue(p1, 1))   //keep getting the value of bit 1 of the port 1 until it has reached the down position
			{
				p1 = readDigitalU8(1); //read port 1
				Sleep(1);
			}
		}
		else if (z_up == 3)
		{
			while (getBitValue(p0, 7))   //keep getting the value of bit 7 of the port 0 until it has reached the dpwn position
			{
				p0 = readDigitalU8(0); //read port 0
				Sleep(1);
			}
		}
		stopZ();   //stop moving in the z axis direction
	}
}

void putPartInCell()
// insert a product in the cell of the current position
{   
	gotoZUp();		//move up until the up sensor
	gotoY(3);		// go inside to the last position (position 3)
	gotoZDown();	// move down until the down sensor
	gotoY(2);		// go outside until the second position
}

void takePartFromCell()
// remove product from cell in the current position
{
	gotoY(3);		// go inside to the last position (position 3)
	gotoZUp();		//move up until the up sensor
	gotoY(2);		// go outside until the second position
	gotoZDown();	// move down until the down sensor
}

void goto_x_task(void* param)
// task to go to a position in x direction
{
	while (true)
	{
		int x;
		xQueueReceive(mbx_xResend, &x, portMAX_DELAY);	//receive x position from mailbox
		if (getYPos() == 2)		// if y position is not inside (position 2)
			gotoX(x);
		xSemaphoreGive(sem_x_done);	 //synchronize with task gotoXZ
	}
}

void goto_z_task(void* param)
// task to go to a position in z direction
{
	while (true)
	{
		int z;
		xQueueReceive(mbx_zResend, &z, portMAX_DELAY);	//receive z position from mailbox
		if (getYPos() == 2)		// if y position is not inside (position 2)
			gotoZ(z);
		xSemaphoreGive(sem_z_done);		//synchronize with task gotoXZ
	}
}

void gotoXZ(void* param)
{
	int option, x, z;

	while (true)
	{
		xSemaphoreTake(sem_canPut, portMAX_DELAY);		// P(Mutex)

		xQueueReceive(mbx_z, &z, portMAX_DELAY);	// wait until z coordinate is received and store it
		xQueueReceive(mbx_x, &x, portMAX_DELAY);	// wait until x coordinate is received and store it
		xQueueSend(mbx_xResend, &x, portMAX_DELAY);		// resend x coordinate to goto_x_task
		xQueueSend(mbx_zResend, &z, portMAX_DELAY);		// resend z coordinate to goto_z_task

		xSemaphoreTake(sem_x_done, portMAX_DELAY);		// wait until gotoX is done
		xSemaphoreTake(sem_z_done, portMAX_DELAY);		// wait until gotoZ is done
		xQueueReceive(mbx_req, &option, portMAX_DELAY);		// receive option to take or put product

		if (option == 1)
			putPartInCell();
		else if (option == 3)
			takePartFromCell();

		xSemaphoreGive(sem_canPut);		// V(Mutex)
	}
}

void show_menu()
{
	printf("\nCalibration keys: r or l");
	printf("\n(1) ....... Put a good in a specific x, z position");
	printf("\n(2) ....... Retrieve a good from a specific x, z position");
	printf("\n(3) ....... List of all items and corresponding expiration time");
	printf("\n(4) ....... List of all expired products and corresponding coordinates");
	printf("\n(5) ....... Get coordinates and corresponding expiration given its reference");
}

void task_lights(void * param)
// control emergency lights 
{
	int seconds;

	while (TRUE)
	{ 
		xQueuePeek(mbx_lights, &seconds, portMAX_DELAY);	//receive from mailbox without removing it

		taskENTER_CRITICAL();
		uInt8 p2 = readDigitalU8(2);   //read port 2
		setBitValue(&p2, 0, 1);			// set bit of up flash is set to 1 
		if (seconds == 62)			// if its an emergency stop
			setBitValue(&p2, 1, 1);		// set bit of down flash to 1
		writeDigitalU8(2, p2);		//update port 2
		taskEXIT_CRITICAL();

		Sleep(seconds);

		taskENTER_CRITICAL();
		p2 = readDigitalU8(2);   //read port 2
		setBitValue(&p2, 0, 0);		// set bit of up flash is set to 0
		setBitValue(&p2, 1, 0);		// set bit of down flash is set to 1
		writeDigitalU8(2, p2);		//update port 2
		taskEXIT_CRITICAL();

		Sleep(seconds);
	}
}

void task_buttonsPressed(void* param)
// control if buttons are pressed
{
	while (TRUE)
	{
		
		uInt8 p2 = readDigitalU8(2);		// read port 2
		
		vTaskDelay(10);

		uInt8 p1 = readDigitalU8(1);		// read port 1
		bool isMoving = FALSE;		// state of parking kit (moving or not)
		int dif = 0;	// dif = 1 is emergency stop, dif = 5 is to remove expired

		vTaskDelay(1);

		if (getBitValue(p2, 7) || getBitValue(p2, 6) || getBitValue(p2, 5) || getBitValue(p2, 4) || getBitValue(p2, 3) || getBitValue(p2, 2))		// if any of the moving bits are set to one
			isMoving = TRUE;

		time_t timeSeconds = time(NULL);	
		time_t secPassed = 0;

		while (getBitValue(p1, 5) && getBitValue(p1, 6))		// while both buttons are not pressed at the same time
		{
			if (isMoving == TRUE)
			{
				dif = 1;
				int sec = 62;		// ticks to get a 0.5s period to flash lights
				xQueueSend(mbx_lights, &sec, portMAX_DELAY);
				break;		// if its moving it's an emergency stop
			}
			vTaskDelay(1);
			p1 = readDigitalU8(1);
			secPassed = time(NULL);
			dif = secPassed - timeSeconds;		//dif = seconds passed pressing the buttons
			if (dif == 5)
				break;
		}
		if (dif == 5)
		{
			int c = '�', sec = 100;    // sec = ticks to get 1s period to flash lights
									
			xQueueSend(mbx_input, &c, portMAX_DELAY);
			xQueueSend(mbx_lights, &sec, portMAX_DELAY);
		}
		else if (dif == 1)
		{
			taskENTER_CRITICAL();
			uInt8 p2 = readDigitalU8(2), prevState;   
			prevState = p2;			//store state
			p2 = 0;			// set new state to 0
			writeDigitalU8(2, p2);		// update port 2
			taskEXIT_CRITICAL();

			printf("\nEMERGENCY STOP PRESSED... To continue press the up button to RESUME or down button to RESET. ");

			Sleep(1000);
			p1 = readDigitalU8(1);		
			while (!getBitValue(p1, 5) && !getBitValue(p1, 6))  // while one of the buttons is not pressed
			{
				p1 = readDigitalU8(1);		
				vTaskDelay(1);
			}
				
			if (getBitValue(p1, 5))
			{
				printf("\nResuming...");
				xQueueReset(mbx_lights);
				p2 = prevState;			// go back to the previous state
				taskENTER_CRITICAL();
				writeDigitalU8(2, p2);
				taskEXIT_CRITICAL();
			}
			else
			{
				xQueueReset(mbx_lights);
				printf("\nReset... Calibrate the storage kit. WARNING: First calibrate y if not already");
				printf("\nUse o, d, u, l and r. Command: ");

				int cmd, y_pos, x_pos, z_pos;
				bool y_calibrated = FALSE, x_calibrated = FALSE, z_calibrated = FALSE;

				y_pos = getYPos();
				x_pos = getXPos();
				z_pos = getZPos();

				if (y_pos == 2)		// check if y is in position 2
					y_calibrated = TRUE;
				if (x_pos == 1 || x_pos == 2 || x_pos == 3)	// check if x is calibrated
					x_calibrated = TRUE;
				if (z_pos == 1 || z_pos == 2 || z_pos == 3)		// check if z is calibrated
					z_calibrated = TRUE;

				while (!y_calibrated || !x_calibrated || !z_calibrated)		// while not all coordinates are calibrated
				{
					printf("\nCommand: ");
					xQueueReceive(mbx_input, &cmd, portMAX_DELAY);

					if (cmd == 'o')		// 'o' to move outside
					{
						if (y_calibrated)		// if y is calibrated already
							printf("\nY is already calibrated");
						else
						{
							moveYOutside();
							while (getYPos() != 2)
								Sleep(1);
							stopY();
							y_calibrated = TRUE;
						}
					}
					else if (cmd == 'd' || cmd == 'u')	// 'd' to move down and 'u' to move up
					{
						if (!y_calibrated)		// y has to be calibrated befoe moving z
							printf("\nPlease calibrate Y");
						else if (z_calibrated)		// if z is already calibrated
							printf("\nZ is already calibrated");
						else
						{
							if (cmd == 'd')		
								moveZDown();
							else
								moveZUp();	
							while (getZPos() == -1)
								Sleep(1);
							stopZ();
							z_calibrated = TRUE;
						}
					}
					else if (cmd == 'l' || cmd == 'r')		// 'l' to move left and 'r' to move right
					{
						if (!y_calibrated)		// y has to be calibrated befoe moving x
							printf("\nPlease calibrate Y");
						else if (x_calibrated)		// if x is already calibrated
							printf("\nX is already calibrated");
						else
						{
							if (cmd == 'l')
								moveXLeft();
							else
								moveXRight();
							while (getXPos() == -1)
								Sleep(1);
							stopX();
							x_calibrated = TRUE;
						}
					}
					else    
						printf("\nCommand not recognized");
				}
				vTaskEndScheduler();		// end program
			}
		}	
	}
}

bool checkCalibration()
// check if x and z are calibrated
{
	int x_pos = getXPos(), z_pos = getZPos();
	if ((x_pos == 1 || x_pos == 2 || x_pos == 3) && (z_pos == 1 || z_pos == 2 || z_pos == 3))
		return TRUE;
	return FALSE;
}

int * ref_inStorage(StoreRequest storage[3][3], int ref_int)
// check if given reference (ref_int) is in the storage
{
	int row, col, result[3];  //result = {*exists: 1 if yes and 0 if no* , *row where it exists* , *column where it exists*}
	bool exists = FALSE;
	
	for (row = 0; row < 3; row++)
	{
		for (col = 0; col < 3; col++)
		{
			if (storage[row][col].isEmpty == FALSE)
			{
				if (storage[row][col].reference == ref_int)
				{
					exists = TRUE;
					break;
				}
			}
		}
		if (exists == TRUE)
			break;
	}
	if (exists)
	{
		result[0] = 1;
		result[1] = row;
		result[2] = col;
	}
	else
		result[0] = 0;

	return result;
}

int* checkExpiration(StoreRequest storage)
// checks if that cell has a expired product
{
	int row, col, timePassed, resExpiration[2];   //resExpiration = {*1 if expired, 0 if not*, *time left to expire*}

	time_t currentTime = time(NULL);
		
	timePassed = (long int)currentTime - storage.entryDate;

	if (timePassed < storage.expiration)
	{
		resExpiration[0] = 0;
		resExpiration[1] = storage.expiration - timePassed;
	}
	else
		resExpiration[0] = 1;

	return resExpiration;
}


void task_storage_services(void* param)
// provides services to the storage based on the user's choice
{
	int cmd = -1, x, z, z_pos, x_pos;
	StoreRequest storage[3][3];
	bool calibrationDone = checkCalibration();

	for (int row = 0; row < 3; row++)		//initialize storage
		for (int col = 0; col < 3; col++)
			storage[row][col].isEmpty = TRUE;
	

	while (true) {
		if (calibrationDone == FALSE)
			printf("\nWARNING! Calibration is needed.");
		show_menu();
		// get selected option from keyboard
		printf("\n\nEnter option = ");
		xQueueReceive(mbx_input, &cmd, portMAX_DELAY);
		if (cmd == 'l' || cmd == 'r')		//'l' for left and 'r' for right
		{
			x_pos = getXPos();
			if (x_pos == 1 || x_pos == 2 || x_pos == 3)
				printf("\nPosition X already calibrated.\n");
			else
			{
				if (cmd == 'l')
					moveXLeft();
				else
					moveXRight();
				while (getXPos() == -1)		// while x position wanted is not reached
					Sleep(1);
				stopX();
			}
			if (checkCalibration() == TRUE)
				calibrationDone = TRUE;
		}
		if (cmd == 'u' || cmd == 'd')		//'u' for up and 'd' for down
		{
			z_pos = getZPos();
			if (z_pos == 1 || z_pos == 2 || z_pos == 3)
				printf("\nPosition Z already calibrated.\n");
			else
			{
				if (cmd == 'u')
					moveZUp();
				else
					moveZDown();
				while (getZPos() == -1)		// while z position wanted is not reached
					Sleep(1);
				stopZ();
			}
			if (checkCalibration() == TRUE)
				calibrationDone = TRUE;
		}
		if (cmd == '�')  //remove expired
		{		
			if (calibrationDone)
			{
				printf("\nRemoving expired products in progress.");

				for (int row = 0; row < 3; row++)
				{
					for (int col = 0; col < 3; col++)
					{
						if (storage[row][col].isEmpty == FALSE)
						{
							int* resExpired = checkExpiration(storage[row][col]);

							if (resExpired[0])
							{
								x = row + 1;
								z = col + 1;
								xQueueSend(mbx_x, &x, portMAX_DELAY);
								xQueueSend(mbx_z, &z, portMAX_DELAY);

								int opt = 3;		// option 3 is to remove from cell

								xQueueSend(mbx_req, &opt, portMAX_DELAY);
								storage[row][col].isEmpty = TRUE;
							}
						}

					}
				}
			}
			int size = uxQueueSpacesAvailable(mbx_req);	

			while (uxQueueSpacesAvailable(mbx_req) != 100)		// while mailbox is not completly free
				Sleep(10);
			Sleep(1000);		// to give some time to the function takePartFromCell before turning off lights
			xQueueReset(mbx_lights);	// empty mailbox mbx_lights
		}
		if (cmd == '1' && calibrationDone == TRUE)		// put a product in a cell
		{
			printf("\nX=");
			xQueueReceive(mbx_input, &x, portMAX_DELAY);

			x = x - '0';	//convert from ascii code to number
			printf("\nZ=");
			xQueueReceive(mbx_input, &z, portMAX_DELAY);
			z = z - '0';	//convert from ascii code to number
			if (x > 0 && x < 4 && z > 0 && z < 4)		// if coordinates are within limits
			{
				if (storage[x - 1][z - 1].isEmpty)
				{
					printf("\nReference Number (3 digits): ");

					int ref[3], ref_int = 0, exists;

					for (int i = 0; i < 3; i++)
					{
						xQueueReceive(mbx_input, &ref[i], portMAX_DELAY);
						ref_int = ref_int * 10 + (ref[i] - '0');		// transform from ascii to int
					}
		
					int *result = ref_inStorage(storage, ref_int);		// check if given refernce is in storage

					if (!result[0])
					{
						int opt = 1;		// option 1 is to put a prduct in a cell

						xQueueSend(mbx_x, &x, portMAX_DELAY);
						xQueueSend(mbx_z, &z, portMAX_DELAY);
						xQueueSend(mbx_req, &opt, portMAX_DELAY);

						storage[x - 1][z - 1].isEmpty = FALSE;
						storage[x - 1][z - 1].reference = ref_int;

						printf("\nExpiration Time (3 digits): ");

						int expTime = 0, ch;

						for (int j = 0; j < 3; j++)
						{
							xQueueReceive(mbx_input, &ch, portMAX_DELAY);
							expTime = expTime * 10 + ch - '0';		// transform from ascii to int
						}
						storage[x - 1][z - 1].expiration = expTime;

						time_t timeSeconds = time(NULL);
						long int time_longInt = timeSeconds;

						storage[x - 1][z - 1].entryDate = time_longInt;
					}
					else
						printf("\nProduct already in storage");
				}
				else
					printf("\nCell is full.");
			}
			else
				printf("\nWrong (x,z) coordinates");
		}
		if (cmd == '2' && calibrationDone == TRUE)		// get a product from a cell
		{
			char ref[3], ref_int = 0;
			bool exists = FALSE;

			printf("\nReference Number (3 digits): ");
			for (int i = 0; i < 3; i++)
			{
				xQueueReceive(mbx_input, &ref[i], portMAX_DELAY);
				ref_int = ref_int * 10 + (ref[i] - '0');		// transform from ascii to int
			}

			int* result = ref_inStorage(storage, ref_int);
			int row, col;

			if (result[0] == TRUE)		// if in storage
			{
				row = result[1] + 1;
				col = result[2] + 1;
				xQueueSend(mbx_x, &row, portMAX_DELAY);
				xQueueSend(mbx_z, &col, portMAX_DELAY);

				int opt = 3;		// option 3 is to take a prduct from the cell

				xQueueSend(mbx_req, &opt, portMAX_DELAY);
				row--;
				col--;
				storage[row][col].isEmpty = TRUE;
			}
			else
				printf("\nItem not available in the storage.");
		}
		if (cmd == '3' && calibrationDone == TRUE)		//lists all products and time left to expire
		{
			int row, col;

			for (row = 0; row < 3; row++)
			{
				for (col = 0; col < 3; col++)
				{
					if (storage[row][col].isEmpty == FALSE)
					{
						int* resExpired = checkExpiration(storage[row][col]);

						if (!resExpired[0])		// if not expired
							printf("\nReference: %d      Expiration: %d", storage[row][col].reference, resExpired[1]);
						else
							printf("\nReference: %d      Expiration: EXPIRED", storage[row][col].reference);
					}
				}
			}
		}
		if (cmd == '4' && calibrationDone == TRUE)		// lists all expired products and its coordinates
		{
			int row, col;

			for (row = 0; row < 3; row++)
			{
				for (col = 0; col < 3; col++)
				{
					if (storage[row][col].isEmpty == FALSE)
					{
						int* resExpired = checkExpiration(storage[row][col]);

						if (resExpired[0])		// if expired
							printf("\nReference: %d      Row: %d      Column: %d", storage[row][col].reference, row+1, col+1);
					}
				}
			}
		}
		if (cmd == '5' && calibrationDone == TRUE)		// with given reference, show expiration and coordinates
		{
			printf("\nInsert the reference to get position in storage and the time left to expire: ");

			int ref[3], ref_int = 0;
			bool exists = FALSE;
			for (int i = 0; i < 3; i++)
			{
				xQueueReceive(mbx_input, &ref[i], portMAX_DELAY);
				ref_int = ref_int * 10 + (ref[i] - '0');		// transform from ascii to int
			}
			int* result = ref_inStorage(storage, ref_int);
			int row, col;

			if (result[0])
			{
				row = result[1];
				col = result[2];

				int* resExpired = checkExpiration(storage[row][col]);

				if (!resExpired[0])
					printf("\nRow: %d, Column: %d, Expiration: %d", row + 1, col + 1, resExpired[1]);
				else
					printf("\nRow: %d, Column: %d, Expiration: EXPIRED", row + 1, col + 1);
			}
		}
		if (cmd == 't' ) // hidden option
		{
			writeDigitalU8(2, 0); //stop all motors;
			vTaskEndScheduler(); // terminates application
		}
		// end while
	}
}

void receive_instructions_task(void* ignore) 
// receive instruction to provide to the tasks
{
	int c = 0;
	while (true) {
		c = _getwch(); 
		putchar(c);
		xQueueSend(mbx_input, &c, portMAX_DELAY);
	}
}




int main(int argc, char** argv) 
{
	printf("\nWaiting for hardware simulator...");
	printf("\nReminding: gotoXZ requires kit calibration first...");

	createDigitalInput(0);
	createDigitalInput(1);
	createDigitalOutput(2);

	writeDigitalU8(2, 0);
	printf("\nGot access to simulator...");

	//semaphores
	sem_x_done = xSemaphoreCreateCounting(1000, 0);    
	sem_z_done = xSemaphoreCreateCounting(1000, 0);
	sem_canPut = xSemaphoreCreateMutex();

	//mailboxes
	mbx_x = xQueueCreate(100, sizeof(int));
	mbx_z = xQueueCreate(100, sizeof(int));
	mbx_xResend = xQueueCreate(100, sizeof(int));
	mbx_zResend = xQueueCreate(100, sizeof(int));
	mbx_input = xQueueCreate(100, sizeof(int));
	mbx_req = xQueueCreate(100, sizeof(int));
	mbx_lights = xQueueCreate(100, sizeof(int));

	// create tasks
	xTaskCreate(goto_x_task, "goto_x_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_z_task, "goto_z_task", 100, NULL, 0, NULL);
	xTaskCreate(task_storage_services, "task_storage_services", 100, NULL, 0, NULL);
	xTaskCreate(receive_instructions_task, "receive_instructions_task", 100, NULL, 0, NULL);
	xTaskCreate(gotoXZ, "gotoXZ", 100, NULL, 0, NULL);
	xTaskCreate(task_buttonsPressed, "task_buttonsPressed", 100, NULL, 1, NULL);		// priority is set to 1 because it has to have priority with receive_instructions_task over all other tasks
	xTaskCreate(task_lights, "task_lights", 100, NULL, 0, NULL);

	vTaskStartScheduler();
}





// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
