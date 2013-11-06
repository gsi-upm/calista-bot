// mainSystem.cpp - main startup and overall control functions

#include "common.h"
//#define AUTONUMBER 1
char* version = "2.0";

#ifdef INFORMATION

Conversation with the chatbot modifies global memory that always needs to be cleaned up before the next user.
This includes:
1. new dictionary entries
2. new facts
3. modification of bits per topic in usedTopicMap and topicFlagsMap 
4. user variables
#endif
int userLog = 2;
unsigned int tokenControl = 0;
char repeatedOutput[MAX_WORD_SIZE];
int prepareMode = false;
static int conceptCount = 0;
bool systemReset = false;
unsigned int echoSource = 0;
FILE* sourceFile = NULL;
int serverLog = 2;
bool sandbox = false;
jmp_buf scriptJump[5];
int jumpIndex = -1;
bool debug = false;
static unsigned int dayConvert[] = {31,28,31,30,31,30,31,31,30,31,30,31};//   days in month
int errors = 0;
int oktest = 0;
static bool raw;
bool all = false;
int inputCounter = 0;
int totalCounter = 0;
char* nextInput;
char respondLevel = 0;       //   rejoinder level of a random choice block
unsigned int inputCount;
unsigned int repeatable;
bool moreToComeQuestion = false;
bool moreToCome = false;
int regression = 0;
#ifdef NOSERVER
bool server = false;
#else
#ifdef WIN32
bool server = false;	// default is standalone on Windows
#elif IOS
bool server = true; // default is server on LINUX
#else
bool server = true; // default is server on LINUX
#endif
#endif

static char botPrefix[MAX_WORD_SIZE];
static char userPrefix[MAX_WORD_SIZE];

char revertBuffer[MAX_BUFFER_SIZE];

unsigned int trace = 0;
unsigned int noreact = 0;
char readBuffer[MAX_BUFFER_SIZE];

unsigned int year;			//   4 digit
unsigned int month;			//   1 = jan
unsigned int day;			//   day of month, like 17
unsigned int week;			//   0..51 BUG?
unsigned int dayOfWeek;		//   0 = mon
unsigned int hour;			//   24 hour time
unsigned int minute;		//   0..59
unsigned int second;		//   0..59
bool leapYear;
unsigned int dayInYear;		//   1..365
unsigned int weekInYear;	//   1..52
unsigned int globalDays;
unsigned int globalWeeks;
unsigned int globalMonths;
unsigned int globalYears;
unsigned int globalDecades;
unsigned int globalCenturies;
unsigned int globalMillenia;

//   replies we have tried already
char chatbotSaid[MAX_USED+1][MAX_MESSAGE];  //   tracks last n messages sent to user
char humanSaid[MAX_USED+1][MAX_MESSAGE]; //   tracks last n messages sent by user in parallel with userSaid
unsigned int humanSaidIndex;
unsigned int chatbotSaidIndex;

bool unusedRejoinder;
#define MAX_BUFFER_COUNT 50

static char buffers[MAX_BUFFER_COUNT][MAX_BUFFER_SIZE]; //   collection of output buffers
static int bufferIndex = 0;		//   current allocated index into buffers[]
RESPONSE responseData[MAX_RESPONSE_SENTENCES+1];
char* currentInput;

void myexit(int n)
{
	exit(n);
}

char* GetTimeInfo() //   Www Mmm dd hh:mm:ss yyyy Where Www is the weekday, Mmm the month in letters, dd the day of the month, hh:mm:ss the time, and yyyy the year. Sat May 20 15:21:51 2000
{
    time_t curr = time(0);
    if (regression) curr = 44444444; 
    char *mytime = ctime(&curr);
    mytime[strlen(mytime)-1] = 0; //   remove newline
    return mytime;
}

void ResetBuffers()
{
	bufferIndex = 0;
}

char* AllocateBuffer()
{
	char* buffer = buffers[bufferIndex++]; 
	if (bufferIndex == MAX_BUFFER_COUNT)
	{
		ReportBug("Used too many buffers");
		--bufferIndex;
	}
	*buffer++ = 0;	//   prior value

	*buffer = 0;	//   empty string
	return buffer;
}

void FreeBuffer()
{
	if (bufferIndex)  --bufferIndex; 
	else ReportBug("Buffer allocation underflow");
}

