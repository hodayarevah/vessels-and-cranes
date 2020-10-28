#define _CRT_SECURE_NO_WARNINGS
#define _CRT_RAND_S
#include <stdio.h> //for I/O
#include <stdlib.h> //for rand/strand
#include <windows.h> //for Win32 API
#include <time.h>    //for using the time as the seed to strand!
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#define BUFFER_SIZE 50
#define STRING_SIZE 256

#define True 1
#define False 0

//Every Vessel waits random time between 5 and 3000 msec between actions
#define MAX_SLEEP_TIME 3000 //Miliseconds (3 second)
#define MIN_SLEEP_TIME 5    //Miliseconds 

//Every crain random carriege weight 
#define MAX_WEIGHT 50
#define MIN_WEIGHT 5

//#define getrandom(min, max) (SHORT)((rand() % (int)(((max)+1) - \(min))) + (min))

DWORD ThreadID; //dummy - for Thread Create invokations
HANDLE* Vessels; //pointer to HANDLE ,after dynamicly allocation will hold threads array of Vessels
int* VesselsID; //pointer to int ,after dynamicly allocation will hold threads-ID array
int VesselSize;  //number of predicted vessel 
int VessIN,VessOut; //number of entered vessels so far

HANDLE ReadHandle, WriteHandle;
DWORD read, written;

HANDLE mutexPipe,mutex;//mutexPipe to manage pipe writing, mutex for enterin unloading quay
HANDLE semADT, *semVessADT;
HANDLE mutexConsole;//shared mutex between processes
HANDLE mutexBarrier;//mutexBarrier to managed entering to Barrier

HANDLE* Cranes;//pointer to HANDELE for each Crane thread
unsigned int* CranesRand;//pointer for array of data holders for Crand random weight generator
int* CranesID;//pointer to int ,after dynamicly allocation will hold threads-ID array of Cranes
int* CranesArr;//
// Crain size holds number of Cranes,freeCrains to managed available Crane for unloading,activeCrane indicate if Craine is active
int CraneSize,freeCranes,activeCrane;

HANDLE* Barrier;//pointer to HANDLE for Array of Scheduling Constraints Semaphores - one for Each Barrier space
int BarrierSize;
int QueueIN, QueueOut;//holds location to entring and exiting from Barrier
int IsFree,tempCountVess;//IsFree to nitify when unloading quay is free,tempCountVess to control QueueIN increasing 
SYSTEMTIME lt; //time stracture 
char AnsReq[BUFFER_SIZE];//on test
char buffer[BUFFER_SIZE]; //to holds the information from the pipe
unsigned int mainRand;

//Thread function for each Vessel
DWORD WINAPI Vessel(PVOID);
//Thread function for each Crain
DWORD WINAPI Crane(PVOID);

//to Initialise and clean global data (mainly for creating the Semaphore Haldles 
//before all Threads start and closing them properly after all Threads finish!)
int initGlobalData();
void cleanupGlobalData();
//function decleration 
void enterBarrier(int);
void CrossToHaifa(int);
void exitBarrier();
void enterUnloadingQuay(int);
void exitUnloadingQuay(int);
int IsPrime(int);
int IsDivisor(int ,int);
int GenerateNumber(int,int*);
int CarinsValidNum(int ,int);
int GemerateWeight(int);
int calcSleepTime();
const char* getTimeString();
void PrintToConsole(char*);

