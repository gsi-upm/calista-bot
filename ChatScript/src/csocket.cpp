// csocket.cpp - handles client/server behaviors

//#define STATSERVER 1

/*
 *   C++ sockets on Unix and Windows Copyright (C) 2002 --- see HEADER file
 */

#ifdef INFORMATION
We have these threads:

1. HandleTCPClient which runs on a thread to service a particular client (as many threads as clients)
2. AcceptSockets, which is a simnple thread waiting for user connections
3. ChatbotServer, which is the sole server to be the chatbot
4. main thread, which is a simple thread that creates ChatbotServer threads as needed (if ones crash in LINUX, it can spawn a new one)

chatLock- either chatbot engine or a client controls it. It is how a client gets started.

While processing chat, the client holds the clientlock and the server holds the chatlock and donelock.
The client is waiting for the donelock.
When the server finishes, he releases the donelock and waits for the clientlock.
If the client exists, server gets the donelock, then releases it and waits for the clientlock.
If the client doesnt exist anymore, he has released clientlock.

It releases the clientlock and the chatlock and tries to reacquire chatlock with flag on.
when it succeeds, it has a new client.


We have these variables :
chatbotExists - the chatbot engine is now initialized.
chatbotWanted - some client wants the data from server. If client times out while server is processing, this will go false and server will discard his answer eventually.

#endif

#include "common.h"
#ifndef NOSERVER
#ifdef WIN32
  #pragma warning(push,1)
  #pragma warning(disable: 4290) 
#endif
#include <cstdio>
#include <errno.h>  
using namespace std;

int ANSWERTIMELIMIT = 5 * 1000;

char serverIP[100];
unsigned int port = 1024;
static TCPServerSocket* serverSocket = NULL;

// buffers used to send in and out of chatbot
static char inputFeed[MAX_BUFFER_SIZE];
char outputFeed[MAX_BUFFER_SIZE];

static char* clientBuffer;			//   current client spot input was and output goes
static bool chatWanted = false;		//  client is still expecting answer (has not timed out/canceled)
static bool chatbotExists = false;	//	has chatbot engine been set up and ready to go?

static void* MainChatbotServer();
static void* HandleTCPClient(void *sock1);
static void* AcceptSockets(void*);
static unsigned int errorCount = 0;
static time_t lastCrash = 0;

clock_t startServerTime;
clock_t ElapsedMilliseconds();

#ifndef WIN32 // for LINUX
#include <pthread.h> 
#include <signal.h> 
pthread_t chatThread;
static pthread_mutex_t chatLock = PTHREAD_MUTEX_INITIALIZER;	//   access lock to shared chatbot processor 
static pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER;		//   right to use log file output
static pthread_mutex_t testLock = PTHREAD_MUTEX_INITIALIZER;	//   right to use test memory
static pthread_cond_t  server_var   = PTHREAD_COND_INITIALIZER; // client ready for server to proceed
static pthread_cond_t  server_done_var   = PTHREAD_COND_INITIALIZER;	// server ready for clint to take data
void GetFutureTime(struct timespec &abs_time,int fuure) ;


#else // for WINDOWS
static bool initialized = false; // winsock init
#include <winsock.h>    
HANDLE  hChatLockMutex;    
int     ThreadNr;     
CRITICAL_SECTION LogCriticalSection; 
CRITICAL_SECTION TestCriticalSection; 
#endif


void Client(char* login)// test client for a server
{
	printf("\r\n\r\n** Client launched\r\n");
	char data[MAX_WORD_SIZE];
	char* from = login;
	char* separator = strchr(from,':'); // login is username  or  username:botname
	char* bot;
	if (separator)
	{
		*separator = 0;
		bot = separator + 1;
	}
	else bot = from + strlen(from);	// just a 0
	sprintf(logbuffer,"log-%s.txt",from);

	// message to server is 3 strings-   username, botname, null (start conversation) or message
	char* ptr = data;
	strcpy(ptr,from);
	ptr += strlen(ptr) + 1;
	strcpy(ptr,bot);
	ptr += strlen(ptr) + 1;
	echo = 1;
	*ptr = 0; // null message - start conversation
	while (1)
	{
		try 
		{
			unsigned int len = (ptr-data) + 1 + strlen(ptr);
			TCPSocket *sock = new TCPSocket(serverIP, port);
			sock->send(data, len );
			printf("Sent data\r\n");

			int bytesReceived = 1;              // Bytes read on each recv()
			int totalBytesReceived = 0;         // Total bytes read
			char* base = ptr;
			while (bytesReceived > 0) 
			{
				// Receive up to the buffer size bytes from the sender
				bytesReceived = sock->recv(base, MAX_WORD_SIZE);
				totalBytesReceived += bytesReceived;
				base += bytesReceived;
				printf("Received %d data\r\n",bytesReceived);
			}
			delete(sock);
			*base = 0;
			printf("Received all data %s\r\n",ptr);

			// chatbot replies this
			Log(STDUSERLOG,"%s",ptr);

			// we say that  until :exit
			printf("\r\n    ");
			ReadALine(ptr,stdin);
			if (!stricmp(ptr,":exit")) break;
		}
		catch(SocketException e) { printf("failed to connect to server\r\n"); exit(0);}
	}
}