void MainLoop() //   local machine loop
{
	char* inBuffer = AllocateBuffer(); //   never need to free it.
	char* mainBuffer = AllocateBuffer(); //   never need to free it.
	*currentFileName = 0;	//   no files are open (if logging bugs)

    printf("\r\nEnter user name: ");
	tokenControl = 0;
    ReadALine(inBuffer,stdin);
	printf("\r\n");
	//   start conversation
	*computerID = 0; // unknown bot
	*mainBuffer = mainBuffer[1] = 0; // no message
	PerformChat(inBuffer,computerID,mainBuffer,NULL,mainBuffer); // unknown bot, no input,no ip
 
	//   keep up the conversation indefinitely
	sourceFile = stdin;

	//local build has no exception handling.. only server build on LINUX does
retry:
#ifdef WIN32
	DWORD time = 0;
#endif
	while (1)
    {
		if (oktest)
		{
			printf("%s\r\n    ",mainBuffer);
			if (oktest == OKTEST) strcpy(inBuffer,"OK");
			else strcpy(inBuffer,"why?");
		}
		else if (systemReset) 
		{
			printf("%s\r\n",mainBuffer);
			*computerID = 0;	// default bot
			*inBuffer = 0;	// restart conversation
		}
		else 
		{
			// output bot response
			if (*botPrefix) printf("%s ",botPrefix);
#ifdef AUTONUMBER
			printf("%d: ",inputCount);
#endif
			printf("%s\r\n",mainBuffer);

			//output user prompt
			if (*userPrefix) printf("%s ",userPrefix);
			else printf("   >");

			inBuffer[0] = ' '; // distinguish EMPTY from conversation start 

			if (!ReadALine(inBuffer+1,sourceFile)) break; // end of input

			// when reading from file, see if line is empty or comment
			if (sourceFile != stdin)
			{
				char word[MAX_WORD_SIZE];
				char* ptr = SkipWhitespace(inBuffer); 
				ReadCompiledWord(ptr,word);
				if (!*word || *word == '#') continue;
				if (echoSource) printf("%s\r\n",inBuffer);
			}
		}
		bufferIndex = 2;		//   if we recovered from a crash, we may need to reset
		*mainBuffer = 0;
		PerformChat(loginID,computerID,inBuffer,NULL,mainBuffer); // no ip
    }
	fclose(sourceFile); //   must have been changed to a file
	sourceFile = stdin;
	goto retry;
}

void SetSource(char* name)
{
	FILE* in = fopen(name,"rb");
	if (in) sourceFile = in;
}


static int VerifyAuthorization(FILE* in) //   is he allowed to use :commands
{
	char buffer[MAX_WORD_SIZE];
	if (!in) return 1;			//   no restriction file
	unsigned int result = 0;
	while (ReadALine(buffer,in) )
    {
		if (!stricmp(buffer,"all") || !stricmp(buffer,callerIP)) //   allowed
		{ 
			result = 1;
			break;
		}
	}
	fclose(in);
	return result;
}

bool DoCommand(char* input)
{
#ifndef NOTESTING
	if (!VerifyAuthorization(fopen("authorizedIP.txt","rb"))) return true;
	char* ptr = NULL;
	ReadNextSystemToken(NULL,ptr,NULL,false,false);		// flush any pending data in input cache
	return (!Command(input)) ? false : true; 
#else
	return false;
#endif
}

void ResetResponses() //   prepare for multiple sentences being processed - data lingers over multiple sentences
{
    //   former user data
	ResetFactSystem();
	ReturnDictionaryToFreeze();
	ClearUserVariables();
	ReestablishBaseVariables();
	ResetTopicSystem();

 	//   ordinary locals
 	chatbotSaidIndex = humanSaidIndex = 0;
    responseIndex = 0;
	sentenceCount = 0;
	unusedRejoinder = true; 
 }

void ResetSentence() //   read for next sentence to process - data fresh each sentence
{
	//   reset function call data
	systemCallArgumentBase = systemCallArgumentIndex = 0;
	userCallArgumentBase = userCallArgumentIndex = 0;

	ClearWhereInSentence();

	//   reset parsing data
	tokenFlags = 0;
	wordStarts[0] = 0;    
    wordCount = 0;  //   words in current sentence

	//   reset topic data
	ResetTopicSentence(); 
	respondLevel = 0; 
}

void PerformChat(char* user, char* usee, char* incoming,char* ip,char* output)
{ //   primary entrypoint for chatbot

    *output = 0;
	globalDepth = 0;

    //   case insensitive logins
    char caller[MAX_WORD_SIZE];
	char callee[MAX_WORD_SIZE];
	callee[0] = 0;
    caller[0] = 0;
	MakeLowerCopy(callee, usee);
    if (user) 
	{
		//   allowed to name callee as part of caller name
		char* separator = strchr(user,':');
		if (separator) *separator = 0;
		strcpy(caller,user);
		if (separator && !*usee) MakeLowerCopy(callee,separator+1); // override the bot
		strcpy(loginName,caller); // original name as he typed it

		MakeLowerCopy(caller,caller);
	}
	if (incoming[1] == '#' && incoming[2] == '!') // naming bot to talk to- and whether to initiate or not - e.g. #!Rosette#my message
	{
		char* next = strchr(incoming+3,'#');
		if (next)
		{
			*next = 0;
			MakeLowerCopy(callee,incoming+3); // override the bot name (including future defaults if such)
			strcpy(incoming+1,next+1);	// the actual message.
			if (!*incoming) incoming = 0;	// login message
		}
	}

    if (trace) Log(STDUSERLOG,"Incoming data- %s | %s | %s\r\n",caller, (callee) ? callee : " ", (incoming) ? incoming : "");
 
    Login(caller,callee,ip); //   get the participants names

    ResetResponses();	//   back to empty state
	if (systemReset) // drop old user
	{
		systemReset = false;
		ReadNewUser(); 
		start_rejoinder_Topic = rejoinder_Topic; 
		start_Rejoinder_at = rejoinder_at; 
	}
	else  ReadUserData();		//   now bring in user state
	ResetSentence();			//   now ready for sentence

	if (!ip) ip = "";

	bool ok = true;
    if (!*incoming) StartConversation(output); //   begin a conversation
    else  
	{
		char copy[MAX_BUFFER_SIZE];
		strcpy(copy,incoming); // so input trace not contaminated by input revisions
		ok = ProcessInput(copy);
	}
	
	if (!ok) return; // command processed

	strcpy(output,ConcatResult());
	OnceCode("$control_post");
	if (!server)
	{
		WORDP dBotPrefix = FindWord("$botprompt");
		if (dBotPrefix && dBotPrefix->w.userValue) strcpy(botPrefix,dBotPrefix->w.userValue);
		else *botPrefix = 0;
		WORDP dUserPrefix = FindWord("$userprompt");
		if (dUserPrefix && dUserPrefix->w.userValue) strcpy(userPrefix,dUserPrefix->w.userValue);
		else *userPrefix = 0;
	}
	WriteUserData(output);  

	char topic[MAX_WORD_SIZE];
	GetActiveTopicName(topic);

	if (noreact) Log(STDUSERLOG,"%s",incoming);
    else if (*incoming ) Log(STDUSERLOG,"user:%s bot:%s ip:%s Responding to:%d %s  ==>  %s  Topic: %s  %s\r\n",loginID,computerID,ip,inputCount,incoming,output,topic,GetTimeInfo());
	else Log(STDUSERLOG,"Starting conversation: user:%s bot:%s ip:%s %s => %s\r\n",loginID,computerID,ip,GetTimeInfo(),output);
}

