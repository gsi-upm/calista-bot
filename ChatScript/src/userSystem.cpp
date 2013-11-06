// userSystem.cpp - handle representation of a user ID, logging in, saving state, etc


#include "common.h"
#define MAX_BLACKLIST 50

static char* saveVersion = "Mar2511";
unsigned int blacklistStartTime[MAX_BLACKLIST];
char blacklist[MAX_BLACKLIST][20]; //   we store numeric  IP addresses, so this is plenty
unsigned int blacklistIndex = 0;
int userFirstLine;
char priorMessage[MAX_MESSAGE];

char computerID[MAX_WORD_SIZE];
char computerIDwSpace[MAX_WORD_SIZE];
char loginID[MAX_WORD_SIZE];    //   user FILE name (lower case)
char loginName[MAX_WORD_SIZE];    //   user typed name
char callerIP[100];
static FACT* userFactBase = 0;

void StartConversation(char* buffer)
{
	topicIndex = 0;
	rejoinder_Topic = rejoinder_at = NO_REJOINDER; 
	start_rejoinder_Topic = start_Rejoinder_at = NO_REJOINDER; //   block rejoinder resume

 	*readBuffer = 0;
	nextInput = buffer;	//   allow system to overwrite input here
	userFirstLine = inputCount+1;
	OnceCode("$control_pre");
    currentInput = "";
	Reply();
}

void PartialLogin(char* caller,char* ip)
{
    //   make user name safe for file system
	char*  id = loginID;
	char* at = caller-1;
	char c;
	while ((c = *++at)) 
	{
		if (IsAlphaOrDigit(c)) *id++ = c;
		else if (c == '_' || c == ' ') *id++ = '_';
	}
	*id = 0;

    sprintf(logbuffer,"USERS/log-%s.txt",loginID);
	if (ip) strcpy(callerIP,ip);
	else callerIP[0] = 0;
}

void Login(char* caller,char* usee,char* ip) //   select the participants
{
    if (*usee) strcpy(computerID,usee);
	if (!*computerID) ReadComputerID(); //   we are defaulting the chatee

	//   for topic access validation
	computerIDwSpace[0] = ' ';
	MakeLowerCopy(computerIDwSpace+1,computerID);
	strcat(computerIDwSpace," ");

	if ( *caller == '.' && ip) // make unique with IP adddress at end
	{
		char name[MAX_WORD_SIZE];
		strcpy(name,caller);
		sprintf(caller,"%s%s",ip,name);
	}

	// alternate to .user for non-unique internet systems, just make them guest with ip at end
	if (!stricmp(caller,"guest") && ip)
	{
		char word[MAX_WORD_SIZE];
		strcpy(word,ip);
		char* ptr;
		while ((ptr = strchr(word,'.'))) *ptr = '_';	// purify for file system
		sprintf(caller,"guest%s",ip);
	}

    //   prepare for chat
    PartialLogin(caller,ip);
 }

void ReadComputerID()
{
	strcpy(computerID,"anonymous");
	WORDP D = FindWord("defaultbot",0); // do we have a FACT with the default bot in it as verb
	if (D)
	{
		FACT* F = GetVerbHead(D);
		if (F)
		{
			D = Meaning2Word(F->subject);
			strcpy(computerID,D->word);
		}
	}
}

void Add2Blacklist(char* ip,unsigned int val)
{
    if (!ip || !val) return;   
    blacklistStartTime[blacklistIndex] = val;
	strcpy(blacklist[blacklistIndex],ip);
    if (++blacklistIndex >= MAX_BLACKLIST) --blacklistIndex;
}

bool Blacklisted(char* ip,char* caller) //   BUG- be able to disable this for P9 etc
{
	if (1) return 0; // DISABLE BLACKLIST
    for (unsigned int i = 0; i < blacklistIndex; ++i)
    {
        if (strcmp(blacklist[i],ip)) continue;
        return (blacklistStartTime[i] - (unsigned int) time(0)) > 0;
    }

    return false;
}

