#define _CRT_SECURE_NO_WARNINGS
#define _CRT_RAND_S

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>


#define BUFFER_SIZE 50
#define STRING_SIZE 256

#define MAX_VASEALS 50
#define MIN_VASEALS 2

//Every Vessel waits random time between 5 and 3000 msec between actions
#define MAX_SLEEP_TIME 3000 //Miliseconds (3 second)
#define MIN_SLEEP_TIME 5    //Miliseconds 

#define True 1
#define False 0



//single mutex to protect access to the global state data. Recall, like in lecture notes, 
//global state data must be accessed exclusively by each Vessels!
HANDLE mutexPipe;
HANDLE mutexConsole;

//pointer to HANDLE for Array of Scheduling Constraints Semaphores - one for Each Vessels
HANDLE* sem;

HANDLE StdinRead, StdinWrite;      /* pipe for writing parent to child  HaifaPort -> EilatPort*/
HANDLE StdoutRead, StdoutWrite;    /* pipe for writing child to parent  EilatPort -> HaifaPort*/
DWORD read, written;

// Array holding Philosopher IDS - exclusive for each Philosopher to be sent exclusively to each
int* VesselsID; // Philosopher's Thread function - to prevent Race Conditions!

DWORD ThreadID;   //dummy - for Thread Create invokations
HANDLE* Vessels;  //pointer of Vessels Thread Handles!

SYSTEMTIME lt;//system time paramter to holds time data
int VessIN; //count Vessels that has returned to Haifa
char buffer[BUFFER_SIZE];//buffer for holding ReadFile receiving value and WriteFile sending value
unsigned int tempRand;//to hold random number from generator


//Thread function for each Vessels
DWORD WINAPI Vessel(PVOID);

//to Initialise and clean global data (mainly for creating the Semaphore Haldles 
//before all Threads start and closing them properly after all Threads finish!)
int initGlobalData(int);
void cleanupGlobalData(int);
//function decleration
void StartSailing(int);
void CrossToEilat(int);
int calcSleepTime();
void Anchorage(int);
const char* getTimeString();
void PrintToConsole(char*);