int Reply() 
{
	if (trace) 
	{
		Log(STDUSERLOG,"\r\n\r\nReply to input: %s\r\n",currentInput);
		Log(STDUSERLOG,"Interesting topics: %s\r\n",ShowInterestingTopics());
	}
	inputCounter = 0;

	//   the MAIN output buffer
	currentOutputBase = AllocateBuffer();

    if (!regression && repeatable != 1000) repeatable = 0; //   script can change, but regression must keep control

	unsigned int result = 0;
	SetUserVariable("$code","main");
	char* name = GetUserVariable("$control_main");
	bool pushed = PushTopic(FindTopicIDByName(name));
	result = PerformTopic(0,currentOutputBase); //   allow control to vary
	if (pushed) PopTopic();

	FreeBuffer();
	return result;
}

bool ProcessInput(char* input)
{
	//try
	{
		
		lastInputSubstitution[0] = 0;
		repeatedOutput[0] = 0;

		//   precautionary adjustments
		char* p = input;
		while ((p = strchr(p,'`'))) *p = '\'';
		p = SkipWhitespace(input);
		if (*p != ':') while ((p = strchr(p,'_'))) *p = ' '; // dont purify commands
  
		char *buffer = SkipWhitespace(input);
		unsigned int len = strlen(buffer);
		if (len >= MAX_MESSAGE) buffer[MAX_MESSAGE-1] = 0; 

		if (*buffer == ':')
		{
			bool commanded = true;
			if (!strnicmp(buffer,":real",5)) // :real allows you to alter user world
			{
				ProcessInput(buffer+6);
				WriteUserData("");
			}
			else commanded = DoCommand(buffer);
			if (!strnicmp(buffer,":revert",7) || !strnicmp(buffer,":undo",5) ) strcpy(buffer,revertBuffer); // revert restates last input after setup, but persona just does a login
			else if (commanded ) return false; 
			else  *input = 0; // :bot or :execute reset(user)  acts like new conversation
		}

		inputCounter = 0;
		totalCounter = 0;
		++inputCount;
		if (trace) Log(STDUSERLOG,"\r\n\r\nInput: %d to %s: %s ----\r\n",inputCount,computerID,input);
	
		if (!strncmp(buffer,"... ",4)) buffer += 4;	//   JUST A MARKER FROM ^input
		else if (!strncmp(buffer,". ",2)) buffer += 2;	//   maybe separator after ? or !

		//   process input now
		strcpy(humanSaid[humanSaidIndex]+1,buffer);
		humanSaid[humanSaidIndex++][0] = ' '; // all entries have a leading space from read/write end test against #
		if (!server) strcpy(revertBuffer,buffer); // for a possible revert

		char* ptr = buffer;

		rulesTested = rulesMatched = 0; 
 		int loopcount = 0;
		OnceCode("$control_pre");
		while (ptr && *ptr) 
		{
			if (++loopcount > 50) //   protect against runaway input 
			{
				ReportBug("loopcount excess %d %s",loopcount,ptr);
				break;
			}
			currentInput = ptr;	
  			topicIndex = topicID = 0; // precaution
			ptr = DoSentence(ptr);

			++sentenceCount;
			if (!wordCount) continue;
		}
		if (!responseIndex && !noreact)
		{
			if (buffer && *buffer && *buffer != ':' && !regression  && !prepareMode && !all) 
			{
				if (*repeatedOutput)
				{
					SaveResponse(repeatedOutput);
				}
				else
				{
					char* intent = GetUserVariable("$$intent");
					ReportBug("No response generated - %s %s", intent,input);
					SaveResponse("Hey, sorry. What were we talking about?");
				}
			}
		}
	}
/*	catch (...)  
	{
		ReportBug("crash - %s", input);
 		SaveResponse("Ouch! Something bit me!");
		SetUserVariable("$it_pronoun","the insect that bit me");
	}
*/
	return true;
}