void WriteUserData(char* said)
{
	if (!topicNumber)  return; //   no topics ever loaded or we are not responding

	char ptr[MAX_WORD_SIZE];
    sprintf(ptr,"USERS/topic_%s_%s.txt",loginID,computerID);
	char name[MAX_WORD_SIZE];
	sprintf(name,"USERS/fact_%s.txt",loginID);

	// Backup allows one to ":revert" for debugging - if you see a bad answer and revert, you can reenter your data while tracing...
	if (!server)
	{
		CopyFile2File("TMP/topics.tmp",ptr,false);	// make backup of current topic in single user mode- aids in debugging
		CopyFile2File("TMP/facts.tmp",name,false);	// make backup of current topic facts in single user mode- aids in debugging
	}
    FILE* out = fopen(ptr,"wb");

	if (!out) 
	{
#ifdef WIN32
		system("mkdir USERS");
#endif
		out = fopen(ptr,"wb");
		if (!out)
		{
			ReportBug("cannot open user state file %s to write\r\n",ptr);
			return;
		}
	}
	fprintf(out,"%s\n",saveVersion); // format validation stamp

    //   turn off  uservars and dump user vars values
    WriteTopicData(out);

    //   recently used messages
    unsigned int i;
	int start = humanSaidIndex - 20;
	if (start < 0) start = 0;
    for (i = start; i < humanSaidIndex; ++i)   fprintf(out,"%s\n",NLSafe(humanSaid[i]));    //   what he said indented 3
	fprintf(out,"#end usr\n");

 	start = chatbotSaidIndex - 20;
	if (start < 0) start = 0;
    for (i = start; i < chatbotSaidIndex; ++i) fprintf(out,"%s\n",NLSafe(chatbotSaid[i]));    //   what we replied indented 1
	fprintf(out,"#end bot\n");

    //   write out fact sets
	fprintf(out,"%x #sets flags\n",(unsigned int) setControl);
    for (i = 1; i < MAX_FIND_SETS; ++i) 
    {
		if (!(setControl & (uint64) (1 << i))) continue; // purely transient stuff

		//   remove dead references
		FACT** set = factSet[i];
        unsigned int count = (ulong_t) set[0];
		unsigned int j;
        for (j = 1; j <= count; ++j)
		{
			FACT* F = set[j];
			if (!F || F->properties & (TRANSIENTFACT | DEADFACT))
			{
				memcpy(&set[j],&set[j+1],sizeof(FACT*) * (count - j));
				--count;
				--j;
			}
		}
        if (!count) continue;
        fprintf(out,"Fact %d %d \r\n",i,count); //   set and count
        for (j = 1; j <= count; ++j)
		{
			char word[MAX_WORD_SIZE];
			fprintf(out,"%s\n",WriteFact(factSet[i][j],false,word));
		}
    }
    fclose(out);

	//   now write user facts, if any have changed. Otherwise file is valid as it stands.
	bool written = (factFree == userFactBase); 
 	while (factFree > topicFacts) 
	{
		// see if any facts are actually important and "new"
		if (!written && !(factFree->properties & (TRANSIENTFACT|DEADFACT))) 
		{
			WriteFacts(fopen(name,"wb"),topicFacts); // write facts from here on down
			written = true;
		}
		FreeFact(factFree--); //   erase new facts
	}

}

