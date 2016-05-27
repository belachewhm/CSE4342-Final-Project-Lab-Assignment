/*
Belachew Haile-Mariam
SERVER CODE w/ Continuous ADC
*/

//Include Files
#include <windows.h>
#include <WinSock.h>
#include <stdio.h>
#include <conio.h>
#include "oldaapi.h"
#include <olmem.h>         
#include <olerrors.h>   
#include <string>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <time.h>
#include <fstream>
//#include "stdafx.h"

#pragma comment(lib, "ws2_32.lib")

//Definitions for the Server
#define ERR_CODE_NONE	0	/* no error */
#define ERR_CODE_SWI	1	/* software error */
#define CMD_LENGTH		5
#define ARG_NONE		1
#define ARG_NUMBER		2

#define TRUE	1
#define FALSE	0
#define START	"START"
#define STOP	"STOP"
#define _5VOLTS	5

//Definitions for the ADC
#define NUM_OL_BUFFERS 4
#define STRLEN 200        /* string size for general text manipulation   */
char str[STRLEN];        /* global string for general text manipulation */
char str2[STRLEN];

double coefArray[101];
double currentsignal[300];
double oldSignal[300];
bool signalFlag = true;
double result[400];
int sampleRate=2000;
double average=0;
double variance=0;
double maxValue=0;
double minValue=0;
int count=0;
double sum=0;
bool sendFlag =true;
//simple structure used with board for ADC
typedef struct tag_board
{
   HDEV  hdrvr;        /* device handle            */
   HDASS hdass;        /* sub system handle        */
   ECODE status;       /* board error status       */
   char name[MAX_BOARD_NAME_LENGTH];  /* string for board name    */
   char entry[MAX_BOARD_NAME_LENGTH]; /* string for board name    */
} BOARD;
typedef BOARD* LPBOARD;
static BOARD board;

//Error handling macros for ADC
#define CHECKERROR(ecode)								\
do														\
{														\
   ECODE olStatus;										\
   if( OLSUCCESS != ( olStatus = ( ecode ) ) )			\
   {													\
      printf("OpenLayers Error %d\n", olStatus );		\
      exit(1);											\
   }													\
}														\
while(0)												\

#define SHOW_ERROR(ecode)								\
MessageBox												\
(														\
	HWND_DESKTOP,										\
	olDaGetErrorString(ecode,str,STRLEN),				\
	"Error",											\
	MB_ICONEXCLAMATION | MB_OK							\
);														\

#define CLOSEONERROR(ecode)								\
if( (board.status = (ecode)) != OLNOERROR )				\
{														\
	SHOW_ERROR(board.status);							\
	olDaReleaseDASS(board.hdass);						\
	olDaTerminate(board.hdrvr);							\
	return (TRUE);										\
}														\

//Structs for the Server
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

BOOL CALLBACK GetDriver( LPSTR lpszName, LPSTR lpszEntry, LPARAM lParam )   
{
   LPBOARD lpboard = (LPBOARD)(LPVOID)lParam;
   
   /* fill in board strings */
	#ifdef WIN32
	   strncpy(lpboard->name,lpszName,MAX_BOARD_NAME_LENGTH-1);
	   strncpy(lpboard->entry,lpszEntry,MAX_BOARD_NAME_LENGTH-1);
	#else
	   lstrcpyn(lpboard->name,lpszName,MAX_BOARD_NAME_LENGTH-1);
	   lstrcpyn(lpboard->entry,lpszEntry,MAX_BOARD_NAME_LENGTH-1);
	#endif

   /* try to open board */
   lpboard->status = olDaInitialize(lpszName,&lpboard->hdrvr);
   if   (lpboard->hdrvr != NULL)
      return FALSE;          /* false to stop enumerating */
   else                      
      return TRUE;           /* true to continue          */
}

//Varibales for the Server
WSADATA wsaData;
double Analog0Value = 0.0;
double Analog1Value = 0.0;
int DigitalInput2 = 0;
char CLIENT_CMD[110] = { 0 };

//Thread for the Server to interface with the ProfileClient
HANDLE hClientThread; 
DWORD dwClientThreadID;

//Function Definitions
VOID client_iface_thread(LPVOID parameters);
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM hAD, LPARAM lParam );
BOOL CALLBACK EnumBrdProc( LPSTR lpszBrdName, LPSTR lpszDriverName, LPARAM lParam );
void conv(double signal[300]);

