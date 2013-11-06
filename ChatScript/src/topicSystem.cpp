// topicSystem.cpp - handle maninpulating topics of rules

#include "common.h"

#ifdef INFORMATION

The first rule to generate normal output for the user will be both noted as a rejoinder location
and erased (if not prevented) so that the chatbot does not repeat itself.

There is a control stack of topics, which represents what topics are currently executing rules.
There is an interesting stack of topics, representing recent topics one wants to converse in.

The control stack is altered by functions that invoke TestRule (which matches patterns)
and ProcessRuleOutput which generates output.
TestRule: RejoinderCode  via FindTypedResponse: GambitCode & PerformTopic
 
#endif
uint64 setControl = 0;
bool debugResponder = false;
unsigned int rulesTested = 0;
unsigned int rulesMatched = 0;
unsigned int duplicateCount = 0;
static bool erasePending = false;
unsigned int topicID = 0;
int rejoinder_at;
int rejoinder_Topic;
int start_rejoinder_Topic = NO_REJOINDER;  //   what topic were we in, that we should check for update
int start_Rejoinder_at;
char fileID[10];

#define MAX_NO_ERASE 100
static char* noEraseSet[MAX_NO_ERASE];
static unsigned int noEraseIndex;

#define MAX_NESTED_TOPIC 10
#define TOPIC_LIMIT 3000 //   max items allowed to match test for topic

static char* startSets[MAX_NESTED_TOPIC][TOPIC_LIMIT]; //   buffers we can use that are not on the stack
static int topicStartIndex = 0;

//   current flow of topic control stack
unsigned int topicIndex = 0;
unsigned int topicStack[MAX_TOPIC_STACK];

//   interesting topics to reurn to
unsigned int topicInterestingIndex = 0;
unsigned int topicInterestingStack[MAX_TOPIC_STACK];

unsigned int topicNumber; //   how many topics exist 1...n

//   topic description
char** topicMap = NULL;           //   all topic data read in so far
char** topicNameMap = NULL;
static unsigned int* topicJumpMap = NULL;
static unsigned short* topicJumpCountMap = NULL;
char** topicRestrictionMap = NULL;	
unsigned int* topicRespondersMap = NULL;
unsigned char** topicUsedMap = NULL;
unsigned char** topicResponderDebugMap = NULL;
unsigned int* topicFlagsMap = NULL;
unsigned int* topicDebugMap = NULL;
uint64* topicChecksumMap = NULL;

char globalTopicFlags = 0;

unsigned char code[] = {//   value to letter  0-78 (do not use - since topic system cares about it) see uncode
    '0','1','2','3','4','5','6','7','8','9',
    'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','A','B','C','D',
    'E','F','G','H','I','J','K','L','M','N',
    'O','P','Q','R','S','T','U','V','W','X',
	'Y','Z','~','!','@','#','$','%','^','&',
	'*','?','/','+','=', '<','>',',','.',
};

unsigned char uncode[] = {//   letter to value - see code[]
    0,0,0,0,0,0,0,0,0,0,				// 0
    0,0,0,0,0,0,0,0,0,0,				// 10
    0,0,0,0,0,0,0,0,0,0,				// 20
    0,0,0,63,0,65,66,67,69,0,			// 30  33=! (63)  35=# (65)  36=$ (66) 37=% (67) 38=& (69)
    0,0,70,73,77,0,78,72,0,1,			// 40  42=* (70) 43=+ (73) 44=, (77)  46=. (78) 47=/ (72) 0-9 = 0-9
    2,3,4,5,6,7,8,9,0,0,				// 50
    75,74,76,71,64,36,37,38,39,40,		// 60  60=< (75)  61== (74)  62=> (76) 63=? (71) 64=@ 65=A-Z  (36-61)
    41,42,43,44,45,46,47,48,49,50,		// 70
    51,52,53,54,55,56,57,58,59,60,		// 80
    61,0,0,0,68,0,0,10,11,12,			// 90  90=Z  94=^ (68) 97=a-z (10-35)
    13,14,15,16,17,18,19,20,21,22,		// 100
    23,24,25,26,27,28,29,30,31,32,		// 110
    33,34,35,0,0,0,62,0,0,0,			// 120  122=z 126=~ (62)
};

void DummyEncode(char* &data)
{
	*data++ = 'a'; //   reserve space for encode
	*data++ = 'a';
#ifndef SMALL_JUMPS
	*data++ = 'a';
#endif
}

void Encode(unsigned int val,char* &ptr)
{ //   digits are base 64
#ifdef SMALL_JUMPS
	if (val > (USED_CODES*USED_CODES)) ReportBug("Encode val too big");
	ptr[0] = code[val / USED_CODES];
    ptr[1] = code[val % USED_CODES];
#else
	if (val > (USED_CODES*USED_CODES*USED_CODES)) ReportBug("Encode val too big");
	int digit1 = val / (USED_CODES*USED_CODES);
    ptr[0] = code[digit1];
	val -= (digit1 * USED_CODES * USED_CODES);
    ptr[1] = code[val / USED_CODES];
    ptr[2] = code[val % USED_CODES];
#endif
}

unsigned int Decode(char* data)
{
    unsigned int val;
#ifdef SMALL_JUMPS
	val = uncode[*data++] * USED_CODES;
    return val + uncode[*data++];
#else
 	val = uncode[*data++] * (USED_CODES*USED_CODES);
    val += uncode[*data++] * USED_CODES;
    return val + uncode[*data++];
#endif
}

void ResetTopicSentence()
{
    noEraseIndex = 0;
	topicStartIndex = 0;
	topicIndex = 0;				//   no pushed topics
	topicID = 0;
}

void ResetTopicSystem()
{
	globalTopicFlags = 0;
    for (unsigned int i = 1; i <= topicNumber; ++i) ResetTopic(i);
    rejoinder_Topic = rejoinder_at = NO_REJOINDER;
	topicIndex = 0;
	topicInterestingIndex = 0;
}

void AddNoErase(char* ptr)
{
    noEraseSet[noEraseIndex++] = ptr;
    if (noEraseIndex == MAX_NO_ERASE) --noEraseIndex;
}

void AddTopicFlag(unsigned int id,unsigned int flag)
{
    if (id > topicNumber || !id) ReportBug("AddTopicFlag flags id %d out of range\r\n",id);
    else  topicFlagsMap[id] |= flag;
}

void RemoveTopicFlag(unsigned int id,unsigned int flag)
{
    if (id > topicNumber || !id) ReportBug("RemoveTopicFlag flags id %d out of range\r\n",id);
    else 
	{
		topicFlagsMap[id] &= -1 ^ flag;
		AddTopicFlag(id,TOPIC_USED); 
	}
}

void ResetTopic(unsigned int id)
{
	if (id) 
	{
		memset(topicUsedMap[id],0,topicRespondersMap[id]);
		topicFlagsMap[id] &= -1 ^ TRANSIENT_FLAGS;
	}
	else ReportBug("Bad topic name to resettopic %s",ARGUMENT(1));
}

