// systemVariables.cpp - answers queries on system values ( %variables )



#include "common.h"

static char systemValue[MAX_WORD_SIZE];

char* Sregression(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	return (regression != 0) ? (char*)"1" : (char*)"";
}

char* SAll(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	return (all != 0) ? (char*)"1" : (char*)"";
}

char* Srejoinder(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	return ( rejoinder_at != NO_REJOINDER) ? (char*)"1" : (char*)"";
}

char* Sserver(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    return (server) ? (char*) "1" : (char*) "";
}

char* Snoreact(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    return (noreact) ? (char*) "1" : (char*) "";
}


char* Ssecond(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    ReadCompiledWord(GetTimeInfo()+17,systemValue);
    systemValue[2] = 0;
    return systemValue;
}

char* Sminute(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	ReadCompiledWord(GetTimeInfo()+14,systemValue);
	systemValue[2] = 0;
	return systemValue;
}

char* Shour(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	sprintf(systemValue,"%d",atoi(GetTimeInfo()+11));
    return  systemValue;
}

char* SdayOfWeek(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    if (regression) return "Monday";
    ReadCompiledWord(GetTimeInfo(),systemValue);
    switch(systemValue[1])
    {
        case 'o': return "Monday";
        case 'u': return (char*)((systemValue[0] == 'T') ? "Tuesday" : "Sunday");
        case 'e': return "Wednesday";
        case 'h': return "Thursday";
        case 'r': return "Friday";
        case 'a': return "Saturday";
    }
	return "";
}

char* SdayNumberOfWeek(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	ReadCompiledWord(GetTimeInfo(),systemValue);
	int n;
    switch(systemValue[1])
    {
		case 'u': n = (systemValue[0] != 'T') ? 1 : 3; break;
		case 'o': n = 2; break;
		case 'e': n = 4; break;
		case 'h': n = 5; break;
		case 'r': n = 6; break;
		case 'a': n = 7; break;
		default: n = 0; break;
	}
	systemValue[0] = (char)(n + '0');
	systemValue[1] = 0;
	return systemValue;
}

char* SmonthNumber(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	if (regression) return "6";
    ReadCompiledWord(GetTimeInfo()+SKIPWEEKDAY,systemValue);
	switch(systemValue[0])
	{
		case 'J':  //   january june july 
			if (systemValue[1] == 'a') return "1";
			else if (systemValue[2] == 'n') return "6";
			else if (systemValue[2] == 'l') return "7";
		case 'F': return "2";
		case 'M': return (systemValue[2] != 'y') ? (char*)"3" : (char*)"5"; 
  		case 'A': return (systemValue[1] == 'p') ? (char*)"4" : (char*)"8";
		case 'S': return "9";
		case 'O': return "10";
        case 'N': return "11";
        case 'D': return "12";
	}
	return "";
}

char* SmonthName(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	if (regression) return "June";
    ReadCompiledWord(GetTimeInfo()+SKIPWEEKDAY,systemValue);
	switch(systemValue[0])
	{
		case 'J':  //   january june july 
			if (systemValue[1] == 'a') return "January";
			else if (systemValue[2] == 'n') return "June";
			else if (systemValue[2] == 'l') return "July";
		case 'F': return "February";
		case 'M': return (systemValue[2] != 'y') ? (char*)"March" : (char*)"May"; 
  		case 'A': return (systemValue[1] == 'p') ? (char*)"April" : (char*)"August";
		case 'S': return "September";
		case 'O': return "October";
        case 'N': return "November";
        case 'D': return "December";
	}
	return "";
}

char* SweekOfMonth(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    if (regression) return "1";
	unsigned int n;
    ReadInt(GetTimeInfo()+8,n);
	systemValue[0] = (char)('0' + (n/7) + 1);
	systemValue[1] = 0;
    return systemValue;
}

char* Sdate(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    ReadCompiledWord(GetTimeInfo()+8,systemValue);
    if (regression) return "1";
    return (systemValue[0] != '0') ? systemValue : (systemValue+1); //   1 or 2 digit date
}

char* Syear(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    ReadCompiledWord(GetTimeInfo()+20,systemValue);
    return (regression) ? (char*)"2001" : systemValue;
}

char* Stime(char* value)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    strncpy(systemValue,GetTimeInfo()+11,5);
    systemValue[5] = 0;
    return systemValue;
}

char* Srand(char* value) //   random 1 .. 100
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	sprintf(systemValue,"%d",random(100)+1);
	return systemValue;
}

char* Smore(char* value) //   pending additional user sentences exist
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    return moreToCome ? (char*)"1" : (char*)"";
}  

char* SmoreQuestion(char* value) //   pending additional user sentence is a question
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    return moreToComeQuestion ? (char*)"1" : (char*)"";
}   

char* Squestion(char* value) 
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
    return tokenFlags & QUESTIONMARK ? (char*)"1" : (char*)"";
}  

