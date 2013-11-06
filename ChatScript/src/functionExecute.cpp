// functionExecute.cpp - handles predefined system functions that script can call


#include "common.h"
#ifdef INFORMATION

Function calls all run through DoCommand().
	
A function call can either be to a system routine or a user routine. 
	
User routines are like C macros, executed in the context of the caller, so the argument 
are never evaluated prior to the call. If you evaluated an argument during the mustering,
you could get bad answers. Consider:
	One has a function: ^foo(^arg1 ^arg2)  ^arg2 ^arg1
	And one has a call ^foo(($val = 1 ) $val )
This SHOULD look like inline code:  $val  $val = 1 
But evaluation at argument time would alter the value of $val and pass THAT as ^arg2. Wrong.

The macroArgumentList to a user function are in an array, whose base starts at userCallArgumentBase and runs
up to (non-inclusive) userCallArgumentIndex.

System routines are proper functions, whose macroArgumentList may or may not be evaluated. 
The macroArgumentList are in an array, whose base starts at index CallingArgumentBase and runs
up to (non-inclusive) CallingArgumentIndex. The description of a system routine tells
how many macroArgumentList it expects and in what way. Routines that set variables always pass
that designator as the first (unevaluated) argument and all the rest are evaluated macroArgumentList.

The following argument passing is supported:
	1. Evaluated - each argument is evaluated and stored (except for a storage argument). 
		If the routine takes optional macroArgumentList these are already also evaluated and stored, 
		and the argument after the last actual argument is a null string.
	2. STREAM_ARG - the entire argument stream is passed unevaled as a single argument,
		allowing the routine to handle processing them itself.

All calls have a context of "executingBase" which is the start of the rule causing this 
evaluation. All calls are passed a "buffer" which is spot in the currentOutputBase it
should write any answers.

Anytime a single argument is expected, one can pass a whole slew of them by making
them into a stream, encasing them with ().  The parens will be stripped and the
entire mess passed unevaluated. This makes it analogous to STREAM_ARG, but the latter
requires no excess parens to delimit it.

In general, the system does not test result codes on argument evaluations. So
issuing a FAILRULE or such has no effect there.

#endif

#include <complex>


int globalDepth = 0;


//   spot macroArgumentList are stored for system function calls
char systemArgumentList[MAX_ARG_LIST][MAX_WORD_SIZE];
int systemCallArgumentIndex = 0;
int systemCallArgumentBase = 0;

//   spot macroArgumentList are stored for user function calls
char macroArgumentList[MAX_ARGUMENT_COUNT+1][MAX_WORD_SIZE];    //   function macroArgumentList
unsigned int userCallArgumentIndex;
unsigned int userCallArgumentBase;
char lastInputSubstitution[MAX_BUFFER_SIZE];

void InitFunctionSystem() //   set dictionary to match builtin functions
{
	unsigned int k = 0;
	SystemFunctionInfo *fn;
	while ((fn = &systemFunctionSet[++k]) && fn->word )
	{
		WORDP D = StoreWord((char*) fn->word);
		AddSystemFlag(D,FUNCTION_NAME);
		D->x.codeIndex = k;
		D->v.argumentCount = fn->argumentCount;
	}
}

unsigned int CommandCode(char* buffer)
{
	char* word = ARGUMENT(1);
	DoCommand(word);
	return 0;
}

WORDP FindCommand(char* word)
{
	return FindWord(word);
}

unsigned int  CodeFail(char* buffer) //   when fail to find named one
{			
	return FAILTOPIC_BIT;
}

unsigned int NoLowerCaseCode(char* buffer) //   when fail to find named one
{	
	MakeLowerCase(ARGUMENT(1));
	WORDP D = FindWord(ARGUMENT(1),0,PRIMARY_CASE_ALLOWED);
	return (D) ? FAILRULE_BIT : 0;
}

unsigned int SetTopicFlagsCode(char* buffer)
{
	globalTopicFlags = atoi(ARGUMENT(1));	// set the globals
	return 0;
}

static unsigned int FactCode(char* buffer) //   when fail to find named one BUG?
{	
	FACT* F = GetFactRefDecode(ARGUMENT(1));
	if (!F) return FAILRULE_BIT;
	WORDP s = Meaning2Word(F->subject);
	WORDP v = Meaning2Word(F->verb);
	WORDP o = Meaning2Word(F->object);
	if (F > factFree) return FAILRULE_BIT;
	if (*ARGUMENT(2) == 's' || *ARGUMENT(2) == 'S') strcpy(buffer,WriteMeaning(F->subject,false));
	else if (*ARGUMENT(2) == 'v' || *ARGUMENT(2) == 'V') strcpy(buffer,WriteMeaning(F->verb,false));
	else if (*ARGUMENT(2) == 'o' || *ARGUMENT(2) == 'O') strcpy(buffer,WriteMeaning(F->object,false));
	else return FAILRULE_BIT;
	return 0;
}

static unsigned int DebugCode(char* buffer) //   when fail to find named one BUG?
{	
	return 0;
}

static unsigned int EvalCode(char* buffer) //   when fail to find named one BUG?
{	
	unsigned int result;
	Output(ARGUMENT(1),buffer,result,OUTPUT_EVALCODE); 
	return result;
}

static unsigned int RejoinderCode(char* buffer)
{ 
	unsigned int result = 0;
    if (!unusedRejoinder) 
	{
		if (trace & BASIC_TRACE) Log(STDUSERLOG," disabled continue\r\n\r\n");
		return 0; //   an earlier response handled this
	}

    //   was this topic ongoing and expecting the answer locally
    //   we last made a QUESTIONWORD or statement, can his reply be expected for that? 
    if (start_Rejoinder_at >= 0) //   rejoinder_at can be from other topics (like system internal ones called from this topic) bound to rejoinder_Topic, not topicID
    {
		bool pushed = PushTopic(start_rejoinder_Topic);
        char* data = GetTopicData(topicID);   //   make sure topic is available
		char* base = data;
        if (!data) 
		{
			if (trace & BASIC_TRACE) Log(STDUSERLOG," empty continue\r\n\r\n");
			if (pushed) PopTopic();
			return result;
 		}
        char* ptr = start_Rejoinder_at + data;
        if (!*ptr)  
		{
			if (pushed) PopTopic();
			return result;
		}
        if (ptr[1] != ':') 
        {
            ReportBug("bad rejoinder value");
			if (pushed) PopTopic();
			return result;
        }
		if (trace & BASIC_TRACE) Log(STDUSERLOG," topic: %s ",GetTopicName(topicID));

		char* start = ptr; //   we were waiting for THIS answer zone

		// first, find the covering top-level rule and make it visbile, so we can do reuse within this rejoinder zone
		unsigned int baseid = 0;
		while (base && base < start && *base)
		{
			base = FindNextResponder(NEXTTOPLEVEL,base);
			++baseid;
		}
		if (baseid) --baseid;	// this is the id of the top level unit covering us. IF is just a precaution for invalid data

        char level[400];
        char word[MAX_WORD_SIZE];
        ReadCompiledWord(ptr,level); //   what marks this level
        rejoinder_at = NO_REJOINDER;
		++globalDepth;
        if (TopLevelRule(start)); //   we have no answers ready for that QUESTIONWORD
		//   the responder must be one that matches the level- so this code replicates FindTypedResponse somewhat
        else while (*ptr) //   loop will search for a level answer it can use
        {
            ReadCompiledWord(ptr,word); //   read responder type
            if (!*word) break; //   no more data
            if (TopLevelRule(word)) break; //   failed to find - erasure will automatically happen when we leave topic and top level responder dies
            else if (*word < *level) break;  //   end of local choices
            else if (!stricmp(word,level)) //   check for answer to QUESTIONWORD
            {
				unsigned int result = TestRule(0,ptr,buffer);
				if (result & (FAILTOPIC_BIT|ENDTOPIC_BIT|FAILSENTENCE_BIT|ENDSENTENCE_BIT|ENDINPUT_BIT)) return result;
				if (!result) result = SUCCESSFULE_BIT; // we found a match, stop us
                if (result && !(result & FAILRULE_BIT)) //   we matched and did ANYTHING except claim we failed the rule
				{
                    unusedRejoinder = false;
					break; //   found and handled and used up, (rejoinder_at and rejoinder_Topic are set up - actually rejoinder_Topic remains unchanged )
                }
				result = 0;
                //   else is 0 or ENDRULE_BIT or FAILRULE_BIT- which just prompt us to try again
           }
           ptr = FindNextResponder(NEXTRESPONDER,ptr); //   wrong or failed responder, swallow this subresponder whole
        }
		--globalDepth;
		if (pushed) PopTopic();
    }

    //   you only get 2 tries of input to match our responders.
    //   when 1st sentence didnt handle it, we tried. When we fail, remove it, lest later in our processing
    //   a gambit tries to continue spot it already did from the 1st sentence reaction.
    if (sentenceCount) 
    {   
        start_Rejoinder_at = NO_REJOINDER;
        unusedRejoinder = false;
    }

    return result;
}

static unsigned int HasPropertyCode(char* buffer)
{
	WORDP D = FindWord(ARGUMENT(1));
	if (!D) return FAILRULE_BIT;
	uint64 flag = FindValueByName(ARGUMENT(2));
	if (!flag) return FAILRULE_BIT;
	return (D->properties & flag) ? 0 : FAILRULE_BIT;
}

static unsigned int  HasKeywordCode(char* buffer) //   when fail to find named one
{
	if (*ARGUMENT(1) == '$')  
	{
		char* at = GetUserVariable(ARGUMENT(1));
		if (at) strcpy(ARGUMENT(1),at);
	}
    WORDP D = FindWord(ARGUMENT(1),0); 
    return (GetNextSpot(D,0,positionStart,positionEnd)) ? 0 : FAILRULE_BIT; //   some topic keyword found in sentence?
}

#ifdef INFORMATION
^unpackfactref(to-set from) - reads facts from from and stores into toset any facts referred to by the facts
#endif

static unsigned int  UnpackFactRefCode(char* buffer)
{
	if (impliedSet == ALREADY_HANDLED) return FAILRULE_BIT;

	int from = atoi(ARGUMENT(1)+1);
	int count = (ulong_t)factSet[from][0];
	int newcount = 0;
	for (int i = 1; i <= count; ++i)
	{
		FACT* F = factSet[from][i];
		if (F->properties & FACTSUBJECT) 
		{
			factSet[impliedSet][++newcount] = GetFactPtr(F->subject);  
		}
		if (F->properties & FACTVERB) 
		{
			factSet[impliedSet][++newcount] = GetFactPtr(F->verb);  
		}
		if (F->properties & FACTOBJECT) 
		{
			factSet[impliedSet][++newcount] = GetFactPtr(F->object);  
		}
	}
	factSet[impliedSet][0] = (FACT*) newcount;
	impliedSet = ALREADY_HANDLED;
	return 0;
}