int main(int argc, char *argv[])
{
	//check command line argument not empty 
	if (argc != 2) {
		printf("Error :: No command Line Arguments were provided!");
		exit(0);
	}
	//convert command line argument to int,for argument different from int it will return zero 
	int numOfVessels = atoi(argv[1]);
	//validates the numOfVessels value is within the allowable range 
	if (numOfVessels < MIN_VASEALS || numOfVessels >MAX_VASEALS)
	{
		printf("Error :: invalid value for the number of vessels ");
		exit(0);
	}
	
	
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	BOOL success;
	TCHAR ProcessName[256];
	VessIN = 0;
	int index;
	char stringToPrint[STRING_SIZE];
	mutexConsole = CreateMutex(NULL, FALSE, TEXT("ConsoleMutex"));
	if (mutexConsole == NULL)
	{
		return False;
	}
	

	/* set up security attributes so that pipe handles are inherited */
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL,TRUE };

	/* allocate memory */
	ZeroMemory(&pi, sizeof(pi));

	/* create the pipe for writing from parent to child */
	if (!CreatePipe(&StdinRead, &StdinWrite, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed\n");
		return 1;
	}

	// Ensure the write handle to the pipe for StdinWrite is not inherited. 
	if (!SetHandleInformation(StdinWrite, HANDLE_FLAG_INHERIT, 0))
		fprintf(stderr, "StdinWrite SetHandleInformation\n");

	/* create the pipe for writing from child to parent */
	if (!CreatePipe(&StdoutRead, &StdoutWrite, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed\n");
		return 1;
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(StdoutRead, HANDLE_FLAG_INHERIT, 0))
		fprintf(stderr, "StdoutRead SetHandleInformation\n");

	/* establish the START_INFO structure for the child process */
	GetStartupInfo(&si);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	/* redirect the standard input to the read end of the pipe */
	si.hStdOutput = StdoutWrite;
	si.hStdInput = StdinRead;
	si.dwFlags = STARTF_USESTDHANDLES;
	
	wcscpy(ProcessName, L"EilatPort.exe");
	//wcscpy(ProcessName, L"..//..//EilatPort//Debug//EilatPort.exe");
	
	// Start the child process.
	if (!CreateProcess(NULL,   // No module name (use command line).
		ProcessName, // Command line.
		NULL,             // Process handle not inheritable.
		NULL,             // Thread handle not inheritable.
		TRUE,            // inherit handles .
		0,                // No creation flags.
		NULL,             // Use parent's environment block.
		NULL,             // Use parent's starting directory.
		&si,              // Pointer to STARTUPINFO structure.
		&pi)             // Pointer to PROCESS_INFORMATION structure.
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return -1;
	}


	printf("[%s] HaifaPort : Eilat Port connected\n",getTimeString());

	printf("[%s]HaifaPort: Confirmation Request for %d Vessels \n", getTimeString(),numOfVessels);

	/* the parent now wants to write to the pipe */
	if (!WriteFile(StdinWrite, argv[1], BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "Error writing to pipe\n");


	/* now read from the pipe */
	success = ReadFile(StdoutRead, buffer, BUFFER_SIZE, &read, NULL);
	if (!success) {
		fprintf(stderr, "HaifaPort: Error reading from pipe\n");
		return -1;
	}
	/* close the unused ends of the pipe */
	CloseHandle(StdoutWrite);
	CloseHandle(StdinRead);
	if (strcmp(buffer, "FALSE") == 0)
	{
		printf("HaifaPort: Request didn't approved  ,number of vaseals unvalid! \n");
		return -1;
	}
	printf("[%s] HaifaPort: Request approved  , Continue... \n",getTimeString());
	if (initGlobalData(numOfVessels) == False)
	{
		printf("HaifaPort: Error while initiating global data \n");
		return -1;
	}
	// creat Vessel threads with VesselId unique grather then 1.
	for (int i = 0; i < numOfVessels; i++)
	{
		VesselsID[i] = i+1;
		Vessels[i] = CreateThread(NULL, 0, Vessel, &VesselsID[i], 0, &ThreadID);
	}
	//read from pipe until VessIN is equal to numOfVessel 
	while (VessIN < numOfVessels)
	{
		// now have the child read from the pipe
		success = ReadFile(StdoutRead, buffer, BUFFER_SIZE, &read, NULL);;

		// we have to output to stderr as stdout is redirected to the pipe
		if (!success)
		{
			fprintf(stderr, "HaifaPort: error reading from pipe , error %d \n",GetLastError());
			return -1;
		}
		else if((index = atoi(buffer))!=0)
		{
			sprintf(stringToPrint, "[%s] Vessel %s - exiting Canal: Red Sea ==> Med. Sea \n", getTimeString(), buffer);
			PrintToConsole(stringToPrint);
			
			if (!ReleaseSemaphore(sem[index-1], 1, NULL))
				printf("HaifaPort ::Unexpected error sem[%d].V()\n", index);
			VessIN++;
		}
	
	}

	WaitForMultipleObjects(numOfVessels,Vessels,TRUE,INFINITE);

	sprintf(stringToPrint, "[%s] Haifa Port : All Vessels Threads are done \n", getTimeString());
	PrintToConsole(stringToPrint);
	
	/* close the read end of the pipe */
	/* close the write end of the pipe */
	CloseHandle(StdinWrite);
	CloseHandle(StdoutRead);

	/* wait for the child to exit */
	WaitForSingleObject(pi.hProcess, INFINITE);

	fprintf(stderr, "[%s] Haifa Port : Exiting... \n", getTimeString());
	cleanupGlobalData(numOfVessels);
	
	/* close all handles */
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 0;

}

//The Thread Function for each Vessel, gets the Vessl Unique ID (index of the Thread in Array of Threads within the main program)
//This Unique Thread ID (called VessID) is used by the state DB to keep track of each Vessel state at any given time!
DWORD WINAPI Vessel(PVOID Param)
{
	//Get the Unique ID from Param and keep it in a local variable
	int VesselID = *(int*)Param;
	
	StartSailing(VesselID);

	Sleep(calcSleepTime());

	CrossToEilat(VesselID);

	Sleep(calcSleepTime());

	Anchorage(VesselID);

	return 0;
}

//generic function to randomise a Sleep time between 1 and MAX_SLEEP_TIME msec
int calcSleepTime()
{
	errno_t err;
	int calc = 0;
	
	while (calc < MIN_SLEEP_TIME)
	{
		err = rand_s(&tempRand);
		if (err != 0)
		{
			printf_s("The rand_s function failed!\n");
		}
		calc = tempRand % MAX_SLEEP_TIME + 1;
	}
	return calc;
}

//StartSailing function for Vessels VessID - begin to sail
void StartSailing(int vessID)
{
	char stringToPrint[STRING_SIZE];
	sprintf(stringToPrint, "[%s] Vessel %d starts sailing @ Haifa Port \n", getTimeString(), vessID);
	PrintToConsole(stringToPrint);
}

//CrossToEilat function for Vessels VessID  - entering Canal
void CrossToEilat(int vessID)
{
	char message[BUFFER_SIZE];
	char stringToPrint[STRING_SIZE];
	//Access to state DB is done exclusively (protected by mutex)
	//mutex.P()
	WaitForSingleObject(mutexPipe, INFINITE);
	
	sprintf(message, "%d", vessID);

	sprintf(stringToPrint, "[%s] Vessel %d - entering Canal: Med. Sea ==> Red Sea \n", getTimeString(), vessID);
	PrintToConsole(stringToPrint);
	Sleep(calcSleepTime());

	/* the parent now wants to write to the pipe */
	if (!WriteFile(StdinWrite, message, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "Error writing to pipe \n %d \n",GetLastError());

	//mutex.V() - release mutex
	if (!ReleaseMutex(mutexPipe))
		fprintf(stderr, "HaifaPort -> CrossToEilat::Unexpected error mutex.V(), error num - %d\n", GetLastError());


	//sem[vessID-1].P() - Down on Scheduling Constraint Semaphore for Vessel VessID.
	//if test was successful for it (i.e. it started eating), then sem[vessID-1].P() is successfull and Vessel proceeds
	//otherwise, sem[vessID-1].P() puts Vessel on Semaphore Waiting queue, until signaled by a pipe indication, Vessel that 
	//finished Sailing!
	WaitForSingleObject(sem[vessID-1], INFINITE);
}

//Anchorage function for Vessel VessID - done sailing
void Anchorage(int VessID)
{
	char stringToPrint[STRING_SIZE];
	sprintf(stringToPrint, "[%s] Vessel %d done sailing @ Haifa Port \n", getTimeString(), VessID);
	PrintToConsole(stringToPrint);

}

//return const string of current locl time
const char* getTimeString()
{
	GetLocalTime(&lt);
	static char currentLocalTime[20];
	sprintf(currentLocalTime, "%02d:%02d:%02d", lt.wHour, lt.wMinute, lt.wSecond);
	return currentLocalTime;

}

//safe console printing 
void PrintToConsole(char *string)
{
	WaitForSingleObject(mutexConsole, INFINITE);
	fprintf(stderr, "%s", string);
	if (!ReleaseMutex(mutexConsole))
		printf("PrintToConsole::Unexpected error mutex.V()\n");
}

//Initialise global Semaphores
//If all Successful - return True, otherwise (if problem arises) return False
//This is invoked before all Vessels Threads start running
int initGlobalData(int size)
{

	mutexPipe = CreateMutex(NULL, FALSE, NULL);
	if (mutexPipe == NULL)
	{
		return False;
	}
	Vessels = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size * sizeof(HANDLE));
	if (Vessels == NULL)
	{
		printf("initGlobalData:: Error allocating Vessels\n");
		return False;
	}
	sem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size*sizeof(HANDLE));
	if (sem == NULL)
	{
		printf("initGlobalData:: Error allocating sem\n");
		return False;
	}
	VesselsID = malloc(size * sizeof(int));
	if (VesselsID == NULL)
	{
		printf("initGlobalData:: Error allocating VesselsID\n");
		return False;
	}
	for (int i = 0; i < size; i++)
	{
		sem[i] = CreateSemaphore(NULL, 0, 1, NULL);
		if (sem[i] == NULL)
		{
			return False;
		}
	}

	return True;
}

//Close all global semaphore handlers and free dynamic allocations - after all Vessels Threads finish.
void cleanupGlobalData(int size)
{
	CloseHandle(mutexPipe);
	CloseHandle(mutexConsole);

	for (int i = 0; i < size; i++)
	{
		CloseHandle(Vessels[i]);
		CloseHandle(sem[i]);
	}
	free(VesselsID);
	HeapFree(GetProcessHeap(),HEAP_NO_SERIALIZE,sem);
	HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, Vessels);

}