int main(int argc,char* argv[])
{

	BOOL success;
	int check,vessIdIn;
	char stringToPrint[STRING_SIZE];
	VessIN = 0;
	activeCrane = 0;
	QueueIN = 0;
	QueueOut = 0;
	IsFree = True;
	
	
	
	ReadHandle = GetStdHandle(STD_INPUT_HANDLE);
	WriteHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if ((WriteHandle == INVALID_HANDLE_VALUE) ||(ReadHandle == INVALID_HANDLE_VALUE))
	{ 
		fprintf(stderr, "EilatPort: last error %d <\n", GetLastError());

		ExitProcess(1);
	}
	success = ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL);
	
	// now have the child read from the pipe
	//while((success = ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL)))
	//{
	//	
	//	if (VesselSize != 0)
	//		break;
	//	//success = ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL);
	//}
	// we have to output to stderr as stdout is redirected to the pipe
	if (!success) 
	{
		fprintf(stderr, "EilatPort: error reading from pipe\n");
		return -1;
	}

	fprintf(stderr, "[%s] EilatPort: Received request of - %s - Vessels \n", getTimeString(), buffer);
	VesselSize = atoi(buffer);
	check = IsPrime(VesselSize);
	if (check != 0) 
	{
		strcpy(AnsReq, "FALSE");
		if (!WriteFile(WriteHandle, AnsReq, BUFFER_SIZE, &written, NULL))
			fprintf(stderr, "EilatPort: Error writing to pipe\n");
		ExitProcess(1);
	}

	CraneSize = CarinsValidNum(VesselSize, 2);
	sprintf(stringToPrint, "[%s] EilatPort: CraneSize %d \n", getTimeString(), CraneSize);
	PrintToConsole(stringToPrint);

	BarrierSize = VesselSize / CraneSize;
	sprintf(stringToPrint, "[%s] EilatPort: BarrierSize %d \n", getTimeString(), BarrierSize);
	PrintToConsole(stringToPrint);


	freeCranes = CraneSize;
	VessOut = VesselSize;
		

	strcpy(AnsReq, "TRUE");

	// now write amended string  to the pipe
	if (!WriteFile(WriteHandle, AnsReq, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "EilatPort: Error writing to pipe\n");
	

	if (initGlobalData() == False)
	{
		fprintf(stderr, "EilatPort: Error while initiating global data \n");
		return -1;
	}
	//read Vessels from the pipe until nummber VessIn equal to received Vesselsize

	while (VessIN <VesselSize)
	{
		// now have the child read from the pipe
		success = ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL);

		// we have to output to stderr as stdout is redirected to the pipe
		if (!success)
		{
			fprintf(stderr, "EilatPort: error reading from pipe\n");
			return -1;
		}
		
		
		sprintf(stringToPrint, "[%s] Vessel %s arrived @ Eilat Port \n", getTimeString(), buffer);
		PrintToConsole(stringToPrint);
		// creat Vessel threads with VesselId unique grather then 1.
		vessIdIn = atoi(buffer);
		VesselsID[vessIdIn-1] = vessIdIn;
		Vessels[vessIdIn-1] = CreateThread(NULL, 0, Vessel, &VesselsID[vessIdIn-1], 0, &ThreadID);
		VessIN++;
	

	}
	
	WaitForMultipleObjects(CraneSize,Cranes,TRUE,INFINITE);
	sprintf(stringToPrint, "[%s] Eilat Port: All Crane Threads are done \n", getTimeString());
	PrintToConsole(stringToPrint);

	WaitForMultipleObjects(VesselSize, Vessels, TRUE, INFINITE);
	sprintf(stringToPrint, "[%s] Eilat Port: All Vassel Threads are done \n", getTimeString());
	PrintToConsole(stringToPrint);

	sprintf(stringToPrint, "[%s] Eilat Port: Exiting... \n", getTimeString());
	PrintToConsole(stringToPrint);

	cleanupGlobalData();

	

	return 0;
}

//The Thread Function for each Crane, gets the Crane Unique ID (index of the Thread in Array of Threads within the main program)
//This Unique Thread ID (called carneID) is used by the Vessel who attached him to unload Vessel carriege
DWORD WINAPI Crane(PVOID Param)
{
	//Get the Unique ID from Param and keep it in a local variable
	int CraneID = *(int*)Param;

	char stringToPrint[STRING_SIZE];
	sprintf(stringToPrint, "[%s] crane - %d has been created \n", getTimeString(), CraneID);
	PrintToConsole(stringToPrint);
	

	for (int i = 0; i < BarrierSize; i++)
	{
		
		WaitForSingleObject(semADT, INFINITE);

		activeCrane++;

		Sleep(calcSleepTime());
		
		sprintf(stringToPrint, "[%s] vessel %d - unloaded  %d Tons at Crane %d\n", getTimeString(), CranesArr[CraneID-1], GemerateWeight(CraneID), CraneID);
		PrintToConsole(stringToPrint);

		Sleep(calcSleepTime());

		activeCrane--;

		if (!ReleaseSemaphore(semVessADT[CraneID-1], 1, NULL))
			fprintf(stderr, "EilatPort->Crane :: Unexpected error sem.V()\n");
	}
	WaitForSingleObject(semADT, INFINITE);
	sprintf(stringToPrint, "[%s] crane - %d done working \n", getTimeString(), CraneID);
	PrintToConsole(stringToPrint);

	return 0;
}