void Dual(char* login)// test client for a server
{
	printf("\r\n\r\n** Dual launched\r\n");
	char data[MAX_WORD_SIZE];
	char data1[MAX_WORD_SIZE];
	
	char* from = login;
	char* separator = strchr(from,':'); // login is username  or  username:botname
	sprintf(logbuffer,"log-%s.txt",from);

	// message to server is 3 strings-   username, botname, null (start conversation) or message
	char* ptr = data;
	strcpy(ptr,from);
	ptr += strlen(ptr) + 1;
	strcpy(ptr,"");
	ptr += strlen(ptr) + 1;
	echo = 1;
	*ptr = 0; // null message - start conversation

	char* ptr1 = data1;
	strcpy(ptr1,from);
	strcat(ptr1,"1");	// extended id
	ptr1 += strlen(ptr1) + 1;
	strcpy(ptr1,"");
	ptr1 += strlen(ptr1) + 1;
	*ptr1 = 0; // null message - start conversation

	while (1)
	{
		try 
		{
			unsigned int len = (ptr-data) + 1 + strlen(ptr);
			TCPSocket *sock = new TCPSocket(serverIP, port);
			sock->send(data, len );

			int bytesReceived = 1;              // Bytes read on each recv()
			char* base = ptr;
			while (bytesReceived > 0) 
			{
				// Receive up to the buffer size bytes from the sender
				bytesReceived = sock->recv(base, MAX_WORD_SIZE);
				base += bytesReceived;
			}
			delete(sock);
			*base = 0;

			// now do secondary message
			strcpy(ptr1,ptr);	// put one into the other
			len = (ptr1-data1) + 1 + strlen(ptr1);
			sock = new TCPSocket(serverIP, port);
			sock->send(data1, len );

			bytesReceived = 1;              // Bytes read on each recv()
			base = ptr1;
			while (bytesReceived > 0) 
			{
				// Receive up to the buffer size bytes from the sender
				bytesReceived = sock->recv(base, MAX_WORD_SIZE);
				base += bytesReceived;
			}
			delete(sock);
			*base = 0;

			strcpy(ptr,ptr1);	// put one into the other
		}
		catch(SocketException e) { printf("failed to connect to server\r\n"); exit(0);}
	}
}

void Load()// test load for a server
{
	// treat each invocation as a new judge
	FILE* in = fopen("load.txt","rb");
	int id = 0;
	char buffer[1000];
	if (in)
	{
		fread(buffer,1,100,in);
		fclose(in);
		id = atoi(buffer);
	}
	++id;
	FILE* out = fopen("load.txt","wb");
	fprintf(out,"%d\r\n",id);
	fclose(out);

	printf("\r\n\r\n** Load %d launched\r\n",id);
	char data[MAX_WORD_SIZE];
	char from[100];
	sprintf(from,"%d",id);
	char* bot = "";
	sprintf(logbuffer,"log-%s.txt",from);
	unsigned int msg = 0;
	unsigned int volleys = 0;
	unsigned int longVolleys = 0;
	unsigned int xlongVolleys = 0;
	char* messages[] = {
		"What is an apple?",
		"What is a toy?",
		"What is an elephant?",
		"What is a pear?",
		"What is swimming?",
		"What is the meaning of life?",
		"What is a deal?",
		"What is an exercise?",
		"What is the point?",
		"Where is my toy?",
		"What is a bird?",
		"What is a tiger?",
		"What is a lion?",
		"What is a seahorse?",
		"What is a sawhorse?",
		"What is an egg?",
		"What is a dinosaur?",
		"What is a peach?",
		"What is a banana?",
		"What is a goose?",
		"What is a duck?",
		"What is a tomboy?",
		"What is purple?",
		0,
	};

	int maxTime = 0;
	int cycleTime = 0;
	int currentCycleTime = 0;
	int avgTime = 0;

	// message to server is 3 strings-   username, botname, null (start conversation) or message
	echo = 1;
	char* ptr = data;
	strcpy(ptr,from);
	ptr += strlen(ptr) + 1;
	strcpy(ptr,bot);
	ptr += strlen(ptr) + 1;
	while (1)
	{

		strcpy(ptr,messages[msg]);
		try 
		{
			unsigned int len = (ptr-data) + 1 + strlen(ptr);
			++volleys;
			clock_t start_time = ElapsedMilliseconds();

			TCPSocket *sock = new TCPSocket(serverIP, port);
			sock->send(data, len );
	
			int bytesReceived = 1;              // Bytes read on each recv()
			int totalBytesReceived = 0;         // Total bytes read
			char* base = ptr;
			while (bytesReceived > 0) 
			{
				// Receive up to the buffer size bytes from the sender
				bytesReceived = sock->recv(base, MAX_WORD_SIZE);
				totalBytesReceived += bytesReceived;
				base += bytesReceived;
			}
			clock_t end_time = ElapsedMilliseconds();
	
			delete(sock);
			*base = 0;
			int diff = end_time - start_time;
			if (diff > maxTime) maxTime = diff;
			if ( diff > 2000) ++longVolleys;
			if (diff > 5000) ++ xlongVolleys;
			currentCycleTime += diff;

			// chatbot replies this
			printf("real:%d avg:%d  max:%d   volleys:%d  2slong:%d 5slong:%d\r\n",diff,avgTime,maxTime,volleys,longVolleys,xlongVolleys);
		}
		catch(SocketException e) { printf("failed to connect to server\r\n"); exit(0);}
		if (messages[msg+1] == 0) 
		{
			msg = 0;
			cycleTime = currentCycleTime;
			currentCycleTime = 0;
			avgTime = cycleTime / 23;
		}
		else msg++;
	}
}