static unsigned int ResetCode(char* buffer)
{
	char* word = ARGUMENT(1);
	if (!stricmp(word,"USER"))
	{
		ReadNewUser(); 
		userFirstLine = 1;
		strcpy(buffer,"I have forgotten all about you.");
		AddResponse(buffer);
#ifndef NOTESTING
		wasCommand = false;	// lie so system will save revised user file
#endif
		return ENDINPUT_BIT;
	}
	if (!stricmp(word,"TOPIC"))
	{
		word = ARGUMENT(2);
		int id = topicID;
		if (*word == '~' ) 
		{
			id = FindTopicIDByName(word);  //   the topic in which we find ourselves at the moment
			ResetTopic(id);
		}
		else if (!*word) ResetTopics();
		else return FAILRULE_BIT;
		return 0;
	}
	return FAILRULE_BIT;
}

#ifdef INFORMATION
^addproperty(item flag flag1 flag2 ... )  - Adds flags to word or members of set/class
#endif

static unsigned int AddPropertyCode(char* buffer)
{
	char* arg1 = ARGUMENT(1);
	if (*arg1 == '~' ) return 0; //ignore
	WORDP D = StoreWord(arg1,0);
	if (!D) return FAILRULE_BIT;
	uint64 val = 0;
	uint64 sysval = 0;
	for (unsigned int i = 2; i < 10; ++i)
	{
		if (!*ARGUMENT(i)) break;
		if (!stricmp(ARGUMENT(i),"PATTERN_WORD")) sysval = PATTERN_WORD;
		else if (!stricmp(ARGUMENT(i),"GRADE5_6")) sysval = GRADE5_6;
		else if (!stricmp(ARGUMENT(i),"GRADE3_4")) sysval = GRADE3_4;
		else if (!stricmp(ARGUMENT(i),"GRADE1_2")) sysval = GRADE1_2;
		else if (!stricmp(ARGUMENT(i),"KINDERGARTEN")) sysval = KINDERGARTEN;
		else if (!stricmp(ARGUMENT(i),"TIMEWORD")) sysval = TIMEWORD;
		else if (!stricmp(ARGUMENT(i),"SPACEWORD")) sysval = SPACEWORD;
		else if (!stricmp(ARGUMENT(i),"PHRASAL_VERB")) sysval = PHRASAL_VERB;
		else if (!stricmp(ARGUMENT(i),"OTHER_SINGULAR")) sysval = OTHER_SINGULAR;
		else if (!stricmp(ARGUMENT(i),"OTHER_PLURAL")) sysval = OTHER_PLURAL;
		else if (!stricmp(ARGUMENT(i),"NOUN_MASS")) sysval = NOUN_MASS;
		else if (!stricmp(ARGUMENT(i),"VERB_CONJUGATE1")) sysval = VERB_CONJUGATE1;
		else if (!stricmp(ARGUMENT(i),"VERB_CONJUGATE2")) sysval = VERB_CONJUGATE2;
		else if (!stricmp(ARGUMENT(i),"VERB_CONJUGATE3")) sysval = VERB_CONJUGATE3;
		else if (!stricmp(ARGUMENT(i),"PREP_LOCATION")) sysval = PREP_LOCATION;
		else if (!stricmp(ARGUMENT(i),"PREP_TIME")) sysval = PREP_TIME;
		else if (!stricmp(ARGUMENT(i),"PREP_RELATION")) sysval = PREP_RELATION;
		else if (!stricmp(ARGUMENT(i),"PREP_INTRODUCTION")) sysval = PREP_INTRODUCTION;
		else if (!stricmp(ARGUMENT(i),"ALLOW_INFINITIVE")) sysval = ALLOW_INFINITIVE;
		else if (!stricmp(ARGUMENT(i),"POTENTIAL_CLAUSE_STARTER")) sysval = POTENTIAL_CLAUSE_STARTER;
		else if (!stricmp(ARGUMENT(i),"NOUN_BEING")) sysval = NOUN_BEING;
		else if (!stricmp(ARGUMENT(i),"EXISTENTIAL_BE")) sysval = EXISTENTIAL_BE;
		else if (!stricmp(ARGUMENT(i),"ADJECTIVE_OBJECT")) sysval = ADJECTIVE_OBJECT;
		else if (!stricmp(ARGUMENT(i),"ADJECTIVE_POSTPOSITIVE")) sysval = ADJECTIVE_POSTPOSITIVE;
		else if (!stricmp(ARGUMENT(i),"PREDETERMINER_TARGET")) sysval = PREDETERMINER_TARGET;
		else if (!stricmp(ARGUMENT(i),"OMITTABLE_TIME_PREPOSITION")) sysval = OMITTABLE_TIME_PREPOSITION;
		else if (!stricmp(ARGUMENT(i),"VERBS_ACCEPTING_OF_AFTER")) sysval = VERBS_ACCEPTING_OF_AFTER;
		else if (!stricmp(ARGUMENT(i),"SYSTEM_MORE_MOST")) sysval = SYSTEM_MORE_MOST;
		else if (!stricmp(ARGUMENT(i),"VERB_DIRECTOBJECT")) sysval = VERB_DIRECTOBJECT;
		else if (!stricmp(ARGUMENT(i),"VERB_INDIRECTOBJECT")) sysval = VERB_INDIRECTOBJECT;
		else 
		{
			uint64 bits = FindValueByName(ARGUMENT(i));
			if (!bits) Log(STDUSERLOG,"Unknown addproperty value %s\r\n",ARGUMENT(i));
			val |= bits;
		}
	}

	if (D->word[0] == '~' ) //   add to all members of set/class
	{
		FACT* F = GetObjectHead(D);
		while (F)
		{
 			if (F->properties & FACT_MEMBER)
			{
				D = Meaning2Word(F->subject);
				AddProperty(D,val);
				AddSystemFlag(D,sysval);
			}
			F = GetObjectNext(F);
		}
	}
	else 
	{
		AddProperty(D,val);
		AddSystemFlag(D,sysval);
	}
	return 0;
}

static unsigned int BlackListCode(char* buffer) //   BUG, can do without ptr arg
{
	unsigned int val;
	if (1) return 0;
	ReadInt(ARGUMENT(1),val);	//   minutes from now
	Add2Blacklist(callerIP,(val * 60) + (unsigned int) time(0));
	return 0;
}

static unsigned int AnalyzeCode(char* buffer)
{
	unsigned int result;
	char* word = ARGUMENT(1);
	Output(word,buffer,result);
	char* at;
	while ((at = strchr(buffer,'_'))) *at = ' '; // remove system underscoring back to blanks

	PrepareSentence(buffer,true,false); // not user input
	*buffer = 0;	// remove what we did there, only interested in effects
	return 0;
}

static unsigned int SetQuestionCode(char* buffer)
{
	tokenFlags |= (*ARGUMENT(1) != '0') ? QUESTIONMARK : 0;
	return 0;
}

static unsigned int ExplodeCode(char* buffer) //   take value and break into facts of burst-items as subjects
{
	char result[MAX_WORD_SIZE];
	result[0] = 0;

	//   prepare spot to store pieces
	int count;
	MEANING verb;
	MEANING object;
	if (impliedWild != ALREADY_HANDLED)  SetWildCardStart(impliedWild); //   start of wildcards to spawn
	object = verb = MakeMeaning(Dburst);
	count = 0;
	*buffer = 0;

	char* ptr = ARGUMENT(1) - 1; //   what to explode
	char word[MAX_WORD_SIZE];
	word[1] = 0;
	while (*++ptr)
	{
		word[0] = *ptr;
		//   store piece before scan marker
		if (impliedWild != ALREADY_HANDLED)  SetWildCard(word,word,0,0);
		else if (impliedSet != ALREADY_HANDLED)
		{
			MEANING T = MakeMeaning(StoreWord(word));
			FACT* F = CreateFact(T, verb,object,TRANSIENTFACT);
			if (F) 
			{
				if (++count >= MAX_FIND) --count;
				factSet[impliedSet][count] = F;
			}
		}
		else //   dump straight to output buffer, first piece only
		{
			strcpy(buffer,word);
			break;
		}
	}
	if (impliedSet != ALREADY_HANDLED)	factSet[impliedSet][0] = (FACT*)count;

	impliedSet = impliedWild = ALREADY_HANDLED;	
	return 0;
}

static unsigned int BurstCode(char* buffer) //   take value and break into facts of burst-items as subjects
{//   ^burst(^cause : )   1: data source 2: burst character
	char result[MAX_WORD_SIZE];
	result[0] = 0;

	//   prepare spot to store pieces
	int count;
	MEANING verb;
	MEANING object;
	if (impliedWild != ALREADY_HANDLED)  SetWildCardStart(impliedWild); //   start of wildcards to spawn
	object = verb = MakeMeaning(Dburst);
	count = 0;
	*buffer = 0;

	char* ptr = ARGUMENT(1); //   what to burst

	//   get string to search for. If quoted, remove the quotes
	char* scan = ARGUMENT(2);	//   how to burst
	if (!*scan) scan = "_"; // default
	else if (*scan == '"' ) // if a quoted string, remove the quotes
	{
		++scan;
		unsigned int len = strlen(scan);
		if (scan[len-1] == '"') scan[len-1] = 0;
	}

	//   loop that splits into pieces and stores them

	char* hold = strstr(ptr,scan);
	unsigned int scanlen = strlen(scan);
	while (hold)
	{
		*hold = 0;	//   terminate first piece

		//   store piece before scan marker
		if (impliedWild != ALREADY_HANDLED)  SetWildCard(ptr,ptr,0,0);
		else if (impliedSet != ALREADY_HANDLED)
		{
			MEANING T = MakeMeaning(StoreWord(ptr));
			FACT* F = CreateFact(T, verb,object,TRANSIENTFACT);
			if (F) 
			{
				if (++count >= MAX_FIND) --count;
				factSet[impliedSet][count] = F;
			}
		}
		else //   dump straight to output buffer, first piece only
		{
			strcpy(buffer,ptr);
			break;
		}

		ptr = hold + scanlen; //   ptr after scan marker
		hold = strstr(ptr,scan); //   find next piece
	}

	//   assign the last piece
	if (impliedWild != ALREADY_HANDLED)  
	{
		SetWildCard(ptr,ptr,0,0);
		SetWildCard("","",0,0); // clear next one
	}
	else if (impliedSet != ALREADY_HANDLED)
	{
		MEANING T = MakeMeaning(StoreWord(ptr));
		FACT* F = CreateFact(T, verb,object,TRANSIENTFACT);
		if (F)  
		{
			if (++count >= MAX_FIND) --count;
			factSet[impliedSet][count] = F;
		}
		factSet[impliedSet][0] = (FACT*)count;
	}
	else if (!*buffer) strcpy(buffer,ptr);
	impliedSet = impliedWild = ALREADY_HANDLED;	//   we did the assignment
	return 0;
}