char* DoSentence(char* input)
{
	unsigned int oldtrace = trace;
	if ( prepareMode) 
	{
		echo = true;
		if (prepareMode == 1) trace = -1;
	}
	char* ptr = PrepareSentence(input,true,true); // user input
	if ( prepareMode) 
	{
		trace = oldtrace;
		echo = false;
		return ptr; // only do prep work
	}

    if (!wordCount)  //   no tokenize (might be he gave an invisible filler only
    {
        if (responseIndex != 0)  return ptr; //   generated a reply,quit, otherwise we will do a gambit
		//   if NOTHING came from tokenization and we have more to parse, skip this stuff
		if (!wordCount && *ptr && !tokenFlags) return ptr; 
    }
    
    GetCurrentDateInfo();   //   present time info

	ptr = SkipWhitespace(ptr);
    moreToCome = strlen(ptr) > 0;                  //   another sentence exists after this one  
	moreToComeQuestion = (strchr(ptr,'?') != 0);
	if (!strnicmp(ptr,"who ",4) || !strnicmp(ptr,"what",4) ||!strnicmp(ptr,"spot",5) ||!strnicmp(ptr,"when",4) ||  !strnicmp(ptr,"why",4) || !strnicmp(ptr,"how",3)) moreToComeQuestion = true;
    //    generate reply by lookup first

    rejoinder_Topic = rejoinder_at = NO_REJOINDER; //   allow react to set rejoinder ptr for its generated stuff
	int result =  Reply();
	if (result & FAILSENTENCE_BIT)  --sentenceCount;
	if (result == ENDINPUT_BIT) nextInput = "";

    //if (!sentenceCount || responseIndex <= response || !response); //   no change
    //   now... IF he has given us a non-eraseable topic response (quibble) and we already have a response, drop this wasted one
 
    return nextInput; //   continuing data 
}

void OnceCode(const char* which) //   run before doing any of his input
{
	topicIndex = topicID = 0; // precaution
	char* name;
	if (!stricmp(which,"$control_pre")) 
	{
		SetUserVariable("$code","pre");
		name = GetUserVariable(which);
		if (trace) 
		{
			Log(STDUSERLOG,"\r\nPrePass\r\n");
			if (trace & VARIABLE_TRACE) DumpVariables();
		}
	}
	else 
	{
		SetUserVariable("$code","post");
		name = GetUserVariable(which);
		if (trace) 
		{
			Log(STDUSERLOG,"\r\n\r\nPostPass\r\n");
			Log(STDUSERLOG,"Interesting topics: %s\r\n",ShowInterestingTopics());
		}
	}
	int topic = FindTopicIDByName(name);
	if (!topic) return;
	bool pushed = PushTopic(topic);
	currentOutputBase = AllocateBuffer();
	PerformTopic(GAMBIT,currentOutputBase);
	FreeBuffer();
	if (pushed) PopTopic();
	if (topicIndex) ReportBug("topics still stacked");
	topicIndex = topicID = 0; // precaution
}

void AddUsed(const char* reply)
{
    unsigned int i = chatbotSaidIndex++;
    chatbotSaidIndex %= MAX_USED; //  overflow is unlikely (allocate one for us, nothing for him)
    chatbotSaid[i][0] = ' ';
	strcpy(chatbotSaid[i]+1,reply); 
}

bool HasAlreadySaid(char* msg)
{
    if (!*msg) return true; 
    if (repeatable || GetTopicFlags(topicID) & TOPIC_REPEAT || globalTopicFlags & TOPIC_REPEAT) return false;
    unsigned int len1 = strlen(msg);
    while (msg[len1-1] == ' ') msg[--len1] = 0; 
    ++len1; //   include end of string marker
    for (unsigned int i = 0; i < chatbotSaidIndex; ++i)
    {
        if (!stricmp(msg,chatbotSaid[i]+1)) return true; // actual sentence is indented one (flag for end reads in column 0)
    }
    return false;
}

bool AddResponse(char* msg)
{
		if (strstr(msg,"Fire"))
		{
			int i = 0;
		}

	char* original = msg;
    char buffer[MAX_BUFFER_SIZE];
	if (!msg || !*msg) return false;
    unsigned int len = strlen(msg);
 	if (len > MAX_BUFFER_SIZE)
	{
		ReportBug("overly long reply %s",msg);
		len = MAX_BUFFER_SIZE-50;
		msg[len] = 0;
	}
    strcpy(buffer,msg);
    ConvertUnderscores(buffer,false); // leave new lines alone
	*buffer = toUppercaseData[(unsigned char)*buffer];

	//   remove spaces before commas (geofacts often have them in city_,_state)
	char* ptr = buffer;
	while (ptr && *ptr)
	{
		char* comma = strchr(ptr,',');
		if (comma && comma != buffer )
		{
			if (*--comma == ' ') memmove(comma,comma+1,strlen(comma));
			ptr = comma+2;
		}
		else if (comma) ptr = comma+1;
		else ptr = 0;
	}

    if (all || HasAlreadySaid(buffer) ) 
    {
		if (all)
		{
			bool oldecho = echo;
			echo = true;
			Log(STDUSERLOG,"Choice: %s\r\n",buffer);
			echo = oldecho;
		}
        else 
		{
			if (trace) Log(STDUSERLOG,"Rejected: %s already said\r\n",buffer);
			strcpy(repeatedOutput,buffer);
		}
        memset(msg,0,strlen(msg)); //   kill partial output
        return false;
    }
    if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERTABLOG,"Message: %s\r\n",buffer);

    SaveResponse(buffer);
    memset(original,0,len+1); //   zap original message, + maybe 1 extra for a leading space
    return true;
}