int digitalOutput(UINT channel)
{
	long value;
   UINT resolution;
   DBL gain = 1.0;

   board.hdrvr = NULL;
   olDaEnumBoards(GetDriver,(LPARAM)(LPBOARD)&board);
   olDaGetDASS(board.hdrvr,OLSS_DOUT,0,&board.hdass);
   olDaSetDataFlow(board.hdass,OL_DF_SINGLEVALUE);
   olDaConfig(board.hdass);
   value = 0;
   olDaPutSingleValue(board.hdass,value,channel,gain);
   olDaGetResolution(board.hdass,&resolution);
   value = (1L<<resolution)-1;
   olDaPutSingleValue(board.hdass,value,channel,gain);
   olDaReleaseDASS(board.hdass);
   return 0;

}

//Main
int main()
{
	//initialize Server Variables
	struct sp_struct profiler; 
	struct sockaddr_in saddr;
	struct hostent *hp;
	int res = 0;
	//char ParamBuffer[110];
	char inputChar[100] = "";
	//str.clear();
	
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
		printf("Tx SERVER instance started\n");
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
	 * Setup a socket for broadcasting data ON PORT 1024
	 **********************************************************************************/
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;
	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1024);
		
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
	 * Setup and bind a socket to listen for commands from client ON PORT 1500
	 **********************************************************************************/
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY; 
	saddr.sin_port = htons(1500);	
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

	//create recieve thread
	hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) client_iface_thread, (LPVOID)&profiler, 0, &dwClientThreadID); 
	SetThreadPriority(hClientThread, THREAD_PRIORITY_LOWEST);

	// initialize hardware on server
    // create a window for messages
    WNDCLASS wc;
    memset( &wc, 0, sizeof(wc) );
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = "DtConsoleClass";
    RegisterClass( &wc );
    HWND hWnd = CreateWindow( wc.lpszClassName, NULL, NULL, 0, 0, 0, 0,NULL,NULL,NULL, NULL );

    if( !hWnd )
	{
       exit( 1 );
	}
	else
	{
		printf( "Open Layers Continuous A/D Win32 Console Example\n" );
	}

	//initialize board settings for ADC
	board.hdrvr = NULL;
    CHECKERROR( olDaEnumBoards( EnumBrdProc, (LPARAM)&board ) );
	board.hdass =NULL;
    CHECKERROR( olDaGetDASS( board.hdrvr, OLSS_AD, 0, &board.hdass) ); 
    CHECKERROR( olDaSetWndHandle( board.hdass, hWnd, 0 ) ); 
    CHECKERROR( olDaSetDataFlow( board.hdass, OL_DF_CONTINUOUS ) );

	//2 channels
    CHECKERROR( olDaSetChannelListSize( board.hdass, 2 ) );

	//define the channels
    CHECKERROR( olDaSetChannelListEntry( board.hdass, 0, 0 ) );
	CHECKERROR( olDaSetChannelListEntry( board.hdass, 1, 1 ) );

    CHECKERROR( olDaSetGainListEntry( board.hdass, 0, 1 ) );
    CHECKERROR( olDaSetTrigger( board.hdass, OL_TRG_SOFT ) );
    CHECKERROR( olDaSetClockSource( board.hdass, OL_CLK_INTERNAL ) ); 
    CHECKERROR( olDaSetClockFrequency( board.hdass, sampleRate) );
    CHECKERROR( olDaSetWrapMode( board.hdass, OL_WRP_NONE ) );
    CHECKERROR( olDaConfig( board.hdass ) );

    HBUF hBufs[NUM_OL_BUFFERS];
    for( int i=0; i < NUM_OL_BUFFERS; i++ )
    {
        if( OLSUCCESS != olDmAllocBuffer( GHND, 400, &hBufs[i] ) )
        {
           for ( i--; i>=0; i-- )
           {
              olDmFreeBuffer( hBufs[i] );
           }   
           exit( 1 );   
        }
        olDaPutBuffer( board.hdass,hBufs[i] );
    }

	//Set LED's to (0,0)

	while(true)
	{
		if( !strcmp(CLIENT_CMD, START) )
		{
			if( OLSUCCESS != ( olDaStart( board.hdass ) ) )
			{
			   printf( "A/D Operation Start Failed\n" );
			}
			else
			{
			   printf( "A/D Operation Started\n\n");
			}
			break;
		}
	}

	// Increase the our message queue size so we don't lose any data acq messages
    MSG msg;                                    
    SetMessageQueue( 50 );      

    bool processing = false;
	while( GetMessage(&msg,hWnd,0,0))
	{
		TranslateMessage( &msg );    // Translates virtual key codes       
		DispatchMessage( &msg );     // Dispatches message to window 

		if(!strcmp(CLIENT_CMD, START))
		{
			Analog0Value = atof(str);
			Analog1Value = atof(str2);
			if(!processing)
			{
				if( Analog1Value < 4.00 )
				{
					printf("Waiting on Analog Input 1 to be Assereted (@ 5V), Curent Val =  %fV\n", Analog1Value);
				}
				else
				{
					printf("\nAnalog Input 1 has been asserted to 5V");

					//digitalOutput(1);

					send(comm->datasock, "PROCESSING", sizeof("PROCESSING"), 0);
					processing = true;
				}
			}
			else
			{
				//printf("\nAnalog Signal = %f", Analog0Value);

				if(sendFlag)
				{
					for(int i = 0; i < 300; i++)
					{
						sum = sum + result[i];
					}

					average = (average+(sum/300))/2;

					variance = ((pow(sum, 2) - pow(sum, 2)/300)/300);

					for(int i = 0; i < 300; i++)
					{
						if(result[i] > maxValue)
						{
							maxValue = result[i];
						}
						if(result[i] < minValue)
						{
							minValue = result[i];
						}
					}
				}
				else
				{
					for(int i = 100; i < 300; i++)
					{
						sum = sum + result[i];
					}

					average = (average+(sum/200))/2;

					variance = ((pow(sum, 2) - pow(sum, 2)/200)/200);

					for(int i = 100; i < 300; i++)
					{
						if(result[i] > maxValue)
						{
							maxValue = result[i];
						}
						if(result[i] < minValue)
						{
							minValue = result[i];
						}
					}
				}

				//send sum
				send(comm->datasock, "SUM", sizeof("SUM"), 0);
				sprintf_s(str,"%f",sum);
				send(comm->datasock, str, sizeof(str), 0);

				//send average
				send(comm->datasock, "AVERAGE", sizeof("AVERAGE"), 0);
				sprintf_s(str,"%f",average);
				send(comm->datasock, str, sizeof(str), 0);

				//send variance
				send(comm->datasock, "VARIANCE", sizeof("VARIANCE"), 0);
				sprintf_s(str,"%f",variance);
				send(comm->datasock, str, sizeof(str), 0);

				//send max
				send(comm->datasock, "MAX", sizeof("MAX"), 0);
				sprintf_s(str,"%f",maxValue);
				send(comm->datasock, str, sizeof(str), 0);

				//send min
				send(comm->datasock, "MIN", sizeof("MIN"), 0);
				sprintf_s(str,"%f",minValue);
				send(comm->datasock, str, sizeof(str), 0);

				//send signal
				char signalArray[30000] = {0};
				
				if(sendFlag)
				{
					for(int i = 0; i < 300; i++)
					{
						char temp[1000] = {};
						sprintf(temp, "%.5f\n", result[i]);
						std::strcat(signalArray,temp);
					}
					sendFlag=false;
				}
				else
				{
					for(int i = 100; i < 300; i++)
					{
						char temp[1000] = {};
						sprintf(temp, "%.5f\n", result[i]);
						std::strcat(signalArray,temp);
					}
				}
				send(comm->datasock, "RESULT", sizeof("RESULT"), 0);
				send(comm->datasock, signalArray, sizeof(signalArray), 0);

				if( Analog1Value < 4.00 )
				{
					processing = false;
				}
			}
		}
		else if(!strcmp(CLIENT_CMD, STOP))
		{
			//Set LED's to (0)
			break;
		}
	}

	//kill everything
    olDaTerminate( board.hdrvr );

	printf("\n\nProgram Ended.\n");
	system("pause");
    exit( 0 );
}