//The Thread Function for each Vessel, gets the Vessl Unique ID (index of the Thread in Array of Threads within the main program)
//This Unique Thread ID (called VessID) is used by the state DB to keep track of each Vessel state at any given time!
DWORD WINAPI Vessel(PVOID Param)
{
	//Get the Unique ID from Param and keep it in a local variable
	int VesselID = *(int*)Param;

	//srand((unsigned)time(NULL));
	Sleep(calcSleepTime());

	enterBarrier(VesselID);

	Sleep(calcSleepTime());

	enterUnloadingQuay(VesselID);

	Sleep(calcSleepTime());

	exitUnloadingQuay(VesselID);

	Sleep(calcSleepTime());
	
	CrossToHaifa(VesselID);

	Sleep(calcSleepTime());

	VessOut--;

	if (VessOut == 0)
	{
		if (!ReleaseSemaphore(semADT, CraneSize, NULL))
			printf("EilatPort.Vessel::Unexpected error mutex.V()\n");
	}

	return 0;
}

//function to control incoming Vessels,when the incoming Vessel is equal to the number of the Cranes at the Unloading Quay
//and Unloading Quay is free the Barrier is released
//used for syncronization point for the process
void enterBarrier(int VessID)
{
	char stringToPrint[STRING_SIZE];
	WaitForSingleObject(mutexBarrier, INFINITE);
	tempCountVess++;//how many threads entered so far,initialize to zero when the Barrier[QueueIn] is full
		
	if ((IsFree == True) && (QueueIN == QueueOut) && (tempCountVess == CraneSize))
	{
		QueueIN++;
		tempCountVess = 0;
		//mutexBarrier.V() - release mutex
		if (!ReleaseMutex(mutexBarrier))
			printf("Eilat :: Unexpected error mutexPipe.V()\n");
		exitBarrier();
	}
	else
	{
		if ((IsFree == True) && (QueueIN > QueueOut))
		{
			exitBarrier();
			if (tempCountVess == CraneSize)
			{
				QueueIN++;
				tempCountVess = 0;
				sprintf(stringToPrint, "[%s] Vessel %d - entering Barrier \n", getTimeString(), VessID);
				PrintToConsole(stringToPrint);
				//mutexBarrier.V() - release mutex
				if (!ReleaseMutex(mutexBarrier))
					printf("Eilat :: Unexpected error mutexPipe.V()\n");
				WaitForSingleObject(Barrier[QueueIN-1], INFINITE);

			}
			else
			{
				sprintf(stringToPrint, "[%s] Vessel %d - entering Barrier \n", getTimeString(), VessID);
				PrintToConsole(stringToPrint);
				//mutexBarrier.V() - release mutex
				if (!ReleaseMutex(mutexBarrier))
					printf("Eilat :: Unexpected error mutexPipe.V()\n");
				WaitForSingleObject(Barrier[QueueIN], INFINITE);
			}
		}
		else
		{
			if (tempCountVess == CraneSize)
			{
				QueueIN++;
				tempCountVess = 0;
				sprintf(stringToPrint, "[%s] Vessel %d - entering Barrier \n", getTimeString(), VessID);
				PrintToConsole(stringToPrint);
				//mutexBarrier.V() - release mutex
				if (!ReleaseMutex(mutexBarrier))
					printf("Eilat :: Unexpected error mutexPipe.V()\n");
				WaitForSingleObject(Barrier[QueueIN - 1], INFINITE);
			}
			else
			{
				sprintf(stringToPrint, "[%s] Vessel %d - entering Barrier \n", getTimeString(), VessID);
				PrintToConsole(stringToPrint);
				//mutexBarrier.V() - release mutex
				if (!ReleaseMutex(mutexBarrier))
					printf("Eilat :: Unexpected error mutexPipe.V()\n");
				WaitForSingleObject(Barrier[QueueIN], INFINITE);
			}
		}
	}
	sprintf(stringToPrint, "[%s] Vessel %d - exiting Barrier \n", getTimeString(), VessID);
	PrintToConsole(stringToPrint);
}

//reales the Barier when the Barrier is ready for release
void exitBarrier()
{
	char stringToPrint[STRING_SIZE];
	sprintf(stringToPrint, "[%s] Barrier Release \n", getTimeString());
	PrintToConsole(stringToPrint);
	if (!ReleaseSemaphore(Barrier[QueueOut], CraneSize, NULL))
		fprintf(stderr, "exitBarrier::Unexpected error Barrier[%d].V()\n", QueueOut);
	QueueOut++;
	IsFree = False;
}