void RegressLoad()// test load for a server
{
	
	FILE* in = fopen("REGRESS/bigregress.txt","rb");
	if (!in) return;
	
	// treat each invocation as a new judge
	FILE* in1 = fopen("load.txt","rb");
	int id = 0;
	char buffer[8000];
	if (in1)
	{
		fread(buffer,1,100,in1);
		fclose(in1);
		id = atoi(buffer);
	}
	++id;
	FILE* out = fopen("load.txt","wb");
	fprintf(out,"%d\r\n",id);
	fclose(out);

	printf("\r\n\r\n** Load %d launched\r\n",id);
	char data[MAX_WORD_SIZE];
	char from[100];
	sprintf(from,"%d",id);
	char* bot = "";
	sprintf(logbuffer,"log-%s.txt",from);
	unsigned int msg = 0;
	unsigned int volleys = 0;
	unsigned int longVolleys = 0;
	unsigned int xlongVolleys = 0;

	int maxTime = 0;
	int cycleTime = 0;
	int currentCycleTime = 0;
	int avgTime = 0;

	// message to server is 3 strings-   username, botname, null (start conversation) or message
	echo = 1;
	char* ptr = data;
	strcpy(ptr,from);
	ptr += strlen(ptr) + 1;
	strcpy(ptr,bot);
	ptr += strlen(ptr) + 1;
	*buffer = 0;
	int counter = 0;
	while (1)
	{

		if (!ReadALine(revertBuffer+1,in)) break; // end of input
		// when reading from file, see if line is empty or comment
		char word[MAX_WORD_SIZE];
		char* at = SkipWhitespace(revertBuffer+1); 
		ReadCompiledWord(at,word);
		if (!*word || *word == '#' || *word == ':') continue;

		strcpy(ptr,revertBuffer+1);
		try 
		{
			unsigned int len = (ptr-data) + 1 + strlen(ptr);
			++volleys;
			clock_t start_time = ElapsedMilliseconds();

			TCPSocket *sock = new TCPSocket(serverIP, port);
			sock->send(data, len );
	
			int bytesReceived = 1;              // Bytes read on each recv()
			int totalBytesReceived = 0;         // Total bytes read
			char* base = ptr;
			while (bytesReceived > 0) 
			{
				// Receive up to the buffer size bytes from the sender
				bytesReceived = sock->recv(base, MAX_WORD_SIZE);
				totalBytesReceived += bytesReceived;
				base += bytesReceived;
			}
			clock_t end_time = ElapsedMilliseconds();
	
			delete(sock);
			*base = 0;
			int diff = end_time - start_time;
			if (diff > maxTime) maxTime = diff;
			if ( diff > 2000) ++longVolleys;
			if (diff > 5000) ++ xlongVolleys;
			currentCycleTime += diff;

			// chatbot replies this
			printf("real:%d avg:%d max:%d volley:%d 2slong:%d 5slong:%d %s => %s\r\n",diff,avgTime,maxTime,volleys,longVolleys,xlongVolleys,ptr,base);
		}
		catch(SocketException e) { printf("failed to connect to server\r\n"); exit(0);}
		if (++counter == 100) 
		{
			counter = 0;
			cycleTime = currentCycleTime;
			currentCycleTime = 0;
			avgTime = cycleTime / 100;
		}
		else msg++;
	}
}

static void Crash()
{
    time_t now = time(0);
    unsigned int delay = (unsigned int) difftime(now,lastCrash);
    if (delay > 180) errorCount = 0; //   3 minutes delay is fine
    lastCrash = now;
    ++errorCount;
	if (errorCount > 6) exit(0); //   too many crashes in a row, let it reboot from scratch
#ifndef WIN32
	//   clear chat thread if it crashed
	if (chatThread == pthread_self()) 
    {
		if (!chatbotExists ) exit(0);
        chatbotExists = false;
        chatWanted = false;
        pthread_mutex_unlock(&chatLock); 
    }
#endif
	longjmp(scriptJump[MAIN_RECOVERY],1);
}

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time 
void clock_get_mactime(struct timespec &ts)
{
	clock_serv_t cclock; 
	mach_timespec_t mts; 
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock); 
	clock_get_time(cclock, &mts); 
	mach_port_deallocate(mach_task_self(), cclock); 
	ts.tv_sec = mts.tv_sec; 
	ts.tv_nsec = mts.tv_nsec; 
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abs_timeout)
{
	int result;
	struct timespec ts1;
	do
	{
		result = pthread_mutex_trylock(mutex);
		if (result == EBUSY)
		{
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_sec = 10000000;

			/* Sleep for 10,000,000 nanoseconds before trying again. */
			int status = -1;
			while (status == -1) status = nanosleep(&ts, &ts);
		}
		else break;
		clock_get_mactime(ts1);
	}
	while (result != 0 and (abs_timeout->tv_sec == 0 ||  abs_timeout->tv_sec > ts1.tv_sec )); // if we pass to new second

	return result;
}
#endif 

#ifndef WIN32
void GetFutureTime(struct timespec &abs_time,int fuure) // LINUX
{
#ifdef __MACH__
	clock_get_mactime( abs_time);
#else
	clock_gettime(CLOCK_REALTIME,&abs_time);
#endif
	abs_time.tv_sec += fuure / 1000; 
	abs_time.tv_nsec += (fuure%1000) * 1000000;
}
#endif