static unsigned int GambitCode(char* buffer) 
{ //   gambit() means from current stack of topics.  gambit(~) means current topic
//   gambit(~name) means that topic and gambit (word) means topic with those keywords
	unsigned int oldindex = responseIndex;
 
	//   if generic "~" value, get current topic name to use for gambits
	char* word = ARGUMENT(1);
	if (*word == '~' && word[1] == 0) strcpy(word,GetTopicName(topicID,false));

   	if (!*word) //   pick prior topic from interesting stack
	{
		unsigned int topic;
        while ((topic = GetInterestingTopic())) //   back up topic stack, trying to find a gambit we can run
        {
			globalDepth++;
			unsigned int id;
			bool pushed =  PushTopic(topic);
			unsigned int result = FindTypedResponse(GAMBIT,buffer,id); 
			if (pushed) PopTopic();
			globalDepth--;
			if (result & ENDCODES) return result;
			if (oldindex != responseIndex) return 0;//   have a response
		}
		return 0;
	}

	int topic = FindTopicIDByName(word);
	if (topic) //   topic named
	{
		if (GetTopicFlags(topic) & TOPIC_BLOCKED) return FAILRULE_BIT;
 		bool pushed = PushTopic(topic);
		unsigned int id;
		unsigned int result = FindTypedResponse(GAMBIT,buffer,id);
		if (pushed) PopTopic();
		if (result == FAILTOPIC_BIT) result = FAILRULE_BIT; // he failed the topic, we fail as a rule
		return (result & ENDCODES) ? result : 0;
	}
	else return FAILRULE_BIT; //   unknown... maybe should have gotten fact intersect of topic and keywords BUG
}

static unsigned int DefineCode(char* buffer)
{ //   define(word) - give its definition
	char* start = buffer;
	WORDP D = FindWord(ARGUMENT(1),0);
	if (!D) return 0; //   failed to find the word.
	bool noun = false;
	bool verb = false;
	bool adjective = false;
	bool adverb = false;

	char* which = ARGUMENT(2);

	for (unsigned int i = 1; i <= D->meaningCount; ++i)
	{
		MEANING T = D->meanings[i]; // points to a master
		WORDP E = Meaning2Word(T);
		if (E->properties & NOUN && !noun && stricmp(which,"VERB"))
		{
			if (buffer != start) strcat(buffer++,"\n");
			sprintf(buffer,"Noun %s: %s.",ARGUMENT(1),E->w.gloss);
			buffer += strlen(buffer);
			noun = true;
       }
		if (E->properties & VERB && !verb && stricmp(which,"NOUN"))
		{
			if (buffer != start) strcat(buffer++,"\n");
			sprintf(buffer,"Verb %s: %s.",ARGUMENT(1),E->w.gloss);
			buffer += strlen(buffer);
			verb = true;
        }
		if (E->properties & ADJECTIVE && !adjective && !*which)
		{
			if (buffer != start) strcat(buffer++,"\n");
			sprintf(buffer,"Adjective %s: %s.",ARGUMENT(1),E->w.gloss);
			buffer += strlen(buffer);
			adjective = true;
        }
 		if (E->properties & ADVERB && !adverb && !*which)
		{
			if (buffer != start) strcat(buffer++,"\n");
			sprintf(buffer,"Adverb %s: %s.",ARGUMENT(1),E->w.gloss);
			buffer += strlen(buffer);
			adverb = true;
        }
	}
    return 0;
}

static unsigned int DisableCode(char* buffer) //   block named topic
{
	if (!stricmp(ARGUMENT(1),"topic"))
	{
		if (!*ARGUMENT(2)) return FAILRULE_BIT;
		if (!stricmp(ARGUMENT(2),"all"))
		{
			unsigned int start = 0;
			while (++start <= topicNumber) 
			{
				if (GetTopicFlags(start) & TOPIC_SYSTEM) continue;
				AddTopicFlag(start,TOPIC_BLOCKED|TOPIC_USED);
			}
			return 0;
		}
		int id = FindTopicIDByName(ARGUMENT(2));
		if (id) 
		{
			if (GetTopicFlags(id) & TOPIC_SYSTEM) return FAILRULE_BIT;
			AddTopicFlag(id,TOPIC_BLOCKED|TOPIC_USED);
			return 0;       
		}
	}
	else if (!stricmp(ARGUMENT(1),"rule")) // just the 1st one you find, not all
	{
		unsigned int id = 0;
		char* t = strchr(ARGUMENT(2),'.');	
		char* which;
		int topic = topicID;
		if (t)// topic.rule format
		{
			*t = 0;
			topic = FindTopicIDByName(ARGUMENT(2));
			if (!topic) return FAILRULE_BIT;
			which = FindNextLabel(t+1, GetTopicData(topic),id,true);  // presumed current topic
		}
		else which = FindNextLabel(ARGUMENT(2), GetTopicData(topicID),id,true);  // presumed current topic
		if (!which) return FAILRULE_BIT;
		if (GetTopicFlags(topic) & TOPIC_SYSTEM) return FAILRULE_BIT;
		SetResponderMark(topic,id);
		return 0;
	}
	else if (!stricmp(ARGUMENT(1),"rejoinder"))
	{
		rejoinder_at = NO_REJOINDER;
		return 0;
	}
	return FAILRULE_BIT;
}

static unsigned int DeleteQuerySetCode(char* buffer) //   delete all facts in collection
{
	int store = ARGUMENT(1)[1]-'0';
	unsigned int count = (ulong_t)factSet[store][0];
	for (unsigned int i = 1; i <= count; ++i)
	{
		FACT* F = factSet[store][i];
		F->properties |= DEADFACT;  
	}
	return 0;
}

static unsigned int LengthCode(char* buffer)
{

	if (*ARGUMENT(1) == '@') 
	{
		unsigned int store = ARGUMENT(1)[1] - '0';
		unsigned int count = (ulong_t)factSet[store][0];
		sprintf(buffer,"%d",count);
	}
	else sprintf(buffer,"%d",(int)strlen(ARGUMENT(1)));
	return 0;
}

#ifdef INFORMATION 
//-- allow optional commas on rguments in functions
^spellmatch(word pattern) - see if string matches pattern. return 0 if does or FAILRULE if not.

Pattern is a sequence of characters, with * matching 0 or more characters and . matching
exactly one. Pattern must cover the entire string. Pattern may be prefixed with a number, which
indicates how long the word must be. E.g.

^spellmatch("this_is_my","*is.m*")						# this matches
^spellmatch("this_is_my","*is.m")						# this fails since pattern doesnt cover end of word
^spellmatch("this_is_my","is.m*")						# this fails on 1st letter of pattern
^spellmatch("that_is_yours_but_this_is_mine","*is.m*") # this matches the second is
^spellmatch("hello","*")								# this matches
^spellmatch("hello","4*")								# this fails because hello is not 4 letters long
#endif

static unsigned int SpellMatchCode(char* buffer)
{//   ^spellmatch(word pattern)  Fails if pattern fails- eg  art*'s*b

	return (MatchesPattern(ARGUMENT(1),ARGUMENT(2))) ? 0 : FAILRULE_BIT;
}

static unsigned int StallCode(char* buffer)
{
	StallTest(false,ARGUMENT(1));
	return 0;
}

static unsigned int SubstituteCode(char* buffer) 
{ //   ^substitute(mode $input $old $new ) (mode=word or character) find old, replace with new (usually pronouns)

	char *out = buffer;
    char* found;
	char mode = *ARGUMENT(1);	//   'w' or 'c'
	char* sub = ARGUMENT(4);
	unsigned int newlen = strlen(sub);
	if (*sub == '"')
	{
		++sub; // skip opening quote
		--newlen;
		if (sub[newlen-1] == '"') sub[--newlen] = 0; // remove closing quote
	}
	while ((found = strchr(sub,'_'))) *found = ' '; //   convert any underscores to spaces in the substitute
	char* at = ARGUMENT(2);
	char* old = ARGUMENT(3);
    unsigned int lenmatch = strlen(old);
	if (*old == '"')
	{
		++old; // skip opening quote
		--lenmatch;
		if (old[lenmatch-1] == '"') old[--lenmatch] = 0; // remove closing quote
	}

	while((found = strstr(at,old))) //   perform all substitutions of word with word3 within word2, storing result eventually on word1 variable
    {
		//   Be careful NOT to find a partial match
		if (mode == 'w')
		{
			char c = found[lenmatch];	//   what follows us. If not a separator, this is not END of a word
			if (IsAlphaOrDigit(c))
			{
				at = found + lenmatch;
				continue;
			}
			//   what preceeds us. Should be start or a space
			if (found != ARGUMENT(2) && IsAlphaOrDigit(*(found-1))) 
			{
				at = found + lenmatch;
				continue;
			}
		}
		strncpy(out,at,found-at);   //   copy up to but not including pattern.
        out += found-at;
        at += found-at;
		at += lenmatch; 
		strcpy(out,sub);
		out += newlen;
	}
	strcpy(out,at);
    return 0;
}

#ifdef INFORMATION
^sort(@1) - sorts a fact set from highest [1] to lowest [n].

By default or if you use a subject reference like @3subject, the subjects
of facts are treated as float numbers and the basis for sort.  If you use an
object reference like @2object then the objects are treated as float numbers and
the facts sorted accordingly. Always successful.

#endif
static unsigned int SortCode(char* buffer)
{
	Sort(ARGUMENT(1));
	return 0;
}

#ifdef INFORMATION
^spell(pattern) - locates up to 100 words in dictionary matching pattern and stores them as facts in @0

Fails if no words are found. Words must begin with a letter and be marked as a part of speech
(noun,verb,adjective,adverb,determiner,pronoun,conjunction,prepostion).

Not all words are found in the dictionary. The system only stores singular nouns and base
forms of verbs, adverbs, and adjectives unless it is irregular.

