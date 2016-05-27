/*
Belachew Haile-Mariam
CLIENT CODE
*/

#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <Windows.h>
#include <WinSock.h>
#include <time.h>
#include <conio.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

#define ERR_CODE_NONE	0	/* no error */
#define ERR_CODE_SWI	1	/* software error */
#define CMD_LENGTH		5
#define ARG_NONE		1
#define ARG_NUMBER		2

#define BEGIN	"BEGIN"
#define START	"START"
#define STOP	"STOP"

double result[300];
char signalArray[5000] = {};

double average=0;
double variance=0;
double maxValue=0;
double minValue=0;
double sum=0;

 typedef struct sp_comm
 {
	WSADATA wsaData;
	SOCKET cmdrecvsock;
	SOCKET cmdstatusock;
	SOCKET datasock;
	struct sockaddr_in server;
} * sp_comm_t; 

 typedef struct sp_flags
 {
	unsigned int start_system:1;
	unsigned int pause_system:1;
	unsigned int shutdown_system:1;
	unsigned int analysis_started:1;
	unsigned int restart:1;
	unsigned int transmit_data:1;
} * sp_flags_t;

 typedef struct sp_struct
 {
	struct sp_comm		comm;
	struct sp_flags		flags;	
} * sp_struct_t;

typedef struct
{
	char cmd[CMD_LENGTH];
	int arg;	
} cmd_struct_t;

char OPERATOR_CMD[110] = { 0 };

WSADATA wsaData;
int x = 1;

/* Thread to interface with the ProfileClient */
HANDLE hClientThread; 
DWORD dwClientThreadID;
VOID client_iface_thread(LPVOID parameters);

int main()
{
	struct sp_struct profiler; 
	struct sockaddr_in saddr;
	struct hostent *hp;
	int res = 0;
	char ParamBuffer[110];
	char inputChar[110] = "";

	memset(&profiler, 0, sizeof(profiler));
	sp_comm_t comm = &profiler.comm;  

	if((res = WSAStartup(0x202,&wsaData)) != 0)
	{
		fprintf(stderr,"WSAStartup failed with error %d\n",res);
		WSACleanup();
		return(ERR_CODE_SWI);
	}
	else
	{
		printf("Rx CLIENT instance started\n\n");
	}

	/**********************************************************************************
	 * Setup data transmition socket to broadcast data
	 **********************************************************************************/

	hp = (struct hostent*)malloc(sizeof(struct hostent));
	hp->h_name = (char*)malloc(sizeof(char)*17);
	hp->h_addr_list = (char**)malloc(sizeof(char*)*2);
	hp->h_addr_list[0] = (char*)malloc(sizeof(char)*5);
	strcpy(hp->h_name, "lab_example\0");
	hp->h_addrtype = 2;
	hp->h_length = 4;

	//broadcast in 255.255.255.255 network
	//loopback IP address 127.0.0.1
	hp->h_addr_list[0][0] = (signed char)127;//192;
	hp->h_addr_list[0][1] = (signed char)0; //168;
	hp->h_addr_list[0][2] = (signed char)0; //0;  
	hp->h_addr_list[0][3] = (signed char)1; //140;
	hp->h_addr_list[0][4] = 0;

	/**********************************************************************************
	 * Setup a socket for broadcasting data ON PORT 1500
	 **********************************************************************************/
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;
	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1500);
		
	if((comm->datasock = socket(AF_INET,SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
	    fprintf(stderr,"socket(datasock) failed: %d\n",WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_NONE);
	}

	if(connect(comm->datasock,(struct sockaddr*)&saddr,sizeof(saddr)) == SOCKET_ERROR)
	{
	    fprintf(stderr,"connect(datasock) failed: %d\n",WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_SWI);
	}

	/**********************************************************************************
	 * Setup and bind a socket to listen for commands from client ON PORT 1024
	 **********************************************************************************/
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY; 
	saddr.sin_port = htons(1024);	
	if((comm->cmdrecvsock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
	    fprintf(stderr,"socket(cmdrecvsock) failed: %d\n",WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_NONE);
	}	

	if(bind(comm->cmdrecvsock,(struct sockaddr*)&saddr,sizeof(saddr) ) == SOCKET_ERROR)
	{
		fprintf(stderr,"bind() failed: %d\n",WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_NONE);
	}

	hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) client_iface_thread, (LPVOID)&profiler, 0, &dwClientThreadID); 
	SetThreadPriority(hClientThread, THREAD_PRIORITY_LOWEST);
	
	//read in sample rate from user and send it to the server
	char sampleRate[50];
	printf("\nPleae enter the sample rate (in hz): ");
	try
	{
		std::cin >> sampleRate;
	}
	catch(...)
	{
		printf("\nAn error occured reading in the sample rate");
	}
	send(comm->datasock, "SR", sizeof("SR"), 0);
	send(comm->datasock, sampleRate, sizeof(sampleRate), 0);
	printf("\nSample Rate Sent to Server!");

	//wait for sample rate response
	Sleep(1000);

	//read in coeffecients file name
	char coefFileName[100];
	printf("\nPlease enter the file name for the coeffecients: ");
	try
	{
		std::cin >> coefFileName;
	}
	catch(...)
	{
		printf("\nAn error occured reading in the file name");
	}

	//read in data from file
	std::ifstream in_file;
	std::string str;
	in_file.open(coefFileName);
	double f[101] = {0.0};
	for(int i = 0; i < 101; i++)
	{		
		std::getline(in_file, str);
		char *dup = strdup(str.c_str());

		char *leftSide = strtok(dup, "e");
		char *rightSide = strtok(NULL,"/n");

		f[i] = atof(leftSide)*pow(10.0, atof(rightSide));

		free(dup);
	}
	in_file.close();
	
	//send the coefficients to the server in a loop
	for(int i=0; i<101; i++)
	{
		char temp[50];
		sprintf(temp, "%.15f", f[i]);
		send(comm->datasock, temp, sizeof(temp), 0);
	}
	//all coefficients sent
	send(comm->datasock, "ACS", sizeof("ACS"), 0);
	printf("\nAll Coefficients Sent to Server!");

	//wait for coefficient recieved response
	Sleep(1000);

	//wait for begin
	printf("\nEnter 'BEGIN' to start the program on the server: ");
	while(x == 1 )
	{
		if(_kbhit())
		{
			scanf("%s", OPERATOR_CMD);
			if (!strcmp(OPERATOR_CMD, BEGIN) || !strcmp(OPERATOR_CMD, STOP))
			{
				if(!strcmp(OPERATOR_CMD, BEGIN))
				{
					strcpy(OPERATOR_CMD, START);
				}
				else
				{
					strcpy(OPERATOR_CMD, STOP);
				}
				strcpy_s(inputChar, OPERATOR_CMD);
				send(comm->datasock, inputChar, sizeof(inputChar), 0);
			}
		}		
	}
	system("pause");
	return 0 ; 
}