VOID client_iface_thread(LPVOID parameters) //LPVOID parameters)
{
	sp_struct_t profiler = (sp_struct_t)parameters;
	sp_comm_t comm = &profiler->comm;
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	char ParamBuffer[110] = { 0 };


	printf("Executing SERVER Thread\n");
	printf("Checking for Data\n");

	int i=0;
	while(1)
	{
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);

		if( !strcmp(ParamBuffer, START) )
		{
			strcpy_s(CLIENT_CMD, START);
			printf("The received value is: %s \n", ParamBuffer);
		}
		else if( !strcmp(ParamBuffer, STOP) )
		{
			//send stop back to the client
			send(comm->datasock, STOP, sizeof(STOP), 0);

			//set the CLIENT_CMD variable to STOP.  Will be read as this in the main thread.
			strcpy_s(CLIENT_CMD, STOP);
		}
		else if(!strcmp(ParamBuffer, "SR"))
		{
			//save the sampleRate
			retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
			
			try
			{
				sampleRate = atoi(ParamBuffer);
				printf("\nSample Rate Received from Client!");
				send(comm->datasock, "SRS", sizeof("SRS"), 0);
			}
			catch(...)
			{
				printf("\nAn error occured saving the sample rate");
			}
		}
		else if(!strcmp(ParamBuffer, "ACS"))
		{
			printf("\nAll Coefficients Received from Client!");
			send(comm->datasock, "ACR", sizeof("ACR"), 0);
		}
		else
		{
			//save the paramBuffer into the coef Array
			try
			{
				coefArray[i]= atof(ParamBuffer);
			}
			catch(...)
			{
				printf("\nAn error occured saving position %d the coefArray", i);
			}
			i++;
		}
	}
}