Pattern is a sequence of characters, with * matching 0 or more characters and . matching
exactly one. Pattern must cover the entire string. Pattern may be prefixed with a number, which
indicates how long the word must be. E.g.

^spell("4*")	# retrieves 100 4-letter words
^spell("3a*")  # retrieves 3-letter words beginning with "a"
^spell("*ing") # retrieves words ending in "ing" 


#endif

static unsigned int SpellCode(char* buffer)
{
	return (Spell(ARGUMENT(1),0)) ? 0 : FAILRULE_BIT;
}

#ifdef INFORMATION
^countmember(set n) - retrieves the nth member of a :class or ~set

returns FAILRULE if n is beyond the set or set does not exist or storeid is bad. 
The first member has index 1.
#endif

#ifdef INFORMATION
^countmember(set) - counts how many top-level members in a ~set
#endif

static unsigned int CountMemberCode(char* buffer)
{
    WORDP D = FindWord(ARGUMENT(1),0);
    if (!D) return FAILRULE_BIT;
	int count = 0;
    FACT* F = GetObjectHead(D);
    while (F)
    {
		WORDP v = Meaning2Word(F->verb);
        if (v->systemFlags & FACT_MEMBER) ++count;//   find a fact type we are looking for
        F = GetObjectNext(F);
    }
	sprintf(buffer,"%d",count);
    return 0;
}

#ifdef INFORMATION
^sexed(word,male,female,neuter) - given word, picks one of the next three macroArgumentList based on sex of the word

If the word is not recognized, the it sex is used.

E.g., ^sexed(Georgina,he,she,it) would return she

#endif

static unsigned int SexedCode(char* buffer)
{
	WORDP D = FindWord(ARGUMENT(1));
	if (!D || !(D->properties & (NOUN_HE|NOUN_SHE))) //   NEUTER
	{
		strcpy(buffer,ARGUMENT(4)); //   it
	}
	else if (D->properties & NOUN_HE)
	{
		strcpy(buffer,ARGUMENT(2)); //   he
	}
	else
	{
		strcpy(buffer,ARGUMENT(3)); //   she
	}
	return 0;
}


#ifdef INFORMATION
Normally silent, you can get an output index using OUTPUT as a flag.
#endif

static unsigned int CreateFactCode(char* buffer)
{ 
	char* word = ARGUMENT(1);
	EatFact(word);
	return 0;
}

#ifdef INFORMATION
returns a random member of a set or class

returns FAILRULE if a bad set is given.

The value is recursive. If the random member chosen is a set or class, the
link is followed and a random member from the next level is chosen, and so on. 
If the value is a wordnet reference you get the synset node-- fix this sometime.

#endif

static unsigned int RandomMemberCode(char* buffer) 
{
    WORDP members[3000];
loop:
	WORDP D = FindWord(ARGUMENT(1));
	if (!D ) return FAILRULE_BIT;

    unsigned int index = 0;
    FACT* F = GetObjectHead(D);
    while (F && index < 2999)
    {
        if (F->properties & FACT_MEMBER) members[index++] = Meaning2Word(F->subject);
        F = GetObjectNext(F);
    }
    if (!index) return FAILRULE_BIT; //   none found

	//   pick one at random
    D = members[random(index)];
    char* answer = D->word;
	static char adjust[MAX_WORD_SIZE];
	if (D->properties & SUBSTITUTE) //   they may have < or > markers -- is this even possible BUG
	{
		if (D->word[0] == '<') answer += 1;
		unsigned int len = strlen(answer);
		if (answer[len-1] == '>')
		{
			strcpy(adjust,answer);
			adjust[len-1] = 0;
			strcpy(buffer,answer);
			return 0;
		}
	}
    if (*answer == '~') goto loop; //   member is a subset or class, get from it
    if (*answer == '<') ++answer; //   interjections have < in front
	strcpy(buffer,answer);
    return 0;
}

static unsigned int ExportFactCode(char* buffer)
{
	if (sandbox)  return FAILRULE_BIT;
	char* set = ARGUMENT(2);
	if (*set != '@') return FAILRULE_BIT;
	return (ExportFacts(ARGUMENT(1),set[1]-'0')) ? 0 : FAILRULE_BIT;
}

static unsigned int ImportFactCode(char* buffer)
{
	return (ImportFacts(ARGUMENT(1),ARGUMENT(2),ARGUMENT(3),ARGUMENT(4))) ? 0 : FAILRULE_BIT;
}

static unsigned int ComputeCode(char* buffer)
{
	int value = -1234567;
	char* arg1 = ARGUMENT(1);
	char* op = ARGUMENT(2);
	char* arg2 = ARGUMENT(3);
	//   for long digits, move to float
	if (strlen(arg2) >= 9 || strlen(arg1) >= 9 || strchr(arg1,'.') || strchr(arg2,'.') || !stricmp(op,"divide") || !stricmp(op,"root") || !stricmp(op,"square_root") || !stricmp(op,"quotient") || *op == '/') //   float
	{
		float value = -1234567;
		float number1 = (IsDigit(*arg1)) ? (float)atof(arg1) : (float)Convert2Integer(arg1);
		float number2 = (IsDigit(*arg2)) ? (float)atof(arg2) : (float)Convert2Integer(arg2);
		//   we must test case insenstive because arg2 might be capitalized (like add and ADD for attention deficit disorder)
		if (*op == '+' || !stricmp(op,"plus") || !stricmp(op,"add")|| !stricmp(op,"and")) value = number1 + number2; 
		else if (!stricmp(op,"minus") || !stricmp(op,"subtract")|| !stricmp(op,"deduct") || *op == '-' ) value = number1 - number2;
		else if (!stricmp(op,"x") || !stricmp(op,"time") || !stricmp(op,"multiply") || *op == '*') value = number1 * number2;
		else if (!stricmp(op,"divide") || !stricmp(op,"quotient") || *op == '/' ) 
		{
			if (number2 == 0) 
			{
				strcpy(buffer,"infinity");
				return 0;
			}
			else value = number1 / number2;
		}
        else if (!stricmp(op,"remainder") || !strcmp(op,"modulo") || !stricmp(op,"mod")|| *op == '%') ReportBug("illegal mod op in float");
        else if (!stricmp(op,"random") ) ReportBug("illegal random op in float");
        else if (!stricmp(op,"root") || !stricmp(op,"square_root") ) value = (float) sqrt(number1);  
        else if (!stricmp(op,"^") || !stricmp(op,"power") || !stricmp(op,"exponent")) 
        {
			int power = Convert2Integer(arg2);
            if (power >= 1 && power < 6)
            {
				value = number1;
				while (--power) value *= value;
			}
            else if (power == 0) value = 1;
		}
		if (value == -1234567) sprintf(buffer," ?");
		else 
		{
			char number[MAX_WORD_SIZE];
			int x = (int) value;
			if ((float)x == value) sprintf(number,"%d",x); //   simplify to integer
			else sprintf(number,"%.2f",value);
			StdNumber(number,buffer,0);
		}
	}
	else //   integer
    {
		char number[MAX_WORD_SIZE];
		int number1 = Convert2Integer(arg1);
		int number2 = Convert2Integer(arg2);
		if (*op == '+' || !stricmp(op,"plus") || !stricmp(op,"add")|| !stricmp(op,"and")) value = number1 + number2;
		else if (!stricmp(op,"minus") || !stricmp(op,"subtract")|| !stricmp(op,"deduct")|| *op == '-') value = number1 - number2;
		else if (!stricmp(op,"x") || !stricmp(op,"time") || !stricmp(op,"multiply") || *op == '*') value = number1 * number2;
		else if (!stricmp(op,"remainder") || !strcmp(op,"modulo") || !stricmp(op,"mod")|| *op == '%') value = number1 % number2;
		else if (!stricmp(op,"random")) value = random(number2-number1)+ number1; //   0 random 7 == 0,1,2,3,4,5,6  
 		else if (*op == '^' || !stricmp(op,"power") || !stricmp(op,"exponent"))
		{
			if (number2 >= 1 && number2 < 6)
			{
				value = number1;
				while (--number2) value *= value;
			}
			else if (number2 == 0) value = 1;
		}
        if (value == -1234567) sprintf(number,"?");
        else sprintf(number,"%d",value);
        StdNumber(number,buffer,0);
	}
	return 0;
}

static unsigned int EndCode(char* buffer)
{ //   good for stopping a loop w/o stopping the rule
	char* word = ARGUMENT(1);
	if (!stricmp(word,"RULE")) return ENDRULE_BIT;
	if (!stricmp(word,"TOPIC")) return ENDTOPIC_BIT;
	if (!stricmp(word,"SENTENCE")) return ENDSENTENCE_BIT;
	if (!stricmp(word,"INPUT")) return ENDINPUT_BIT;
    return FAILRULE_BIT;
}

static unsigned int RetryCode(char* buffer)
{
	unsigned int result;
	Output(ARGUMENT(1),buffer,result); //(space ) converted to space space 0
    return RETRY_BIT;
}

unsigned int QueryCode(char* buffer)
{ //   kind, s, v, o, count,  from, to, propogate, mark 
	unsigned int iarg1 = 0;
	MakeLowerCase(ARGUMENT(3));
	if (IsDigit(ARGUMENT(5)[0])) ReadInt(ARGUMENT(5),iarg1); // defaults to ? if not given
	if (iarg1 == 0) iarg1 = -1; // infinite
	//   how many args given? we need to default the rest to ?
	int argcount = systemCallArgumentIndex - systemCallArgumentBase - 1;
	for (unsigned int i = argcount + 1; i <= 9; ++i) strcpy(ARGUMENT(i),"");

	if (argcount < 2) return FAILRULE_BIT; //   must give code and SOMETHING to search on
	if (argcount != 9) while (++argcount <= 9) strcpy(ARGUMENT(argcount),"?"); //   default rest of macroArgumentList

	return Query(ARGUMENT(1),ARGUMENT(2), ARGUMENT(3), ARGUMENT(4), iarg1, ARGUMENT(6), ARGUMENT(7),ARGUMENT(8), ARGUMENT(9)) ? 0 : FAILRULE_BIT;
}

unsigned int QueryTopicsCode(char* buffer)
{
	unsigned int store = ARGUMENT(1)[1]-'0';
	if (!QueryTopicsOf(ARGUMENT(2),store,NULL)) return FAILRULE_BIT; //   unable to index (goes to set 0)
	return 0;
}

#ifdef INFORMATION
always returns in order, returns 3 + not + tense  BUG should be returning flags?
#endif