static unsigned int LoadTopicData(const char* name,unsigned int topic,unsigned int build)
{
	FILE* in = fopen(name,"rb");
	if (!in) return topic;
	char* data = (char*) malloc(MAX_TOPIC_SIZE);
	ReadALine(data,in); // skip count
	while (ReadALine(data,in,MAX_TOPIC_SIZE))  //~xtest 275 all 0n u: (_* ) ''_#0 who?`00 
	{
		++topic;
		char word[MAX_WORD_SIZE];
		char* ptr = ReadCompiledWord(data,word);	// TOPIC: keyword
		char* space = AllocateString(ptr); //   save all the topic data
		ptr = ReadCompiledWord(space,word); //   get topic name
		*(ptr-1) = 0;	//   end topic name
		if (!topicNameMap)
		{
			printf("TOPICS directory bad. Erase contents of it (not the directory itself) and start with :build 0 again. Interrupt to quit.\r\n");
			while(1);
		}
		WORDP D = StoreWord(word); 
		D->x.topicIndex = topic;
		AddSystemFlag(D,TOPIC_NAME|build|CONCEPT);  
		topicNameMap[topic] = D->word;

		//   flags
		ptr = ReadInt(ptr,topicFlagsMap[topic]); 
		ptr = ReadInt64(ptr,topicChecksumMap[topic]); 

		unsigned int TopLevelRules;
 		ptr = ReadInt(ptr,TopLevelRules); 
	
		unsigned int bytes = (TopLevelRules+7) / 8;	//   how many bytes needed to bit mask the responders
		if (!bytes) bytes = 1;
		topicUsedMap[topic] = (unsigned char*) AllocateString(NULL,bytes);
		memset(topicUsedMap[topic],0,bytes);
		topicResponderDebugMap[topic] = (unsigned char*) AllocateString(NULL,bytes);
		memset(topicResponderDebugMap[topic],0,bytes);
		topicRespondersMap[topic] = bytes;
		//   bot restriction if any
		ptr = SkipWhitespace(ptr);
		char* start = ptr;
		ptr = ReadCompiledWord(ptr,word); //   bot restriction
		*start = ' ';
		*(ptr-2) = ' ';
		*(ptr-1) = 0;	//   end bot restriction - double space after botname
		topicRestrictionMap[topic] = (!strstr(word," all ")) ? start : NULL;
		D->w.topicRestriction = topicRestrictionMap[topic];

		//   topic data
		topicMap[topic] = ptr;
		SetTopicData(topic,ptr);

		// now find the break between gambit and responder - a value of 0 means starts at front OR none exist
		ptr = GetTopicData(topic);
		unsigned int count = 0;
		while (ptr && *ptr && TopLevelGambit(ptr)) 
		{
			++count;
			ptr = FindNextResponder(NEXTTOPLEVEL,ptr);
		}
		topicJumpMap[topic] = (ptr) ? (ptr - GetTopicData(topic)) :  0;
		topicJumpCountMap[topic] = count; // what id is this one
	}
	fclose(in);
	free(data);
	return topic;
}


bool UsableResponder(unsigned int n,unsigned int topic) //   has this top level responder been used up
{
	unsigned int byteOffset = n / 8; 
	unsigned int bitOffset = n % 8;
	unsigned char* testByte = topicUsedMap[topic] + byteOffset;
	unsigned char value = (*testByte & (unsigned char) (0x80 >> bitOffset));
	return !value;
}

bool DebuggableResponder(unsigned int n) //   has this top level responder been marked for debug
{
	unsigned int byteOffset = n / 8; 
	unsigned int bitOffset = n % 8;
	unsigned char* testByte = topicResponderDebugMap[topicID] + byteOffset;
	unsigned char value = (*testByte & (unsigned char) (0x80 >> bitOffset));
	return value != 0;
}

void SetResponderMark(unsigned int topic,unsigned int n)
{
	unsigned int byteOffset = n / 8; 
	unsigned int bitOffset = n % 8;
	unsigned char* testByte = topicUsedMap[topic] + byteOffset;
	*testByte |= (unsigned char) (0x80 >> bitOffset);
	AddTopicFlag(topic,TOPIC_USED); 
}

void SetDebugMark(unsigned int n,int topic)
{
	unsigned int byteOffset = n / 8; 
	unsigned int bitOffset = n % 8;
	unsigned char* testByte = topicResponderDebugMap[topic] + byteOffset;
	*testByte ^= (unsigned char) (0x80 >> bitOffset);
	debugResponder = true;
}

void ClearResponderMark(unsigned int topic,unsigned int n)
{
	unsigned int byteOffset = n / 8; 
	unsigned int bitOffset = n % 8;
	unsigned char* testByte = topicUsedMap[topic] + byteOffset;
	*testByte &= -1 ^ (unsigned char) (0x80 >> bitOffset);
	AddTopicFlag(topic,TOPIC_USED); 
}

void ClearResponderMarks(unsigned int topic) //   erase all used bits
{
	memset(topicUsedMap[topicID],0,topicRespondersMap[topic]);
}