clock_t ElapsedMilliseconds()
{
	clock_t count;
#ifdef WIN32
	count = GetTickCount();
#else // non-windows

#ifdef __MACH__
	struct timespec abs_time; 
	clock_get_mactime( abs_time);
	count = abs_time.tv_sec * 1000; 
	count += abs_time.tv_nsec / 1000000; // nanoseconds
#else
	struct timeval x_time;
	gettimeofday(&x_time, NULL);
	count = x_time.tv_sec * 1000; 
	count += x_time.tv_usec / 1000; // microseconds
#endif

#endif
	return count;
}
// LINUX/MAC SERVER
#ifndef WIN32

void GetLogLock()
{
	pthread_mutex_lock(&logLock); 
}

void ReleaseLogLock()
{
	pthread_mutex_unlock(&logLock); 
}

void GetTestLock()
{
	pthread_mutex_lock(&testLock); 
}

void ReleaseTestLock()
{
	pthread_mutex_unlock(&testLock); 
}

static bool ClientGetChatLock(int timeleft)
{
	timeleft -= 20;	// presumed time needed for server to process
	if (timeleft <= 0 ) return false;	// assume we time out since not enough time left to use server
	struct timespec abs_time; 
#ifdef __MACH__
	clock_get_mactime( abs_time);
#else
	clock_gettime(CLOCK_REALTIME,&abs_time);
#endif
	abs_time.tv_sec += 5; 
	// BUG
	//GetFutureTime(abs_time,timeleft);
	int rc = pthread_mutex_timedlock (&chatLock, &abs_time); // wait for a while to get the mutex
	if (rc != 0 ) return false;	// we timed out
	return true;
}

static bool ClientWaitForServer(char* data,char* msg,int timeleft)
{
	if (timeleft <= 0) return false;

	struct timespec abs_time; 
	pthread_cond_signal( &server_var ); // let server know he is needed
	GetTestLock();	// control the testlock...
	pthread_mutex_unlock (&chatLock); 
#ifdef __MACH__
	clock_get_mactime( abs_time);
#else
	clock_gettime(CLOCK_REALTIME,&abs_time);
#endif
	abs_time.tv_sec += 5; // we will wait for 5 sec
	// BUG GetFutureTime(abs_time,timeleft);
	int rc = pthread_cond_timedwait(&server_done_var, &testLock, &abs_time);   
	ReleaseTestLock();
	return (*data == 0); // we succeeded or not
}

static void LaunchClient(void* junk) // accepts incoming connections from users
{
	pthread_t clientThread;     
	pthread_attr_t attr; 
    pthread_attr_init(&attr); 
	//pthread_attr_setstacksize(&attr, 5000);
	pthread_attr_setdetachstate(&attr,  PTHREAD_CREATE_DETACHED);
	int val = pthread_create(&clientThread,&attr,HandleTCPClient,(void*)junk);
	 int x;
	 if ( val != 0)
	 {
		 switch(val)
		 {
		 case EAGAIN: 
			 
			  x = sysconf( _SC_THREAD_THREADS_MAX );
			  printf("create thread failed EAGAIN limit: %d \r\n",(int)x);   break;
		 case EPERM:  printf("create thread failed EPERM \r\n");   break;
		 case EINVAL:  printf("create thread failed EINVAL \r\n");   break;
		 default:  printf("create thread failed default \r\n");   break;
		 }
	 }
}

//   use sigaction instead of signal -- bug

//   we really only expect the chatbot thread to crash, when it does, the chatLock will be under its control
void AnsiSignalMapperHandler(int sigNumber) 
{
    signal(-1, AnsiSignalMapperHandler); 
    Crash();
}

void fpViolationHandler(int sigNumber) 
{
    Crash();
}

void SegmentFaultHandler(int sigNumber) 
{
    Crash();
}

void* ChatbotServer(void* junk)
{
	//   get initial control over the mutex so we can start. Any client grabbing it will be disappointed since chatbotExists = false, and will relinquish.
	MainChatbotServer();
}

static void ServerStartup()
{
	pthread_mutex_lock(&chatLock);   //   get initial control over the mutex so we can start - now locked by us. Any client grabbing it will be disappointed since chatbotExists = false
}

static void ServerGetChatLock()
{
	chatWanted = false;	
	while (!chatWanted) //   dont exit unless someone actually wanted us
	{
		pthread_cond_wait( &server_var, &chatLock ); // wait for someone to signal the server var...
	}
}

void InternetServer()
{
  
	// set error handlers
  //BUG  signal(-1, AnsiSignalMapperHandler);  
    
	struct sigaction action; 
    memset(&action, 0, sizeof(action)); 
   //BUG  sigemptyset(&action.sa_mask); 

    action.sa_flags = 0; 
    action.sa_handler = fpViolationHandler; 
  //BUG   sigaction(SIGFPE,&action,NULL); 
    action.sa_handler = SegmentFaultHandler; 
//BUG     sigaction(SIGSEGV,&action,NULL); 

    //   thread to accept incoming connections, doesnt need much stack
    pthread_t socketThread;     
	pthread_attr_t attr; 
    pthread_attr_init(&attr); 
	//pthread_attr_setstacksize(&attr, 5000);
	//pthread_attr_setdetachstate(&attr,  PTHREAD_CREATE_DETACHED);
	pthread_create(&socketThread, &attr, AcceptSockets, (void*) NULL);   

    //  we merely loop to create chatbot services. If it dies, spawn another and try again
    while (1)
    {
        pthread_create(&chatThread, &attr, ChatbotServer, (void*) NULL);  
        void* status; 
        pthread_join(chatThread, &status); 
        if (!status) break; //   chatbot thread exit (0= requested exit - other means it crashed
		Log(SERVERLOG,"Spawning new chatserver\r\n");
     }
	exit(1);
}
#endif  // end LINUX