static unsigned int FLRCode(char* buffer,char which)
{  
	unsigned int store;
	*buffer = 0;
	char* word = ARGUMENT(1);
	if (*word != '@') ReportBug("bad store id");
	store = word[1] - '0';
	if (store > 9) store = 0;

	unsigned int count = (ulong_t)factSet[store][0];
	if (!count) 
	{
		if (impliedWild != ALREADY_HANDLED)
		{
			SetWildCardStart(impliedWild);
			SetWildCard("","",0,0); // subject
			SetWildCard("","",0,0);	// verb
			SetWildCard("","",0,0);	// object
			SetWildCard("","",0,0);	// flags
		}
		impliedWild = ALREADY_HANDLED;
		return ENDRULE_BIT; //   terminates but does not cancel output
	}
	
	if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"[%d] => ",count);
	
	// get the fact
	unsigned int choice;
	if (which == 'l') choice = count; //   last
	else if (which == 'f') choice = 1; //   first
	else choice = random(count) + 1;    //   pick random
	FACT* F = factSet[store][choice];
	if (!F) 
	{
		ReportBug("missing fact");
		impliedWild = ALREADY_HANDLED;
		return FAILRULE_BIT;
	}

	//   now remove fact from set, it is used up
	factSet[store][0] = (FACT*) (count-1);
    memcpy(&factSet[store][choice],&factSet[store][choice+1],sizeof(FACT*) * (count - choice));    //   delete  fact
			
	if (word[2] == 'f' || !word[2]) // return the fact intact
	{
		if (impliedSet != ALREADY_HANDLED)
		{
			unsigned int count =  (ulong_t)factSet[impliedSet][0] + 1;
			if (count >= MAX_FIND) return FAILRULE_BIT; 
			factSet[impliedSet][count] = F;
			factSet[impliedSet][0] = (FACT*) count;
		}
		else sprintf(buffer,"%d",Fact2Index(F));
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG," %s  ",buffer);
		impliedWild = ALREADY_HANDLED;
		impliedSet = ALREADY_HANDLED;
		return 0;
	}

	//   Search auto decodes a fact referecet. WE DONT WANT TO HERE, in case facts are nested.
	//   like the favorites system-- and we must match @0subject, @0verb, etc

	//   now have the fact, peel it onto wildcards
	MEANING Mfirst;
	uint64 flagsFirst;
	MEANING Mlast;
	uint64 flagsLast;
	char* wordx;
	char factref[100];
	if ( word[2] == 's')
	{
		MEANING M = F->subject;
		if (F->properties & FACTSUBJECT) sprintf(buffer,"%d",M);
		else sprintf(buffer,"%s",Meaning2Word(M)->word);
	}
	else if ( word[2] == 'v')
	{
		MEANING M = F->verb;
		if (F->properties & FACTVERB) sprintf(buffer,"%d",M);
		else sprintf(buffer,"%s",Meaning2Word(M)->word);
	}
	else if ( word[2] == 'o')
	{
		MEANING M = F->object;
		if (F->properties & FACTOBJECT) sprintf(buffer,"%d",M);
		else sprintf(buffer,"%s",Meaning2Word(M)->word);
	}
	else if ( word[2] == 'a' || word[2] == '+')
	{
		Mfirst = F->subject;
		flagsFirst = F->properties & FACTSUBJECT;
		Mlast = F->object;
		flagsLast = F->properties & FACTOBJECT;
	}
	else //  or -
	{
		Mlast = F->subject;
		flagsLast = F->properties & FACTSUBJECT;
		Mfirst = F->object;
		flagsFirst= F->properties & FACTOBJECT;
	}
	if ( word[2] == 'a' || word[2] == '+' || word[2] == '-' || word[2] ==  0 || word[2] ==  ' ') // spread
	{
		if (flagsFirst) 
		{
			sprintf(factref,"%d",Mfirst);
			wordx = factref;
		}
		else wordx = Meaning2Word(Mfirst)->word;
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG," %s  ",wordx);

		// _wildcard can take all, otherwise you get just a field
		// for variables. not legal for sets

		if (impliedWild == ALREADY_HANDLED) strcpy(buffer,wordx);
		else 
		{
			SetWildCardStart(impliedWild);
			SetWildCard(wordx,wordx,0,0); 

			 //   verb
			MEANING M = F->verb;
			if (F->properties & FACTVERB) 
			{
				sprintf(factref,"%d",M);
				wordx = factref;
			}
			else wordx = Meaning2Word(M)->word;
			SetWildCard(wordx,wordx,0,0);
			if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"%s  ",wordx);

			//   object
			if (flagsLast) 
			{
				sprintf(factref,"%d",Mlast);
				wordx = factref;
			}
			else wordx = Meaning2Word(Mlast)->word;
			SetWildCard(wordx,wordx,0,0); 
			if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"%s  ",wordx);

			if ( word[2] == 'a') // all
			{
				//   special flags
				char buf[MAX_WORD_SIZE];
				sprintf(buf,"0x%016llx",F->properties);
				SetWildCard(buf,buf,0,0);
				if (F->properties && trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"%s ",buf);
			}
		}
		impliedWild = ALREADY_HANDLED;	//   we have used up the data. cant have implied set here
	}
	return 0;
}

static unsigned int FLRCodeF(char* buffer)
{
	return FLRCode(buffer,'f');
}

static unsigned int FLRCodeL(char* buffer)
{
	return FLRCode(buffer,'l');
}

static unsigned int FLRCodeR(char* buffer)
{
	char* word = ARGUMENT(1);
	if (*word == '@') return FLRCode(buffer,'r');
	else if (*word == '~') return RandomMemberCode(buffer);
	else return FAILRULE_BIT;
}

unsigned int NoEraseCode(char* buffer)
{
	AddNoErase(executingRule); //   meaningless in rejoinders, since they only erase when parent goes. And gambits ALWAYS erase
	return 0;
}

unsigned int NotNullCode(char* buffer)
{
	unsigned int result;
	Output(ARGUMENT(1),buffer,result);
	if (*buffer) *buffer = 0;
	else return FAILRULE_BIT;
	return 0;
}

unsigned int RepeatCode(char* buffer)
{ 
	repeatable = true;//  local repeat allowed this round
	return 0;
}

static unsigned int EnableCode(char* buffer)
{
	char* what = ARGUMENT(2);
	if (!stricmp(ARGUMENT(1),"topic"))
	{
		 //   topic name to enable
		if (!*what) return FAILRULE_BIT;
		if (!stricmp(what,"all"))
		{
			unsigned int start = 0;
			while (++start <= topicNumber) 
			{
				if (GetTopicFlags(start) & TOPIC_SYSTEM) continue;
				RemoveTopicFlag(start,TOPIC_BLOCKED);
			}
			return 0;
		}
		int id = FindTopicIDByName(what);
		if (!id)
			return FAILRULE_BIT;
		if (GetTopicFlags(id) & TOPIC_SYSTEM) return FAILRULE_BIT;
		RemoveTopicFlag(id,TOPIC_BLOCKED);
		return 0;
	}
	if (!stricmp(ARGUMENT(1),"rule")) // just the first one you find
	{
		unsigned int id = 0;
		char* t = strchr(what,'.');	
		char* which;
		int topic = topicID;
		if (t)// topic.rule format
		{
			*t = 0;
			topic = FindTopicIDByName(what);
			if (!topic)
				return FAILRULE_BIT;
			which = FindNextLabel(t+1, GetTopicData(topic),id,true);  // presumed current topic
		}
		else which = FindNextLabel(what, GetTopicData(topic),id,true);  // presumed current topic
		if (!which) return FAILRULE_BIT;
		if (GetTopicFlags(topic) & TOPIC_SYSTEM) return FAILRULE_BIT;
		UndoErase(which,topic,id);
		AddTopicFlag(topic,TOPIC_USED); 
		return 0;
	}
	return FAILRULE_BIT;
}

unsigned int ReuseCode(char* buffer) //   likely only good above us, and only for looping back in a contination tree
{ 
	//   ^reuse can fail if answer has already been used and erased. REUSE of something should also erase it,
        //   but only by delayed erase... that is, change the responder type to DELAYERASE that cannot match and will autoerase on save...
        //   Rejoinder will be at the reuse line, not the line requesting reuse.
	unsigned int id = 0;
	char* word = ARGUMENT(1);
	unsigned int topicNumber = topicID;
	if (*word == '~') // topicname.label
	{
		char* dot = strchr(word,'.');
		if (dot)
		{
			*dot = 0;
			topicNumber = FindTopicIDByName(word);
			if (!topicNumber) topicNumber = topicID;
			word = dot+1;
		}
	}

	char* arg2 = ARGUMENT(2);
	bool alwaysAllowed = !*arg2; // default is we can reuse stuff
	char* which = FindNextLabel(word, GetTopicData(topicNumber),id,alwaysAllowed); // just 1st found
	if (!which) return FAILRULE_BIT;
	globalDepth++;

	char* oldrule = executingRule;
	executingRule = which;
	unsigned int oldtopic = topicID;
	topicID = topicNumber;
	unsigned int result = ProcessRuleOutput(which,id,buffer); 
	executingRule = oldrule;
	topicID = oldtopic;
	globalDepth--;

	if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERTABLOG,""); //   restore index from lower level
	return result;
}

#ifdef INFORMATION
Refine is a kind of switch. It tries each of its rejoinders in turn until one matches.
It will not try the rest when that happens. If the matched rejoinder does not fail, the refine
is considered not to have failed (though it may or may not have generated any output).
If the rejoinder returns a failcode, that is passed back as the result of the refine.
#endif

unsigned int RefineCode(char* buffer) 
{
	unsigned int result = 0;

    char level = 'a';
    if (*executingRule >= 'a' && *executingRule <= 'f') level = *executingRule+1;
    char* which = FindNextResponder(level,executingRule); //   go to end of this one and try again
    while (*which && !TopLevelRule(which)) //   try all choices, will NEVER be top level so always have (
    {
        char* ptr = which + 3; //   now pointing at first useful data (skip kind: and space)
		char c = *ptr;
		if (c== '(');	//   pattern area
		else ptr +=  c - '0'; //   there is a label here - &aLABEL (, get to paren
 		unsigned int gap = 0;
		unsigned int wildcardSelector = 0;
		wildcardIndex = 0;  //   reset wildcard allocation on top-level pattern match
		unsigned int junk;
		bool match = Match(ptr+2,0,0,'(',true,gap,wildcardSelector,junk,junk); 
        if (match)  //   matched pattern
        {
			if (trace && TraceIt(PATTERN_TRACE|MATCH_TRACE|OUTPUT_TRACE)) //   display the entire matching responder and maybe wildcard bindings
			{
				if (!TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,"Refined %s\r\n",GetText(ptr,0,40,tmpword,true)); //   we werent showing patterns, show abstract of it for the match
				else Log(STDUSERTABLOG,"Refine-match \r\n");
			}

			executingID = -1;
			globalDepth++;
            unsigned int result = ProcessRuleOutput(which,0,buffer); 
			globalDepth--;
			if (result & (ENDCODES|SUCCESSFULE_BIT)) return result;
			else return 0;
		}
		else  if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERLOG," FAIL\r\n");
        which = FindNextResponder(level,which); //   go to end of this one and try again
    }
    return FAILRULE_BIT;
}