//convolution function
void convolve(double sig[300])
{
	for(int i = 0; i < 400; i++)
 	{
		result[i]=0;
		for(int j = 0; j < 101; j++)
		{
			if(i-j>0)
			{
				result[i] = result[i]+(coefArray[j]*sig[i-j]);
			}
		}

	}
}

/*
void convolve(double sig[200])
{
	size_t n;

	for (n = 0; n < 200 + 101 - 1; n++)
	{
		size_t kmin, kmax, k;

		result[n] = 0;

		kmin = (n >= 101 - 1) ? n - (101 - 1) : 0;
		kmax = (n < 200 - 1) ? n : 200 - 1;

		for(k = kmin; k <= kmax; k++)
		{
			result[n] += sig[k] * coefArray[n - k];
		}
	}
}
*/
/*
void convolve(double sig[300]){

for ( int i = 0; i < 300; i++ )
{
    result[i] = 0;                       // set to zero before sum
    for (int j = 0; j < 100; j++ )
    {
		if(i-j>0){
        result[i] += sig[i - j] * coefArray[j];    // convolve: multiply and accumulate
		}
}
}
}*/



BOOL CALLBACK EnumBrdProc( LPSTR lpszBrdName, LPSTR lpszDriverName, LPARAM lParam )
{
   // Make sure we can Init Board
   if( OLSUCCESS != ( olDaInitialize( lpszBrdName, (LPHDEV)lParam ) ) )
   {
      return TRUE;  // try again
   }

   // Make sure Board has an A/D Subsystem 
   UINT uiCap = 0;
   olDaGetDevCaps ( *((LPHDEV)lParam), OLDC_ADELEMENTS, &uiCap );
   if( uiCap < 1 )
   {
      return TRUE;  // try again
   }

   printf( "%s succesfully initialized.\n", lpszBrdName );
   return FALSE;    // all set , board handle in lParam
}

LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM hAD, LPARAM lParam )
{
	///////////////////////////////////////////////////////////////
	DBL min=0,max=0;
	ULNG samples;
	UINT encoding=0;
	UINT resolution=0;
	HBUF  hBuffer = NULL;
	PDWORD  pBuffer32 = NULL;
	PWORD  pBuffer = NULL;
	//char WindowTitle[128];
	//char temp[128];    
	//DASSINFO ssinfo;

	ULNG value_1;
	ULNG value_2;
	ULNG bufferValue;

	DBL volts_1;
	DBL volts_2;
	DBL bufferVolts;

	////////////////////////////////////////////////////////////////
	switch( msg )
	{
		case OLDA_WM_BUFFER_DONE:
			CHECKERROR (olDaGetBuffer(board.hdass, &hBuffer));
			if( hBuffer )
			{
				/* get sub system information for code/volts conversion */
				CLOSEONERROR (olDaGetRange(board.hdass,&max,&min));
				CLOSEONERROR (olDaGetEncoding(board.hdass,&encoding));
				CLOSEONERROR (olDaGetResolution(board.hdass,&resolution));
				
				if(!strcmp(CLIENT_CMD, START))
				{
					/* get max samples in input buffer */
					CLOSEONERROR (olDmGetValidSamples( hBuffer, &samples ) );
				
					/* get pointer to the buffer */
					if (resolution > 16)
					{
						CLOSEONERROR (olDmGetBufferPtr( hBuffer,(LPVOID*)&pBuffer32));
						
						/* get last sample in buffer */
						value_1 = pBuffer32[samples-2];
						value_2 = pBuffer32[samples-1];
					}
					else
					{
						CLOSEONERROR (olDmGetBufferPtr( hBuffer,(LPVOID*)&pBuffer));
				
						/* get last sample in buffer */
						value_1 = pBuffer[samples-2];
						value_2 = pBuffer[samples-1];
					}
				
					/* put buffer back to ready list */
					CHECKERROR (olDaPutBuffer(board.hdass, hBuffer));
				
					/*  convert value to volts */
					if (encoding != OL_ENC_BINARY) 
					{
						/* convert to offset binary by inverting the sign bit */
						value_1 ^= 1L << (resolution-1);
						value_1 &= (1L << resolution) - 1;     /* zero upper bits */

						/* convert to offset binary by inverting the sign bit */
						value_2 ^= 1L << (resolution-1);
						value_2 &= (1L << resolution) - 1;     /* zero upper bits */
					}
				
					volts_1 = ((float)max-(float)min)/(1L<<resolution)*value_1 + (float)min;
					volts_2 = ((float)max-(float)min)/(1L<<resolution)*value_2 + (float)min;
					
					//printf("%f\n", volts_1);

					/* copy the value to the variable */
					sprintf_s(str,"%f",volts_1);
					sprintf_s(str2,"%f",volts_2);


					std::ofstream myfile;
					myfile.open("bufferVolts.csv", std::ios::app);
					std::ofstream convFile;
					convFile.open("convFile.csv", std::ios::app);
					//myfile.clear();

					//FILE *myfile;

					//myfile = fopen("bufferVolts.csv", "a+");
					int bufferCount = 0;
					for(int i =0; i<100;i++)
					{
						currentsignal[i] = 0;
					}
					for(int i = 0; i < 400; i += 2)
					{	
						bufferValue = pBuffer[i];
						bufferVolts = ((float)max-(float)min)/(1L<<resolution)*bufferValue + (float)min;

						myfile << bufferVolts;
						myfile << "\n";

						currentsignal[bufferCount+100] = bufferVolts;

						bufferCount++;
					}

					//fclose(myfile);
					myfile.close();

					if(signalFlag)
					{
						convolve(currentsignal);

						for(int i = 0; i<300;i++){
							convFile << result[i];
							convFile << "\n";
						}
						for(int i = 0; i<100;i++)
						{
							oldSignal[i] = currentsignal[i+200];
						}

						signalFlag = false;
					}
					else
					{
						for(int i =0; i<200;i++)
						{
							oldSignal[i+100] = currentsignal[i+100];
						}
				

						convolve(oldSignal);

						for(int i = 100; i<300;i++){
							convFile << result[i];
							convFile << "\n";
						}

						for(int i = 0; i<100;i++){
							oldSignal[i]=oldSignal[i+200];
						}
					}
	
					count++;
				}
			}
			return (TRUE);   /* Did process a message */

       case OLDA_WM_QUEUE_DONE:
		   printf( "\nAcquisition stopped, rate too fast for current options." );
		   PostQuitMessage(0);
		   system("pause");
		   break;

       case OLDA_WM_TRIGGER_ERROR:
		   printf( "\nTrigger error: acquisition stopped." );
		   PostQuitMessage(0);
		   system("pause");
		   break;

       case OLDA_WM_OVERRUN_ERROR:
		   printf( "\nInput overrun error: acquisition stopped." );
		   PostQuitMessage(0);
		   system("pause");
		   break;

       default: 
          return DefWindowProc( hWnd, msg, hAD, lParam );
    }
    
    return 0;
}