#ifdef WIN32
 
void GetLogLock()
{
	EnterCriticalSection(&LogCriticalSection);
}

void ReleaseLogLock()
{
	LeaveCriticalSection(&LogCriticalSection);
}

void GetTestLock()
{
	EnterCriticalSection(&TestCriticalSection);
}

void ReleaseTestLock()
{
	LeaveCriticalSection(&TestCriticalSection);
}

static bool ClientGetChatLock(int timeleft)
{
	timeleft -= 20;
	if (timeleft <= 0) return false;
	// wait to get attention of chatbot server
	DWORD dwWaitResult;
	DWORD time = GetTickCount();
	while ((GetTickCount()-time)  < timeleft) // loop to get control over chatbot server - timeout if takes too long
    {
		dwWaitResult = WaitForSingleObject(hChatLockMutex,100); // try and return after 100 ms
        // we now know he is ready, he released the lock, unless it is lying around foolishly
		if (dwWaitResult == WAIT_TIMEOUT) continue; // we didnt get the lock
        else if (!chatbotExists || chatWanted) ReleaseMutex(hChatLockMutex); // not really ready
        else return true;
    }
	return false;
}

static bool ClientWaitForServer(char* data,char* msg,int timeleft)
{
	if (timeleft <= 0) return false;
	bool result;
	DWORD startTime = GetTickCount();
	ReleaseMutex(hChatLockMutex); // chatbot server can start processing my request now
	while (1) // loop to get control over chatbot server - timeout if takes too long
	{
		Sleep(4);
		unsigned long msTimeAmount = GetTickCount() - startTime;
		GetTestLock();
		if (*data == 0) // he answered us, he will turn off chatWanted himself
		{
			result = true;
			break;
		}
		// he hasnt answered us yet, so chatWanted must still be true
		if ( msTimeAmount >= timeleft) // give up
		{
			chatWanted = result = false;	// indicate we no longer want the answer
			break;
		}
		ReleaseTestLock();
	}

	ReleaseTestLock();
	
	return result; // timed out - he can't write any data to us now
}

static void LaunchClient(void* sock) 
{
	_beginthread((void (__cdecl *)(void *)) HandleTCPClient,0,sock);
}

static void ServerStartup()
{
	WaitForSingleObject( hChatLockMutex, INFINITE );
	//   get initial control over the mutex so we can start - now locked by us. Any client grabbing it will be disappointed since chatbotExists = false
}

static void ServerGetChatLock()
{
	chatWanted = false;		
    while (!chatWanted) //   dont exit unless someone actually wanted us
    {
		ReleaseMutex(hChatLockMutex);
 		WaitForSingleObject( hChatLockMutex, INFINITE );
        //wait forever til somebody has something for us - we may busy spin here waiting for chatWanted to go true
	}

	// we have the chatlock and someone wants server
}

void PrepareServer()
{
	InitializeCriticalSection(&LogCriticalSection ); 
	InitializeCriticalSection(&TestCriticalSection ) ;
}

void InternetServer()
{
	_beginthread((void (__cdecl *)(void *) )AcceptSockets,0,0);	// the thread that does accepts... spinning off clients
	MainChatbotServer();  // run the server from the main thread
	CloseHandle( hChatLockMutex );
	DeleteCriticalSection(&TestCriticalSection);
	DeleteCriticalSection(&LogCriticalSection);
	exit(1);
}

#endif

static void ServerTransferDataToClient(char* priorMessage)
{
	// if client times out, he will have set chatWanted false
	GetTestLock();
  	if (chatWanted) // client still wants his data - give him output + what we said before to him
	{

		strcpy(clientBuffer+SERVERTRANSERSIZE,outputFeed);
		int len = strlen(outputFeed);



		char* prior = clientBuffer+SERVERTRANSERSIZE+len+1;
		strcpy(prior,priorMessage);
		prior += strlen(prior) + 1;
		sprintf(prior,"%d",inputCount); // total relationship
		clientBuffer[0] = 0; // mark we are done....
	}
#ifndef WIN32
	pthread_cond_signal( &server_done_var ); // let client know we are done
#endif

	ReleaseTestLock();
}

static void* AcceptSockets(void*) // accepts incoming connections from users
{
    try {
        while(1)
        {  
            char* time = GetTimeInfo()+SKIPWEEKDAY;
            TCPSocket *sock = serverSocket->accept();
			LaunchClient((void*)sock);
         }
	} catch (SocketException &e) {printf("accept busy\r\n"); ReportBug("***Accept busy\r\n");}
    exit(1);
	return NULL;
}

static void* Done(TCPSocket * sock,char* memory)
{
	try{
 		char* output = memory+SERVERTRANSERSIZE;
		unsigned int len = strlen(output);
		if (len)  sock->send(output, len);
	}
	catch(...) {printf("sending failed\r\n");}
	delete sock;
	free(memory);
	return NULL;
}

