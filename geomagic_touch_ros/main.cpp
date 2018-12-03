#ifdef  _WIN64
#pragma warning (disable:4996)
#endif

#include <iostream>
#include <sstream>
#include <assert.h>

#include <WinSock2.h>	//do not put behind <Windows.h>

#if defined(WIN32)
# include <windows.h>
# include <conio.h>
#else
#include <time.h>
# include "conio.h"
# include <string.h>
#define FALSE 0
#define TRUE 1
#endif

#include <HD/hd.h>
#include <HDU/hduError.h>
#include <HDU/hduVector.h>

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define DEVICE_NAME "phantom1"
//#define SERVER "127.0.0.1"  //ip address of udp server
//#define SERVER "192.168.205.128"  //ip address of udp server
#define SERVER "192.168.50.132"
#define BUFLEN 1024  //Max length of buffer
#define PORT 1234   //The port on which to listen for incoming data

using namespace std;

//global
double current_pos[3];
double current_joint_angle[3];
HDdouble force_device[3];
HHD device_id = hdInitDevice( DEVICE_NAME );


//function
HDCallbackCode HDCALLBACK GetInfoCallback( void *data );
HDCallbackCode HDCALLBACK ForceOutputCallback(void *data);
void ssToChar(char* dst, stringstream* src);


int main(int argc, char* argv[])
{
	Sleep(500);

	char sendtoBuf[BUFLEN];
	char recvBuf[BUFLEN];
	stringstream ss;
	
	//----------socket parameter----------
	struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
	
    WSADATA wsa;

    //Initialise winsock
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialised.\n");
    
    //create socket
    if ( (s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
    {
        printf("socket() failed with error code : %d" , WSAGetLastError());
        exit(EXIT_FAILURE);
    }
     
    //setup address structure
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);
     
	//----------omni----------
	HDErrorInfo error;

	if (HD_DEVICE_ERROR(error = hdGetError()))
	{
		hduPrintError(stderr,&error,"Failed to initialize first haptic device");
		fprintf(stderr,"Make sure the configuration\"%s\"exists\n",DEVICE_NAME);
		fprintf(stderr,"\nPress any key to quit.\n");
		getchar();
		exit(-1);
	}
	//set omni & scheduler
	//device_id = hdInitDevice( DEVICE_NAME );
	
	hdEnable(HD_FORCE_OUTPUT);
	hdStartScheduler();
	//HDSchedulerHandle Scheduler_GetInfo = hdScheduleAsynchronous( GetInfoCallback, 0, HD_MAX_SCHEDULER_PRIORITY );
	HDSchedulerHandle Scheduler_ForceOutput = hdScheduleAsynchronous( ForceOutputCallback, (void*) 0, HD_MAX_SCHEDULER_PRIORITY );		
	

	//	use sendto() before using recvfrom() to implicitly bind
	for (int i = 0; i < 6; i++)
		ss << "0 ";

	ssToChar(sendtoBuf, &ss);

	if (sendto(s, sendtoBuf, BUFLEN, 0, (struct sockaddr *) &si_other, slen) == SOCKET_ERROR)
	{
		printf("1sendto() failed with error code : %d", WSAGetLastError());
		system("PAUSE");
		exit(EXIT_FAILURE);
	}
	
	while( !_kbhit() )
    {
		//----------omni----------
		//get position
		hdScheduleAsynchronous( GetInfoCallback, 0, HD_MAX_SCHEDULER_PRIORITY );
		/*
		if ( !hdWaitForCompletion( Scheduler_GetInfo, HD_WAIT_CHECK_STATUS ) )
        {
            fprintf(stderr, "Press any key to quit.\n");
            _getch();
            break;
        }*/
		//----------socket----------
		//save data and send to server
		//sprintf( message, "%f %f %f %f %f %f %f %f %f %f %f %f", current_pos[0], current_pos[1], current_pos[2], current_pos[0], current_pos[1], current_pos[2], current_pos[0], current_pos[1], current_pos[2], current_pos[0], current_pos[1], current_pos[2] );
		//cout << message << endl;
		
		//transfrom omni to z-up coordinate
		ss << current_pos[2] << " " << current_pos[0] << " " << current_pos[1] << " " 
			<< current_joint_angle[0] << " " << current_joint_angle[1] << " " << current_joint_angle[2];

		ssToChar(sendtoBuf, &ss);
		if (sendto(s, sendtoBuf, BUFLEN , 0 , (struct sockaddr *) &si_other, slen) == SOCKET_ERROR)
		{
            printf("sendto() failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }

        //receive a reply and print it
        //clear the buffer by filling null, it might have previously received data
        memset(recvBuf,'\0', BUFLEN);
        cout << "wait force..." << endl;
		//try to receive some data, this is a blocking 
        if (recvfrom(s, recvBuf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == SOCKET_ERROR)
        {
            printf("recvfrom() failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }
        puts(recvBuf);

		char* token = NULL;
		//char* rest = NULL;
		const char* delim = " ";

		force_device[0] = atof( strtok( recvBuf, delim ) );
		force_device[1] = atof( strtok( NULL, delim ) );
		force_device[2] = atof( strtok( NULL, delim ) );

		//generate force
		if ( !hdWaitForCompletion( Scheduler_ForceOutput, HD_WAIT_CHECK_STATUS ) )
        {
            fprintf(stderr, "Press any key to quit.\n");
            _getch();
            break;
        }
		
		Sleep( 50 );    //sampling time
	}
	
	//----------omni----------
	hdStopScheduler();
    hdUnschedule( Scheduler_ForceOutput );
	hdDisableDevice( device_id );	//correspond to the function "hdInitDevice"
		
	while (!_kbhit())

	{  
		fprintf(stderr, "\nThe main scheduler callback has exited\n");
		fprintf(stderr, "\nPress any key to quit.\n");
		getch();
		break;
	}
	
	//----------socket----------
    closesocket(s);
    WSACleanup();

	return 0;
}

HDCallbackCode HDCALLBACK GetInfoCallback( void *data )
{
	HDdouble position_device[3];
	HDdouble joint_angle_device[3];
	
	hdBeginFrame( device_id );
	hdGetDoublev( HD_CURRENT_POSITION, position_device );
	hdGetDoublev( HD_CURRENT_JOINT_ANGLES, joint_angle_device );
	//cout << "z(uav_x):" << position_device1[2] << " x(uav_y):" << position_device1[0] << " y(uav_z):" << position_device1[1] << endl;	//for testing

	//update current position
	for( int i = 0; i < 3; i++ )
	{
		current_pos[i] = position_device[i] * 0.01;	//mapping
		current_joint_angle[i] = joint_angle_device[i];
	}


	hdEndFrame( device_id );
	return HD_CALLBACK_DONE;
}
/*
void OpenFile()
{
	position = fopen( "..\\..\\data\\position.txt", "w" );
}

void CloseFile()
{
	fclose( position );
}
*/


HDCallbackCode HDCALLBACK ForceOutputCallback(void *data)
{
	hduVector3Dd force(force_device[0], force_device[1], force_device[2]);
	//HDdouble force[3];
	for(int i = 0; i < 3; i++)
		cout<< force[i] << " ";
	cout << endl;
	
	hdBeginFrame(device_id);
	hdMakeCurrentDevice(device_id);
	hdSetDoublev(HD_CURRENT_FORCE, force);
	hdEndFrame(device_id);
		
	return HD_CALLBACK_CONTINUE;
}

void ssToChar(char* dst, stringstream* src)
{
	const string tmp = src->str();
	const char* result = tmp.c_str();

	strcpy(dst, result);

	/* clear stringstream */
	src->str("");
	src->clear();

}