void GetCurrentDateInfo()
{
	time_t rawtime;
	time (&rawtime );
	struct tm* timeinfo = localtime (&rawtime );

    second = timeinfo->tm_sec;
    minute = timeinfo->tm_min; 
    hour = timeinfo->tm_hour;
    day = timeinfo->tm_mday;
    week = ((month-1) * 4) + (day / 7);
    month = timeinfo->tm_mon;
    year = timeinfo->tm_year;
    dayInYear = day;
    if (month) for (unsigned int i = 0; i < month-1; ++i) dayInYear += dayConvert[i];  //   add in days in specific months elapsed
    if (month > 2 && leapYear) ++dayInYear;
    weekInYear = (dayInYear/7) + 1; 
    leapYear = !(year % 400) || (!(year % 4) && (year % 100));
    
    globalMonths = ((year-1) * 12) + month;
    globalWeeks = ((year-1) * 52) + weekInYear;
    globalDays = ((year-1) * 365) + dayInYear; //   ignore leap years
    globalYears = year;
    globalDecades = (year % 10);
    globalCenturies = (year % 100);
    globalMillenia = (year % 1000);
}

char* ConcatResult()
{
    static char  result[MAX_SENTENCE_BYTES];
    result[0] = 0;

	for (unsigned int i = 0; i < responseIndex; ++i) 
    {
        if (responseData[i].response[0]) 
		{
			char* reply = responseData[i].response;

			//   whatever we say, we most expect him to respond to the last thing.
			unsigned int len = strlen(reply);
			if (len > MAX_SENTENCE_BYTES)
			{
				ReportBug("overly long reply");
				reply[MAX_SENTENCE_BYTES-50] = 0;
			}
			AddUsed(reply); // we generate a used for each reply we make
		}
    }

	//   now join up all the responses as one output and store as temporary facts
	for (unsigned int i = 0; i < responseIndex; ++i) 
    {
        if (!responseData[i].response[0]) continue;
		//   decode _ to spaces.
		char* ptr;
		while ((ptr = strchr(responseData[i].response,'_'))) *ptr = ' '; //   revert to spaces from underscores

		if (*result) strcat(result," "); //   separator between responses
		strcat(result,responseData[i].response); 

		ptr = &responseData[i].response[0];

		while (*ptr)
		{
			char* old = SkipWhitespace(ptr);
			ptr = Tokenize(old,wordCount,wordStarts);   //   only used to locate end of sentence but can also affect tokenFlags (no longer care)
			
			//   strip trailing blanks
			char* at = ptr;
			while (*--at == ' '); 
			char c = *++at;
			*at = 0;

			//   save sentences as facts
			WORDP D = StoreWord(old);
			MEANING M = MakeMeaning(Dchatoutput);
			CreateFact(MakeMeaning(D),M,M,TRANSIENTFACT);

			*at = c;
		}
    }

    return result;
}

void Reload()
{//   reset the basic system through wordnet but before topics
	InitFacts(); 
	InitDictionary();
	if (raw) LoadRawDictionary(); 
	else LoadDictionary();
 	InitFunctionSystem();
#ifndef NOTESTING
	InitCommandSystem();
#endif
	ExtendDictionary();
	ClearUserVariables();

	if (raw) return;

	ReadFacts("DICT/facts.txt",BUILD0);
	ReadFacts("LIVEDATA/systemfacts.txt",BUILD0);
    ReadDeadFacts("DICT/deadfacts.txt"); // kill off some problematic dictionary facts
	InitSpellChecking();
	PreBuildLockDictionary();
	printf("Read %s Dictionary facts\r\n",StdNumber(Fact2Index(factFree)));
}