unsigned int FindLinearResponse(char type, char* buffer, unsigned int&id)
{
	unsigned int oldtrace = trace;
	if (trace)
	{
		if (TraceIt(MATCH_TRACE|PATTERN_TRACE)) id = Log(STDUSERTABLOG,"Topic: %s linear %c: ",GetTopicName(topicID),type);
		if (TraceIt(PATTERN_TRACE)) Log(STDUSERLOG,"\r\n");
	}
	char* ptr = GetTopicData(topicID);  
	char* responders = ptr + topicJumpMap[topicID];// start of responders
	unsigned int responderID = 0;
	if (type == STATEMENT || type == QUESTION || type == STATEMENT_QUESTION) 
	{
		responderID = topicJumpCountMap[topicID];
		ptr = responders; 
	}
	else if ( ptr == responders) return FAILRULE_BIT;	// no gambits exist

    unsigned int result = 0;
	unsigned int rindex = responseIndex;

	while (ptr && *ptr) //   find all choices-- layout is like "t: xxx () yyy"  or   "u: () yyy"  or   "t: this is text" -- there is only 1 space before useful label or data
	{
		if (!UsableResponder(responderID))
		{
			if (TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,"try %d: used up\r\n",responderID);
		}
		else if (*ptr == type || (type != GAMBIT  && type != RANDOM_GAMBIT && *ptr == STATEMENT_QUESTION)) //   is this the next unit we want to consider?
		{
			if (debugResponder && DebuggableResponder(responderID)) trace = -1;
			result = TestRule(responderID,ptr,buffer);
			trace = oldtrace;
 			if (result & (SUCCESSFULE_BIT|FAILTOPIC_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDTOPIC_BIT|ENDSENTENCE_BIT)) break;
			else if (responseIndex != rindex) break; //   got a new answer
		}
		else if (type == GAMBIT && ptr == responders) break;    //   gambits are first, so we ran out. If we had randomgambits we'd be in different code
		ptr = FindNextResponder(NEXTTOPLEVEL,ptr); //   go to next top level responder,
		++responderID;
	}
	if (result & (FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) return result; // stop beyond mere topic
	return (result & ENDCODES) ? FAILTOPIC_BIT : 0; 
}

unsigned int FindRandomResponse(char type, char* buffer, unsigned int&id)
{
	if (trace)
	{
		if (TraceIt(MATCH_TRACE|PATTERN_TRACE)) id = Log(STDUSERTABLOG,"Topic: %s random %c: ",GetTopicName(topicID),type);
		if (TraceIt(PATTERN_TRACE)) Log(STDUSERLOG,"\r\n");
	}
	char* ptr = GetTopicData(topicID);  
	char* responders = ptr + topicJumpMap[topicID];// start of responders
	unsigned int responderID = 0;
	if (type == STATEMENT || type == QUESTION || type == STATEMENT_QUESTION) 
	{
		responderID = topicJumpCountMap[topicID];
		ptr = responders; 
	}
	else if ( ptr == responders) return FAILRULE_BIT;	// no gambits exist

	//   gather the choices
	if (topicStartIndex == MAX_NESTED_TOPIC) 
	{
		ReportBug("too many nested topics");
		return FAILRULE_BIT;
	}
    char** starts = startSets[topicStartIndex++];
    unsigned int index = 0;
  	unsigned int idResponder[TOPIC_LIMIT];
  	unsigned int map[TOPIC_LIMIT];
	while (ptr && *ptr)
    {
		if (!UsableResponder(responderID))
		{
			if (TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,"try %d: used up\r\n",responderID);
		}
		else if (*ptr == type || (type != GAMBIT && type != RANDOM_GAMBIT && *ptr == STATEMENT_QUESTION)) 
		{
			idResponder[index] = responderID;
			map[index] = index;
			starts[index++] = ptr;
			if (index > TOPIC_LIMIT-1)  //   dont overflow
			{
               ReportBug("findTopic overflow");
               break; 
			}
        }
        else if ((type == GAMBIT || type == RANDOM_GAMBIT) && ptr == responders) break;			//   gambits are first, if we didn match we must be exhausted
		ptr = FindNextResponder(NEXTTOPLEVEL,ptr);
		++responderID;
   }

 	unsigned int result = 0;
	unsigned int rindex = responseIndex;
	unsigned int oldtrace = trace;
	//   we need to preserve the ACTUAL ordering information so we have access to the responseID.
    while (index)
    {
        int n = random(index);
		if (debugResponder && DebuggableResponder(idResponder[map[n]])) trace = -1;
        result = TestRule(idResponder[map[n]],starts[map[n]],buffer);
		trace = oldtrace;
		if (repeatable != 1000) repeatable = false;	//   if it set it, it is done using it now
		if (result & (SUCCESSFULE_BIT|ENDTOPIC_BIT|FAILTOPIC_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) break;
        else if (rindex != responseIndex) break;
        map[n] =  map[--index] ; //   erase choice
    }
	
	--topicStartIndex;
	if ( result & (FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) return result;
	return (result & ENDCODES) ? FAILTOPIC_BIT : 0; 
}

unsigned int FindRandomGambitContinuation(char type, char* buffer, unsigned int&id)
{
	if (trace)
	{
		if (TraceIt(MATCH_TRACE|PATTERN_TRACE)) id = Log(STDUSERTABLOG,"Topic: %s random %c: ",GetTopicName(topicID),type);
		if (TraceIt(PATTERN_TRACE)) Log(STDUSERLOG,"\r\n");
	}
	char* ptr = GetTopicData(topicID);  

	unsigned int result = 0;
	unsigned int rindex = responseIndex;
	unsigned int responderID = 0;
	bool available = false;
	unsigned int oldtrace = trace;
 	while (ptr && *ptr)
    {
		if (!UsableResponder(responderID))
		{
			if (TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,"try %d: used up\r\n",responderID);
			if (*ptr == RANDOM_GAMBIT) available = true; //   we are allowed to use gambits part of this subtopic
		}
		else if (*ptr == GAMBIT) 
		{
			if (available) //   we can try it
			{
				if (DebuggableResponder(responderID)) trace = -1;
				result = TestRule(responderID,ptr,buffer);
				trace = oldtrace;
				if (repeatable != 1000) repeatable = false;	//   if it set it, it is done using it now
				if (result & (SUCCESSFULE_BIT|ENDTOPIC_BIT|FAILTOPIC_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) break;
				else if (rindex != responseIndex) break;
			}
		}
		else if (*ptr == RANDOM_GAMBIT) available = false; //   this random gambit not yet taken
        else break; //   gambits are first, if we didn match we must be exhausted
		ptr = FindNextResponder(NEXTTOPLEVEL,ptr);
		++responderID;
	}
	if (result & (FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) return result;
	if (result & ENDCODES) return FAILTOPIC_BIT;
	if (rindex != responseIndex) return 0;
	return FAILRULE_BIT;  //   a flag that it should try random
}

unsigned int FindTypedResponse(char type,char* buffer,unsigned int& id)
{
	//   a returncode of FAILTOPIC will make it think it failed
    char* ptr = GetTopicData(topicID);  
    if (!ptr || !*ptr || GetTopicFlags(topicID) & TOPIC_BLOCKED) return FAILTOPIC_BIT;

	unsigned int result;
	unsigned int oldtrace = trace;
	if (topicDebugMap[topicID]) 
	{
		trace = topicDebugMap[topicID];
		if (trace == NO_TRACE) trace = 0; // not tracing this topic
		else if (trace) trace = -1;
	}
	globalDepth++;
	if (*ptr == RANDOM_GAMBIT && type == GAMBIT)
	{
		result = FindRandomGambitContinuation(type,buffer,id);
		if (result & FAILRULE_BIT) result = FindRandomResponse(type,buffer,id);
	}
	else if (GetTopicFlags(topicID) & TOPIC_RANDOM) result = FindRandomResponse(type,buffer,id);
	else result = FindLinearResponse(type,buffer,id);
	trace = oldtrace;
	globalDepth--;
    return result;
}

void WriteTopicData(FILE* out)
{ 
    char word[MAX_WORD_SIZE];
    //   dump current topics list and current rejoinder
    unsigned int i;

	WriteVariables(out);

     //   current location in topic system -- written by NAME, so topic data can be added (if change existing topics, must kill off lastquestion)
    *word = 0;
	if (rejoinder_at == NO_REJOINDER) fprintf(out,"%d %d %d 0 # glblflags, convo start, input#, no rejoinder\n",globalTopicFlags,userFirstLine,inputCount);
	else 
	{
		GetText(GetTopicData(rejoinder_Topic)+rejoinder_at,0,40,word,false);
		fprintf(out,"%d %d %d %s %d %llu %d # glblflags, convo start, input#, rejoinder %s\n",
			globalTopicFlags,userFirstLine,inputCount,GetTopicName(rejoinder_Topic),
			rejoinder_at,topicChecksumMap[rejoinder_Topic], topicRespondersMap[rejoinder_Topic],word);
	}
    if (topicIndex)  ReportBug("topic system failed to clear out topic stack\r\n");
   
	for (i = 0; i < topicInterestingIndex; ++i) fprintf(out,"%s ",GetTopicName(topicInterestingStack[i])); 
	fprintf(out,"#interesting\n");
 
    //   write out dirty topics
    for (i = 1; i <= topicNumber; ++i) 
    {
        char* name = topicNameMap[i];// actual name, not common name
        unsigned int flags = topicFlagsMap[i];
        if (!(flags & (TOPIC_USED|TOPIC_BLOCKED))) continue;
		if ( flags & TOPIC_SYSTEM) continue;

		// if this is a topic with a bot restriction and we are not that bot, we dont care about it.
		if (topicRestrictionMap[i] && !strstr(topicRestrictionMap[i],computerIDwSpace)) 
			continue;

		// see if topic is all virgin still. if so we wont care about its checksum and rule count or used bits
  		unsigned int size = topicRespondersMap[i];
   		unsigned char* bits = topicUsedMap[i];
		unsigned int test = size;
		while (test-- && !*bits++); // see if all off (default can save on read write)
		if (TOPIC_BLOCKED & flags || test <= size) // we dont write out unblocked topics that are pure default data
		{
			//   now write out data- if topic is not eraseable, it wont change, but used flags MIGHT (allowing access to topic)
 			char c = (flags & TOPIC_BLOCKED) ? '-' : '+';
			fprintf(out,"%s %c",name,c);
			if (test <= size) // if test goes negative, that becomes large positive
			{
				fprintf(out," %llu %d ",topicChecksumMap[i],size);
				bits = topicUsedMap[i];
				while (size--)
				{
					unsigned char value = *bits++;
					fprintf(out,"%c%c",((value >> 4) & 0x0f) + 'a',(value & 0x0f) + 'a');
				}
			}
			fprintf(out,"\n");
		}
    }
	fprintf(out,"#end topics\n"); 
}

void ReadTopicData(FILE* in)
{
    char word[MAX_WORD_SIZE];
	while (ReadALine(readBuffer, in)) //   user variables
	{
		if (*readBuffer != '$') break; // end of variables
        char* ptr = strchr(readBuffer,'=');
        *ptr = 0;
        SetUserVariable(readBuffer,ptr+1);
    }

    //   flags, status, rejoinder
	char* ptr = ReadCompiledWord(readBuffer,word); //   topic flags
    globalTopicFlags = (unsigned char) atoi(word);
  	ptr = ReadCompiledWord(ptr,word); 
    userFirstLine = atoi(word);
    ptr = ReadCompiledWord(ptr,word); 
    inputCount = atoi(word);
    ptr = ReadCompiledWord(ptr,word);  //   rejoinder topic name
	if (*word == '0')  rejoinder_Topic = rejoinder_at = NO_REJOINDER; 
    else
	{
		rejoinder_Topic  = FindTopicIDByName(word);
		unsigned int x;
		ptr = ReadInt(ptr,x);
		rejoinder_at = x;

		// prove topic didnt change (in case of system topic or one we didnt write out)
		uint64 checksum;
		ptr = ReadInt64(ptr,checksum);
		unsigned int size;
		ptr = ReadInt(ptr,size);
		if (!rejoinder_Topic || (checksum != topicChecksumMap[rejoinder_Topic] && topicChecksumMap[rejoinder_Topic]) || size != topicRespondersMap[rejoinder_Topic]) //   topic changed, flush our stuff
		{
			rejoinder_Topic = rejoinder_at = NO_REJOINDER; 
		}
	}

    //   topic interesting stack
    ReadALine(readBuffer, in);
    ptr = readBuffer;
    topicInterestingIndex = 0;
    while (ptr && *ptr) //   read each topic name
    {
        ptr = ReadCompiledWord(ptr,word); //   topic name
		if (*word == '#') break; //   comment ends it
        unsigned int id = FindTopicIDByName(word); 
        if (id) topicInterestingStack[topicInterestingIndex++] = id;
    }

    //   read in used topics
    char title[MAX_WORD_SIZE];
    while (ReadALine(readBuffer, in) != 0) 
    {
        int len = tokenLength; //   tokenlength by ReadALine is length of buffer filled
        if (len < 3) continue;
		if (*readBuffer == '#') break;  //   #end
        char* at = readBuffer;
        at = ReadCompiledWord(at,title); //   topic name
		WORDP D = FindWord(title);			
		if (!D || !(D->systemFlags & TOPIC_NAME)) continue; // should never fail unless topic disappears from a refresh
		unsigned int id = D->x.topicIndex;
		if (!id) continue;	//   no longer exists
		at = ReadCompiledWord(at,word); //   blocked status (+ ok - blocked)
		if (*word == '-') topicFlagsMap[id] |= TOPIC_BLOCKED|TOPIC_USED; // want to keep writing it out as blocked
		if (*at) // has more data
		{
			uint64 checksum;
			at = ReadInt64(at,checksum);
			unsigned int size;
			at = ReadInt(at,size);
			// a topic checksum of 0 implies it was changed manually, and set to 0 because  it was a minor edit.
			if ((checksum != topicChecksumMap[id] && topicChecksumMap[id]) || size != topicRespondersMap[id]) //   topic changed, flush our used rules map
			{
			}
			else // topic matches, read in our status with it
			{
				topicFlagsMap[id] |= TOPIC_USED; 
				unsigned char* bits = topicUsedMap[id];
				while (size--)
				{
					*bits++ = ((*at -'a') << 4) + (at[1] - 'a'); 
					at += 2;
				}
			}
		}
	
    }

	randIndex = atoi(GetUserVariable("$randindex")); //   the base rand assigned at chat birth
	randIndex += inputCount % MAXRAND; //   which input item is this and the rand base for it
 }

void SetRejoinder()
{
	if (executingID < 0 || rejoinder_at != NO_REJOINDER) return; //   not markable OR already set 
	if (GetTopicFlags(topicID) & TOPIC_BLOCKED) return; //   not allowed to be here (must have been set along the way)
 
	char level = TopLevelRule(executingRule)   ? 'a' :  (*executingRule+1); //   the next DEFAULT level for responses
	char* ptr = FindNextResponder(NEXTRESPONDER,executingRule);
    if (respondLevel) level = respondLevel; //   random selector wants a specific level to match. so hunt for that level to align at start.
    
    //   now align ptr to desired level. If not found, force to next top level unit
    bool startcont = true;
    //   dont use nested targeted choice units... cannot tell which level a jump belongs to easy.
    while (ptr && *ptr && !TopLevelRule(ptr)) //   walk units til find respond level matching
    {
        if (startcont && *ptr == level) break;     //   found desired starter
        if (!respondLevel && *ptr < level) //   only doing sequentials and we are exhausted
        {
            while (ptr && *ptr) //   skip to end of all rejoinders
            {
                ptr = FindNextResponder(NEXTRESPONDER,ptr); //   spot next unit is -- if ptr is start of a unit, considers there. if within, considers next and on
                if (TopLevelRule(ptr)) break; //   we CAN continue here as a gambit, if we stay in topic
            }
            break;
        }

        unsigned int priorlevel = *ptr;  //   we are here now
        ptr = FindNextResponder(NEXTRESPONDER,ptr); //   spot next unit is -- if ptr is start of a unit, considers there. if within, considers next and on
        startcont = (ptr && *ptr) ? (*ptr != (int)(priorlevel+1)) : false; //   we are in a subtree if false, rising, since  subtrees in turn are sequential, 
    }
 
    if (ptr && *ptr == level) //   will be on the level we want
    {
		//   need to insure base is the correct one.
		char* data = GetTopicData(topicID);
        rejoinder_at = ptr - data; //   resuming here for answers or gambit rejoinder  - having chosen correct base to use
 	    rejoinder_Topic = topicID;
        if (trace & OUTPUT_TRACE) 
        {
            GetText(GetTopicData(topicID),rejoinder_at,30,tmpword,false);
            Log(STDUSERLOG,"  **set rejoinder at %s  %s\r\n",GetTopicName(topicID),tmpword);
        }
    }
}

unsigned int PerformTopic(int active,char* buffer)//   MANAGE current topic full reaction to input (including rejoinders and generics)
{//   returns 0 if the system believes some output was generated. Otherwise returns a failure code
//   if failed, then topic stack is spot it was 
	unsigned int tindex = topicIndex;
	unsigned int id = 0;
	if (topicID == 0 || !topicMap) return FAILTOPIC_BIT;
	if (!active) active = (tokenFlags & QUESTIONMARK)  ? QUESTION : STATEMENT;
    //   does this topic handle the statement or question explicitly
    unsigned int result;

	if (topicRestrictionMap[topicID] && !strstr(topicRestrictionMap[topicID],computerIDwSpace)) result = FAILTOPIC_BIT;	//   not allowed this bot
    else result = FindTypedResponse((active == QUESTION || active == STATEMENT || active == STATEMENT_QUESTION ) ? (char)active : GAMBIT,buffer,id);

	//   flush any deeper stack back to spot we started
	if (result & (FAILRULE_BIT | FAILTOPIC_BIT | FAILSENTENCE_BIT)) topicIndex = tindex; 
	//   or remove topics we matched on so we become the new master path

    //   insure topic does not stay active (except as rejoinder) if it fails or is NOSTAY
    if (GetTopicFlags(topicID) & TOPIC_NOSTAY || result & (ENDTOPIC_BIT | FAILTOPIC_BIT | FAILRULE_BIT | FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) 
    {
        //   if rejoinder set in this topic, kill it if its top level, no real rejoinder exists
        if (topicID == (unsigned int) rejoinder_Topic && rejoinder_at != NO_REJOINDER)
        {
            char* data = GetTopicData(rejoinder_Topic);   //   make sure topic is available
            if (data) data += rejoinder_at;
            if (data && TopLevelRule(data)) rejoinder_at = NO_REJOINDER; //   Kill rejoinder
		}
    }
		
	if (trace && TraceIt(MATCH_TRACE|PATTERN_TRACE))
	{
		if (result & ENDRULE_BIT) Log(id,"--> EndRule ");
		else if (result & FAILRULE_BIT) Log(id,"--> FailRule ");
		else if (result & ENDTOPIC_BIT) Log(id,"--> EndTopic ");
		else if (result & FAILTOPIC_BIT) 
			Log(id,"--> FailTopic ");
		else if (result & ENDSENTENCE_BIT) Log(id,"--> EndSentence ");
		else if (result & FAILSENTENCE_BIT) Log(id,"--> FailSentence ");
		else if (result & ENDINPUT_BIT) Log(id,"--> EndInput ");
		else if (result & RETRY_BIT) Log(id,"--> Retry ");
		else if (result & SUCCESSFULE_BIT) Log(id,"--> Answer ");
		else Log(id,"--> OKTopic ");
		Log(STDUSERLOG,"%s   ",GetTopicName(topicID));
		Log(STDUSERLOG,"\r\n");
	}

	return (result & (FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) ? result : 0;
}

unsigned int TestRule(unsigned int responderID,char* rule,char* buffer)
{
	unsigned int start = 0;
retry:
    char* ptr = rule;
    ptr += 3;  //   now pointing at first useful data - skipping kind and space (eg: t: )
	char label[MAX_WORD_SIZE];
	*label = 0;
 	char c = *ptr;
	if (c== '(');	//   pattern area
	else if (c == ' ') ++ptr; //   no pattern
	else //   there is a label here - &aLABEL (
	{
		int len = c - '0'; //   size of label word + jump field + space
		if (trace && TraceIt(PATTERN_TRACE)) //   we want to see the label
		{
			strncpy(label,ptr+1,len-2);
			label[len-2] = 0;
		}
		ptr += len;			//   past jumpoffset + label and space-- points to (
	}

	unsigned int id;
	++rulesTested;	// mere statistics
	lastwordIndex = wordCount; //   current context
	if (*ptr == '(') //   pattern requirement to match
	{
		ptr += 2; //   skip over the paren and its blank after it
		if (trace && TraceIt(PATTERN_TRACE))
		{
			if (*label) id = Log(STDUSERTABLOG, "try %d %s: \\",responderID,label); //    the \\ will block linefeed on next Log call
			else id = Log(STDUSERTABLOG, "try %d: \\",responderID);
		}
		unsigned int wildcardSelector = 0;
		wildcardIndex = 0;
		unsigned int junk;
		unsigned int gap = 0;
		bool matched = Match(ptr,0,start,'(',true,gap,wildcardSelector,start,junk);  //   main normal match, returns start as the location for retry skip
		lastwordIndex = wordCount; //   current context
		if (!matched)  //   failed to match pattern
		{
			if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERLOG," FAIL\r\n");
			return FAILRULE_BIT; 
		}
	}
	else if (trace && TraceIt(PATTERN_TRACE)) //   gambits w/o patterns
	{
		if (*label) id = Log(STDUSERTABLOG, "try %s: \\",label); //    the \\ will block linefeed on next Log call
		else id = Log(STDUSERTABLOG, "try: \\");
	}
	
	if (trace && TraceIt(PATTERN_TRACE|MATCH_TRACE)) //   display the entire matching responder and maybe wildcard bindings
	{
		char junk[MAX_WORD_SIZE];
		if (!TraceIt(PATTERN_TRACE)) //   we werent showing patterns, show abstract of it for the match
		{
		    GetText(rule,0,40,junk,true);
			if (*label) id = Log(STDUSERTABLOG, "try %s: %s",label,junk); //    the \\ will block linefeed on next Log call
			else id = Log(STDUSERTABLOG, "try: %s",junk);
		}
		
		Log(STDUSERATTNLOG,"**Match: %s",GetText(ptr,0,40,junk,true)); //   show abstract result we will apply
		if (wildcardIndex)
		{
			Log(STDUSERLOG," wildcards: ");
			for (unsigned int i = 0; i < wildcardIndex; ++i)
			{
				if (wildcardOriginalText[i][0]) Log(STDUSERLOG,"_%d=%s ",i,wildcardOriginalText[i] );
			}
		}
		Log(STDUSERLOG,"\r\n");
	}

	globalDepth++;
	++rulesMatched;
	unsigned int result = ProcessRuleOutput(rule,responderID,buffer);
	globalDepth--;
	if (repeatable != 1000) repeatable = false; 
	erasePending = false;
	if (result & RETRY_BIT) goto retry;
	return result; 
}

char* GetTopicData(unsigned int i)
{
    if (!topicMap || !i) return NULL;
    if (i > topicNumber)
    {
        ReportBug("GetTopicData flags id %d out of range",i);
        return 0;
    }
    char* data = topicMap[i]; //   predefined topic or user private topic
    if (!data) return NULL;
    return data+JUMP_OFFSET; //   point past accellerator to the t:
}

int GetTopicFlags(unsigned int id)
{
	if (!id) return 0;
    if (id > topicNumber)
    {
        ReportBug("GetTopicFlags id %d out of range\r\n",id);
        return 0;
    }
    return topicFlagsMap[id];
}

void ResetTopics()
{
	for (unsigned int i = 1; i <= topicNumber; ++i) ResetTopic(i);
}

char* GetTopicName(unsigned int i,bool actual)
{
	if (!i || !topicNameMap || !topicNameMap[i]) 
		return "~unknown-topic";
	if (actual) return topicNameMap[i]; //   predefined topic

	static char name[MAX_WORD_SIZE];
	strcpy(name,topicNameMap[i]);
	char* dot = strchr(name,'.');
	if (dot) *dot = 0;
	return name;
}

void SetTopicData(unsigned int i,char* data)
{
    if (i > topicNumber) ReportBug("SetTopicData id %d out of range\r\n",i);
	else topicMap[i] = data; //   predefined topic
}

unsigned int FindTopicIDByName(char* name)
{
  	char word[MAX_WORD_SIZE];
	char word1[MAX_WORD_SIZE];
	if (!name || !*name) 
		return 0;
    
	word[0] = '~';
	MakeLowerCopy((*name == '~') ? word : (word+1),name);
	WORDP D = FindWord(word);
	duplicateCount = 0;
	while (D && D->systemFlags & TOPIC_NAME) 
	{
		unsigned int topic = D->x.topicIndex;
		if (!topicRestrictionMap[topic]) return topic;
		if (strstr(topicRestrictionMap[topic],computerIDwSpace)) return topic;

		++duplicateCount;
		sprintf(word1,"%s.%d",word,duplicateCount);
		D = FindWord(word1);
	}
	return 0;
}

char* ShowInterestingTopics()
{
	static char word[MAX_WORD_SIZE];
	*word = 0;
	char* ptr = word;
	for (int i = topicInterestingIndex-1; i >= 0; --i)
	{
		sprintf(ptr,"%s ",GetTopicName(topicInterestingStack[i])); 
		ptr += strlen(ptr);
	}
	return word;
}

void SetErase()
{ 
	if (!TopLevelRule(executingRule)) return; //   internal rejoinders disappear when top level responder is erased by DumpTopic

	if (*executingRule == 't' || *executingRule == 'r');	// gambits MUST erase
 	else if (GetTopicFlags(topicID) & TOPIC_NOERASE || globalTopicFlags & TOPIC_NOERASE)  //   someone executed ^noerase()  or system is just testing and doesnt want it real or topic is not eraseable
	{
		erasePending = true;
		return;
	}

	for (unsigned int i = 0; i < noEraseIndex; ++i) // not allowed to erase these
    {
        if (noEraseSet[i] == executingRule) 
		{
			erasePending = true;
			return; 
		}
	}

    if (trace & OUTPUT_TRACE)  
	{
        GetText(executingRule,0,30,tmpword,false);
 		Log(STDUSERTABLOG,"**erasing %s  %s\r\n",GetTopicName(topicID),tmpword);
	}
	SetResponderMark(topicID,executingID);
}

void UndoErase(char* ptr,unsigned int topic,unsigned int id)
{
    if (trace & BASIC_TRACE)  Log(STDUSERLOG,"Undoing erase %s\r\n",GetText(ptr,0,40,tmpword,true));
	ClearResponderMark(topic,id);
}

//   we are at AT. responses come AFTER the +
char* FindNextResponder(signed char level, char* ptr)
{ //   expect NEXTRESPONDER  NEXTTOPLEVEL or a level character
	if (!*ptr) return NULL; // at end, dont go farther

	char* start = ptr;
    char prior = TopLevelRule(ptr) ? ('a'-1) : *ptr; 
    if (level >= 0 && level <= 1) level = respondLevel; //   case default
    if (level == 0) level = 'a';           //   default
    if (ptr[1] != ':') 
	{
		GetText(ptr,0,40,tmpword,false); //   what text we'd want to see
		if (buildID)  BADSCRIPT("In topic %s missing colon for responder %s - look at prior responder for bug",GetTopicName(topicID) ,tmpword);
		ReportBug("not ptr start of responder %d %s %s - killing data",topicID, GetTopicName(topicID) ,tmpword);
		topicMap[topicID] = 0;
		return NULL;
	}
	ptr +=  Decode(ptr-JUMP_OFFSET);	//   now pointing to next responder
	if (level == NEXTRESPONDER || !*ptr) return ptr; //   find ANY next responder- we skip over + x x space to point ptr t: or whatever - or we are out of them now

	//   looking for a specific responder
    int cycle = 0;
    start = ptr; //   now beginning ptr next one
    while (*ptr) 
    {
        ++cycle;
		if (ptr[1] != ':')
		{
			if (buildID)  BADSCRIPT("Bad layout %c %s",level,start);
			ReportBug("Bad layout %c %s",level,start);
			return NULL;
		}
        if (TopLevelRule(ptr)) return ptr; //   no more in reactor zone
        if (level == NEXTTOPLEVEL);
        else if (ptr[0] == level) return ptr;     //   found what we wanted
        //   a rejoinder set is sequential. so if we hunt for the next and the series drops, abort
        //   if we are in C looking for D.... we dont want to find it in another responder. so if we see B or C, we have to drop it
        else if (level == (prior+1) && *ptr < prior) return FindNextResponder(NEXTTOPLEVEL,ptr); 
        //   but if we want F and its not a series contination (a skip) we can keep hunting.

		//   wrong response level, try again
        prior = *ptr;
		ptr += Decode(ptr-JUMP_OFFSET);	//   now pointing to next responder
    }
    return ptr;
}

unsigned int TestTopic(unsigned int newtopicID,bool test,char* buffer)
{ //   topic presumed loaded, see if it matches and let the results exist if it succeeds
	bool pushed = PushTopic(newtopicID);
    unsigned int result = PerformTopic(test ? 0 : GAMBIT,buffer);
	if (pushed) PopTopic();
    if (result & (FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT))  return result;
	if (result != 0) return FAILTOPIC_BIT;
    return 0;
}

unsigned int ProcessRuleOutput(char* rule, unsigned int id,char* buffer)
{
    unsigned int oldtopicID = topicID;    //   topic we start in - a topic command can change the topic. 

	//   get to output area
	char* ptr = rule + 3;  //   now pointing at first useful data (the pattern maybe)
 	char c = *ptr;
	if (c == ' ') ++ptr;	//   no pattern
	else
	{
		if (c != '(')  ptr += c - '0';	//   there is a label here - &aLABEL (- point to the paren
		ptr = BalanceParen(ptr,false);  
	}

   //   now process response
    unsigned int result;

	char* holdExecutingRule = executingRule;
	executingRule = rule;
	int holdExecutingID = executingID;
	executingID = id;

	unsigned int startingIndex = responseIndex;
	Output(ptr,buffer,result);
	bool answered = startingIndex != responseIndex;
	bool madeResponse = false;
	if (result & (FAILRULE_BIT | FAILTOPIC_BIT | FAILSENTENCE_BIT | ENDSENTENCE_BIT)) *buffer = 0; //   kill partial output
	else
	{
		if (answered) result |= SUCCESSFULE_BIT;
		result &= -1 ^ ENDRULE_BIT; //   this leaves, nothing or  or ENDTOPIC or SUCCESSFULE_BIT
		//   we will fail to add a response if:  we repeat  OR  we are doing a gambit or topic passthru
		 madeResponse = (*buffer != 0);
		if (*buffer) 
		{
			//   the topic may have changed but OUR topic matched and generated the buffer, so we get credit. change topics to us for a moment.
			//   How might this happen? We generate output and then call poptopic to remove us as current topic.
			unsigned int holdc = topicID;
			topicID = oldtopicID;
			//   since we added to the buffer and are a completed pattern, we push the entire message built so far.
			//   OTHERWISE we'd leave the earlier buffer content (if any) for higher level to push
			if (!AddResponse(currentOutputBase))
			{
				result |= FAILRULE_BIT;
 				madeResponse = false;
			}
			topicID = holdc;
			result |= SUCCESSFULE_BIT;
	   }

	}
	// if we actually put output into the stream, we want to erase ourselves if possible.
	// if we did a REUSE or a REFINE and someone else would erase but didnt, then erasePending will be set and WE become responsible
	if (madeResponse || erasePending)  SetErase(); 
	if (madeResponse || answered) SetRejoinder(); // we made a response directly or triggered one deeper

	if (startingIndex != responseIndex) AddInterestingTopic(topicID);

	executingRule = holdExecutingRule;
	executingID = holdExecutingID;
	respondLevel = 0; // in case set

    return result;
}

static void ReadPatternData(const char* name)
{
    FILE* in = fopen(name,"rb");
    char word[MAX_WORD_SIZE];
	if (!in) return;
	currentFileLine = 0;
	while (ReadALine(readBuffer,in) != 0) 
	{
		char* ptr = ReadCompiledWord(readBuffer,word); //   skip over double quote or QUOTE
		if (!*word) continue;
		if (word[0] == '\'') 
		{
			WORDP D = StoreWord(word+1,0); //   force entry in dictionary
			AddSystemFlag(D,PATTERN_WORD); 
		}
        else if (word[0] == '"')
        {
            int n = BurstWord(word); 
            WORDP D = StoreWord(JoinWords(n,false),0); //   force entry in dictionary of single-word version
 			AddSystemFlag(D,PATTERN_WORD); 
        }
        else 
        {
            char word1[MAX_WORD_SIZE];
            ReadCompiledWord(ptr,word1);
            WORDP D = StoreWord(word,0); //   full original word
			AddSystemFlag(D,PATTERN_WORD); 
        }
    }
    fclose(in);
}

static void InitKeywords(const char* name,unsigned int build)
{
	FILE* in = fopen(name,"rb");
	if (!in) return;
	currentFileLine = 0;
	bool endseen = true;
	MEANING  T;
	while (ReadALine(readBuffer, in)) //~hate (~dislikeverb )
	{
		char word[MAX_WORD_SIZE];
		char* ptr = SkipWhitespace(readBuffer);
		if (*readBuffer == '~' || endseen) // new synset definition (rejoinders are indented)
		{
			ptr = ReadCompiledWord(ptr,word); //   leaves ptr on next good word
			T = ReadMeaning(word,true);
			WORDP D = Meaning2Word(T);
			AddSystemFlag(D,build|CONCEPT); // defined as a concept or a topic
			if (*ptr != '(') //   has a description
			{
				WORDP C = StoreWord("comment",0);
				MEANING T = MakeMeaning(C,0);
				ptr = ReadCompiledWord(ptr,word);
				CreateFact(MakeMeaning(StoreWord(word)),T,T);
			}
			char* dot = strchr(word,'.');
			if (dot) // convert the topic family to the root name
			{
				*dot = 0;
				T = ReadMeaning(word,false);
			}
			ptr += 2;	//   skip the (and space
			endseen = false;
		}
		while (1)
		{
			ptr = ReadCompiledWord(ptr,word);
			if (*word == ')' ||  !*word  ) break; // til end of keywords or end of line
			MEANING U = ReadMeaning(word,true);
			CreateFact(U,Mmember,T,(*word == '\'') ? ORIGINAL_ONLY : 0); 
		}
		if (*word == ')') endseen = true;
	}
	fclose(in);
}

static void InitMacros(const char* name,unsigned int build)
{
	FILE* in = fopen(name,"rb");
	if (!in) return;
	currentFileLine = 0;
	while (ReadALine(readBuffer, in)) //   ^showfavorite O 2 _0 = ^0 _1 = ^1 ^reuse (~xfave FAVE ) 
	{
		if (!*readBuffer) continue;
		char word[MAX_WORD_SIZE];
		char* ptr = ReadCompiledWord(readBuffer,word); //   the name
		if (!*word) continue;
		WORDP D = StoreWord(word); 
		AddSystemFlag(D,FUNCTION_NAME|build); 
		D->x.codeIndex = 0;	//   if one redefines a system macro, that macro is lost.
		ptr = ReadCompiledWord(ptr,word);
		if (*word == 'P') AddSystemFlag(D,IS_PATTERN_MACRO); 
		ptr = ReadCompiledWord(ptr,word);
		D->v.argumentCount = *word-'0';
		D->w.fndefinition = AllocateString(ptr);
	}
	fclose(in);
}

static void InitTopicMemory()
{
	topicStack[0] = 0;
	unsigned int total;
	//   topic layout is:  count-of-topics
	//		topic1 flags checksum respondercount botrestriction topic-data
	//		...
	//		topicn flags checksum respondercount botrestriction topic-data
	topicNumber = 0;
	FILE* in = fopen("TOPIC/script0.txt","rb");
	if (in)
	{
		currentFileLine = 0;
		ReadALine(readBuffer,in);
		ReadInt(readBuffer,total);
		fclose(in);
		topicNumber += total;
	}
	in = fopen("TOPIC/script1.txt","rb");
	if (in)
	{
		currentFileLine = 0;
		ReadALine(readBuffer,in);
		ReadInt(readBuffer,total);
		fclose(in);
		topicNumber += total;
	}
	if (!topicNumber) return; // nothing there to load yet

	total = topicNumber+1;
    topicFlagsMap = (unsigned int*) AllocateString(NULL,sizeof(int) * total);  
    memset(topicFlagsMap,0,sizeof(int) * total);
    topicDebugMap = (unsigned int*) AllocateString(NULL,sizeof(int) * total);  
    memset(topicDebugMap,0,sizeof(int) * total);
    topicChecksumMap = (uint64*) AllocateString(NULL,sizeof(uint64) * total);  
    memset(topicChecksumMap,0,sizeof(uint64) * total);
    topicMap = (char**) AllocateString(NULL,sizeof(char*) * total);  
    memset(topicMap,0,sizeof(char*) * total);
    topicNameMap = (char**) AllocateString(NULL,sizeof(char*) * total); 
    topicRestrictionMap = (char**) AllocateString(NULL,sizeof(char*) * total); 
    memset(topicRestrictionMap,0,sizeof(char*) * total);
	topicUsedMap = (unsigned char**) AllocateString(NULL,sizeof(char*) * total);  
    memset(topicUsedMap,0,sizeof(int*) * total);
	topicResponderDebugMap = (unsigned char**) AllocateString(NULL,sizeof(char*) * total);  
    memset(topicResponderDebugMap,0,sizeof(int*) * total);
	topicRespondersMap = (unsigned int*) AllocateString(NULL,sizeof(int) * total);  
    memset(topicRespondersMap,0,sizeof(int) * total);
	topicJumpMap = (unsigned int*) AllocateString(NULL,sizeof(int) * total);  
    memset(topicJumpMap,0,sizeof(int) * total);
	topicJumpCountMap = (unsigned short*) AllocateString(NULL,sizeof(short) * total);  
    memset(topicJumpCountMap,0,sizeof(short int) * total);
	topicNameMap[0] = "";
 
	//   topic layout is:  count-of-topics
	//		topic1 flags checksum respondercount botrestriction topic-data
	//		...
	//		topicn flags checksum respondercount botrestriction topic-data
}
void RestartTopicSystem()
{
	//   purge any prior topic system - except any patternword marks made on basic dictionary will remain (doesnt matter if we have too many marked)
	while (factFree > dictionaryFacts) FreeFact(factFree--); //   restore to end of system
	ReturnDictionaryToWordNet(); // return dictionary and string space to pretopic conditions
	int topic = 0;
	InitTopicMemory();
	ClearBaseVariables();

	// read build0 layer
	ReadPatternData("TOPIC/patternWords0.txt");
	InitKeywords("TOPIC/keywords0.txt",BUILD0);

	InitMacros("TOPIC/macros0.txt",BUILD0);
	ReadFacts("TOPIC/facts0.txt",BUILD0); //   FROM topic system build of topics
	topic = LoadTopicData("TOPIC/script0.txt",topic,BUILD0);
	Build0LockDictionary();
	if (factFree != dictionaryFacts) printf("Read %s Build0 facts\r\n",StdNumber(factFree-dictionaryFacts));

	// read build1 layer
	ReadPatternData("TOPIC/patternWords1.txt");
	InitKeywords("TOPIC/keywords1.txt",BUILD1);
	InitMacros("TOPIC/macros1.txt",BUILD1);
	ReadFacts("TOPIC/facts1.txt",BUILD1); //   FROM topic system build of topics
	topic = LoadTopicData("TOPIC/script1.txt",topic,BUILD1);
	if (factFree != build0Facts) printf("Read %s Build1 facts\r\n\r\n",StdNumber(factFree-build0Facts));
	SetBaseVariables();
	ReadPosPatterns("LIVEDATA/posrules.txt"); // after concepts are defined but before lock of space

	fileID[0] = 0; 
    LockDictionary();
}

void GetActiveTopicName(char* buffer)
{
	unsigned int topic = topicID;
	*buffer = 0;

	// the real current topic might be the control topic or a user topic
	// when we are in a user topic, return that or something from the nested depths. Otherwise return most interesting topic.

	if (topic && !(GetTopicFlags(topic) & (TOPIC_SYSTEM|TOPIC_BLOCKED|TOPIC_NOSTAY)))
	{
		strcpy(buffer,GetTopicName(topic,false));
	}
	else if (topicIndex)
	{
		for (unsigned int i = topicIndex; i > 1; --i) // 0 will always be the null topic
		{
			if (!(GetTopicFlags(topicStack[i]) & (TOPIC_SYSTEM|TOPIC_BLOCKED|TOPIC_NOSTAY)))
			{
				strcpy(buffer,GetTopicName(topicStack[i],false));
				break;
			}
		}
	}
	if (!*buffer)
	{
		topic = GetInterestingTopic();
		if (topic) // put it back since Get removed it
		{
			AddInterestingTopic(topic);
			strcpy(buffer,GetTopicName(topic,false));
		}
	}
}

bool TopLevelQuestion(char* word)
{
	if (word[1] != ':') return false;
	if (word[0] != '?' && word[0] != 'u') return false;
	if (word[2] && word[2] != ' ') return false;
	return true;
}

bool TopLevelStatement(char* word)
{
	if (word[1] != ':') return false;
	if (word[0] != 's' && word[0] != 'u') return false;
	if (word[2] && word[2] != ' ') return false;
	return true;
}

bool TopLevelGambit(char* word)
{
	if (word[1] != ':') return false;
	if (word[0] != 'r' && word[0] != 't') return false;
	if (word[2] && word[2] != ' ') return false;
	return true;
}

bool TopLevelRule(char* word)
{
	if (!word || !*word) return true; //   END is treated as top level
	if (!stricmp(word,"#ai")) return true;	// treat this comment as a rule
	if (TopLevelGambit(word)) return true;
	if (TopLevelStatement(word)) return true;
	return TopLevelQuestion(word);
}

bool Continuer(char* word)
{
	if ((word[2] != 0 && word[2] != ' ') || word[1] != ':' || !IsAlpha(*word)) return false;
	return (*word >= 'a' && *word <= 'q') ? true : false;
}

bool IsRule(char* word)
{
	return TopLevelRule(word) || Continuer(word);
}


static bool HasGambits(int topic)
{
	char* data = GetTopicData(topic);
	unsigned int id = 0;
	while (data && *data)
	{
		if (TopLevelQuestion(data) || TopLevelStatement(data)) return false;

		if (!TopLevelGambit(data)) //   skip continuers
		{
			data = FindNextResponder(NEXTRESPONDER,data);
			continue;
		}

		if (UsableResponder(id))
		{
			char* ptr = data + JUMP_OFFSET;  //   now pointing at first useful data
 			char c = *ptr;
			if (c== '(');	//   pattern area
			else if (c == ' ') return true; //   no pattern
			else //   there is a label here - &aLABEL (
			{
				int len = c - '0'; //   size of label word + jump field + space
				ptr += len;			//   past jumpoffset + label and space-- points to (
			}
			ptr += 2; //   next real item
			//   want gambit with no real test in it
			if (*ptr == '*' && ptr[2] == ')') return true;	
		}
		else
		{
			if (TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,"try %d: used up\r\n",id);
		}
		++id;

		data = FindNextResponder(NEXTRESPONDER,data);
	}
	return false;
}

unsigned int GetTopicsWithGambits(char* buffer)
{ 
    unsigned int topicid;
	MEANING verb;
	MEANING object;
	verb = object = MakeMeaning(StoreWord("^gambittopics"));
    unsigned int start = 0;
	unsigned int count = 0;
	*buffer = 0;
    while (++start <= topicNumber) 
    {
        topicid = start;
        if (topicid == topicID) continue;
        char* cat = GetTopicData(topicid);
        if (!cat) continue;
        unsigned int flags = GetTopicFlags(topicid);
        if (flags & (TOPIC_SYSTEM|TOPIC_BLOCKED)) continue; //   never use system or blocked topics
		if (topicRestrictionMap[topicid] && !strstr(topicRestrictionMap[topicid],computerIDwSpace) ) continue;
		if (HasGambits(topicid)) 
		{
			if (impliedSet == ALREADY_HANDLED) // no place to store it, use output ONCE.
			{
				strcpy(buffer,GetTopicName(topicid));
				return 0;
			}
			MEANING T = MakeMeaning(StoreWord(GetTopicName(topicid)));
			FACT* F = CreateFact(T, verb,object,TRANSIENTFACT);
			if ( F)  
			{
				if (++count >= MAX_FIND) --count;
				factSet[impliedSet][count] =  F;
			}
		}
	}
	factSet[impliedSet][0] = (FACT*)count;
	impliedSet = impliedWild = ALREADY_HANDLED;	//   we did the assignment
	return (count) ? 0 : FAILRULE_BIT;
}

static int OrderTopics(unsigned short topicList[MAX_TOPIC_KEYS],unsigned int matches[MAX_TOPIC_KEYS])
{
	bool newpass = topicList[1] != 0;
	unsigned int max = 2;
    unsigned int index = 0;
    unsigned int i;
	char currentTopic[MAX_WORD_SIZE];
	GetActiveTopicName(currentTopic); //   first REAL topic, not system or nostay.
	unsigned int baseid = FindTopicIDByName(currentTopic);

	//  value on each topic
    for (i = 1; i <= topicNumber; ++i) // value 0 means we havent computed it yet. Value 1 means it has been erased.
    {
        char* name = GetTopicName(i);
	    if (GetTopicFlags(i) & (TOPIC_BLOCKED | TOPIC_SYSTEM) || i == baseid) continue; 
		if (topicRestrictionMap[i] && !strstr(topicRestrictionMap[i],computerIDwSpace)) continue;

	    unsigned int val = topicList[i];
        if (!val) //   compute match value
        {
            char word[MAX_WORD_SIZE];
            strcpy(word,name);
			char* dot = strchr(word,'.');
			if (dot) *dot = 0;	// use base name of the topic, not topic family name.
            WORDP D = FindWord(word); //   go look up the ~word for it
            if (!D) continue;

			// Note- once we have found an initial match for a topic name, we dont want to match that name again...
			// E.g., we have a specific topic for a bot, and later a general one that matches all bots. We dont want that later one processed.
  			if (D->stamp.inferMark == inferMark) continue;	// already processed a topic of this name
			D->stamp.inferMark = inferMark;

            //   look at references for this topic
            int start = 0;
            while (GetIthSpot(D,++start)) //   find all matches in sentene
            {
                // value of match of this topic in this sentence
                for (unsigned int k = positionStart; k <= positionEnd; ++k)
                {
					if (trace & OUTPUT_TRACE)  Log(STDUSERLOG,"%s->%s ",wordStarts[k],word);
                    val += 10 + strlen(wordStarts[k]);   //   each WORDP gets credit 10 and length of word is subcredit
					if (!stricmp(wordStarts[k],name+1)) val += 20; //   exact HIT on topic name
					if (!stricmp(wordCanonical[k],name+1)) val += 20;
                }
				if ( positionEnd < positionStart) // phrase subcomponent
				{
					if (trace & OUTPUT_TRACE)  Log(STDUSERLOG,"%s->%s",wordStarts[positionStart],word);
					val += 10;
  				}
				if (val && topicRestrictionMap[i]) val += 2;
            }

			//   Priority modifiers

			//   see if NAME of topic used in sentence as ordinary word
			D = FindWord(name+1,0);	
			if (GetIthSpot(D,0)) val += 20 + strlen(name);   

			if (GetTopicFlags(i) & TOPIC_PRIORITY && val) val  *= 3; //   triple its value
  			else if (GetTopicFlags(i) & TOPIC_LOWPRIORITY && val) val  /= 3; //   lower its value

			topicList[i] = (unsigned short)(val + 1); //   1 means we did compute it, beyond that is value
			if (trace && val > 1) 
			{
				Log(STDUSERLOG,"(%d) ",topicList[i]);
			}
		} //   close if

        if (val >= max) //   find all best
        {
            if (val > max) //   new high value
            {
                max = val;
                index = 0;
            }
            matches[++index] = i;
        }
    }
	if (trace && newpass ) Log(STDUSERLOG,"\r\n");
	matches[0] = max;
    return index;
}

unsigned int KeywordTopicsCode(char* buffer)
{	//   find  topics best matching his input words - never FAILS but can return 0 items stored
    unsigned short topicList[MAX_TOPIC_KEYS]; //   max topic count for now -- this is priority of topic choice
    memset(topicList,0,MAX_TOPIC_KEYS * sizeof(short)); //   clear counts
	int set = impliedSet;
	if (impliedSet != ALREADY_HANDLED) factSet[set][0] = 0;
	NextinferMark();

    //   now consider topics in priority order
    unsigned int counter = 0;
	unsigned int index;
    unsigned int matches[MAX_TOPIC_KEYS];
	while ((index = OrderTopics(topicList,matches))) //   finds all at this level. 1st call evals topics. other calls just retrieve.
    {
        //   see if equally valued topics found are feasible, if so, return one chosen at random
        while (index) 
        {
            unsigned int which = random(index) + 1; 
            unsigned int topic = matches[which];
			char word[MAX_WORD_SIZE];
			sprintf(word,"%s",topicNameMap[topic]);
			if (impliedSet == ALREADY_HANDLED) //   not embedded in an assignment statement
			{
				//   BUG addspace needed?
				strcpy(buffer,word);
				break;
			}
			char value[100];
			sprintf(value,"%d",matches[0]);
			MEANING M = MakeMeaning(StoreWord(word));
			factSet[set][++counter] = CreateFact(M,MakeMeaning(Dtopic),MakeMeaning(StoreWord(value)),TRANSIENTFACT);
            topicList[topic] = TOPIC_DONE; 
            matches[which] = matches[--index];  //   delete and try again
        }   
    }
	factSet[set][0] = (FACT*) (ulong_t)counter;
	impliedSet = ALREADY_HANDLED;
    return 0;
}

void AddInterestingTopic(unsigned int topic)
{//   these are topics we want to return to
//   topics added THIS turn are most interesting in first stored order
//   topics added previously should retain their old order 

	if (GetTopicFlags(topic) & (TOPIC_SYSTEM|TOPIC_NOSTAY|TOPIC_BLOCKED)) 	//   cant add this but try its caller
	{
		
		// may not recurse in topics
		for (unsigned int i = topicIndex; i >= 1; --i) // #1 will always be 0, the prior nontopic
		{
			topic = topicStack[i];
			char* name = topicNameMap[topic];
			if (i == 1) 
				return;
			if (GetTopicFlags(topic) & (TOPIC_SYSTEM|TOPIC_NOSTAY|TOPIC_BLOCKED)) continue;	//   cant 
			break;
		}
	}

	RemoveInterestingTopic(topic);	//   remove any old reference
	topicInterestingStack[topicInterestingIndex++] = topic;
	if (topicInterestingIndex >= MAX_TOPIC_STACK) //   too many, flush oldest
	{
		memcpy(&topicInterestingStack[0],&topicInterestingStack[1],sizeof(int) * (topicInterestingIndex-- - 1));
	}
}

void DumpInterestingTopics()
{
	for (unsigned int i = 0; i < topicInterestingIndex; ++i) Log(STDUSERLOG," %s ",topicNameMap[topicInterestingStack[i]]);
	if (topicInterestingIndex) Log(STDUSERLOG,"\r\n");
}

bool IsCurrentTopic(unsigned int topic)
{
	for (unsigned int i = 0; i < topicInterestingIndex; ++i)
	{
		if (topicInterestingStack[i] == topic) return true;
	}
	return false;
}

void RemoveInterestingTopic(unsigned int topic)
{
	for (unsigned int i = 0; i < topicInterestingIndex; ++i)
	{
		if (topicInterestingStack[i] == topic)
		{
			memcpy(&topicInterestingStack[i],&topicInterestingStack[i+1],sizeof(int) * (topicInterestingIndex-- - 1 - i));
			break;
		}
	}
}

unsigned int GetInterestingTopic()
{
	if (!topicInterestingIndex) return 0;	//   nothing there
	return topicInterestingStack[--topicInterestingIndex];
}

bool PushTopic(unsigned int topic)
{//   this is flow of control, what is currently executing rules.  
 
	if (topic == topicID) 
	{
		return false; 
	}

	// may not recurse in topics
	for (unsigned int i = 2; i <= topicIndex; ++i) // #1 will always be 0, the prior nontopic
	{
		if (topicStack[i] == topic) // we are already in this topic
		{
			ReportBug("recursing on topic %d %s",topic,topicNameMap[topic]);
			return false; 
		}
	}
  
	if (!topic) 
	{
		ReportBug("new topic missing");
		return false;
	}
    if (topicIndex >= MAX_TOPIC_STACK) 
    {
        ReportBug("Topic overflow");
        return false;	//   decline
    }

    topicStack[++topicIndex] = topicID;//   save old topic
	topicID = topic; // become new topic
    return true;
}

void PopTopic()
{
	if (topicIndex)   topicID = topicStack[topicIndex--];
	else topicID = 0;	// no topic now
}


char* FindNextLabel(char* label, char* ptr, unsigned int & id,bool alwaysAllowed)
{
	// Alwaysallowed (0=false, 1= true, 2 = rejoinder) would be true coming from enable or disable, for example, because we want to find the
	// label in order to tamper with its availablbilty. We can only find the first such label.

	// UsableResponder checks for TOP LEVEL rules only. A rejoinder of a top-level rule
	// is unavailable if the top level rule is not available. HOWEVER...
	// when testing for a rejoinder, if we are INSIDE that top level, it will be marked but
	// WE can still be found (as can anyone on the path including the top level)

	bool available = true;
	while (*ptr) //   always sits at t: x
	{
		char c = ptr[3];
		if (c != ' ' && c != '(') //   an accellerator before a label
		{
			c -= '0' + 2;	//   length of the label
			if (!strnicmp(ptr+4,label,c) && !label[c]) //   see if label matches
			{
				if (available || id == executingID) return ptr; //   matches and available or WITHIN continuer zone
			}
		}
		if (!Continuer(ptr)) //   a top level unit
		{
			available = (alwaysAllowed) ? true : UsableResponder(id);
			++id;
		}
		ptr = FindNextResponder(NEXTRESPONDER,ptr); //   go to end of this one and try again at next responder (top level or not)
	}
	return NULL;
}