unsigned int RhymeCode(char* buffer) 
{   
	char letter = ARGUMENT(1)[0];
	for (char i = 'a'; i <= 'z'; ++i)
	{
		if (i == letter || (i - 'a' + 'A') == letter) continue;    //   no repeat his word
		ARGUMENT(1)[0] = i;
		if (FindWord(ARGUMENT(1))) //   we have a word
		{
			strcpy(buffer,ARGUMENT(1));
			return 0;
		}
	}
	return 0;
}

unsigned int PrePrintCode(char* buffer) 
{     
	unsigned int result;
	char* word = ARGUMENT(1);
	Output(word,buffer,result);
    unsigned int index = responseIndex;
	AddResponse(buffer);
	int diff = responseIndex -index; // how many responses were generated.
	if (diff)
    {          
		int i;
		for (i = 0; i < diff; ++i) memcpy(&responseData[responseIndex+i],&responseData[responseIndex-diff-i],sizeof(RESPONSE));
		for (i = responseIndex-1-diff; i >= 0; --i) memcpy(&responseData[i+diff],&responseData[i],sizeof(RESPONSE));
		for (i = 0; i < diff; ++i) memcpy(&responseData[0+i],&responseData[responseIndex+i],sizeof(RESPONSE));
	}
	return result;
}

unsigned int PopTopicCode(char* buffer) 
{     
	if (*ARGUMENT(1) == '~') RemoveInterestingTopic(FindTopicIDByName(ARGUMENT(1)));
	else RemoveInterestingTopic(topicID); 
	return ENDTOPIC_BIT;
}

unsigned int POSCode(char* buffer)
{
	char* arg1 = ARGUMENT(1);
	char* arg2 = ARGUMENT(2);
	char* arg3 = ARGUMENT(3);
	if (!stricmp(arg1,"verb"))
	{
		char* infin = GetInfinitive(arg2); 
		if (!infin) return FAILRULE_BIT;
		if (!stricmp(arg3,"participle")) 
		{
			char* use = GetPresentParticiple(arg1);
			if (!use) return FAILRULE_BIT;
			strcpy(buffer,use);
			return 0;		
		}
		if (!stricmp(arg3,"infinitive")) 
		{
			strcpy(buffer,infin);
			return 0;		
		}
		if (!stricmp(arg3,"past")) 
		{
			char* past = GetPastTense(infin);
			if (!past) return FAILRULE_BIT;
			strcpy(buffer,past);
			return 0;
		}
	}
	else if (!stricmp(arg1,"aux"))
	{
		char* result = GetInfinitive(arg2);
		if (!result) return FAILRULE_BIT;
   
		if (!strcmp(arg2,"do")) //   present tense
		{
			if (strcmp(arg3,"I") && strcmp(arg3,"you")) result = "does"; 
			else result = "do";
		}
		else if (!strcmp(arg2,"have")) 
		{
			if (strcmp(arg3,"I") && strcmp(arg3,"you")) result = "has"; 
			else result = "have";
		}
		else if (!strcmp(arg2,"be")) 
		{
			if (!strcmp(arg3,"I") ) result = "am";
			else if (!strcmp(arg3,"you")) result = "are"; 
			else result = "is";
		}
		else if (!strcmp(arg2,"was") || !strcmp(arg2,"were")) //   past tense
		{
			if (!strcmp(arg3,"I") ) result = "was";
			result = "were";
		}
		else result = arg2;
		strcpy(buffer,result);
		return 0;
	}
	else if (!stricmp(arg1,"noun"))
	{
		if (!stricmp(arg3,"proper")) 
		{
			unsigned int n = BurstWord(arg2);
			for (unsigned int i = 1; i <= n; ++i)
			{
				char* word = GetBurstWord(i);
				WORDP D = FindWord(word,0,LOWERCASE_LOOKUP);
				if (D && D->properties & LOWERCASE_TITLE); //   allowable particles and connecting words that can be in lower case
				else *word = toUppercaseData[(unsigned char)*word];
				strcat(buffer,word);
				if (i != n) strcat(buffer," ");
			}
			return 0;
		}

		char* noun =  GetSingularNoun(arg2);
		if (!noun) return FAILRULE_BIT;
		if (!stricmp(arg3,"singular") || (atoi(arg3) == 1 && !strchr(arg3,'.'))) // allow number 1 but not float
		{
			strcpy(buffer,noun);
			return 0;		
		}
		else if (!stricmp(arg3,"plural") || IsDigit(arg3[0]) ) // allow number non-one
		{
			WORDP D = FindWord(noun);
			{
				//   swallow the args. for now we KNOW they are wildcard references
				char* plural = GetPluralNoun(StoreWord(noun));
				if (!plural) return FAILRULE_BIT;
				strcpy(buffer,plural);
			}
			return 0;
		}
	}
	else if (!stricmp(arg1,"determiner")) //   DETERMINER noun
	{
		unsigned int len = strlen(arg2);
		if (arg2[len-1] == 'g' && GetInfinitive(arg2)) //   no determiner on gerund
		{
			strcpy(buffer,arg2);
			return 0;
		}
		//   already has one builtinto the word or phrase
		if (!strnicmp(arg2,"a_",2) || !strnicmp(arg2,"an_",3) || !strnicmp(arg2,"the_",4)) 
		{
			strcpy(buffer,arg2);
			return 0;
		}

		WORDP D = FindWord(arg2);
		if (D && D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL))  //no determiner, is mass or proper name
		{
			strcpy(buffer,arg2);
			return 0;
		}

		//   if a plural word, use no determiner
		char* s = GetSingularNoun(arg2);
		if (!s || stricmp(arg2,s)) //   if has no singular or isnt same, assume we are plural and add the
		{
			sprintf(buffer,"the %s",arg2);
			return 0;
		}

		//   provide the determiner now
		*buffer++ = 'a';
		*buffer = 0;
		if (IsVowel(*arg2)) *buffer++ = 'n'; //   make it "an"
		*buffer++ = ' ';	//   space before the word
		strcpy(buffer,arg2);
		return 0;
	}
	else if (!stricmp(arg1,"place"))
	{
		int value = Convert2Integer(arg2);
		if ((value%10) == 1) sprintf(buffer,"%dst",value); 
		if ((value%10) == 2) sprintf(buffer,"%dnd",value);
		if ((value%10) == 3) sprintf(buffer,"%drd",value);
		else sprintf(buffer,"%dth",value);
		return 0;
	}
	else if (!stricmp(arg1,"capitalize"))
	{
		strcpy(buffer,arg2);
		buffer[0] = toUppercaseData[buffer[0]];
		return 0;
	}
	else if (!stricmp(arg1,"lowercase"))
	{
		MakeLowerCopy(buffer,arg2);
		return 0;
	}
	else if (!stricmp(arg1,"canonical"))
	{
		WORDP entry = NULL,canonical = NULL;
		uint64 flags = GetPosData(arg2,entry,canonical);
		if (canonical) strcpy(buffer,canonical->word);
		else if (entry) strcpy(buffer,entry->word);
		else strcpy(buffer,arg2);
		return 0;
	}
	return FAILRULE_BIT;
}

unsigned int AddTopicCode(char* buffer) 
{     
	if (*ARGUMENT(1) == '_' && ARGUMENT(1)[1] == 0) return 0; //   "_" from a fact?
	int topic = FindTopicIDByName(ARGUMENT(1));
	if (topic) //   does not fail, just may not do
	{
		AddInterestingTopic(topic); //   BECOMES an interesting topic
		rejoinder_at = NO_REJOINDER;
	}
	return 0;
}

unsigned int LogCode(char* buffer)
{
	unsigned int result;
	Output(ARGUMENT(1),buffer,result);
	Log(STDUSERLOG,buffer);
	*buffer = 0;
	return 0;
}

unsigned int MarkCode(char* buffer) 
{  
		// argument1 represents a word or ~set and then match variable location
	char word[MAX_WORD_SIZE];
	char word1[MAX_WORD_SIZE];
	char* ptr = ARGUMENT(1);
	unsigned int result;
	ptr = ReadCommandArg(ptr,word,result);// set
	if (result & ENDCODES) return result;

	ptr = SkipWhitespace(ptr);
	ptr = ReadCompiledWord(ptr,word1);  // the _data
	unsigned position = wordCount;
 	if (*word1)
	{
		if (*word1 != '_') return FAILRULE_BIT;
		position = wildcardPosition[word1[1] - '0']; // the match location
	}
	unsigned int endposition = position;
	ptr = ReadCompiledWord(ptr,word1);  // the _data
 	if (*word1 && *word1 != '_') return FAILRULE_BIT;
	if (*word1) endposition = wildcardPosition[word1[1] - '0'];

	WORDP D = FindWord(word);
	if (D)
	{
		if (position > wordCount) position = wordCount;
		MarkFacts(MakeMeaning(D),position,endposition);
		return 0;	//   we only need ONE idiom matched
	}
	else return FAILRULE_BIT;
}

unsigned int MatchCode(char* buffer) 
{     
	char word[MAX_WORD_SIZE];
	char word1[MAX_WORD_SIZE];
	char* at = ReadCompiledWord(ARGUMENT(1),word1);
	if (*word1 == '$' && !*at) strcpy(word,GetUserVariable(word1)); //   solitary user var, decode it  eg match($var)
	else if (*word1 == '_' && !*at) strcpy(word,wildcardCanonicalText[word1[1]-'0']); //   solitary user var, decode it  eg match($var)
	else strcpy(word,word1); // otherwise its match(some expression to match)
	if (*word)  strcat(word," )");

	unsigned int gap = 0;
	unsigned int wildcardSelector = 0;
	wildcardIndex = 0;  //   reset wildcard allocation on top-level pattern match
	unsigned int junk;
	if (!*word) return FAILRULE_BIT;	// NO DATA?
    bool match = Match(word,1,0,'(',true,gap,wildcardSelector,junk,junk) != 0;  //   skip paren and treat as NOT at start depth, dont reset wildcards- if it does match, wildcards are bound
    if (!match) return FAILRULE_BIT;
	return 0;
}