char* PrepareSentence(char* input,bool mark,bool user) 
{
	unsigned int mytrace = trace;
	if ( prepareMode == 2) mytrace = 0;
  
	char* ptr = input;
	currentInput = ptr;	//   consider us as STARTING here...
    ResetSentence();			//   ready to accept interjection data
    ptr = Tokenize(ptr,wordCount,wordStarts); 
 	if (mytrace)
	{
		Log(STDUSERLOG,"\r\nOriginal Input: %s\r\n",input);
		Log(STDUSERLOG,"Tokenized Input: ");
		for (unsigned int i = 1; i <= wordCount; ++i) Log(STDUSERLOG,"%s  ",wordStarts[i]);
		Log(STDUSERLOG,"\r\n");
	}
	char* basic[MAX_SENTENCE_LENGTH];
	unsigned int basicCount = wordCount;
	if (mytrace) memcpy(basic+1,wordStarts+1,wordCount * sizeof(char*));	// replicate for test

	if (tokenControl & DO_SUBSTITUTES)  
	{
		ProcessSubstitutes();
 		if (mytrace )// show changes
		{
			int changed = 0;
			if (wordCount != basicCount) changed = true;
			for (unsigned int i = 1; i <= wordCount; ++i) if (basic[i] != wordStarts[i]) changed = i;
			if (changed)
			{
				Log(STDUSERLOG,"Substituted Input: ");
				for (unsigned int i = 1; i <= wordCount; ++i) Log(STDUSERLOG,"%s  ",wordStarts[i]);
				Log(STDUSERLOG,"\r\n");
			}
			basicCount = wordCount;
			memcpy(basic+1,wordStarts+1,wordCount * sizeof(char*));	// replicate for test
		}
	}
	// test for punctuation not done by substitutes
	for (unsigned int i = 1; i <= wordCount; ++i) 
	{
		if (wordStarts[i][0] == '?' || wordStarts[i][0] == '!') 
		{
			tokenFlags |= (wordStarts[i][0] == '?') ? QUESTIONMARK : EXCLAMATIONMARK;
			memcpy(wordStarts[i],wordStarts[i+1],(wordCount - i) * sizeof(char*));
			--wordCount;
			--i;
		}  
	}
	if (tokenControl & DO_PROPERNAME_MERGE)  ProcessCompositeName();   
 	if (tokenControl & DO_NUMBER_MERGE)  ProcessCompositeNumber(); //   numbers AFTER titles, so they dont change a title
 	if (mytrace) // show changes
	{
		int changed = 0;
		if (wordCount != basicCount) changed = true;
		for (unsigned int i = 1; i <= wordCount; ++i) if (basic[i] != wordStarts[i]) changed = i;
		if (changed)
		{
			if ((tokenControl & (DO_PROPERNAME_MERGE | DO_NUMBER_MERGE)) == (DO_SUBSTITUTES | DO_SPELLCHECK)) Log(STDUSERLOG,"Name/Number-merged Input: ");
			else if (tokenControl & DO_PROPERNAME_MERGE) Log(STDUSERLOG,"Name-merged Input: ");
			else Log(STDUSERLOG,"Number-merged Input: ");
			for (unsigned int i = 1; i <= wordCount; ++i) Log(STDUSERLOG,"%s  ",wordStarts[i]);
			Log(STDUSERLOG,"\r\n");
		}
	}
	if (tokenControl & DO_SPELLCHECK)  
	{
		SpellCheck();
 		if (mytrace)// show changes
		{
 			int changed = 0;
			if (wordCount != basicCount) changed = true;
			for (unsigned int i = 1; i <= wordCount; ++i) if (basic[i] != wordStarts[i]) changed = i;
			if (changed)
			{
				Log(STDUSERLOG,"Spellchecked Input: ");
				for (unsigned int i = 1; i <= wordCount; ++i) Log(STDUSERLOG,"%s  ",wordStarts[i]);
				Log(STDUSERLOG,"\r\n");
			}
			basicCount = wordCount;
			memcpy(basic+1,wordStarts+1,wordCount * sizeof(char*));	// replicate for test
		}
	}
	if (echoSource == 2) 
	{
		bool oldecho = echo;
		echo = true;
		Log(STDUSERLOG,"  => ");
		for (unsigned int i = 1; i <= wordCount; ++i) Log(STDUSERLOG,"%s  ",wordStarts[i]);
		Log(STDUSERLOG,"\r\n");
		echo = oldecho;
	}
	nextInput = ptr;	//   allow system to overwrite input here
 
	if (tokenControl & DO_INTERJECTION_SPLITTING && wordCount > 1 && wordStarts[1][0] == '~') // interjection. handle as own sentence
	{
		// formulate an input insertion
		char buffer[BIG_WORD_SIZE];
		*buffer = 0;
		for (unsigned int i = 2; i <= wordCount; ++i)
		{
			strcat(buffer,wordStarts[i]);
			strcat(buffer," ");
		}
		if (tokenFlags & QUESTIONMARK) strcat(buffer,"? ");
		else if (tokenFlags & EXCLAMATIONMARK) strcat(buffer,"! ");
		else strcat(buffer,". ");
		char* end = buffer + strlen(buffer);
		strcpy(end,nextInput); // this is the REST of what we want to see from before. and we will add before it the revised interjection
		strcpy(nextInput,buffer);
		ptr = nextInput;
		wordCount = 1;
	}
	tokenFlags |= (user) ? USERINPUT : 0; // remove any question mark
    if (mark) MarkAllImpliedWords();
	return ptr;
}

static void CloseSystem()
{
    CloseDictionary();
    CloseFacts(); 
}

static void CountConcepts(WORDP D,void* junk)
{
	if (D->word[0] == '~') ++conceptCount;
}