static void* HandleTCPClient(void *sock1)  // individual client, data on STACK... might overflow...
{
	clock_t starttime = ElapsedMilliseconds(); 
	char* memory = (char*) malloc(2*SERVERTRANSERSIZE+2); // our data in 1st chunk, his reply info in 2nd
	if (!memory) return NULL; // ignore him if we run out of memory
	char* output = memory+SERVERTRANSERSIZE;
	*output = 0;
	char* buffer;
	TCPSocket *sock = (TCPSocket*) sock1;
	char IP[20];
    try {
        strcpy(memory,sock->getForeignAddress().c_str());	// get IP address
		strcpy(IP,memory);
		buffer = memory + strlen(memory) + 1;				// use this space after IP for messaging
    } catch (SocketException e)
	{ 
		ReportBug("Socket errx"); cerr << "Unable to get port" << endl;
		return Done(sock,memory);
	}
	char timeout = ' ';

	char userName[500];
	*userName = 0;
	char* user;
	char* bot;
	char* msg;
	char* prior = "";
	// A user name with a . in front of it means tie it with IP address so it remains unique 
	try{
		unsigned int len = sock->recv(buffer, SERVERTRANSERSIZE-50); // leave ip address in front alone
		memset(buffer+len,0,3); 
	
		user = buffer;
		bot = user + strlen(user) + 1;
		msg = bot + strlen(bot) + 1;

		if (!*user)
		{
			if (*bot == '1' && bot[1] == 0) // echo test to prove server running (generates no log traces)
			{
				strcpy(output,"1");
				return Done(sock,memory);
			}

			ReportBug("%s %s bot: %s msg: %s  NO USER ID \r\n",IP,GetTimeInfo()+SKIPWEEKDAY,bot,msg);
			strcpy(output,"[you have no user id]\r\n"); 
			return Done(sock,memory);
		}

		if (Blacklisted(memory,user)) // if he is blacklisted, we ignore him
		{
			PartialLogin(user,memory); // sets LOG file so we record his messages
			Log(SERVERLOG,"%s %s/%s %s msg: %s  BLOCKED \r\n",IP,user,bot,GetTimeInfo()+SKIPWEEKDAY,msg);
			strcpy(output,"[ignoring you.]\r\n"); //   response is JUST this
 			Log(STDUSERLOG,"blocked %s  =>  %s\r\n",msg,output);
			return Done(sock,memory);
		}
		strcpy(userName,user);

		// wait to get attention of chatbot server, timeout if doesnt come soon enough

		bool ready = ClientGetChatLock( ANSWERTIMELIMIT - (ElapsedMilliseconds() - starttime) ); // chatlock  ...
		if (!ready)  // if it takes to long waiting to get server attn, give up
		{
 			PartialLogin(userName,memory); // sets LOG file so we record his messages
			switch(random(4))
			{
				case 0: strcpy(output,"Hey, sorry. Had to answer the phone. What was I saying?"); break;
				case 1: strcpy(output,"Hey, sorry. There was a salesman at the door. Where were we?"); break;
				case 2: strcpy(output,"Hey, sorry. Got distracted. What did you say?"); break;
				case 3: strcpy(output,"Hey, sorry. What did you say?"); break;
			}
			Log(STDUSERLOG,"Timeout waiting for service: %s  =>  %s\r\n",msg,output);
			return Done(sock,memory);
		}

	  } catch (...)  
	  {
			ReportBug("***%s client presocket failed %s\r\n",IP,GetTimeInfo()+SKIPWEEKDAY);
 			return Done(sock,memory);
	  }
	 clock_t serverstarttime = ElapsedMilliseconds(); 
	 try{
 		chatWanted = true;	// indicate we want the chatbot server - no other client can get chatLock while this is true
		clientBuffer = memory; 
		int remain = ANSWERTIMELIMIT - (ElapsedMilliseconds() - starttime);

		if (!ClientWaitForServer(memory,msg,remain)) // release lock, loop wait for data,  and maybe we give up waiting for him
		{
			timeout = '*';
			switch(random(4))
			{
				case 0: strcpy(output,"Hey, sorry. Had to answer the phone. What was I saying?"); break;
				case 1: strcpy(output,"Hey, sorry. There was a salesman at the door. Where were we?"); break;
				case 2: strcpy(output,"Hey, sorry. Got distracted. What did you say?"); break;
				case 3: strcpy(output,"Hey, sorry. What did you say?"); break;
			}
		}
		unsigned int len = strlen(output);
		if (len) sock->send(output, len);
		prior = output + len + 1;			// bot prior message to us
} catch (...)  {
		printf("client socket fail\r\n");
		ReportBug("***%s client socket failed %s \r\n",IP,GetTimeInfo()+SKIPWEEKDAY);}
	delete sock;
	char* date = GetTimeInfo()+SKIPWEEKDAY;
	date[15] = 0;	// suppress year
	clock_t endtime = ElapsedMilliseconds(); 

	char* prior1 = prior + strlen(prior) + 1;
	int volleys = atoi(prior1);
	if (*msg) Log(SERVERLOG,"%c %s %s/%s %s time:%d/%d v:%d   msg: %s  =>  %s   <= %s\n",timeout,IP,user,bot,date, (int)(endtime - starttime),(int)(endtime - serverstarttime),volleys,msg,output,prior);
	else Log(SERVERLOG,"%c %s %s/%s %s start  =>  %s   <= %s\n",timeout,IP,user,bot,date,output,prior);
	free(memory);
#ifndef WIN32
	pthread_exit(0);
#endif
	return NULL;
}
	
void GrabPort() // enable server port if you can... if not, we cannot run. 
{
    try { serverSocket = new TCPServerSocket(port); }
    catch (SocketException &e) {printf("busy port %d\r\n",port) ; exit(1);}
	echo = 1;
	server = true;
 
#ifdef WIN32
	hChatLockMutex = CreateMutex( NULL, FALSE, NULL );   
#endif
}