#ifdef INFORMATION
^join(stream of stuff ) - merge the stream and reformat as needed- IS THIS DIFFERENT FROM EVAL NORMAL - BUG
#endif

unsigned int JoinCode(char* buffer) 
{     
	char* original = buffer;
	char* ptr = ARGUMENT(1);
	unsigned int result;
    while (ptr)
	{
		char word[MAX_WORD_SIZE];
		char* at = ReadCompiledWord(ptr,word); 
        if (*word == ')' || !*word) break; //   done
		if (*word == '^' && word[1] == '"')
		{
			ReformatString(word+1);
			ptr = at;
		}
 		else 
		{
			ptr = ReadCommandArg(ptr,word,result);
			if (result & ENDCODES) return result;
		}
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"%s ",word);
        strcpy(buffer,word);
		buffer += strlen(buffer);
    }
	if (trace && TraceIt(OUTPUT_TRACE))
		Log(STDUSERLOG,") = %s ",original);
 	return 0;
}

#ifdef INFORMATION
^find(~pokerhand royal_flush) - given a set, find the ordered position of the 2nd argument in it
#endif

unsigned int FindCode(char* buffer) 
{   
	char word[MAX_WORD_SIZE];
	strcpy(word,JoinWords(BurstWord(ARGUMENT(2)),false)); //   the value to find
	WORDP D = FindWord(ARGUMENT(1));
	if (D)
	{
		int n = -1;
		FACT* set = GetObjectHead(D);  
		while (set ) // walks set MOST recent (right to left)
		{
			if (set->properties & FACT_MEMBER) 
			{
				++n;
				WORDP item = Meaning2Word(set->subject);
				if (!stricmp(item->word,word))
				{
					sprintf(buffer,"%d",n);
					return 0;
				}
			}
			set = GetObjectNext(set);
		}
		return FAILRULE_BIT;
	}
	return FAILRULE_BIT; 
}

unsigned int NoFailCode(char* buffer)
{      
	char token[MAX_WORD_SIZE];
	char* ptr = ARGUMENT(1);
	ptr = ReadCompiledWord(ptr,token);
	unsigned int result;
	Output(ptr,buffer,result);
	if (!stricmp(token,"RULE")) return result & (ENDTOPIC_BIT|FAILTOPIC_BIT|ENDSENTENCE_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT);
	if (!stricmp(token,"TOPIC")) return result & (ENDSENTENCE_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT);
	if (!stricmp(token,"SENTENCE") || !stricmp(token,"INPUT")) return 0;
	ReportBug("bad fail code to NOFAIL");
	return FAILRULE_BIT; // not a legal choice
}

unsigned int FailCode(char* buffer) 
{      
	char* word = ARGUMENT(1);
	if (!stricmp(word,"RULE")) return FAILRULE_BIT;
	if (!stricmp(word,"TOPIC")) return FAILTOPIC_BIT;
	if (!stricmp(word,"SENTENCE")) return FAILSENTENCE_BIT;
	return FAILRULE_BIT;
}

void AddInput(char* buffer)
{
	unsigned int n = BurstWord(buffer);
	char heavyBuffer[MAX_BUFFER_SIZE];
	strcpy(heavyBuffer,nextInput);

	strcpy(nextInput,"... "); //   visual separator that system ignores
	nextInput += 3;
	char* word = "";
	for (unsigned int i = 1; i <= n; ++i)
	{
		word = GetBurstWord(i);
        strcat(nextInput,word);
		strcat(nextInput," ");
	}
	strcat(nextInput,heavyBuffer);
	unsigned int len = strlen(nextInput);
	if (len > 800) nextInput[800] = 0;	//   safety limit

}

unsigned int InputCode(char* buffer) 
{      // when supplying multiple sentences, must do them in last first order
	if (inputCounter++ > 5) return FAILRULE_BIT;// limit per sentence reply (but adding this input and restarting will reset )
	if (totalCounter++ > 15) return FAILRULE_BIT; // limit per input from user
	//   SINCE PRONOUNS MAY BE REWRITING, KEEP whatever rejoinder we have pending

	if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"\r\n");
	unsigned int result;
	char* word = ARGUMENT(1);
	Output(word,buffer,result);
	char* p;
	while(( p = strchr(buffer,'_'))) *p = ' '; // separate underscored words that came from wildcard fillins and the like
	if (strcmp(lastInputSubstitution,buffer)) AddInput(buffer); //   dont allow direct repeat loops
	else return FAILRULE_BIT;
	strcpy(lastInputSubstitution,buffer);
    *buffer = 0;
	return 0;
}

unsigned int MarkedCode(char* buffer)
{
	WORDP D = FindWord(ARGUMENT(1),0);
	if (!D) return FAILRULE_BIT;
	return (GetNextSpot(D,0,positionStart,positionEnd)) ?  0 : FAILRULE_BIT;
}

unsigned int IntersectHierarchyCode(char* buffer) 
{      
	if (!HierarchyIntersect(ARGUMENT(1),ARGUMENT(2),ARGUMENT(3))) return FAILRULE_BIT;
	return 0;
}

unsigned int IntersectTopicsCode(char* buffer) 
{      
	if (!TopicIntersect(ARGUMENT(2),ARGUMENT(1))) return FAILRULE_BIT;
	return 0;
}

unsigned int UnmarkCode(char* buffer)
{
	// argument1 is word or ~set and then match variable location
	char word[MAX_WORD_SIZE];
	char word1[MAX_WORD_SIZE];
	char* ptr = ARGUMENT(1);
	unsigned int result;
	ptr = ReadCommandArg(ptr,word,result);// set
	if (result & ENDCODES) return result;
	ptr = SkipWhitespace(ptr);
	ptr = ReadCompiledWord(ptr,word1);  // the _data
	unsigned position = wordCount;
 	if (*word1 && *word1 != '_') return FAILRULE_BIT;
	position = wildcardPosition[word1[1] - '0']; // the match location

	WORDP D = FindWord(word); //   set or word to unmark
	if (D) 
	{
		if (D->v.patternStamp != matchStamp) return 0; // not marked

		uint64 val = GetWhereInSentence(D);

		//   see if data on this position is already entered
		int shift = 0;
		uint64 has;
		while (shift < PHRASE_STORAGE_BITS && (has = ((val >> shift) & PHRASE_FIELD )) != 0) 
		{
			unsigned int began = has & PHRASE_START; // his field starts here
			if ( began == position) // we need to erase him
			{
				uint64 beforeMask = 0;
				for (unsigned int i = shift; i > 0; --i) beforeMask |= 0x01ULL << (i-1); // create mask for before
				uint64 before = val & beforeMask;
				uint64 after = val & (-1 ^ beforeMask);	// all the others
				after >>= PHRASE_BITS;	// lose the data
				before |= after;
				SetWhereInSentence(D,before);
				break;
			}

			shift += PHRASE_BITS;  // 9 bits per phrase: 
		}
	}
	return 0;
}

#ifdef INFORMATION
^respond(topicname) - execute responders of topicname
#endif

unsigned int RespondCode(char* buffer)
{ // we NEVER fail this, would ruin our control system. We just fail to respond
	char* name = ARGUMENT(1);
	unsigned int topic = FindTopicIDByName(name);
	if (!topic)  return FAILRULE_BIT;  //   failed to find it
	if (GetTopicFlags(topic) & TOPIC_BLOCKED) return 0;
	bool pushed =  PushTopic(topic); 

	globalDepth++;
	unsigned int result = PerformTopic(0,buffer); //   ACTIVE handle - 0 is good result
	globalDepth--;
	if (*buffer) result |= SUCCESSFULE_BIT;
	if (pushed) PopTopic();
	AddNoErase(executingRule);  //   do not allow responders to erase his nest call whether or not he succeeds
	if (result == FAILTOPIC_BIT) return 0; // he failed the topic
	//   dont care that subtopic ended - but we do care if it is considered fails- fails means caller will try another rule always. Others mean if responses stored, caller will NOT
	result &= -1 ^ (ENDTOPIC_BIT|ENDRULE_BIT|FAILRULE_BIT); 
	return result; 
}

unsigned int SystemCode(char* buffer)
{
	if (sandbox)  return FAILRULE_BIT;
	char word[MAX_WORD_SIZE];
	*word = 0;
	int j = 0;
	while (*ARGUMENT(++j))
	{
		char* arg = ARGUMENT(j);
		strcat(word,arg);
		strcat(word," ");
	}
	return !system(word) ? 0 : FAILRULE_BIT;
}

unsigned int SaveCode(char* buffer)
{
	if (ARGUMENT(1)[0] != '@') return FAILRULE_BIT;
	uint64 set = ARGUMENT(1)[1]-'0';
	if (*ARGUMENT(2) == '0' || !stricmp(ARGUMENT(2),"false")) setControl &= -1 ^ (1 << set);
	else setControl |= (uint64) (1 << set);
	return 0;;
}