void CreateSystem()
{
	int old = trace; // in case trace turned on by default
	trace = 0;

	Reload(); // builds layer1 facts and dictionary (from wordnet)
    if (raw) // wont happen anymore-- master dictionary has been built
    {
        WriteFacts(fopen("DICT/facts.txt","wb"),factBase); 
		sprintf(logbuffer,"USERS/build_log.txt"); //   all data logged here by default
		FILE* in = fopen(logbuffer,"wb");
		if (in) fclose(in);
		myexit(0);
	}

	RestartTopicSystem();
	*currentFileName = 0;
	conceptCount = 0;
	WalkDictionary(CountConcepts,0);
	printf("Concept sets: %s\r\n",StdNumber(conceptCount));
	char buf[100];
	strcpy(buf,StdNumber(factEnd-factFree));
	printf("Free space: %s bytes  FreeFacts: %s \r\n",StdNumber(TotalFreeSpace()),buf);

	trace = old;
}

void InitSystem(int argc, char * argv[])
{
	// custom single user prefixing
	*botPrefix = *userPrefix = 0;
	currentFileName[0] = 0;
    raw = false;
	//raw  = true;

    logbuffer[0] = 0; 
    sprintf(logbuffer,"serverlog%d.txt",port);
	computerID[0] = 0;
	loginName[0] = loginID[0] = 0;
    echo = true;
	bool portGiven = false;
	for (int i = 1; i < argc; ++i)
	{
		if (!stricmp(argv[i],"trace")) trace = -1; 
#ifndef NOSERVER
		else if (!strnicmp(argv[i],"client=",7)) // client=1.2.3.4:1024 format  client=localhost:1024
		{
			server = false;
			char buffer[MAX_WORD_SIZE];
			strcpy(serverIP,argv[i]+7);
		
			char* portVal = strchr(serverIP,':');
			if ( portVal)
			{
				*portVal = 0;
				port = atoi(portVal+1);
			}
			printf("\r\nEnter client user name: ");
			ReadALine(buffer,stdin);
			printf("\r\n");
			Client(buffer);
			exit(0);
		}  
		else if (!strnicmp(argv[i],"dual=",5)) // client=1.2.3.4:1024 format  client=localhost:1024
		{
			server = false;
			char buffer[MAX_WORD_SIZE];
			strcpy(serverIP,argv[i]+5);
		
			char* portVal = strchr(serverIP,':');
			if ( portVal)
			{
				*portVal = 0;
				port = atoi(portVal+1);
			}
			printf("\r\nEnter client user name: ");
			ReadALine(buffer,stdin);
			printf("\r\n");
			Dual(buffer);
			exit(0);
		} 
		else if (!stricmp(argv[i],"load"))
		{
			server = false;
			strcpy(serverIP,"localhost");
			Load();
			exit(0);
		}  
		else if (!stricmp(argv[i],"regressload"))
		{
			server = false;
			strcpy(serverIP,"localhost");
			RegressLoad();
			exit(0);
		} 		
		else if (!strnicmp(argv[i],"time=",5))
		{
			ANSWERTIMELIMIT = atoi(argv[i]+5); // seconds before abort
		}  
		else if (!stricmp(argv[i],"local")) server = false; // local standalone
		else if (!stricmp(argv[i],"nouserlog")) userLog = 0;
		else if (!stricmp(argv[i],"userlog")) userLog = 1;
		else if (!stricmp(argv[i],"noserverlog")) serverLog = 0;
		else if (!stricmp(argv[i],"serverlog")) serverLog = 1;
		else if (!strnicmp(argv[i],"port=",5))  // be a server
		{
			portGiven = true;
			port = atoi(argv[i]+5); // accept a port=
			GrabPort(); 
			#ifdef WIN32
			PrepareServer();
			#endif
		}
#endif			
		else if (!stricmp(argv[i],"debug")) debug = true;
		else if (!strnicmp(argv[i],"sandbox",7))  // sandbox safe- no system or file write allowed
		{
			sandbox = true;
		}
	}
#ifndef NOSERVER
#ifndef WIN32
		if (!portGiven && server) GrabPort(); 
#endif
#endif
	// defaults where not specified
	if (server)
	{
		if (userLog == 2) userLog = 0;	// default OFF for user if unspecified
		if (serverLog == 2) serverLog = 1; // default ON for server if unspecified
	}
	else
	{
		if (userLog == 2) userLog = 1;	// default ON for nonserver if unspecified
		if (serverLog == 2) serverLog = 1; // default on for nonserver 
	}
	printf("ChatScript Version %s  %d bit\r\n",version,sizeof(char*) * 8);
	CreateSystem();

	// special macroArgumentList to program
	for (int i = 1; i < argc; ++i)
	{
#ifndef NOSCRIPTCOMPILER
		if (!strnicmp(argv[i],"build0=",7))
		{
			sprintf(logbuffer,"USERS/build0_log.txt");
			FILE* in = fopen(logbuffer,"wb");
			if (in) fclose(in);
			ReadTopicFiles(argv[i]+7,BUILD0,dictionaryFacts,0);
 			myexit(0);
		}  
		if (!strnicmp(argv[i],"build1=",7))
		{
			sprintf(logbuffer,"USERS/build1_log.txt");
			FILE* in = fopen(logbuffer,"wb");
			if (in) fclose(in);
			ReadTopicFiles(argv[i]+7,BUILD1,build0Facts,0);
 			myexit(0);
		}  
#endif
		if (!stricmp(argv[i],"trace")) trace = -1;
		if (*argv[i] == '$')
		{
			char* eq = strchr(argv[i],'=');
			if (eq)
			{
				char name[MAX_WORD_SIZE];
				strcpy(name,argv[i]);
				name[eq - argv[i]] = 0;
				SetUserVariable(name,eq+1);
			}
		}
	}
	if (server) 
	{
		Log(STDUSERLOG,"\r\n\r\n======== Began server %s on port %d at %s\r\n",version,port,GetTimeInfo());
		Log(SERVERLOG,"\r\n\r\n======== Began server %s on port %d at %s\r\n",version,port,GetTimeInfo());
	}
}