void StallTest(bool startTest,char* label)
{
	static  clock_t start;
	if (startTest) start = ElapsedMilliseconds();
	else
	{
		clock_t now = ElapsedMilliseconds();
		if ((now-start) > 40) 
			printf("%d %s\r\n",(unsigned int)(now-start),label);
		//else printf("ok %d %s\r\n",now-start,label);
	}
}

static void* MainChatbotServer()
{
	sprintf(serverLogfileName,"LOGS/serverlog%d.txt",port);
	ServerStartup(); //   get initial control over the mutex so we can start. - on linux if thread dies, we must reacquire here 
	// we now own the chatlock
	clock_t lastTime = ElapsedMilliseconds(); 

	if (setjmp(scriptJump[MAIN_RECOVERY])) // crashes come back to here
	{
		printf("***Server exception\r\n");
		ReportBug("***Server exception\r\n");
#ifdef WIN32
		char* bad = GetUserVariable("$crashmsg");
		if (*bad) strcpy(outputFeed,bad);
		else strcpy(outputFeed,"Hey, sorry. I forgot what I was thinking about.");
		ServerTransferDataToClient("");
#endif
		ResetBuffers(); //   in the event of a trapped bug, return here, we will still own the chatlock
	}
    chatbotExists = true;   //  if a client can get the chatlock now, he will be happy
	Log(SERVERLOG,"Server ready\r\n");
	printf("Server ready: %s\r\n",serverLogfileName);
#ifdef WIN32
 _try { // catch crashes in windows
#endif
	int counter = 0;
	while (1)
	{
		ServerGetChatLock();
		startServerTime = ElapsedMilliseconds(); 

		// chatlock mutex controls whether server is processing data or client can hand server data.
		// That we now have it means a client has data for us.
		// we own the chatLock again from here on so no new client can try to pass in data. 
		// CLIENT has passed server in globals:  clientBuffer (ip,user,bot,message)
		// We will send back his answer in clientBuffer, overwriting it.
		char user[MAX_WORD_SIZE];
		char bot[MAX_WORD_SIZE];
		char* ip = clientBuffer;
		char* ptr = ip;
		// incoming is 4 strings together:  ip, username, botname, message
		ptr += strlen(ip) + 1;	// ptr to username
		strcpy(user,ptr); // allow user var to be overwriteable, hence a copy
		ptr += strlen(ptr) + 1; // ptr to botname
		strcpy(bot,ptr);
		ptr += strlen(ptr) + 1; // ptr to message
		strcpy(inputFeed,ptr); // xfer user message to our incoming feed
		echo = false;

		PerformChat(user,bot,inputFeed,ip,outputFeed);	// this takes however long it takes, exclusive control of chatbot.
#ifdef STATSERVER
		clock_t now = ElapsedMilliseconds();
		if ( (now / 1000) > (lastTime / 1000)) // starting different second
		{
			printf("%d\r\n",counter);
			counter = 0;
			lastTime = now; 
		}
		++counter;
		if ((now-startServerTime) > 2000) 
		{
			printf("Compute Stall? %d\r\n",now-startServerTime);
		}
#endif		
	ServerTransferDataToClient(priorMessage);
	}
#ifdef WIN32
		}_except (true) {
			ReportBug("crash\r\n"); Crash();}
#endif
	return NULL;
}

// CODE below here is from PRACTICAL SOCKETS

//   SocketException Code

SocketException::SocketException(const string &message, bool inclSysMsg)
  throw() : userMessage(message) {
  if (inclSysMsg) {
    userMessage.append(": ");
    userMessage.append(strerror(errno));
  }
}

SocketException::~SocketException() throw() {}
const char *SocketException::what() const throw() { return userMessage.c_str();}

//   Function to fill in address structure given an address and port
static void fillAddr(const string &address, unsigned short port, sockaddr_in &addr) {
  memset(&addr, 0, sizeof(addr));  //   Zero out address structure
  addr.sin_family = AF_INET;       //   Internet address
  hostent *host;  //   Resolve name
  if ((host = gethostbyname(address.c_str())) == NULL)  throw SocketException("Failed to resolve name (gethostbyname())");
  addr.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
  addr.sin_port = htons(port);     //   Assign port in network byte order
}

//   Socket Code
#define MAKEWORDX(a, b)      ((unsigned short)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((unsigned short)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))

CSocket::CSocket(int type, int protocol) throw(SocketException) {
  #ifdef WIN32
    if (!initialized) {
      WSADATA wsaData;
      unsigned short wVersionRequested = MAKEWORDX(2, 0);              //   Request WinSock v2.0
      if (WSAStartup(wVersionRequested, &wsaData) != 0) throw SocketException("Unable to load WinSock DLL");  //   Load WinSock DLL
      initialized = true;
    }
  #endif

  //   Make a new socket
  if ((sockDesc = socket(PF_INET, type, protocol)) < 0)  throw SocketException("Socket creation failed (socket())", true);
}

CSocket::CSocket(int sockDesc) {this->sockDesc = sockDesc;}

CSocket::~CSocket() {
  #ifdef WIN32
    ::closesocket(sockDesc);
  #else
    ::close(sockDesc);
  #endif
  sockDesc = -1;
}

string CSocket::getLocalAddress() throw(SocketException) {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);
  if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)  throw SocketException("Fetch of local address failed (getsockname())", true);
  return inet_ntoa(addr.sin_addr);
}