//CrossToHaifa function for Vessels VessID 
void CrossToHaifa(int vessID)
{
	char stringToPrint[STRING_SIZE];
	//Access to state DB is done exclusively (protected by mutex)
	//mutexPipe.P()
	WaitForSingleObject(mutexPipe, INFINITE);
	sprintf(AnsReq, "%d", vessID);
	sprintf(stringToPrint, "[%s] Vessel %d entering Canal: Red Sea ==> Med. Sea \n", getTimeString(), vessID);
	PrintToConsole(stringToPrint);
	Sleep(calcSleepTime());

	/* the parent now wants to write to the pipe */
	if (!WriteFile(WriteHandle, AnsReq, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "Eilat ::Error writing to pipe\n");
	
	//mutexPipe.V() - release mutex
	if (!ReleaseMutex(mutexPipe))
		printf("Eilat :: Unexpected error mutexPipe.V()\n");

	
}

void enterUnloadingQuay(int VessID)
{
	int availCrain;
	char stringToPrint[STRING_SIZE];
	WaitForSingleObject(mutex, INFINITE);
	for (availCrain = 0; availCrain < CraneSize; availCrain++)
	{
		if (CranesArr[availCrain] == -1)
		{
			sprintf(stringToPrint, "[%s] Vessel %d -settles down at Crane %d  \n", getTimeString(), VessID, availCrain+1);
			PrintToConsole(stringToPrint);
			CranesArr[availCrain] = VessID;
			freeCranes--;
			break;
		}
	}
	// mutex.V() - release mutex
	if (!ReleaseMutex(mutex))
		printf("UnloadingQuay::Unexpected error mutex.V()\n");
	if (freeCranes == 0)//if there is no more free crane signal crane threads
	{
		if (!ReleaseSemaphore(semADT,CraneSize,NULL))
			printf("UnloadingQuay::Unexpected error mutex.V()\n");
	}
	WaitForSingleObject(semVessADT[availCrain], INFINITE);

}

void exitUnloadingQuay(int VessID)
{
	int availCrain;
	char stringToPrint[STRING_SIZE];
	WaitForSingleObject(mutex, INFINITE);
	for (availCrain = 0; availCrain < CraneSize; availCrain++)
	{
		if (CranesArr[availCrain] == VessID)
		{
			sprintf(stringToPrint, "[%s] Vessel %d -left Crane %d  \n", getTimeString(), VessID, availCrain+1);
			PrintToConsole(stringToPrint);
			CranesArr[availCrain] = -1;
			freeCranes++;
			break;
		}
	}
	// mutex.V() - release mutex
	if (!ReleaseMutex(mutex))
		printf("UnloadingQuay::Unexpected error mutex.V()\n");
	if (freeCranes == CraneSize)
	{
		IsFree = True;
		if ((VessIN == VesselSize) && (QueueIN>QueueOut)) 
		{
			exitBarrier();
		}
	}
}

//checks if num is a Prime number
int IsPrime(int num)
{
	int i;
	for (i = 2; i*i <= num; i++) {
		if (num % i == 0)
			return 0;
	}
	return 1;
}

//checks if num is a divisor of the limit number
int IsDivisor(int num, int limit)
{
	if (limit % num == 0 && limit != num)
		return 1;
	return 0;

}

//secure random number generator
int GenerateNumber(int limit,int* randNum)
{
	errno_t err;
	unsigned int tempRand;
	err = rand_s(randNum);
	if (err != 0)
	{
		printf_s("The rand_s function failed!\n");
	}
	tempRand = ((unsigned int)*randNum) % limit + 1;
	return tempRand;
}

//generate valid CraneSize
int CarinsValidNum(int limit, int min)
{
	int temp = GenerateNumber(limit,&mainRand);
	while (!IsDivisor(temp, limit) || temp < min)
	{
		temp = GenerateNumber(limit,&mainRand);
	}
	return temp;
}

// Generate weight for Crain thread when Vessel unload his carriege at the Unloading Quay
int GemerateWeight(int CranesID)
{
	int randWeight = 0;
	while (randWeight < 5)
	{
		randWeight = GenerateNumber(MAX_WEIGHT,&CranesRand[CranesID-1]);
	}
	return randWeight;
}

//generic function to randomise a Sleep time between MIN_SLEEP_TIME and MAX_SLEEP_TIME msec
int calcSleepTime()
{
	int randSleep = 0;
	while (randSleep < MIN_SLEEP_TIME)
	{
		randSleep = GenerateNumber(MAX_SLEEP_TIME,&mainRand);
	}
	return randSleep;
}

//safe console printing 
void PrintToConsole(char *string)
{
	WaitForSingleObject(mutexConsole, INFINITE);
	fprintf(stderr, "%s", string);
	if (!ReleaseMutex(mutexConsole))
		printf("PrintToConsole::Unexpected error mutex.V()\n");
}

//return const string of current locl time
const char* getTimeString()
{
	GetLocalTime(&lt);
	static char currentLocalTime[20];
	sprintf(currentLocalTime, "%02d:%02d:%02d", lt.wHour, lt.wMinute, lt.wSecond);
	return currentLocalTime;

}

//Initialise global Semaphores
//If all Successful - return True, otherwise (if problem arises) return False
//This is invoked before all Vessels Threads start running
int initGlobalData()
{
	int i;
	mutex = CreateMutex(NULL, FALSE, NULL);
	if (mutex == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error creating mutex\n");
		return False;
	}
	mutexPipe = CreateMutex(NULL, FALSE, NULL);
	if (mutexPipe == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error creating mutexPipe\n");
		return False;
	}
	mutexConsole = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("ConsoleMutex"));
	if (mutexConsole == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error creating mutexConsole\n");
		return False;
	}
	mutexBarrier = CreateMutex(NULL, FALSE, NULL);
	if (mutexBarrier == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error creating nutexBarrier\n");
		return False;
	}
	semADT = CreateSemaphore(NULL,0,CraneSize, NULL);
	if (semADT == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error creating semADT\n");
		return False;
	}
	semVessADT = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (CraneSize * sizeof(HANDLE)));
	if (semVessADT == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error creating semADT\n");
		return False;
	}
	Vessels = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (VesselSize*sizeof(HANDLE)));
	if (Vessels == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating Vessels\n");
		return False;
	}
	Barrier = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (BarrierSize * sizeof(HANDLE)));
	if (Barrier == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating Barrier\n");
		return False;
	}
	Cranes = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (CraneSize * sizeof(HANDLE)));
	if (Cranes == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating Cranes\n");
		return False;
	}
	CranesID = malloc(CraneSize * sizeof(int));
	if (CranesID == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating CranesID\n");
		return False;
	}
	CranesRand = malloc(CraneSize * sizeof(unsigned int));
	if (CranesRand == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating CranesRand\n");
		return False;
	}
	VesselsID = malloc(VesselSize * sizeof(int));
	if (VesselsID == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating VesselsID\n");
		return False;
	}
	CranesArr = malloc(CraneSize * sizeof(int));
	if (CranesArr == NULL)
	{
		fprintf(stderr, "initGlobalData:: Error allocating CranesArr\n");
		return False;
	}
	for (i = 0; i < BarrierSize; i++)
	{
		Barrier[i] = CreateSemaphore(NULL, 0, CraneSize, NULL);
		if (Barrier[i] == NULL)
		{
			return False;
		}
	}
	for (i = 0; i < CraneSize; i++)
	{
		CranesID[i] = i+1;
		CranesArr[i] = -1;
		CranesRand[i] = 0;
		Cranes[i] = CreateThread(NULL, 0, Crane, &CranesID[i], 0, &ThreadID);
		semVessADT[i] = CreateSemaphore(NULL, 0, 1, NULL);
	}

	return True;

}

//Close all global semaphore handlers and free dynamic allocations - after all Vessels Threads finish.
void cleanupGlobalData()
{
	int i;
	CloseHandle(mutexPipe);
	CloseHandle(semADT);
	CloseHandle(mutex);
	CloseHandle(mutexBarrier);

	for (int i = 0; i < VesselSize; i++)
	{
		CloseHandle(Vessels[i]);
	}
	for (i = 0; i < BarrierSize; i++)
	{
		CloseHandle(Barrier[i]);
	}
	for (i = 0; i < CraneSize; i++)
	{
		CloseHandle(Cranes[i]);
		CloseHandle(semVessADT[i]);
		
	}
	
	free(CranesID);
	free(CranesArr);
	free(VesselsID);
	free(CranesRand);
	HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, Barrier);
	HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, Vessels);
	HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, Cranes);
	HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, semVessADT);
	
}