char* DoFunction(char* name,char* ptr,char* buffer,unsigned int &result)
{//   the buffer used for output ALWAYS has a character at start that can be tested
//   but this is now ReadCompiledWord secure
	WORDP D = FindWord(name,0,PRIMARY_CASE_ALLOWED);
	if (!D || !(D->systemFlags & FUNCTION_NAME)) //   a guess by Output that didnt work out
    {
		result = UNDEFINED_FUNCTION;
		return ptr; 
	}
	result = 0;
	char word[MAX_WORD_SIZE];
    ptr = ReadCompiledWord(ptr,word); //   aimed next token after (

	SystemFunctionInfo* info;
	unsigned int id;
	if (D->x.codeIndex) //   system function call - macroArgumentList can be evaluated in advance of call
	{
		unsigned int oldArgumentBase = systemCallArgumentBase;
		unsigned int oldArgumentIndex = systemCallArgumentIndex;
		if (((unsigned int) systemCallArgumentIndex) >  100)
		{
			int xx = 0;
		}
		systemCallArgumentBase = systemCallArgumentIndex - 1;
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERTABLOG, "SysCall %s(",name);

		info = &systemFunctionSet[D->x.codeIndex];
		while (ptr && *ptr != ')')  //   stop with close of call
		{//   BUG warning arglists do not have a blank before the EOF marker...
			if (info->argumentCount == STREAM_ARG) //   just copy the arg stream over unevaled
			{
				char* base = ptr-2; //   point to the open paren
				ptr = BalanceParen(base,false)-2; //   back up so bottom will return +2 to put us on next token
				strncpy(systemArgumentList[systemCallArgumentIndex],base+2,ptr-base-2); //   leave trailing blank
				systemArgumentList[systemCallArgumentIndex][ptr-base-2] = 0;
			}
			else //   argument must be evaluated-- BUG is [] () and {} treated as a single arg? should be
			{
				ptr = ReadCommandArg(ptr,systemArgumentList[systemCallArgumentIndex],result);
			}
			if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"%s, ",systemArgumentList[systemCallArgumentIndex]);
			if (++systemCallArgumentIndex == MAX_ARG_LIST) 
			{
				result = UNDEFINED_FUNCTION;
				return ptr; 
			}
		}
		systemArgumentList[systemCallArgumentIndex][0] = 0; //   mark end of args

		if (trace && TraceIt(OUTPUT_TRACE)) 
		{
			id = Log(STDUSERLOG,") = ");
		}
		if (result & ENDCODES); //   failed during argument processing
		else if (systemCallArgumentIndex >= (MAX_ARG_LIST-1)) result = FAILRULE_BIT; //   too deep
		else result = (*info->fn)(buffer);

		//   flush macroArgumentList
		systemCallArgumentIndex = oldArgumentIndex;
		systemCallArgumentBase = oldArgumentBase;
	} 
	else //   user function, eg  ^call (_10 ^2 it ^call2 (3 ) )  spot each token has 1 space separator
	{
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERTABLOG, "Execute %s(",name);
		unsigned int oldArgumentBase = userCallArgumentBase;
		unsigned int newArgumentBase = userCallArgumentIndex;
		char* definition = D->w.fndefinition;
        while (*ptr && *ptr != ')') //   ptr is after opening (and before an arg but may have white space
        {
			char* arg = macroArgumentList[userCallArgumentIndex++];
			if (executingRule == NULL) //   this is a table function- DONT EVAL ITS ARGUMENTS AND... keep quoted item intact
			{
				ptr = ReadCompiledWord(ptr,arg); // return dq args as is
				if (ptr == NULL) 
				{
#ifndef NOSCRIPTCOMPILER
					BADSCRIPT("Arguments to %s ran out",currentFunctionDefinition->word);
#endif
				}
			}
			else ptr = ReadArgument(ptr,arg); //   ptr returns on next significant char
			
			//   within a function, seeing function argument as an argument (limit 9 macroArgumentList)
			//   switch to incoming arg now, later userCallArgumentBase will be wrong
			if (*arg == '^' && IsDigit(arg[1]) ) 
			{
				int index = atoi(arg+1) + userCallArgumentBase;
				char* p = macroArgumentList[index];
				strcpy(arg,p); 
			}
			if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG, "%s, ",arg);
		}
		if (trace && TraceIt(OUTPUT_TRACE)) id = Log(STDUSERLOG, ")\n");
		userCallArgumentBase = newArgumentBase; //   spot our macroArgumentList started

		//   run the definition
		globalDepth++;
		if (definition) Output(definition,buffer,result);
		globalDepth--;

		//   pop argument list
		userCallArgumentIndex = userCallArgumentBase;	 
		userCallArgumentBase = oldArgumentBase;
	}
	if (trace && TraceIt(OUTPUT_TRACE)) 
	{
		int how = id;
		if (D->x.codeIndex && !(info->properties & NEWLINE_RESULT)) how = 101; //   ALWAYS keep on same line
		if (result & ENDRULE_BIT) Log(how,"ENDRULE");
		else if (result & FAILRULE_BIT) Log(how,"FAILRULE");
		else if (result & ENDTOPIC_BIT) Log(how,"ENDTOPIC");
		else if (result & ENDSENTENCE_BIT) Log(how,"ENDSENTENCE");
		else if (result & FAILSENTENCE_BIT) Log(how,"FAILSENTENCE");
		else if (result & ENDINPUT_BIT) Log(how,"ENDINPUT");
		else if (result & RETRY_BIT) Log(how,"RETRY_BIT");
		else if (result & SUCCESSFULE_BIT) Log(how,"ANSWER");
		else Log(how,"OK");
		Log(STDUSERLOG," (%s)\r\n",name);
	}
	return ptr+2;	//   skip over closing paren and space
}

void DumpFunctions()
{
	unsigned int k = 0;
	SystemFunctionInfo *fn;
	while ((fn = &systemFunctionSet[++k]) && fn->word )
	{
		Log(STDUSERLOG,"%s - %s\r\n",fn->word,fn->comment);
	}
}

SystemFunctionInfo systemFunctionSet[] =
{//   1st arg must always be all lower case
	{"",CodeFail,0,0,""},
	{"^addproperty",AddPropertyCode,VARIABLE_ARGS,0,"Add value to dictionary entry properies or systemFlags"},
	{"^addtopic",AddTopicCode,1,0,"note a topic as interesting"},
	{"^analyze",AnalyzeCode,STREAM_ARG,0,""},
	{"^blacklist",BlackListCode,1,0,"declare this ip persona non grata for some minutes"},				//   DONE
	{"^burst",BurstCode,VARIABLE_ARGS,0,"break a string into component words"}, 
	{"^explode",ExplodeCode,1,0,"break a word into component letters"}, 
	{"^command",CommandCode,STREAM_ARG,0,""},
	{"^compute",ComputeCode,3,0,"perform a numerical computation"},
	{"^countmember",CountMemberCode,1,0,"counts how many top level members are in a set"},
	{"^createfact",CreateFactCode,STREAM_ARG,OWNTRACE,"create a triple"},
	{"^debug",DebugCode,0,0,"only useful for debug code breakpoint"},
	{"^define",DefineCode,VARIABLE_ARGS,0,""},
	{"^delete",DeleteQuerySetCode,1,0,""},
	{"^disable",DisableCode,VARIABLE_ARGS,0,"stop a rule or topic"},
	{"^enable",EnableCode,2,0,"allow a rule or topic"},
	{"^end",EndCode,1,0,"cease current processing thru this level"},
	{"^eval",EvalCode,STREAM_ARG,NEWLINE_RESULT,"evaluate stream"},
	{"^export",ExportFactCode,2,0,"write fact set to a file"},
	{"^fact",FactCode,2,0,""},
	{"^fail",FailCode,1,0,""},
	{"^find",FindCode,2,0,""},
	{"^first",FLRCodeF,STREAM_ARG,0,""},
	{"^gambit",GambitCode,VARIABLE_ARGS,NEWLINE_RESULT,"execute topic in gambit mode"},
	{"^gambittopics",GetTopicsWithGambits,0,0,""},
	{"^hasproperty",HasPropertyCode,2,0,"argument 1 has property bit argument2?"},
	{"^haskeyword",HasKeywordCode,1,0,""},
	{"^import",ImportFactCode,4,0,"read fact set from a file"},
	{"^input",InputCode,STREAM_ARG,0,"submit stream as input immediately after current input"},
	{"^intersecthierarchy",IntersectHierarchyCode,3,0,""},
	{"^intersecttopics",IntersectTopicsCode,2,0,""},
	{"^join",JoinCode,STREAM_ARG,OWNTRACE,"merge words into one"},
	{"^keywordtopics",KeywordTopicsCode,0,0,""},
	{"^last",FLRCodeL,STREAM_ARG,0,""},
	{"^length",LengthCode,1,0,""},
	{"^log",LogCode,STREAM_ARG,0,"add to logfile"},
	{"^mark",MarkCode,STREAM_ARG,0,"mark word/concept in sentence"},
	{"^marked",MarkedCode,1,0,"is word/concept marked in sentence"},
	{"^match",MatchCode,STREAM_ARG,0,""},
	{"^noerase",NoEraseCode,0,0,"do not erase rule after use"},
	{"^nofail",NoFailCode,STREAM_ARG,0,"execute script but ignore all failures"},
	{"^nolowercase",NoLowerCaseCode,1,0,""},
	{"^notnull",NotNullCode,STREAM_ARG,0,""},
	{"^pick",FLRCodeR,STREAM_ARG,0,""},
	{"^poptopic",PopTopicCode,VARIABLE_ARGS,0,"remove current topic from interesting set"},
	{"^pos",POSCode,VARIABLE_ARGS,0,"compute some part of speech value"},
	{"^preprint",PrePrintCode,STREAM_ARG,0,"add output before existing output"},
	{"^query",QueryCode,VARIABLE_ARGS,0,"hunt for fact in fact database"}, 
	{"^querytopics",QueryTopicsCode,2,0,""},
	{"^refine",RefineCode,0,NEWLINE_RESULT,"execute continuations until one matches"},
	{"^rejoinder",RejoinderCode,0,0,"try to match a pending rejoinder"},
	{"^repeat",RepeatCode,0,0,"set repeat flag so can repeat output"},
	{"^respond",RespondCode,1,NEWLINE_RESULT,"execute a topic's responders"},
	{"^reset",ResetCode,VARIABLE_ARGS,0,"reset a user or a topic or all topics back to initial state"},
	{"^retry",RetryCode,0,0,"reexecute a rule with a later match"},
	{"^reuse",ReuseCode,VARIABLE_ARGS,0,"jump to a script label and execute output section"},
	{"^rhyme",RhymeCode,1,0,"find a rhyming word"},
	{"^save",SaveCode,2,0,"mark fact set to be saved into user data"},
	{"^setquestion",SetQuestionCode,1,0,"set the question flag true"},
	{"^settopicflags",SetTopicFlagsCode,1,0,"set global topic flags"},
	{"^sexed",SexedCode,4,0,"pick a word based on sex of given word"},
	{"^sort",SortCode,1,0,""},
	{"^spell",SpellCode,1,0,"find words matching pattern and store as facts"},
	{"^spellmatch",SpellMatchCode,2,0,"see if given word matches a pattern"},
	{"^stall",StallCode,1,0,"perform timing check"},
	{"^system",SystemCode,STREAM_ARG,0,"except a command in the operating system"},
	{"^substitute",SubstituteCode,4,0,"alter a string by substitution"},
	{"^unmark",UnmarkCode,STREAM_ARG,0,"remove a mark on a word/concept in the sentence"},
	{"^unpackfactref",UnpackFactRefCode,1,0,""}, //   stores into fact set
	{0,0,0,0,""}	
};