unsigned short CSocket::getLocalPort() throw(SocketException) {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);
  if (getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)  throw SocketException("Fetch of local port failed (getsockname())", true);
  return ntohs(addr.sin_port);
}

void CSocket::setLocalPort(unsigned short localPort) throw(SocketException) {
#ifdef WIN32
    if (!initialized) 
	{
      WORD wVersionRequested;
      WSADATA wsaData;

      wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
      if (WSAStartup(wVersionRequested, &wsaData) != 0) {  // Load WinSock DLL
        throw SocketException("Unable to load WinSock DLL");
      }
      initialized = true;
    }
  #endif

	int on = 1;
#ifdef WIN32
	setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on));
#else
	setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (void*) &on, sizeof(on));
#endif
	
	//   Bind the socket to its port
	sockaddr_in localAddr;
	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddr.sin_port = htons(localPort);
	if (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0) throw SocketException("Set of local port failed (bind())", true);
}

void CSocket::setLocalAddressAndPort(const string &localAddress,
    unsigned short localPort) throw(SocketException) {
	//   Get the address of the requested host
	sockaddr_in localAddr;
	fillAddr(localAddress, localPort, localAddr);
	int on = 1;
#ifdef WIN32
	setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on));
#else
	setsockopt(sockDesc, SOL_SOCKET, SO_REUSEADDR, (void*) &on, sizeof(on));
#endif
	if (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0)  throw SocketException("Set of local address and port failed (bind())", true);
}

void CSocket::cleanUp() throw(SocketException) {
  #ifdef WIN32
    if (WSACleanup() != 0)  throw SocketException("WSACleanup() failed");
  #endif
}

unsigned short CSocket::resolveService(const string &service,
                                      const string &protocol) {
  struct servent *serv;        /* Structure containing service information */
  if ((serv = getservbyname(service.c_str(), protocol.c_str())) == NULL) return atoi(service.c_str());  /* Service is port number */
  else return ntohs(serv->s_port);    /* Found port (network byte order) by name */
}

//   CommunicatingSocket Code

CommunicatingSocket::CommunicatingSocket(int type, int protocol)  throw(SocketException) : CSocket(type, protocol) {
}

CommunicatingSocket::CommunicatingSocket(int newConnSD) : CSocket(newConnSD) {
}

void CommunicatingSocket::connect(const string &foreignAddress,unsigned short foreignPort) throw(SocketException) {
  //   Get the address of the requested host
  sockaddr_in destAddr;
  fillAddr(foreignAddress, foreignPort, destAddr);

  //   Try to connect to the given port
  if (::connect(sockDesc, (sockaddr *) &destAddr, sizeof(destAddr)) < 0)   throw SocketException("Connect failed (connect())", true);
}

void CommunicatingSocket::send(const void *buffer, int bufferLen) throw(SocketException) {
  if (::send(sockDesc, (raw_type *) buffer, bufferLen, 0) < 0)  throw SocketException("Send failed (send())", true);
}

int CommunicatingSocket::recv(void *buffer, int bufferLen) throw(SocketException) {
  int rtn;
  if ((rtn = ::recv(sockDesc, (raw_type *) buffer, bufferLen, 0)) < 0)  throw SocketException("Received failed (recv())", true);
  return rtn;
}

string CommunicatingSocket::getForeignAddress()  throw(SocketException) {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);
  if (getpeername(sockDesc, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0)  throw SocketException("Fetch of foreign address failed (getpeername())", true);
  return inet_ntoa(addr.sin_addr);
}

unsigned short CommunicatingSocket::getForeignPort() throw(SocketException) {
  sockaddr_in addr;
  unsigned int addr_len = sizeof(addr);
  if (getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0)  throw SocketException("Fetch of foreign port failed (getpeername())", true);
  return ntohs(addr.sin_port);
}

//   TCPSocket Code

TCPSocket::TCPSocket() throw(SocketException) : CommunicatingSocket(SOCK_STREAM, IPPROTO_TCP) {
}

TCPSocket::TCPSocket(const string &foreignAddress, unsigned short foreignPort) throw(SocketException) : CommunicatingSocket(SOCK_STREAM, IPPROTO_TCP) {
  connect(foreignAddress, foreignPort);
}

TCPSocket::TCPSocket(int newConnSD) : CommunicatingSocket(newConnSD) {
}

//   TCPServerSocket Code

TCPServerSocket::TCPServerSocket(unsigned short localPort, int queueLen) throw(SocketException) : CSocket(SOCK_STREAM, IPPROTO_TCP) {
  setLocalPort(localPort);
  setListen(queueLen);
}

TCPServerSocket::TCPServerSocket(const string &localAddress, unsigned short localPort, int queueLen) throw(SocketException) : CSocket(SOCK_STREAM, IPPROTO_TCP) {
  setLocalAddressAndPort(localAddress, localPort);
  setListen(queueLen);
}

TCPSocket *TCPServerSocket::accept() throw(SocketException) {
  int newConnSD;
  if ((newConnSD = ::accept(sockDesc, NULL, 0)) < 0)  throw SocketException("Accept failed (accept())", true);
  return new TCPSocket(newConnSD);
}

void TCPServerSocket::setListen(int queueLen) throw(SocketException) {
  if (listen(sockDesc, queueLen) < 0)  throw SocketException("Set listening socket failed (listen())", true);
}

#ifdef WIN32
#pragma warning( pop )
#endif

#endif