char* Suser(char* value) 
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	return tokenFlags & USERINPUT ? (char*)"1" : (char*)"";
}   

char* StopicName(char* value) //   current interesting topic name (not including system or no-stay)
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	GetActiveTopicName(systemValue);
    return systemValue;
}

char* SinputLine(char* value) //   number of inputs this user has given
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	sprintf(systemValue,"%d",inputCount); 
	return systemValue;
}

char* Slength(char* value) //   words in current sentence
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
 	sprintf(systemValue,"%d",lastwordIndex); 
	return systemValue;
}

char* SresponseCount(char* value) //   number of output lines stored
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	sprintf(systemValue,"%d",responseIndex);
	return systemValue;
}   

char* SuserStartSessionCount(char* value) //   number of user input which starts current session  %userfirstline
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	sprintf(systemValue,"%d",userFirstLine); 
	return systemValue;
}

char* Stense(char* value) //   tense of sentence if known, default PRESENT
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	if (tokenFlags & PAST) return "past";
	else if (tokenFlags & FUTURE) return "future";
	else return "present";
}

char* SlastOutput(char* value) //   last line we have for user  %lastoutput
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	return (responseIndex) ? responseData[responseIndex-1].response : (char*)"";
}

char* SlastQuestion(char* value) //   last response is a question?
{
	static char hold[50];
	if (value) return strcpy(hold,value);
	if (*hold != '.') return hold;
	if (!responseIndex) return "";
	char* sentence = responseData[responseIndex-1].response;
	unsigned int len = strlen(sentence);
	return (sentence[len-1] == '?') ? (char*)"1" : (char*)"";
}

SYSTEMVARIABLE sysvars[] =
{ 
	{"",0,""},
	{"%all",SAll,"Boolean is all flag on"},
	{"%date",Sdate,"Numeric day of the month"},
	{"%day",SdayOfWeek,"Named day of the week"},
	{"%daynumber",SdayNumberOfWeek,"Numeric day of week (0=sunday)"}, 
	{"%hour",Shour,"Numeric hour of day (0..23)"},
	{"%input",SinputLine,"Numeric total volleys in relationship"},
	{"%lastoutput",SlastOutput,"Last line of currently generated output"},
	{"%lastquestion",SlastQuestion,"Boolean did last output end in a ?"},
	{"%length",Slength,"Numeric words of current input sentence"},
	{"%minute",Sminute,"Numeric 2-digit current minute"},
	{"%month",SmonthNumber,"Numeric month number (1..12)"},
	{"%monthname",SmonthName,"Name of month"},
	{"%more",Smore,"Boolean is there more input pending"},
	{"%morequestion",SmoreQuestion,"Boolean is there a ? in pending input"},
	{"%question",Squestion,"Boolean is the current input a question"},
	{"%rand",Srand,"Numeric random number (1..100)"},
	{"%regression",Sregression,"Boolean is regression flag on"},
	{"%rejoinder",Srejoinder,"Boolean are we doing a rejoinder"},
	{"%response",SresponseCount,"Numeric count of responses generated for current input"},
	{"%second",Ssecond,"Numeric 2-digit current second"},
	{"%server",Sserver,"Boolean is this a server (vs standalone)"},
	{"%tense",Stense,"Tense of current input (present, past, future)"},
	{"%time",Stime,"Current military time (e.g., 21:07)"},
	{"%topic",StopicName,"Current topic executing"},
	{"%userfirstline",SuserStartSessionCount,"Numeric volley count at start of session"},
	{"%userinput",Suser,"Boolean is input coming from user"},
	{"%week",SweekOfMonth,"Numeric week of month (1..5)"},
	{"%year",Syear,"Current 4-digit year"},
	{"%noreact",Snoreact,"Boolean the noreact flag"},
	{NULL,NULL,""},
};

char* GetSystemVariable(char* word)
{
	WORDP D = FindWord(word);
	if (!D) return "";

	int index = D->x.topicIndex;
	return (index) ?  (*sysvars[index].address)(NULL) : (char*)"";
}

void OverrideSystemVariable(char* word,char* value)
{
	WORDP D = FindWord(word);
	if (D) 
	{
		int index = D->x.topicIndex;
		if (index) (*sysvars[index].address)(value);
	}
}

void DefineSystemVariables()
{
	int i = 0;
	while (sysvars[++i].name)
	{
		WORDP D = StoreWord((char*) sysvars[i].name);
		D->x.topicIndex = i;
		(*sysvars[i].address)("."); //   clear prehold value
	}
}

void DumpSystemVariables()
{
	int i = 0;
	while (sysvars[++i].name)
	{
		Log(STDUSERLOG,"%s - %s = %s\r\n",sysvars[i].name,sysvars[i].comment, (*sysvars[i].address)(NULL));
	}
}