#ifdef XMPP
extern "C" { int XMPPmain(int argc, char * argv[]);}
#endif
//#define LOEBNER_TRANSCRIPT_DECODE 1

int main(int argc, char * argv[]) 
{
#ifdef LOEBNER_TRANSCRIPT_DECODE
	FILE* in = fopen("tmp.txt","rb");
	char word[MAX_WORD_SIZE];
	char who[MAX_WORD_SIZE];
	char who2[MAX_WORD_SIZE];
	char old[MAX_WORD_SIZE];
	char begin[MAX_WORD_SIZE];
	char target[MAX_WORD_SIZE];
	*target = 0;
	*old = 0;
	char buffer[MAX_BUFFER_SIZE];
	*buffer = 0;
	strcpy(logbuffer,"out.txt");
	char comp[MAX_WORD_SIZE];
	char time[MAX_WORD_SIZE];
	FILE* out = fopen("out.txt","ab");
	char lastchar[MAX_WORD_SIZE];
	*begin = 0;
	char* want = "local";
	char round[MAX_WORD_SIZE];
	while (ReadALine(readBuffer, in))
	{
		char* ptr = SkipWhitespace(readBuffer);
   		ptr = ReadCompiledWord(ptr,time); 
		if (!stricmp(time,"starting")) 
		{
			ptr = ReadCompiledWord(ptr,time); 
			ptr = ReadCompiledWord(ptr,time); 
			strcpy(round,time);
			ptr = ReadCompiledWord(ptr,word); // left=
			memmove(word,word+5,strlen(word)-4);
			if (stricmp(word,"human")) strcpy(comp,"left");
			else strcpy(comp,"right");
			fprintf(out,"\r\n\r\n%s\r\n",readBuffer);
			continue;
		}

  		ptr = ReadCompiledWord(ptr,word); 
  		ptr = ReadCompiledWord(ptr,who); 
  		ptr = ReadCompiledWord(ptr,who2);
		if (stricmp(who2,comp)) continue; // ignore human confederate chat

		if (*begin == 0 && !stricmp(who,want)) // who we want is starting?
		{
			strcpy(begin,time);
			strcpy(old,who);
			strcpy(target,who2);
		}

		if (stricmp(old,who) || stricmp(who2,target))// change of speaker or target
		{
			if (*lastchar == '?' || *lastchar == '.' || *lastchar == '!') // he ignores cr
			{
				if (*buffer) 
				{
					if (!stricmp(old,"local")) sprintf(old,"%s Judge: ",round);
					else sprintf(old,"%s      : ",round);
					fprintf(out,"%s %s %s\r\n",begin,old,buffer);
				}
				*buffer = 0;
				strcpy(begin,time);
			}
			*lastchar = 0;
			strcpy(old,who);
			strcpy(target,who2);
		}
		if (stricmp(who,want)) continue;	// not who we want

		ptr = ReadCompiledWord(ptr,word);
		char marker[MAX_WORD_SIZE];
		ReadCompiledWord(ptr,marker);
		if (*marker)
		{
			int i = 0; // debug makr
		}
		if (!stricmp(word,"space")) strcpy(word," ");
		if (!stricmp(word,"cr"))
		{
			if (*buffer) 
			{
				if (!stricmp(who,"local")) sprintf(word,"%s Judge: ",round);
				else sprintf(word,"%s      : ",round);
				fprintf(out,"%s %s %s\r\n",begin,word,buffer);
			}
			*buffer = 0;
			*lastchar = 0;
			*old = 0;
			*begin = 0;
			continue;
		}
		if (!stricmp(word,"backspace")) 
		{
			int len = strlen(buffer);
			buffer[len-1] = 0;
		}
		else 
		{
			strcat(buffer,word);
			strcpy(lastchar,word);
		}
	}
	fclose(out);
	fclose(in);

	if (1==1) return 1;
#endif


#ifdef XMPP
	XMPPmain(argc,argv);
	exit(0);
#endif
	*newBuffer = 0;
	InitSystem(argc,argv);
	echo = false;
    if (!server) MainLoop();
#ifndef NOSERVER
    else InternetServer();
#endif
    CloseSystem(); 
    return 0;
}


void SaveResponse(char* msg)
{
    if (strlen(msg) >= MAX_SENTENCE_BYTES) strcpy(msg+MAX_SENTENCE_BYTES-5,"..."); //   prevent trouble
    strcpy(responseData[responseIndex].response,msg);
    unsigned int len = strlen(msg);
    while (len && responseData[responseIndex].response[--len] == ' ') responseData[responseIndex].response[len] = 0; //   strip trailing blanks
    responseData[responseIndex].responseInputIndex = sentenceCount;
    responseIndex++;
}