void ReadUserData()
{	
	*priorMessage = 0;
	tokenControl = 0;	// change nothing of the data
    chatbotFacts = factFree; 
	sprintf(readBuffer,"USERS/topic_%s_%s%s.txt",loginID,computerID,fileID);
    FILE* in = fopen(readBuffer,"rb");
	*fileID = 0;
	if ( in) // check validity stamp
	{
		ReadALine(readBuffer,in);
		if (stricmp(readBuffer,saveVersion)) // obsolete format
		{
			fclose(in);
			in = 0;
		}
	}

    if (!in) //   this is a new user
    {
		ReadNewUser(); 
		start_rejoinder_Topic = rejoinder_Topic; 
		start_Rejoinder_at = rejoinder_at; 
		char* value = GetUserVariable("$token");
		tokenControl = (*value) ? atoi(value) : (DO_INTERJECTION_SPLITTING|DO_SUBSTITUTES|DO_NUMBER_MERGE|DO_PROPERNAME_MERGE|DO_SPELLCHECK|DO_UTF8_CONVERT);
		return;
    }

	ReadTopicData(in);
 
    //   ready in recently used messages
    for (humanSaidIndex = 0; humanSaidIndex < MAX_USED; ++humanSaidIndex) 
    {
        ReadALine(humanSaid[humanSaidIndex], in);
		if (humanSaid[humanSaidIndex][0] == '#') break; // #end
    }

	for (chatbotSaidIndex = 0; chatbotSaidIndex < MAX_USED; ++chatbotSaidIndex) 
    {
        ReadALine(chatbotSaid[chatbotSaidIndex], in);
		if (chatbotSaid[chatbotSaidIndex][0] == '#') break; // #end
    }

    //   read in fact sets
    char word[MAX_WORD_SIZE];
    *word = 0;
    ReadALine(readBuffer, in); //   setControl
	ReadHex(readBuffer,setControl);
    while (ReadALine(readBuffer, in)) 
    {
        char* ptr = ReadCompiledWord(readBuffer,word);
        unsigned int setid;
        unsigned int count;
        ptr = ReadInt(ptr,setid); 
        ptr = ReadInt(ptr,count); 
        factSet[setid][0] = (FACT*) count;
        for (unsigned int i = 1; i <= count; ++i) 
        {
            if (!ReadALine(readBuffer, in)) break; 
			ptr = readBuffer;
            factSet[setid][i] = ReadFact(ptr);
        }
    }
    
    fclose(in);

	start_rejoinder_Topic = rejoinder_Topic; 
	start_Rejoinder_at = rejoinder_at; 

	//   read user facts
    char name[MAX_WORD_SIZE];
    sprintf(name,"USERS/fact_%s.txt",loginID);
    ReadFacts(name,0); 

	userFactBase = factFree;	// note waterline of facts, to see if more are added..
	// BUG -- need delete of facts to alter userFactBase also.

	char* value = GetUserVariable("$token");
	tokenControl = (*value) ? atoi(value) : (DO_INTERJECTION_SPLITTING|DO_SUBSTITUTES|DO_NUMBER_MERGE|DO_PROPERNAME_MERGE|DO_SPELLCHECK|DO_UTF8_CONVERT);
	
	// for server logging
	char* prior = (chatbotSaidIndex < 1) ? ((char*)"") : chatbotSaid[chatbotSaidIndex-1];
	strcpy(priorMessage,prior);

 }

void ReadNewUser()
{
	userFirstLine = 1;
	inputCount = 0;
	ClearUserVariables();
	ResetTopicSystem();

	//   set his random seed
	bool hasUpperCharacters;
	unsigned int rand = (unsigned int) Hashit((unsigned char *) loginID,strlen(loginID),hasUpperCharacters);
	char word[MAX_WORD_SIZE];
	randIndex = rand & 4095;
    sprintf(word,"%d",randIndex);
	SetUserVariable("$randindex",word ); 
	strcpy(word,computerID);
	word[0] = toUppercaseData[(unsigned char)word[0]];
	SetUserVariable("$bot",word ); 
	SetUserVariable("$login",loginName);

	sprintf(readBuffer,"^%s",computerID);
	WORDP D = FindWord(readBuffer,0,LOWERCASE_LOOKUP);
	if (!D) 
	{
		ReportBug("Cannot find bot %s\r\n",computerID);
		return;
	}

	int oldjump = jumpIndex;
	char* macroArgumentList = AllocateBuffer();
	*macroArgumentList = 0;
	globalDepth = 2;
	unsigned int result;
	currentOutputBase = macroArgumentList;
	DoFunction(D->word,macroArgumentList,macroArgumentList,result);
	globalDepth = 0;
	jumpIndex = oldjump; //   return to  old error handler
	FreeBuffer();
}