/************************************************************************************
 * VOID client_iface_thread(LPVOID)
 *
 * Description: Thread communicating commands from client and the status of their 
 *				completion back to client. 
 * 
 *
 ************************************************************************************/
VOID client_iface_thread(LPVOID parameters) //LPVOID parameters)
{
	sp_struct_t profiler = (sp_struct_t)parameters;
	sp_comm_t comm = &profiler->comm; 
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	char ParamBuffer[3000] = {0} ;

	std::ofstream myfile;
	myfile.open("results.csv");
	//myfile.clear();

	//printf("Executing Thread\n");
	//printf("Checking for Data\n");
	while(ParamBuffer[0] != '!') 
	{
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len =sizeof(saddr);		
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);   	
		
		if( !strcmp(ParamBuffer, STOP) )
		{
			x = 0;
			std::ofstream dataOutFile;
			dataOutFile.open("dataOutput.txt");
			dataOutFile << "average: ";
			dataOutFile << average;
			dataOutFile << "\n";
			dataOutFile << "variance: ";
			dataOutFile << variance;
			dataOutFile << "\n";
			dataOutFile << "maxValue: ";
			dataOutFile << maxValue;
			dataOutFile << "\n";
			dataOutFile << "minValue: ";
			dataOutFile << minValue;
			dataOutFile << "\n";
			dataOutFile << "sum: ";
			dataOutFile << sum;
			dataOutFile << "\n";
			dataOutFile.close();
			printf("\n\nProgram Ended.\n");
			break;
		}
		else if( !strcmp(ParamBuffer, "PROCESSING") )
		{
			printf("\nProcessing...\n");
		}
		else if( !strcmp(ParamBuffer, "SRS") )
		{
			printf("\nSample Rate Received by Server!");
		}
		else if( !strcmp(ParamBuffer, "ACR") )
		{
			printf("\nAll Coefficients Received by Server!");
		}
		else if( !strcmp(ParamBuffer, "SUM") )
		{
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			printf("Sum: %s ", ParamBuffer);
			sum = atof(ParamBuffer);
		}
		else if( !strcmp(ParamBuffer, "AVERAGE") )
		{
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			printf("Average: %s ", ParamBuffer);
			average = atof(ParamBuffer);
		}
		else if( !strcmp(ParamBuffer, "VARIANCE") )
		{
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			printf("Variance: %s ", ParamBuffer);
			variance = atof(ParamBuffer);
		}
		else if( !strcmp(ParamBuffer, "MAX") )
		{
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			printf("Max: %s ", ParamBuffer);
			maxValue = atof(ParamBuffer);
		}
		else if( !strcmp(ParamBuffer, "MIN") )
		{
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			printf("Min: %s ", ParamBuffer);
			minValue = atof(ParamBuffer);
		}
		else if( !strcmp(ParamBuffer, "RESULT") )
		{
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			sprintf_s(signalArray,ParamBuffer);
			
			//save the array to file
			myfile << signalArray;
		}
		printf("\n");
	}
	myfile.close();
}