// scriptCompile.cpp - converts user scripts into internal executable notation

#include "common.h"
#ifndef NOSCRIPTCOMPILER

WordMap generalMap;

//   handle validate _ use from _0 to _9
//   remeber that output uses format string, and ref to functionvar must be replaced with ^2 refs.
//   remember that private topics of some flavor do not allow keywords, since it mirrors main topic
static int scriptSpellCheck = 0;
bool patternContext = false;
static FILE* patternFile = NULL;
static unsigned int hasWarnings;
static char duplicateTopicName[MAX_WORD_SIZE];
static char assignCall[MAX_WORD_SIZE];

static int tcolon,rcolon,scolon,qcolon,ucolon,acolon,ccount,tcount;
static char priorOutput[MAX_BUFFER_SIZE];
static char currentOutput[MAX_BUFFER_SIZE];
bool bCallBackScriptCompiler = false;
static char holdReadBuffer[MAX_BUFFER_SIZE]; 
static char lastTopLabel[MAX_WORD_SIZE];
static char ruleContext;

static char* generatedBuffer = NULL;
static char* gambitBuffer = NULL;
static char* responderBuffer = NULL;
static char* currentGambitPtr;
static char* currentGeneratedPtr;
static char* currentResponderPtr;
static char* currentAIPtr;
static bool isGambitSpace = false;
static bool initedGeneralizer = false;

FILE* copyOut = NULL;

static char* ReadRule(char* type,char* ptr, FILE* in,char* data,char* basedata);

char* holddata;		//   just a place to put debug info if we want it

static char readTopicName[MAX_WORD_SIZE];
static char lowercaseForm[MAX_WORD_SIZE];

static char argset[50][MAX_WORD_SIZE];

static const char* predefinedSets[] = //   internally mapped concepts -- BUG  we prelabel the emotions
{
    "~number","~placenumber","~propername","~spacename","~url","~timeword","~unknownword","~locationword","~money","~overflownumber",

	"~mainsubject","~mainverb","~mainobject","~mainindirectobject",
	
	"~childword","~adultword","~qwords",
    "~malename","~femalename","~humanname","~firstname",
    "~repeatme","~repeatinput1","~repeatinput2","~repeatinput3","~repeatinput4","~repeatinput5","~repeatinput6",
    NULL
};



static int topicCount = 0;

WORDP currentFunctionDefinition;

static char* topicFiles[] = //   files created by a topic refresh from scratch 
{
	"TOPIC/factsx.txt",			//   hold facts	
	"TOPIC/patternWordsx.txt",	//   things we want to detect in patterns that may not be normal words
	"TOPIC/scriptx.txt",		//   hold topic definitions
	"TOPIC/keywordsx.txt",		//   holds topic and concepts keywords
	"TOPIC/macrosx.txt",		//   holds macro definitions

	"TOPIC/missingSets.txt",	//   sets needing delayed testing
	"TOPIC/missingLabel.txt",	//   reuse/unerase needing delayed testing for label
	0
};


#ifdef INFORMATION
The goal of script compilation is to validate topic data files AND to build them into efficient-to-execute forms.
This means creating a uniform spacing of tokens and adding annotations as appropriate.

Reading a topic file (on the pattern side) often has tokens jammed together. For example all grouping characters like
() [ ] { } should be independent tokens, as should all prefix pattern characters like ' _ ! &  (except possessive 's should return as one token).
To refer to the possessive apostrophe, you must do backslash apostrophe , but normally one will have a concept containing it. 

One does not refer to & as a character, because tokenization always translates that to "and". 
Just as all contractions will get expanded to the full word.
In order to be able to read these special characters literally, one can prefix it with \  as in \[  . The token returned includes the \.
In particular, \! means the exclamation mark at end of sentence. You are not required to do \? because it is directly a legal
token, but you can.  You CANNOT test for . because it is the default and is subsumed automatically.

For convenience you can use #define newname oldname to make meaningful names for _# and @# variables. E.g.,
#define _stock_items _15
#define @stock_items @5

These defines can be placed before a topic or before a rule. If placed within a topic then
the defines disappear when the topic is finished, otherwise they are globally defined thereafter.

#endif


static char* GetSetName(char* word)
{//   name on relation test?
	static char junk[MAX_WORD_SIZE];
	strcpy(junk,word);
	char* comp = strchr(junk,'<');
	if (!comp) comp = strchr(junk,'>');
	if (!comp) comp = strchr(junk,'?');
	if (!comp) comp = strchr(junk,'=');
	if (comp) *comp = 0;
	if (*junk == '=' || *junk == '!' || *junk == '_') memcpy(junk,junk+1,strlen(junk));
	return junk;
}

static bool TopLevelWord(char* word)
{
	return (!stricmp(word,"data:") || !stricmp(word,"topic:") || !stricmp(word,"concept:")  || !stricmp(word,"patternMacro:") || !stricmp(word,"outputMacro:") 
		|| (*word == ':' && word[1])  || !stricmp(word,"table:") );
}

static bool IsSet(char* word)
{
	if (!word[1]) return true;
	word = GetSetName(word);
	MakeLowerCase(word);
	WORDP D = FindWord(word); //   handles stuff with things after
	if (!D) return false;
	if (D->word[0] == '~' && D->systemFlags & CONCEPT) return true;
	if (D->word[0] == '%') return true;
	return false;
}

static void DoubleCheckSet()
{
	FILE* in = fopen("TOPIC/missingsets.txt","rb");
	if (!in) return;
	while (ReadALine(readBuffer,in) != 0) 
    {
		char word[MAX_WORD_SIZE];
		ReadCompiledWord(readBuffer,word);
		if (IsSet(word));
		else 
		{
			++hasWarnings;
			Log(STDUSERLOG,"*** Warning- missing set definition %s\r\n",word);
		}
	}
	fclose(in);
}

static void CheckSet(char* word) //   we want to prove all references to set get defined
{
	if (IsSet(word)) return;
	WORDP D = StoreWord(word);
	if (D->v.patternStamp == matchStamp) return;	//   already written to file
	D->v.patternStamp = matchStamp;
	FILE* out = fopen("TOPIC/missingsets.txt","ab");
	fprintf(out,"%s\r\n",word);
	fclose(out);
}

static void ReadGeneralizers(char* file)
{
    char original[MAX_WORD_SIZE];
    FILE* in = fopen(file,"rb");
    if (!in) return;
    while (ReadALine(readBuffer,in) != 0) 
    {
        if (readBuffer[0] == '#' || readBuffer[0] == 0) continue;
		char* ptr = SkipWhitespace(readBuffer);
        ptr = ReadCompiledWord(ptr,original); //   original phrase
        if (*original == 0 || *original == '#') continue;
		ptr = SkipWhitespace(ptr);
		unsigned int len = strlen(ptr);
		while (len &&  IsWhiteSpace(ptr[len - 1])) ptr[--len] = 0; // trim junk space off end
		WORDP D = StoreWord(original); 
		MEANING M = MakeMeaning(D);
		generalMap[D->word] = AllocateString(ptr); // wont last thru multiple compiles... because allocate string space shifts.
	}
    fclose(in);
}

static char* NoteGenerateResponder(char* ptr,char* reuse)
{ 
// #ai  =							   (reuse last gambit and prior gambit's question) -- also w/o the =
// #ai Do you like me =				   (reuse on last gambit)
// #ai Do you like me = No I don't.    (full responder)
	ptr = SkipWhitespace(ptr);
	char* equal = strchr(ptr,'='); // this is a self-responder, not tied to a gambit
	if (equal) *equal = 0;

	if (!initedGeneralizer)
	{
		initedGeneralizer = true;
		ReadGeneralizers("LIVEDATA/generalizer.txt");
	}

	unsigned int len = strlen(readBuffer); //   iniially 0 (w offse 3 to 1st) but thereafter its offset 4 (endmarker,2byte junp, space).
	if (equal == ptr || !equal) ptr = priorOutput;// use the output of the prior responder
	sprintf(currentGeneratedPtr,"#! %s\r\n",ptr);
	currentGeneratedPtr += strlen(currentGeneratedPtr);
	PrepareSentence(ptr,false,false);	// tokenize but dont mark
	sprintf(currentGeneratedPtr,"u: (<< ");
	currentGeneratedPtr += strlen(currentGeneratedPtr);

	char c = wordStarts[wordCount][0]; // ending puncutation
	if (c == '?' || c == '!' || c == '.') --wordCount;	// dont register it
	static WORDP words[100];
	unsigned int index = 0;
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		char* word = wordStarts[i];
		if (i == 1) *word = toLowercaseData[(unsigned char) *word];

		// change verbs to base form (eaten -> eat)
		char* verb = GetInfinitive(word);
		if (verb && stricmp(verb,word)) word = verb;
		// change nouns to singular
		char* noun = GetSingularNoun(word);
		if (noun && stricmp(noun,word)) word = noun;

		WORDP D = FindWord(word,0,LOWERCASE_LOOKUP);
		if ( D && D->properties & (AUX_VERB|CONJUNCTION_BITS)) // ignore leading aux verbs and conjunctions
		{
			if (!(D->properties & PRONOUN)) continue;	// keep where, for example.
		}
		if (D && D->properties & PREPOSITION)
		{
			if (!(D->properties &(NOUN|VERB))) 
				continue;
		}
		char* canon = GetCanonical(D);
		if (canon) D = FindWord(canon,0,LOWERCASE_LOOKUP);
			char* general = NULL;
			if (D) general = generalMap[D->word];
			if (general) 
			{
				if ( !stricmp(general,"null")) continue;	// dont do anything for this
				char replace[MAX_WORD_SIZE];
				strcpy(replace,general);
				char* under;
				while ( (under = strchr(replace,'_'))) *under = ' '; // break apart
				strcpy(word,replace);  // convert to best generalization
			}

			// use a word in a pattern only ONCE
			D = FindWord(word);
		if (D)
		{
			for (unsigned int j = 0; j < index; ++j)
			{
				if (D == words[j]) 
				{
					D = NULL;
					break;
				}
			}
			if (!D) continue;
			words[index++] = D;
		}
		sprintf(currentGeneratedPtr," %s ",word);
		currentGeneratedPtr += strlen(currentGeneratedPtr);
	}
	char* after = (equal) ? SkipWhitespace(equal+1) : NULL;
	if (after && *after) sprintf(currentGeneratedPtr," >>) %s\r\n\r\n",equal); // he gives a response
	else if (*reuse) sprintf(currentGeneratedPtr," >>) ^reuse(%s)\r\n\r\n",reuse); // reuse old response
	else 
	{
		Log(1,"cannot use #ai without labelling prior rule - %s",readBuffer);
		sprintf(currentGeneratedPtr," >>) **bad \r\n\r\n");
	}
	currentGeneratedPtr += strlen(currentGeneratedPtr);
	memset(readBuffer,0,30); //   kill off data to everyone else (will cover next token which is all we need)
	return readBuffer;
}

void CallBackScriptCompiler(char* buffer) // new line read
{
	if (*holdReadBuffer != 1) 
	{
		sprintf(currentAIPtr,"%s\r\n",holdReadBuffer); // have prior content, push it out now
		currentAIPtr += strlen(currentAIPtr);
	}
	if (!buffer) *holdReadBuffer = 1;		// mark it empty
	else strcpy(holdReadBuffer,buffer);		// put in new content
}

static void OutFile(char* buffer)
{
	fprintf(copyOut,"%s",buffer);
}
  
void AIGenerate(char* filename)
{
	ResetWarnings();
	gambitBuffer = (char*) malloc(1000000);
	responderBuffer = (char*) malloc(1000000);
	generatedBuffer = (char*) malloc(1000000);
	*generatedBuffer = *gambitBuffer = *responderBuffer = 0;
	currentGambitPtr = gambitBuffer;
	currentResponderPtr = responderBuffer;
	currentGeneratedPtr = generatedBuffer;
	currentAIPtr = currentGambitPtr;
	isGambitSpace = true;

	*holdReadBuffer = 1;
	char name[MAX_WORD_SIZE];
	char* embed = strstr(filename,".ai");
	if (embed) *embed = 0;	// remove suffix
	sprintf(name,"%s.ai",filename);
	strcpy(currentFileName,name);
	currentFileLine = 0;
	FILE* in = fopen(name,"rb");
	if (!in) 
	{
		printf("%s not found\r\n",name);
		return;
	}
	sprintf(name,"%s.top",filename);
	copyOut = fopen(name,"wb");
	if (!copyOut) return;
	char lastOutput[MAX_WORD_SIZE];
	*lastTopLabel = 0;
	*lastOutput = 0;
	bCallBackScriptCompiler = true;
	char* ptr = "";
	char* data = (char*) malloc(MAX_TOPIC_SIZE);
	while (ptr) 
	{
		char tmp[MAX_WORD_SIZE];
		char word[MAX_WORD_SIZE];
		ptr = SkipWhitespace(ptr);
		ptr = ReadNextSystemToken(in,ptr,tmp,false,false); // get token
		if (!*tmp) break;
		MakeLowerCopy(word,tmp);

		// handle ai command
		if (!stricmp(word,"#ai"))
		{
			ptr = SkipWhitespace(ptr);
			ptr = NoteGenerateResponder(ptr,lastTopLabel); //  special generator
			*holdReadBuffer = 1; // discard AI buffer
			if (isGambitSpace) currentGambitPtr = currentAIPtr;
			else currentResponderPtr = currentAIPtr;
			*currentAIPtr = 0;
		}
		else if (TopLevelRule(word)) // read a rule
		{
			if (isGambitSpace) currentGambitPtr = currentAIPtr;
			else currentResponderPtr = currentAIPtr;
			if (TopLevelGambit(word)) // use gambit space
			{
				currentAIPtr = currentGambitPtr;
				isGambitSpace = true;
			}
			else // use responder space
			{
				currentAIPtr = currentResponderPtr;
				isGambitSpace = false;
			}
			*currentAIPtr = 0;
			ptr = ReadRule(tmp,ptr,in,data,data); 
		}
		else if (TopLevelWord(word)) // insure we are in gambit space for everything but responder rules
		{
			if (!isGambitSpace) 
			{
				currentResponderPtr = currentAIPtr;
				isGambitSpace = true;
			}
			if (!stricmp(word,"topic:")) // starting new topic, flush the old data to file
			{
				*currentResponderPtr = 0;
				*currentGambitPtr = 0;
				OutFile(gambitBuffer);		// gambit zone first
				OutFile(responderBuffer);		// responder zone second
				if (*generatedBuffer) OutFile("\r\n\r\n# *** Start of generated responders \r\n\r\n");
				OutFile(generatedBuffer);		// responder zone second
				currentGeneratedPtr = generatedBuffer;
				currentResponderPtr = responderBuffer;
				currentGambitPtr = gambitBuffer;
				*generatedBuffer = *gambitBuffer = *responderBuffer = 0;
			}
			currentAIPtr = currentGambitPtr;
		}
	}
	CallBackScriptCompiler(NULL); // flush the buffer
	if (isGambitSpace) currentGambitPtr = currentAIPtr;
	else currentResponderPtr = currentAIPtr;
	*currentGeneratedPtr = 0;
	*currentResponderPtr = 0;
	*currentGambitPtr = 0;
	bCallBackScriptCompiler = false;
	OutFile(gambitBuffer);			// gambit zone first
	OutFile(responderBuffer);		// responder zone second
	if (*generatedBuffer) OutFile("\r\n\r\n# *** Start of generated responders \r\n\r\n");
	OutFile(generatedBuffer);		// responder zone second
	fclose(copyOut);
	fclose(in);
	free(gambitBuffer);
	free(responderBuffer);
	free(generatedBuffer);
	free(data);
	generatedBuffer = gambitBuffer = responderBuffer = 0;
}

char* NoteTester(char* data)
{
	unsigned int len = strlen(data); //   iniially 0 (w offse 3 to 1st) but thereafter its offset 4 (endmarker,2byte junp, space).
	if (len) len -= JUMP_OFFSET; //    minus the starup header and the extra end marker
	char name[100];
	if (duplicateCount) sprintf(name,"VERIFY/%s/%d.txt",readTopicName+1,duplicateCount);
	else sprintf(name,"VERIFY/%s.txt",readTopicName+1); 
	FILE* valid  = fopen(name,"ab");
	if (valid)
	{
		char* ptr = readBuffer;
		while(*ptr && IsWhiteSpace(*ptr)) ++ptr;
		fprintf(valid,"%d %s\r\n",len,ptr);
		fclose(valid);
	}
	memset(readBuffer,0,30); //   kill off data to everyone else (will cover next token which is all we need)
	return readBuffer;
}

#ifdef INFORMATION
We mark words that are not normally in the dictionary as pattern words if they show up in patterns.
For example, the names of synset heads are not words, but we use them in patterns. They will
be marked during the scan phase of matching ONLY if some pattern "might want them". I.e., 
they have a pattern-word mark on them. same is true for multiword phrases we scan.

Having marked words also prevents us from spell-correcting something we were expecting but which is not a legal word.
#endif

void WritePatternWord(char* word)
{
	WORDP D = StoreWord(word);
	if (D->properties & PART_OF_SPEECH) return; //   no need to write, already in dictionary
	if (IsDigit(*word)) return;		//   dont care for numbers 
	if (D->v.patternStamp == matchStamp) return;	//   already written to file
	D->v.patternStamp = matchStamp;
	AddSystemFlag(D,PATTERN_WORD);
	if (patternFile) fprintf(patternFile,"%s\r\n",word);
}

static char* ReadCall(char* name, char* ptr, FILE* in, char* &data,bool call,bool patterncall)
{	//   returns with no space after it
	//   ptr is just after the ^name -- user can make a call w/o ^ in name but its been patched. Or this is function argument
	char reuseTarget1[MAX_WORD_SIZE];
	char reuseTarget2[MAX_WORD_SIZE];
	char hold[MAX_BUFFER_SIZE];
	char word[MAX_WORD_SIZE];
	char nextToken[MAX_WORD_SIZE];
	char callName[MAX_WORD_SIZE];
	MakeLowerCopy(callName,name);
	reuseTarget2[0] = reuseTarget1[0]  = 0;	//   in case this turns out to be a ^reuse call
	char* mydata = hold;	//   read in all data, then treat macroArgumentList as output data for processing
	WORDP D = FindWord(callName);
	if (!call || !D || !(D->systemFlags & FUNCTION_NAME))  //   not a function, is it a function variable?
	{
		if (!IsDigit(callName[1])) 
		{
			BADSCRIPT("Call to function not yet defined %s",callName)
			return ptr;
		}

		//   remap named variable to numbered call argument (0-9)
		*data++ = '^';
		*data++ = callName[1];
		if (IsDigit(callName[2])) *data++ = callName[2];
		*data = 0; 
		return ptr;
	}
	bool isStream = false;		//   dont check contents of stream, just format it
	if (D->x.codeIndex) //   system call
	{
		SystemFunctionInfo* info = &systemFunctionSet[D->x.codeIndex];
		if (info->argumentCount == STREAM_ARG) isStream = true;
	}
	else if (patterncall && !(D->systemFlags & IS_PATTERN_MACRO)) BADSCRIPT("Cannot call output macro from pattern area - %s",callName)
	else if (!patterncall && D->systemFlags & IS_PATTERN_MACRO) BADSCRIPT("Cannot call pattern macro from output area - %s",callName);

	if (!strcmp(callName,"^query"))
	{
		for (unsigned int i = 2; i <= 9; ++i) strcpy(argset[i],"?"); //   default EVERYTHING before we test it later
	}
	if (!strcmp(callName,"noerase") && (ruleContext == 't' || ruleContext == 'r'))
	{
		Log(STDUSERLOG,"*** Warning- ^noerase being applied to a gambit.\r\n");
		++hasWarnings;

	}

	strcpy(data,callName); 
	data += strlen(callName);
	*data++ = ' ';	
	ptr = ReadNextSystemToken(in,ptr,word,false);
	*data++ = '(';	
	*data++ = ' ';	

	// for debugging engine---
	char* holdStart = ptr;

	//   validate argument counts and stuff locally, then swallow data offically as output data
	int parenLevel = 1;
	unsigned int argumentCount = 0;
	while (1) //   read as many tokens as needed to complete the call, storing them locally
	{
		ptr = ReadNextSystemToken(in,ptr,word,false);
		if (!*word) break;
		MakeLowerCopy(lowercaseForm,word);
		if (TopLevelWord(word) || TopLevelRule(lowercaseForm)) 
		{
			// we allow commands as macroArgumentList
			if (*word != ':') BADSCRIPT("Function call not completed %s",callName); //   definition ends when another major unit or top level responder starts
		}

		//   note that in making calls, [] () and {}  count as a single argument with whatver they have inside
		switch(*word)
		{
		case '(': case '[': case '{':
			++parenLevel;
			break;
		case ')': case ']': case '}':
			--parenLevel;
			if (parenLevel == 1) ++argumentCount;	//   completed a () argument
			break;
		case '\'': 
			if (!word[1]) //   merge with next token
			{
				*mydata++ = '\'';
				continue;
			}
			break;
		default:
			if (parenLevel == 1)  
			{
				ReadNextSystemToken(in,ptr,nextToken,false,true);

				// argument is itself a call?
				if (*word != '^' && *nextToken == '(') //   looks like a call, reformat it if it is
				{
					char rename[MAX_WORD_SIZE];
					rename[0] = '^';
					MakeLowerCopy(rename+1,word);	//   in case user omitted the ^
					WORDP D = FindWord(rename,0,PRIMARY_CASE_ALLOWED);
					if (D && D->systemFlags & FUNCTION_NAME) strcpy(word,rename); //   a recognized call
				}
				if (*word == '^' )   //   function call or function var ref 
				{
					char* arg = mydata;
					ptr = ReadCall(word,ptr,in,mydata,*nextToken == '(',true);
					*mydata = 0;
					strcpy(argset[++argumentCount],arg);
					*mydata++ = ' ';
					continue;
				}

				strcpy(argset[++argumentCount],word);
			}
			if (!stricmp(callName,"^reuse") || !stricmp(callName,"^enable")  || !stricmp(callName,"^disable" )) //   note the macroArgumentList
			{
				MakeUpperCase(word); //   labels must be upper case
				if (!*reuseTarget1) strcpy(reuseTarget1,word);
				else strcpy(reuseTarget2,word);
			}
			if (!stricmp(callName,"^createfact")) 
			{
				if (argumentCount == 2 &&  !stricmp(word,"member"))
				{
					reuseTarget1[0] = 1; //   flag we see it
				}
			}
		}

		if (patterncall && IsAlpha(*word)) WritePatternWord(word);
		//   add simple item into data
		strcpy(mydata,word);
		mydata += strlen(mydata);
		if (parenLevel == 0) break;	//   we finished the call (no trailing space)
		*mydata++ = ' ';	
	}
	*--mydata = 0;  //   remove closing paren
	
	if (!stricmp(nextToken,"^first") || !stricmp(nextToken,"^last") || !stricmp(nextToken,"^random"))
	{
		if (argset[2][0]) BADSCRIPT("Too many macroArgumentList to first/last/random - %s",argset[2])
	}

	// validate assignment calls if we can - this will be a first,last,random call
	if (*assignCall)
	{
		char kind = argset[1][2];
		if (!kind) 
			BADSCRIPT("assignment must designate how to use factset (s v or o)- %s",assignCall);
		if ((kind == 'a' || kind == '+' || kind == '-') && *assignCall != '_') BADSCRIPT("can only spread a fact onto a match var - %s",assignCall);
		if (*assignCall == '$' || *assignCall == '_');	// can assign any field or entire fact onto user var or match var
		if (*assignCall == '%') 
		{
			if (kind == 'f' ||  !kind)	BADSCRIPT("cannot assign fact into system variable")
		}
		if (*assignCall == '@') 
		{
			if (kind != 'f')	BADSCRIPT("cannot assign fact field into fact set")
		}
		// unknown WHAT it means to assign to ^var -- BUG???
	}

	//   validate inference calls if we can
	if (!strcmp(D->word,"^query"))
	{
		if (!stricmp(argset[1],"unipropogate"))
		{
		//	if (*argset[2] == '?') BADSCRIPT("Must name subject argument to query");
		//	if (*argset[4] == '?') BADSCRIPT("Must name object argument to query");
		//	if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
		//	if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
	}
		if (!stricmp(argset[1],"direct_s"))
		{
			if (*argset[2] == '?') BADSCRIPT("Must name subject argument to query");
			if (*argset[3] != '?') BADSCRIPT("Cannot name verb argument to query");
			if (*argset[4] != '?') BADSCRIPT("Cannot name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
		}
		if (!stricmp(argset[1],"direct_v"))
		{
			if (*argset[2] != '?') BADSCRIPT("Cannot name subject argument to query");
			if (*argset[3] == '?') BADSCRIPT("Must name verb argument to query");
			if (*argset[4] != '?') BADSCRIPT("Cannot name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
		}
		if (!stricmp(argset[1],"direct_o"))
		{
			if (*argset[2] != '?') BADSCRIPT("Cannot name subject argument to query");
			if (*argset[3] != '?') BADSCRIPT("Cannot name verb argument to query");
			if (*argset[4] == '?') BADSCRIPT("Must name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
		}
		if (!stricmp(argset[1],"direct_sv") )
		{
			if (*argset[2] == '?') 
				BADSCRIPT("Must name subject argument to query");
			if (*argset[3] == '?') 
				BADSCRIPT("Must name verb argument to query");
			if (*argset[4] != '?') BADSCRIPT("Cannot name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
		}
		if (!stricmp(argset[1],"direct_sv_member"))
		{
			if (*argset[2] == '?') BADSCRIPT("Must name subject argument to query");
			if (*argset[3] == '?') BADSCRIPT("Must name verb argument to query");
			if (*argset[4] != '?') BADSCRIPT("Cannot name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] == '?') BADSCRIPT("Must name match argument to query");
		}
		if (!stricmp(argset[1],"direct_ov"))
		{
			if (*argset[2] != '?') BADSCRIPT("Cannot name subject argument to query");
			if (*argset[3] == '?') BADSCRIPT("Must name verb argument to query");
			if (*argset[4] == '?') BADSCRIPT("Must name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
		}
		if (!stricmp(argset[1],"direct_svo"))
		{
			if (*argset[2] == '?') BADSCRIPT("Must name subject argument to query");
			if (*argset[3] == '?') BADSCRIPT("Must name verb argument to query");
			if (*argset[4] == '?') BADSCRIPT("Must name object argument to query");
			if (*argset[8] != '?') BADSCRIPT("Cannot name propgation argument to query");
			if (*argset[9] != '?') BADSCRIPT("Cannot name match argument to query");
		}
	}

	if (parenLevel != 0) BADSCRIPT("Failed to properly close (or [ in call to %s",callName);

	if (!isStream && argumentCount != D->v.argumentCount && D->v.argumentCount != VARIABLE_ARGS) //   verify  argument count to  function
	{
		BADSCRIPT("Incorrect argument count to function %s- %d instead of %d",callName,argumentCount,D->v.argumentCount & 255);
	}
	if (!stricmp(callName,"^reuse") || !stricmp(callName,"^enable") || !stricmp(callName,"^disable")) //   note the macroArgumentList
	{
		if ((!stricmp(callName,"^enable") || !stricmp(callName,"^disable")))
		{
			if (!stricmp(reuseTarget1,"rule")) 
			{
				strcpy(reuseTarget1,reuseTarget2);
				*reuseTarget2 = 0;
			}
			else *reuseTarget1 = 0; // its some other enable/disable type junk
		}

		if (!*reuseTarget1);
		else if (*reuseTarget1 != '~') //   only had name, not topic.name, fill in
		{
			strcpy(reuseTarget2,reuseTarget1);
			if (currentFunctionDefinition) strcpy(reuseTarget1,currentFunctionDefinition->word);
			else strcpy(reuseTarget1,readTopicName);
		}
		else
		{
			char* dot = strchr(reuseTarget1,'.');
			if (dot) // move apart topic name and label
			{
				strcpy(reuseTarget2,dot+1);
				*dot = 0;
			}
		}

		if (!*reuseTarget1);
		else if (*reuseTarget1 != '$' && *reuseTarget1 != '_' && *reuseTarget2 != '$' && *reuseTarget2 != '_') //   we cant crosscheck variable choices
		{
			strcat(reuseTarget1,".");
			strcat(reuseTarget1,reuseTarget2); // compose the name
			WORDP E = FindWord(reuseTarget1);
			if (!E) //   not found yet, note it
			{
				FILE* out = fopen("TOPIC/missingLabel.txt","ab");
				if (out)
				{
					if (currentFunctionDefinition)  fprintf(out,"%s %s %s %d\r\n",reuseTarget1,currentFunctionDefinition->word,currentFileName,currentFileLine);
					else fprintf(out,"%s %s %s %d\r\n",reuseTarget1,readTopicName,currentFileName,currentFileLine);
					fclose(out);
				}
			}
		}
	}

	//   now generate stuff as an output stream with its validation
	int oldspell = scriptSpellCheck;
	scriptSpellCheck = 0;
	ReadOutput(hold,NULL,data,NULL);
	scriptSpellCheck = oldspell;
	*data++ = ')'; 
	//   outer layer generates the trailing space
	
	return ptr;	
}

static void CheckScriptWord(char* input,int startSeen) // output = -1  pattern = 0 and 1
{
	char word[MAX_WORD_SIZE];
	strcpy(word,input);
	unsigned int len = strlen(word);

	unsigned int warn = hasWarnings;
	WORDP D = FindWord(word);
	if (!D && startSeen < 0)
	{
		if (!IsAlpha(word[len-1])) word[len-1] = 0;	// remove punctuation
		D = FindWord(word);
	}
	if (D && D->substitutes && startSeen != -1)
	{
		D = D->substitutes;
		if (D->word[1])	Log(STDUSERLOG,"*** Warning- Substitutes.txt changes %s on input to %s\r\n",word,D->word+1);
		else Log(STDUSERLOG,"*** Warning- %s gets erased on input (substitutes.txt).\r\n",word);
		++hasWarnings;
	}
	else if (IsUpperCase(word[0])); // proper name
	else if (!IsAlpha(word[0]) || strchr(word,'\'') || strchr(word,'.') || strchr(word,'_') || strchr(word,'-') || strchr(word,'~')); // ignore sets, numbers, composite words, wordnet references, etc
	else if (!D || (!(D->properties & PART_OF_SPEECH) && !(D->systemFlags & (PATTERN_WORD|PATTERN_ALLOWED))))
	{
		char* x = FindCanonical(word, 2);
		if (!x)  
		{
			Log(STDUSERLOG,"*** Warning- %s is not a word. Did you misspell something?\r\n",word);
			++hasWarnings;
		}
	}
	char test[MAX_WORD_SIZE];
	sprintf(test,"<%s",word);
	D = FindWord(test);
	if (D && D->substitutes && startSeen == 0)
	{
				D = D->substitutes;
				if (D->word[1])	Log(STDUSERLOG,"*** Warning- Substitutes.txt changes %s as sentence start to %s\r\n",word,D->word+1);
				else Log(STDUSERLOG,"*** Warning- %s gets erased if sentence start (substitutes.txt).\r\n",word);
				++hasWarnings;
	}
	sprintf(test,"%s>",word);
	D = FindWord(test);
	if (D && D->substitutes && startSeen != -1)
	{
				D = D->substitutes;
				if (D->word[1])	Log(STDUSERLOG,"*** Warning- Substitutes.txt changes %s as sentence end to %s\r\n",word,D->word+1);
				else Log(STDUSERLOG,"*** Warning- %s gets erased if sentence end (substitutes.txt).\r\n",word);
				++hasWarnings;
	}
	sprintf(test,"<%s>",word);
	D = FindWord(test);
	if (D && D->substitutes && startSeen == 0)
	{
				D = D->substitutes;
				if (D->word[1])	Log(STDUSERLOG,"*** Warning- Substitutes.txt changes %s as standalone sentence to %s\r\n",word,D->word+1);
				else Log(STDUSERLOG,"*** Warning- %s gets erased if standalone input (substitutes.txt).\r\n",word);
				++hasWarnings;
	}

	if (warn != hasWarnings)
	{
		if (*currentFileName) Log(STDUSERLOG,"   in %s at %d: %s\r\n\r\n    ",currentFileName,currentFileLine,readBuffer);
	}
}
	

char* ReadPattern(char* ptr, FILE* in, char* &data,bool macro)
{ //   called from topic or patternmacro
#ifdef INFORMATION //   meaning of leading characters
< >	 << >>	sentence start & end boundaries, any 
!			NOT 
nul			end of data from macro definition or argument substitution
* *1 *~ *~2 *-1	 gap (infinite, exactly 1 word, 0-2 words, 0-2 words, 1 word before)
_  _2		memorize next match or refer to 3rd memorized match (0-based)
@			_1 (positional set) and factset references @5subject
$			user variable 
^	^1		function call or function argument (user)
()[]{}		nesting of some kind (sequence AND, OR, OPTIONAL)
dquote		string token
?			a question input
~dat  ~		topic/set reference or current topic 
%			system variable 
=xxx		comparison test (= > < )
apostorphe and apostrophe!		non-canonical meaning on next token or exclamation test
\			escape next character as literal (\$ \= \~ \(etc)
#xxx		a constant number symbol, but only allowed on right side of comparison
------default values
-1234567890	number token
12.14		number token
1435		number token
a-z,A-Z,|,_	normal token
,			normal token (internal sentence punctuation) - period will never exist since we strip tail and cant be internal

----- these are things which must all be insured lower case (some can be on left or right side of comparison op)
%			system variable 
~dat 		topic/set reference
a: thru u:	responder codes
if/loop/loopcount	constructs
^call  call	function/macro calls with or without ^
^fnvar		function variables
^$glblvar	global function variables
$			user variable 
@			debug ahd factset references
labels on responders
responder types s: u: t: r: 
name of topic  or concept

#endif
	char word[MAX_WORD_SIZE];
	char nextToken[MAX_WORD_SIZE];
	char nestKind[100];
	int nestIndex = 0;
	patternContext = true;

	//   if macro call, there is no opening (or closing )
	//   We claim an opening and termination comes from finding a toplevel token
	if (macro) nestKind[nestIndex++] = '(';
	bool unorderedSeen = false;
	bool notSeen = false;
	bool quoteSeen = false;
	bool variableGapSeen = false;
	bool underscoreSeen = false;
	bool startSeen = false; // track if an interjection happens or not
	while (1) //   read as many tokens as needed to complete the definition
	{
		ptr = ReadNextSystemToken(in,ptr,word); //   pattern separates _
		if (!*word) break; //   end of file
		MakeLowerCopy(lowercaseForm,word);
		if (TopLevelWord(word) || TopLevelRule(lowercaseForm)) 
		{
			ptr -= strlen(word);
			break;
		}
		char* at = FindComparison(word);
		char c;
		switch(word[0])
		{
			case '<':	//   sentence start < or  unordered start <<
				if (underscoreSeen) BADSCRIPT("Cannot use _ before < or <<");
				if (quoteSeen) BADSCRIPT("Cannot use ' before < or <<");
				if (word[1] == '<')  //   <<
				{
					if (unorderedSeen) BADSCRIPT("Already have << in progress");
					unorderedSeen = true;
					if (variableGapSeen) BADSCRIPT("Cannot use * before <<");
				}
				else //   <
				{
				}
				variableGapSeen = notSeen = false;
				break; 
			case '@': 
				if (word[1] == '_')
				{
					if (!IsDigit(word[2])) BADSCRIPT("@_ must be match variable- %s",word);
					if (word[3]) BADSCRIPT("@_ match reference limited to _9 - %s",word);
				}
				else if (!IsDigit(word[1]))  BADSCRIPT("@_0 or a factset reference are the only legal @ patterns - %s",word);
				break;
			case '>':	//   sentence end > or unordered end >>
				if (underscoreSeen) BADSCRIPT("Cannot use _ before > or <>>");
				if (quoteSeen) 
					BADSCRIPT("Cannot use ' before > or >>");
				if (word[1] == '>') //   >>
				{
					if (!unorderedSeen) BADSCRIPT("Have no << in progress");
					unorderedSeen = false;
					if (variableGapSeen) BADSCRIPT("Cannot use * inside >>");
				}
				else  //   >
				{
					if (underscoreSeen) BADSCRIPT("Cannot use _ before >");
				}
				variableGapSeen = notSeen = false;
				break; //   sentence end align
			case '(':	//   sequential pattern unit begin
				nestKind[nestIndex++] = '(';
				break;
			case ')':	//   sequential pattern unit end
				if (variableGapSeen && nestIndex > 1) BADSCRIPT("cannot have * followed by closer - %s",word);
				if (nestKind[--nestIndex] != '(') BADSCRIPT(") is not closing (");
				break;
			case '[':	//   list of pattern choices begin
				nestKind[nestIndex++] = '[';
				break;
			case ']':	//   list of pattern choices end
				if (variableGapSeen) BADSCRIPT("cannot have * followed by closer - %s",word);
				if (nestKind[--nestIndex] != '[') BADSCRIPT("] is not closing [");
				break;
			case '{':	//   list of optional choices begins
				if (notSeen) BADSCRIPT("!{ makes no sense since optional can fail or not")
				nestKind[nestIndex++] = '{';
				break;
			case '}':	//   list of optional choices ends
				if (variableGapSeen) BADSCRIPT("cannot have * followed by closer - %s",word);
				if (nestKind[--nestIndex] != '{') BADSCRIPT("} is not closing {");
				break;
			case '\\': //   literal next character
				//   BUG - prove \(\[ hold together
				if (!word[1]) BADSCRIPT("Isolated / ");
				underscoreSeen = variableGapSeen = notSeen = false;
				break;
			case '!': //   NOT
				if (notSeen) BADSCRIPT("Cannot use ! after !");
				*data++ = '!';
				*data++ = ' ';
				notSeen = true;
				continue;
			case '*': //   gap: * *1 *~2 *-1	(infinite, exactly 1 word, 0-2 words, 0-2 words, 1 word before
				if (notSeen) BADSCRIPT("cannot have ! before a gap - %s",word);
				if (unorderedSeen) 
					BADSCRIPT("cannot have * of any kind inside << >> - %s",word);
				if (variableGapSeen) BADSCRIPT("cannot have * followed by * of any kind - %s",word);
				if (IsDigit(word[1])) //   enumerated gap size
				{
					int n = atoi(word+1);
					if (n == 0) BADSCRIPT("*0 is meaningless - %s",word);	 
					if (n > 9) BADSCRIPT("*9 is the largest fixed gap - %s",word);
				}
				else if (word[1] == '-') //   backwards
				{
					int n = atoi(word+2);
					if (word[2] == '0') BADSCRIPT("*-1 is the smallest backward, word before - %s",word);
					if (n > 9) BADSCRIPT("*-9 is the largest backward - %s",word);
				}
				else if (word[1] == '~') //   close-range gap
				{
					variableGapSeen = true;
					int n = atoi(word+2);
					if (!word[2]) BADSCRIPT("*~ is not legal, you need a digit after it - %s",word) //   *~ 
					else if (n == 0) BADSCRIPT("*~1 is the smallest close-range gap - %s",word)
					else if (n > 9) BADSCRIPT("*~9 is the largest close-range gap - %s",word);
				}
				else if (word[1]) BADSCRIPT("* jammed against some other token %s",word)
				else variableGapSeen = true;
				underscoreSeen = false;
				startSeen = true;
				break;
			case '_':	//   memorize OR var reference
				if (underscoreSeen) BADSCRIPT("Cannot use _ after _");
				if (IsDigit(word[1])) //   memorized reference - allowed to use 2 digit but limit to 19
				{
					if (atoi(word+1) > MAX_WILDCARDS) BADSCRIPT("_%d is max reference - %s",MAX_WILDCARDS-1,word);
					variableGapSeen = notSeen = false;
				}
				else //   memorize- 
				{
					*data++ = '_';
					*data++ = ' ';
					underscoreSeen = true;
					continue;
				}
				break;
			case '?': //   question input ?   
				if (underscoreSeen) BADSCRIPT("Cannot have _ before ?");
				if (variableGapSeen) BADSCRIPT("Cannot have * gap before ?");
				notSeen = false;
				break;
			case '$':	//   user var
				if (underscoreSeen) BADSCRIPT("Cannot have _ before user variable - %s",word);
				variableGapSeen = notSeen = false;
				break;
			case '"': //   string
				underscoreSeen = variableGapSeen = notSeen = false;
				strcpy(word,JoinWords(BurstWord(word)));// rewrite it as a composite token
				WritePatternWord(word); 
				break;
			case '\'': //   non-canonical next token OR ordinary token 's (' requires \')
				ReadNextSystemToken(in,ptr,nextToken,true,true); 

				if (*nextToken == 's' && !nextToken[1]) //   's is considered one token
				{
					*data++ = '\'';
					*data++ = 's';
					*data++ = ' ';
					ptr = ReadNextSystemToken(in,ptr,nextToken,true,false);  //   use up token
					notSeen = underscoreSeen = variableGapSeen = false;
				}
				else //   ordinary quote in front of some other token
				{
					*data++ = '\'';
					*data++ = ' ';
					quoteSeen = true;
				}
				continue;
			case '%': //   system data
				//   ! is allowed before system var--- for tests like !%tense=xxx
				if (underscoreSeen) BADSCRIPT("cannot use _ before system var - %s",word);
				if (at)
				{
					c = *at;
					*at = 0;
				}
				if (!word[1]); //   simple %
				else if (FindWord(word));
				else BADSCRIPT("%s is not a system variable",word);
				if (at) *at = c;
				variableGapSeen = notSeen = false;
				break;
			case '~':
				quoteSeen = underscoreSeen = variableGapSeen = notSeen = false;
				startSeen = true;
				break;
			default: //   normal token (or ~set,  and anon function call)
				ReadNextSystemToken(in,ptr,nextToken,true,true); 
				//    MERGE user pattern words into one? , e.g. drinking age == drinking_age in dictionary
				//   only in () sequence mode. Dont merge [old age] or {old age} or << old age >>
				if (nestKind[nestIndex-1] == '(' && !unorderedSeen) //   BUG- do we need to see what triples etc wordnet has
				{
					WORDP F = FindWord(word);
					WORDP E = FindWord(nextToken);
					if (E && F && F->properties & PART_OF_SPEECH  && E->properties & PART_OF_SPEECH)
					{
						char join[MAX_WORD_SIZE];
						sprintf(join,"%s_%s",word,nextToken);
						E = FindWord(join);
						if (E && E->properties & PART_OF_SPEECH) //   change to composite word
						{
							strcpy(word,join);		//   joined word replaces it
							ptr = ReadNextSystemToken(in,ptr,nextToken,true,false); //   swallow the lookahead
						}
					}
				}
				if (*word == '~' ) CheckSet(word);
				quoteSeen = underscoreSeen = variableGapSeen = notSeen = false;
				startSeen = true;
				break;
		}

		if (nestIndex == 0) //   we completed this level
		{
			strcpy(data,word);
			data += strlen(data);
			*data++ = ' ';	
			break; 
		}

		// relation	comparison token like $var=:foo or _1>$var or *1=~set (legal?)
		//	member operation $var? and _0?
		//	comparison ^$var=foo and _0=^$var and ^_9=foo and _0=^_0  also ==
		//   watch out for << and >> that they not be caught as compariosns
		if (at) //   is a comparison of some kind
		{
			if (*word == '~') // check left side operator
			{
				char c = *at;
				*at = 0;
				CheckSet(word); 
				*at = c;
			}

			char* rhs = at+1;
			if (*rhs == '=') ++rhs;
			if (!*rhs && word[0] == '$'); // allowed member in sentence
			else if (!*rhs && word[0] == '_' && IsDigit(word[1])); // allowed member in sentence
				//   if compare against #xxx replace constant with decimal value
			else if (*rhs == '#')
			{
				uint64 n = FindValueByName(rhs+1);
				if (n) sprintf(rhs,"%lld",n);
				else BADSCRIPT("No #constant recognized - %s",at);
			}
			else if (IsAlphabeticDigitNumeric(*rhs))  WritePatternWord(rhs);		//   ordinary token
			else if (*rhs == '~') 
			{
				MakeLowerCase(rhs);
				CheckSet(rhs);	//   set
			}
			else if (*rhs == '^' && (rhs[1] == '$' || rhs[1] == '_')) MakeLowerCase(rhs);	//   user var fnref or _ fnref
			else if (*rhs == '$') MakeLowerCase(rhs);	//   user var
			else if (*rhs == '%') MakeLowerCase(rhs);	//   system  var
			else if (*rhs == '_');
			else if (*rhs == '\'' && (rhs[1] == '_' || rhs[1]== '$')); //   unevaled user var or raw wild
			else if (!at[2] && *word == '$'); // find in sentence
			else BADSCRIPT("Illegal comparison type %s",word);

			int len = (at - word) + 2; //   include the = and jump code in length

			//   if quoteSeen then we need to incorporate that into our token
			//   '_0=the becomes =x'_0=the
			if (quoteSeen)
			{
				char word1[MAX_WORD_SIZE];
				sprintf(word1,"'%s",word);
				strcpy(word,word1);
				++len;	//   account for added '
				data -= 2;	//   remove the quote stored in the buffer
			}

			//   rebuild token
			char tmp[MAX_WORD_SIZE];
			tmp[0] = '=';		//   comparison header
			if (len > 74) BADSCRIPT("left side of comparison must not exceed 70 characters - %s",word);
			tmp[1] = code[len % 75];	//   jump code to comparison xx
			strcpy(tmp+2,word); //   copy left side over
			strcpy(word,tmp);	//   replace original token
			quoteSeen = notSeen = false;
		}
		else if (*word == '~') CheckSet(word);

		//   see if we have an implied call (he omitted the ^)
		ReadNextSystemToken(in,ptr,nextToken,true,true); 
		if (*word != '^' && *nextToken == '(') //   looks like a call, reformat it if it is
		{
			char rename[MAX_WORD_SIZE];
			rename[0] = '^';
			MakeLowerCopy(rename+1,word);	//   in case user omitted the ^
			WORDP D = FindWord(rename,0,PRIMARY_CASE_ALLOWED);
			if (D && D->systemFlags & FUNCTION_NAME) strcpy(word,rename); //   a recognized call
		}
		if (*word == '^')   //   function call or function var ref or global var ref
		{
			if (underscoreSeen) BADSCRIPT("Cannot call function after _ ");
			if (notSeen) BADSCRIPT("Cannot call function after ! ");
			if (word[1] == '$')
			{
				strcpy(data,word);
				data += strlen(data);
			}
			else ptr = ReadCall(word,ptr,in,data,*nextToken == '(',true);
			*data++ = ' ';
			continue;
		}

		if (IsAlpha(*word) && scriptSpellCheck) CheckScriptWord(word,startSeen ? 1 : 0);
		if (IsAlpha(*word)) WritePatternWord(word); //   memorize it to know its important

		//   just put out the next token, with space after it
		strcpy(data,word);
		data += strlen(data);
		*data++ = ' ';	

		if (nestIndex == 0) break; //   we completed this level
	}

	//   leftovers?
	if (macro && nestIndex != 1) 
		BADSCRIPT("Failed to balance ([ or { properly in macro")
	else if (!macro && nestIndex != 0) 
		BADSCRIPT("Failed to balance ([ or { properly");

	if (unorderedSeen) BADSCRIPT("failed to close <<");
	if (notSeen) BADSCRIPT("failed to provide token after !");
	if (quoteSeen) 
		BADSCRIPT("failed to provide token after '");
	if (underscoreSeen) BADSCRIPT("failed to provide token after _");

	patternContext = false;
	*data = 0;
	return ptr;
}

static char* ReadBody(char* word, char* ptr, FILE* in, char* &data,char* rejoinders)
{	//   returns the stored data, not the ptr, starts with the {
	char* body = AllocateBuffer();
	*data++ = '{';
	*data++ = ' ';
	char* save = body;
	int level = 0;
	while (1) //   gather all data first into a buffer (we know it ends with balancing } ). Then we can validate it.
	{
		ptr = ReadNextSystemToken(in,ptr,word,false); //   the '('
		MakeLowerCopy(lowercaseForm,word);
		if (!*word || TopLevelWord(word) || TopLevelRule(lowercaseForm) || Continuer(lowercaseForm)) 
		{
			if (level == 1) BADSCRIPT("Incomplete IF body - %s",word); // higher levels would be arugments to functions
		}
		char c = *word;
		level += nestingData[c];
		if (level == 0) break; //   end of stream of if body
		strcpy(save,word);
		save += strlen(save);
		*save++ = ' ';
	}
	*save = 0;

	//   now validate it as output...
	ReadOutput(body+2,NULL,data,rejoinders); //   ignore the leading {, we already wrote it out, and we didnt add the closing }
	*data++ = '}';
	//   body has no blank after it, done by higher level
	FreeBuffer();
	return ptr;
}

#ifdef INFORMATION

An IF consists of:
	if (test-condition code) xx
	{body code} yy
	else (test-condition code) xx
	{body code} yy
	else (1) xx
	{body code} yy
	
spot yy is offset to end of entire if and xx if offset to next branch of if before "else".

#endif

static char* ReadIf(char* word, char* ptr, FILE* in, char* &data,char* rejoinders)
{
	char nextToken[MAX_WORD_SIZE];
	char* bodyends[1000];				//   places to patch for jumps
	unsigned int bodyendIndex = 0;
	strcpy(data,"if ");
	data += 3;
	int paren = 0;
	while (1)
	{
		//   read test 

		//   read the (
		ptr = ReadNextSystemToken(in,ptr,word,false); //   the '('
		MakeLowerCopy(lowercaseForm,word);
		if (!*word || TopLevelWord(word) || TopLevelRule(lowercaseForm) || Continuer(lowercaseForm)) BADSCRIPT("Incomplete IF statement - %s",word);
		if (*word != '(') BADSCRIPT("Missing (for IF test - %s",word);
		++paren;
		*data++ = '(';
		*data++ = ' ';
		//   test is either a function call OR an equality comparison OR an IN relation OR an existence test
		//   the followup will be either (or  < > ==  or  IN  or )
		//   The function call returns a status code, you cant do comparison on it
		//   but both function and existence can be notted- IF (!$var) or IF (!read(xx))
		//   You can have multiple tests, separated by AND and OR.
pattern: 
		ptr = ReadNextSystemToken(in,ptr,word,false,false); 
		bool notted = false;
		if (*word == '!' && !word[1]) 
		{
			notted = true;
			*data++ = '!';
			*data++ = ' ';
			ptr = ReadNextSystemToken(in,ptr,word,false,false); 
		}
		if (*word == '\'' && !word[1]) 
		{
			*data++ = '\'';
			ptr = ReadNextSystemToken(in,ptr,word,false,false); 
			if (*word != '_') BADSCRIPT("Can only quote _matchvar in IF test");
		}
		if (*word == '!') BADSCRIPT("IF TEST- Cannot do two ! in a row");
		ReadNextSystemToken(in,ptr,nextToken,false,true); 
		MakeLowerCase(nextToken);
		if (*nextToken == '(') 
		{
			if (*word != '^') //    a call w/o its ^
			{
				char rename[MAX_WORD_SIZE];
				rename[0] = '^';
				strcpy(rename+1,word);	//   in case user omitted the ^
				strcpy(word,rename);
			}
			ptr = ReadCall(word,ptr,in,data,true,false);  //   read call
		}
		else if (*nextToken == '<' || *nextToken == '?' || *nextToken == '>' || ((*nextToken == '=' || *nextToken == '!' ) && nextToken[1] == '=')) //   relation
		{
			if (notted) BADSCRIPT("IF test- cannot do ! in front of comparison %s",nextToken);
			if (*word != '@' &&*word != '$' && *word != '_' && *word != '^' && *word != '%') 
				BADSCRIPT("IF test comparison must be with $var, _#, %sysvar, @1subject or ^fnarg -%s",word);
			strcpy(data,word);
			data += strlen(word);
			*data++ = ' ';
			ptr =  ReadNextSystemToken(in,ptr,word,false,false); //   swallow operator
			strcpy(data,word);
			data += strlen(word);
			*data++ = ' ';
			ptr =  ReadNextSystemToken(in,ptr,word,false,false); //   swallow value
			strcpy(data,word);
			data += strlen(word);
		}
		else if (*nextToken == ')' || !stricmp(nextToken,"and") || !stricmp(nextToken,"or")) //   existence test
		{
			if (*word != '$' && *word != '_' && *word != '@' && *word != '^'  && *word != '%') BADSCRIPT("Bad If existence test - %s. Must be ($var) or (%var) or (_#) or (@#) ",word);
			strcpy(data,word);
			data += strlen(word);
		}
		else BADSCRIPT("Illegal If test - %s %s . Use (X > Y) or (Foo()) or (X IN Y) or ($var) or (_3)",word,nextToken); 
		*data++ = ' ';
		
		//   check for close or more conditions
		ptr =  ReadNextSystemToken(in,ptr,word,false,false); //   )
		if (*word == ')')
		{
			*data++ = ')';
			*data++ = ' ';
		}
		else if (!stricmp(word,"or") || !stricmp(word,"and"))
		{
			MakeLowerCopy(data,word);
			data += strlen(word);
			*data++ = ' ';
			goto pattern;	//   handle next element
		}
		else 
			BADSCRIPT("IF test comparison must close with ) -%s",word);

		char* ifbase = data;
		*data++ = 'a'; //   reserve space for offset after the closing ), which is how far to go past body
		*data++ = 'a';
		*data++ = ' ';

		//   swallow body of IF after test --  must have { surrounding now
		ReadNextSystemToken(in,ptr,word,false,true); //   {
		if (*word != '{') BADSCRIPT("IF body must start with { -%s",word);
		ptr = ReadBody(word,ptr,in,data,rejoinders);
		*data++ = ' ';
		bodyends[bodyendIndex++] = data; //   jump offset to end of if (backpatched)
		DummyEncode(data); //   reserve space for offset after the closing ), which is how far to go past body
		*data++ = ' ';
		Encode(data-ifbase,ifbase);	//   store branch offset to get to ELSE or ELSE IF from start of body
		
		//   now see if ELSE branch exists
		ReadNextSystemToken(in,ptr,word,false,true); //   else?
		if (stricmp(word,"else"))  break; //   caller will add space after our jump index

		//   there is either else if or else
		ptr = ReadNextSystemToken(in,ptr,word,false,false); //   swallow the else
		strcpy(data,"else ");
		data += 5;
		ReadNextSystemToken(in,ptr,word,false,true); //   see if or {
		if (*word == '{') //   swallow the ELSE body now since no IF - add fake successful test
		{
			//   successful test condition for else
			*data++ = '(';
			*data++ = ' ';
			*data++ = '1';
			*data++ = ' ';
			*data++ = ')';
			*data++ = ' ';

			ifbase = data; 
			DummyEncode(data);//   reserve skip data
			*data++ = ' ';
			ptr = ReadBody(word,ptr,in,data,rejoinders);
			*data++ = ' ';
			bodyends[bodyendIndex++] = data; //   jump offset to end of if (backpatched)
			DummyEncode(data);//   reserve space for offset after the closing ), which is how far to go past body
			Encode(data-ifbase,ifbase);	//   store branch offset to get to ELSE or ELSE IF from start of body
			break;
		}
		ptr = ReadNextSystemToken(in,ptr,word,false,false); //   eat the IF
	}
	if (*(data-1) == ' ') --data;	//   remove excess blank

	//   store offsets from successful bodies to the end
	while (bodyendIndex != 0)
	{
		char* at = bodyends[--bodyendIndex];
		Encode(data-at+1,at); //   relative offset,includes blank
	}

	*data = 0;
	return ptr; //   we return with no extra space after us, caller adds it
}

static char* ReadLoop(char* word, char* ptr, FILE* in, char* &data,char* rejoinders)
{
	MakeLowerCase(word);
	strcpy(data,word);
	data += strlen(data);
	*data++ = ' ';

	ptr = ReadNextSystemToken(in,ptr,word,false,false); //   (
	if (*word != '(') BADSCRIPT("Loopcount must start with (around count -%s",word);
	*data++ = '(';
	*data++ = ' ';
	ptr = ReadNextSystemToken(in,ptr,word,false,false); //   counter - 
	if (*word == ')') strcpy(data,"-1"); //   omitted, use -1
	else if (!IsDigit(*word) && *word != '$' && *word != '_' && *word != '^') BADSCRIPT("Loopcounter must be $var, _#, or ^fnarg  -%s",word)
	else 
	{
		strcpy(data,word);
		ptr = ReadNextSystemToken(in,ptr,word,false,false); //   )
	}
	data += strlen(data);
	*data++ = ' ';
	if (*word != ')') BADSCRIPT("Loop counter must end with )  -%s",word);
	*data++ = ')';
	*data++ = ' ';
	char* loopend = data;
	DummyEncode(data); //   encode jump to end of botdy
	*data++ = ' ';
	//   now do body
	ReadNextSystemToken(in,ptr,word,false,true); 
	if (*word != '{') BADSCRIPT("Loop body must start with { -%s",word);
	ptr = ReadBody(word,ptr,in,data,rejoinders);
	Encode(data-loopend+1,loopend);	//   store branch offset to get to body end from start of body (includes extra space WE didnt put here)
	*data = 0;
	return ptr; //   we return with no extra space after us, caller adds it
}

char* ReadOutput(char* ptr, FILE* in,char* &data,char* rejoinders)
{
	char* original = data;
	char word[MAX_WORD_SIZE];
	char nextToken[MAX_WORD_SIZE];
	*assignCall = 0;
	
	while (1) //   read as many tokens as needed to complete the responder definition
	{
		if ((data-original) >= MAX_JUMP_OFFSET) BADSCRIPT("code exceeds size limit of %d bytes",MAX_JUMP_OFFSET);
		ptr = ReadNextSystemToken(in,ptr,word,false); 
		if (!*word)  break; //   end of file
		MakeLowerCopy(lowercaseForm,word);
		if (*word == '#' && (word[1] == '!' || !stricmp(word+1,"ai") ))  //   special comment
		{
			ptr -= strlen(word); //   let someone else see this  also
			break; 
		}

		if (TopLevelWord(word) || TopLevelRule(lowercaseForm) || Continuer(lowercaseForm)) //   responder definition ends when another major unit or top level responder starts
		{
			if (*word != ':') // allow commands here 
			{
				ptr -= strlen(word); //   let someone else see this starter also
				break; 
			}
		}

		ReadNextSystemToken(in,ptr,nextToken,false,true); //   caching request

		switch(*word)
		{
		case '\'':  //   dont separate quote BUG?
			strcpy(data,word);
			data += strlen(data);
			continue;
		case '@': //   factset ref
			if (!IsDigit(word[1])) 
				BADSCRIPT("bad output factset reference - %s",word);
			break;
		case '~': case '$': case '%': 
			break;
		case '[': //   in output area, random choice [] 
				//   or [...]	-- intentionally empty
				//   or [b: xxx] -- has designated rejoinder
				//   or [$var xxx] -- only valid if var is defined
				//   or [$var b: xxx ] -- valid test and designated rejoinder
			*data++ = '[';
			*data++ = ' ';
			if (nextToken[0] == '$')
			{
				ptr = ReadNextSystemToken(in,ptr,word,false,false); //   use up lookahead
				strcpy(data,word);	//   add simple item into data
				data += strlen(data);
				*data++ = ' ';
				ReadNextSystemToken(in,ptr,nextToken,false,true); //   caching request
			}
			if (nextToken[1] == ':' && !nextToken[2]) 
			{
				if (nextToken[0] < 'a' || nextToken[0] >= 'q') BADSCRIPT("Bad level label %s in [ ]",nextToken)
				ptr = ReadNextSystemToken(in,ptr,word,false,false); //   use up lookahead
				MakeLowerCase(word);
				strcpy(data,word);	//   add simple item into data
				data += strlen(data);
				*data++ = ' ';
				if (rejoinders) rejoinders[1+word[0]-'a'] = 2; //   authorized level
			}
			continue;
		}

		if (*nextToken == '=' && !nextToken[1]) // assignment
		{
			strcpy(data,word);	//   add simple item into data
			data += strlen(data);
			*data++ = ' ';
			ptr = ReadNextSystemToken(in,ptr,nextToken,false,false); //   use up lookahead of =
			strcpy(data,"=");	
			++data;
			*data++ = ' ';
			ReadNextSystemToken(in,ptr,nextToken,false,true); //   aim lookahead at followup
			if (!stricmp(nextToken,"^first") || !stricmp(nextToken,"^last") || !stricmp(nextToken,"^random") || 
				!stricmp(nextToken,"first") || !stricmp(nextToken,"last") || !stricmp(nextToken,"random"))
			{
				strcpy(assignCall,word); // need to verfiy it
			}
			continue;
		}
		else if (*nextToken != '(');
		else if (!stricmp(word,"if"))  
		{
			ptr = ReadIf(word,ptr,in,data,rejoinders);
			*data++ = ' ';
			continue;
		}
		else if (!stricmp(word,"loop")) 
		{
			ptr = ReadLoop(word,ptr,in,data,rejoinders);
			*data++ = ' ';
			continue;
		}
		else if (*word != '^') //   looks like a call ... if its ALSO a normal word, presume it is not a call, like: I like (American) football
		{
			// be wary.. respond(foo) might have been text...  
			// How does he TELL us its text? interpret normal word SPACE ( as not a function call?
			char rename[MAX_WORD_SIZE];
			rename[0] = '^';
			strcpy(rename+1,word);	//   in case user omitted the ^
			MakeLowerCase(rename);
			WORDP D = FindWord(rename,0,PRIMARY_CASE_ALLOWED);
			if (D && D->systemFlags & FUNCTION_NAME) // it is a function
			{
				// is it also english. If builtin function, do that for sure
				// if user function AND english, more problematic.  maybe he forgot
				WORDP E = FindWord(word);
				if (!E || !(E->properties & PART_OF_SPEECH) || D->x.codeIndex) strcpy(word,rename); //   a recognized call
				else // use his spacing to decide
				{
					if (*ptr == '(') strcpy(word,rename);
				}
			}
		}
		if (*word == '^' && word[1] != '"' )
		{
			ptr = ReadCall(word,ptr,in,data,*nextToken == '(',false); //   add function call or function var ref
			*assignCall = 0;
		}
		else 
		{
			if (IsAlpha(word[0]) && scriptSpellCheck == 2) CheckScriptWord(word,-1);
			strcpy(data,word);	//   add simple item into data
			data += strlen(data);
		}

		*data++ = ' ';
	}
	*data = 0;

	//   now verify no choice block exceeds CHOICE_LIMIT and that each [ is closed with ]
	while (*original)
	{
		original = ReadCompiledWord(original,word);
		if (*original != '[') continue;

		unsigned int count = 0;
		char* at = original;
		while (*at == '[')
		{
			//   find the closing ]
			while (1) 
			{
				at = strchr(at+1,']'); //   find closing ] - we MUST find it (check in initsql)
				if (!at) BADSCRIPT("Failure to close [ choice in output")
				if (*(at-2) != '\\') break; //   found if not a literal \[
			}
            ++count;
			at += 2;	//   at next token
		}
		if (count >= (CHOICE_LIMIT - 1)) BADSCRIPT("Max %d choices in a row",CHOICE_LIMIT)
		original = at;
	}
	
	return ptr;
}

static char* ReadRule(char* type,char* ptr, FILE* in,char* data,char* basedata)
{//   handles 1 responder/gambit + all rejoinders attached to it

	char kind[MAX_WORD_SIZE];
	strcpy(kind,type);
	char word[MAX_WORD_SIZE];
	char rejoinders[256];	//   legal levels a: thru q:
	memset(rejoinders,0,sizeof(rejoinders));
	if (*type == 't' || *type == 'r') 
		strcpy(priorOutput,currentOutput);
	//   rejoinders == 1 is normal, 2 means authorized in []  3 means authorized and used
	rejoinders[0] = 1;	//   we know we have a responder. we will see about rejoinders later
	while (1) //   read responser + all rejoinders
	{
		MakeLowerCase(kind);
		
		//   validate rejoinder is acceptable
		if (Continuer(kind))
		{
			int level = *kind - 'a' + 1;	//   1 ...
			if (rejoinders[level] >= 2) rejoinders[level] = 3; //   authorized by [b:] and now used
			else if (!rejoinders[level-1]) BADSCRIPT("Illegal rejoinder level %s",kind)
			else rejoinders[level] = 1; //   we are now at this level, enables next level
			//   levels not authorized by [b:][g:] etc are disabled
			while (++level < 20)
			{
				if (rejoinders[level] == 1) rejoinders[level] = 0;
			}
			++acolon;
		}

		//   store the kind away (responder or rejoinder)
		if (kind[0] == 't' || kind[0] == 'r') ++tcolon;
		else if (kind[0] == 'u') ++ucolon;
		else if (kind[0] == 's') ++scolon;
		else if (kind[0] == '?') ++qcolon;
		ruleContext = kind[0];

		strcpy(data,kind); 
		data += 2;
		*data++ = ' ';	
		bool patternDone = false;

#ifdef INFORMATION

A responder of any kind consists of a prefix of `xx  spot xx is an encoded jump offset to go the the
end of the responder. Then it has the kind item (t:   s:  etc). Then a space.
Then one of 3 kinds of character:
	a. a (- indicates start of a pattern
	b. a space - indicates no pattern exists
	c. a 1-byte letter jump code - indicates immediately followed by a label and the jump code takes you to the (

#endif
		char label[MAX_WORD_SIZE];
		label[0] = 0;
		while (1) //   read as many tokens as needed to complete the responder definition
		{
			ptr = ReadNextSystemToken(in,ptr,word,false); 
			if (!*word)  break;
			MakeLowerCopy(lowercaseForm,word);

			unsigned int len = strlen(word);
			if (TopLevelWord(word) || TopLevelRule(lowercaseForm)) 
				break;//   responder definition ends when another major unit or top level responder starts

			if (Continuer(lowercaseForm)) //   a rejoinder marker
			{
				label[0] = 0;
				//   close prior responder
				while (*--data == ' '); //   back up all extra blanks (just in case)
				*++data = ' ';	//   insure closing blank
				strcpy(++data,ENDUNITTEXT); //   close out last topic responder, leaving 1 space
				data += strlen(data);

				//   set up a new responder start
				patternDone = false; //   we are on a new responder really.
				strcpy(data,lowercaseForm); //   must be a top level responder kind
				data += 2;
				*data++ = ' ';	
				continue;
			}

			if (*word == '(') //   found pattern, no label
			{
				if (TopLevelRule(kind)) *lastTopLabel = 0;	// for aigeneration
				ptr = ReadPattern(ptr-1,in,data,false); //   back up and pass in the paren for pattern
				patternDone = true;
				break;
			}
			else //   label or start of output
			{
				char nextToken[MAX_WORD_SIZE];
				ReadNextSystemToken(in,ptr,nextToken,false,true);	//   peek what comes after

				if (*nextToken == '(' && *word != '^') //   label then pattern
				{
					char name[MAX_WORD_SIZE];
					name[0] = '^';
					strcpy(name+1,word);
					WORDP D = FindWord(name,0,LOWERCASE_LOOKUP);
					if (D && D->systemFlags & FUNCTION_NAME)
					{
						Log(STDUSERLOG,"*** Warning- label: %s is a potential macro at line %d in %s. Add ^ if you want it treated as such.\r\n",word,currentFileLine,currentFileName);
						++hasWarnings;
					}

					//   note potential reuse label
					strcpy(label,readTopicName); //   lowercase
					strcat(label,".");
					MakeUpperCase(word);
					strcat(label,word); //   uppercase
					StoreWord(label); //   store full label
					if (TopLevelRule(kind)) strcpy(lastTopLabel,word);	// for aigeneration

					if (len > 40) BADSCRIPT("Label %s must be less than 40 characters",word);
					*data++ = (char)('0' + len + 2); //   prefix attached to label
					strcpy(data,word);
					data += len;
					*data++ = ' ';
					ReadNextSystemToken(NULL,NULL,NULL); // drop lookahead token
					ptr = ReadPattern(ptr,in,data,false); //   read ( for real in the paren for pattern
					patternDone = true;
				}
				else //   we were seeing start of output (no label and no pattern), proceed to output
				{
					if (TopLevelRule(kind)) *lastTopLabel = 0;	// for aigeneration
					*data++ = ' ';
					if (TopLevelStatement(kind) || TopLevelQuestion(kind) || Continuer(word)) BADSCRIPT("Pattern is required for %s responder",kind);
					patternDone = true;
					ReadNextSystemToken(NULL,NULL,NULL); // drop token cache (ptr still in main buffer so we can reread main buffer)
					ptr -= strlen(word); // back up to resee the word
				}
				break;
			}
		} //   END OF WHILE
		if (patternDone) 
		{
			char* newdata = data;
			ptr = ReadOutput(ptr,in,data,rejoinders);
			if ( *kind == 't' || *kind == 'r') 
				strcpy(currentOutput,newdata); // for autogeneration from gambits
	
			//   data points AFTER last char added. Back up to last char, if blank, leave it to be removed. else restore it.
			while (*--data == ' '); 
			*++data = ' ';
			strcpy(data+1,ENDUNITTEXT); //   close out last topic item+
			data += strlen(data);

			while (1)
			{
				ptr = ReadNextSystemToken(in,ptr,word,false); 
				MakeLowerCopy(lowercaseForm,word);
				if (*word == '#' && word[1] == '!')  ptr = NoteTester(basedata); //   special comment, pass it to top
				else break;
			}
			if (!*word || TopLevelWord(word) || TopLevelRule(lowercaseForm))  
			{
				ptr -= strlen(word);
				break;//   responder definition ends when another major unit or top level responder starts
			}

			//   Word must be a rejoinder item
			strcpy(kind,lowercaseForm);
		}
		else ReportBug("unexpected in ReadRule");
	}

	//   did he forget to fill in any [] jumps
	for (unsigned int i = ('a'-'a'); i <= ('q'-'a'); ++i)
	{
		if (rejoinders[i] == 2) BADSCRIPT("Failed to define rejoinder %c: for responder just ended", i + 'a' - 1);
	}

	*data = 0;
	return ptr;
}

static char* ReadMacro(char* ptr,FILE* in,char* kind,unsigned int zone)
{
	bool table = !stricmp(kind,"table:");
	bool patternmacro = !stricmp(kind,"patternMacro:");
	char macroName[MAX_WORD_SIZE];
	*macroName = 0;
	functionArgumentCount = 0;
	char word[MAX_WORD_SIZE];
	char data[MAX_BUFFER_SIZE];
	*data = 0;
	char* pack = data;
	int parenLevel = 0;
	WORDP D = NULL;
	bool gettingArguments = true;
	while (gettingArguments) //   read as many tokens as needed to get the name and macroArgumentList
	{
		ptr = ReadNextSystemToken(in,ptr,word,false);
		if (!*word) break; //   end of file

		if (!*macroName) //   get the macro name
		{
			if (*word == '^') strcpy(word,word+1); //   remove his ^
			MakeLowerCase(word);
			if (!IsAlpha(word[0]) && !table) BADSCRIPT("Macro name must be alpha %s",word);
			if (table)
			{
				strcpy(macroName,"tbl:");
				strcpy(macroName+4,word);
				Log(STDUSERLOG,"Reading table %s\r\n",macroName);
			}
			else
			{
		
				macroName[0] = '^';
				strcpy(macroName+1,word);
				Log(STDUSERLOG,"Reading %s %s\r\n",kind,macroName);
			}
			D = StoreWord(macroName);
			if (D->systemFlags & FUNCTION_NAME && !table) BADSCRIPT("macro %s already defined",macroName);
			continue;
		}

		unsigned int len = strlen(word);
		if (TopLevelWord(word)) //   definition ends when another major unit starts
		{
			ptr -= len; //   let someone else see this starter also
			break; 
		}

		switch(*word)
		{
			case '(': 
				if (parenLevel++ != 0) BADSCRIPT("bad paren level in macro definition %s",macroName);
				continue; //   macroArgumentList open
			case ')':
				if (--parenLevel != 0) 
					BADSCRIPT("bad closing paren in macro definition %s",macroName);
				gettingArguments = false;
				break;
			case '^': 
				//   declaring a new argument
				strcpy(functionArguments[functionArgumentCount++],word);	//   save the name
				if (functionArgumentCount > MAX_ARGUMENT_COUNT)  BADSCRIPT("Too many macroArgumentList to %s - max is %d",macroName,MAX_ARGUMENT_COUNT);
				continue;
			default:
				BADSCRIPT("Bad argument to macro definition %s",macroName);
		}
	}
	if (!D) return ptr; //   nothing defined

	AddSystemFlag(D,FUNCTION_NAME|zone); 
	if (patternmacro) AddSystemFlag(D,IS_PATTERN_MACRO); 
	D->v.argumentCount = functionArgumentCount;
	currentFunctionDefinition = D; //   global handover for table processing

	ptr = patternmacro ? ReadPattern(ptr,in,pack,true) : ReadOutput(ptr,in,pack,NULL); 
	*pack = 0;

	//   record that it is a macro, with appropriate validation information
	D->w.fndefinition = AllocateString(data);
	D->x.codeIndex = 0;	//   if one redefines a system macro, that macro is lost.

	if (!table) //   tables are not real macros, they are temporary
	{
		//   write out definition -- this is the real save of the data
		FILE* out = UTF8Open(zone == BUILD0 ? (char*)"TOPIC/macros0.txt" : (char*)"TOPIC/macros1.txt","ab");
		fprintf(out,"%s %c %d %s\r\n",macroName,(patternmacro) ? 'P' : 'O',functionArgumentCount,data);
		fclose(out);
	}
	return ptr;
}

static char* ReadTable(char* ptr, FILE* in,unsigned int zone)
{
	char word[MAX_WORD_SIZE];
	int oldjump = jumpIndex;
	ptr = SkipWhitespace(ptr);
	bool shortAllowed = false;
	if (!strnicmp(ptr,"... ",4))
	{
		shortAllowed = true;
		ptr += 4;
	}

	ptr = ReadMacro(ptr,in,"table:",zone); //   defines the name,macroArgumentList, and script
	char post[MAX_WORD_SIZE]; 
	char* pre;
	char* macroArgumentList = AllocateBuffer();
	ptr = ReadNextSystemToken(in,ptr,word,false,false); //   the DATA: separator
	if (stricmp(word,"DATA:")) 
		BADSCRIPT("missing DATA: separator for table - %s",word);
	
	jumpIndex = 2;
	int holdDepth = globalDepth;
	while (1) //   process table lines into fake function calls
	{
		if (setjmp(scriptJump[2])) //   flush on error
		{
			jumpIndex = oldjump;
			globalDepth = holdDepth;
			while (1)
			{
				ptr = ReadNextSystemToken(in,ptr,word,false,false);
				if (!*word) break;
				if (TopLevelWord(word)) //   ok, outside handle this now
				{
					ptr -= strlen(word);
					break;
				}
			}
			break;
		}

		ptr = ReadNextSystemToken(in,ptr,word,false,false); //   get 1st token of this line

		if (*word == ':' && word[1]) //system command, like to alter tracing
		{
			DoCommand(readBuffer);
			*readBuffer = 0;
			*ptr = 0;
			continue;
		}
		if (!*word || TopLevelWord(word) ) // end of file, top level
		{
			ptr -= strlen(word);
			break; //   definition ends when another major unit or top level responder starts
		}
	
		char* original = readBuffer;	//   able to repeat this line
		ptr = readBuffer;	//   we will read only in this line for now
		
		//   process a data set from the line
		char* systemArgumentList = macroArgumentList;
		*systemArgumentList++ = '(';
		*systemArgumentList++ = ' ';
		unsigned int argCount = 0;
		unsigned int multiCount = 0;
		char* multi = NULL; //   the multiple interior
		while (1) //   for a table entry get its general argument flavor. One argument can be a set (multiple values)
		{
			ptr = ReadSystemToken(ptr,word);	//   next item to associate
			if (!*word && !shortAllowed) break;			//   end of LINE of items stuff

			if ((!*word && shortAllowed) || !strcmp(word,"...")) // fill in with * defaulting the remaining fields
			{
				while (argCount < currentFunctionDefinition->v.argumentCount)
				{
					strcpy(systemArgumentList,"*");
					systemArgumentList += strlen(systemArgumentList);
					*systemArgumentList++ = ' ';
					++argCount;
				}
				break;
			}
	
			if (!stricmp(word,"\\n")) 
			{
				strcpy(readBuffer,ptr);	//   erase earlier stuff we've read
				ptr = readBuffer;
				break; //   pretend end of line
			}
			if (*word == '[' ) 
			{
				multiCount = argCount; // multi is at this arg
				pre = systemArgumentList;  //   memorize the before set, spot future multi items get put
				if (multi)  
					BADSCRIPT("Too many multiple choices");
				multi = ptr; //   multi choices start here
				char* at = strchr(ptr,']'); //   prove there is an end
				if (!at) BADSCRIPT("bad [ ] ending %s in table %s",readBuffer,currentFunctionDefinition->word)
				else ptr = at + 1; //   continue main macroArgumentList AFTER the multi set
				++argCount;
				continue; //   skipping over this arg
			}
			unsigned int control  = 0;
			if (*word == '^' && word[1] == '"')
			{
				control = COMPILEDBURST;
				word[0] = '"';
				word[1] = ' '; 
			}
			strcpy(word,JoinWords(BurstWord(word,CONTRACTIONS|control)));
			WORDP baseWord = StoreWord(word,(control) ? AS_IS : 0);
			strcpy(word,baseWord->word); 

			//   store next argument
			if (*word == '\'') //   quoted value
			{
				ptr = ReadSystemToken(ptr,word);	//   next item to associate
				if (!ptr || !*word ) break;			//   end of LINE of items stuff
				baseWord = StoreWord(word,(control) ? AS_IS : 0);
				strcpy(word,baseWord->word); 
			}
			strcpy(systemArgumentList,word);
			systemArgumentList += strlen(systemArgumentList);
			*systemArgumentList++ = ' ';
			++argCount;

			//   handle synonyms as needed
			ptr = SkipWhitespace(ptr); //   to align to see if (given 
			MEANING base = MakeMeaning(baseWord);
			if (*ptr == '(' && ++ptr) while (1) //   synonym given, make it
			{
				ptr = ReadSystemToken(ptr,word);
				if (!*word)  BADSCRIPT("Failure to end synomyms in table %s",currentFunctionDefinition->word);
				if (*word == ')') break;	//   end of synonms
				strcpy(word,JoinWords(BurstWord(word,CONTRACTIONS)));
				WORDP E = StoreWord(word,0);
				MEANING syn = MakeMeaning(E);
				CreateFact(syn,Mmember,base); 
			}
		}
		*systemArgumentList = 0;
		if (multi) strcpy(post,pre); //   save the macroArgumentList after the multi

		//   now we have one map of the macroArgumentList row
		if (argCount && argCount != currentFunctionDefinition->v.argumentCount) 
		{
			BADSCRIPT("Bad table %s in table %s, want %d macroArgumentList and have %d",original,currentFunctionDefinition->word,currentFunctionDefinition->v.argumentCount,argCount);
		}

		//   table line is read, now execute rules on it, perhaps multiple times, after stuffing in the multi if one
		if (argCount) //   we swallowed a dataset. Process it
		{
			while (1)
			{
				//   prepare variable macroArgumentList
				if (multi) //   do it with next multi
				{
					multi = ReadSystemToken(multi,word); //   get choice
					unsigned int control = 0;
					if (*word == '^' && !word[1] && *ptr== '"')
					{
						control = COMPILEDBURST;
						word[0] = '"';
						word[1] = ' '; 
					}
					strcpy(word,JoinWords(BurstWord(word,CONTRACTIONS|control)));
					strcpy(word,StoreWord(word,(control) ? AS_IS : 0)->word); 

					if (!*word || *word == ']') break;			//   end of multi
					if (*word == '\'') //   quoted value
					{
						multi = ReadSystemToken(multi,word); //   get 1st of choice
						if (!*word || *word == ']') break;			//   end of LINE of items stuff
						strcpy(pre,StoreWord(word,0)->word); //   record the local w/o any set expansion
					}
					else 
					{
						WORDP D = StoreWord(word,0);
						strcpy(pre,D->word); //   record the multi
						multi = SkipWhitespace(multi);
						if (*multi == '(' && ++multi) while(multi) //   synonym 
						{
							multi = ReadSystemToken(multi,word);
							if (!*word) BADSCRIPT("Failure to close synonym list in table %s",currentFunctionDefinition->word);
							if (*word == ')') break;	//   end of synonms
							WORDP E = StoreWord(word,0);
							MEANING syn = MakeMeaning(E);
							MEANING base = MakeMeaning(D);
							CreateFact(syn,Mmember,base); 
						}
					}
					char* at = pre + strlen(pre);
					*at++ = ' ';
					strcpy(at,post); //   add rest of macroArgumentList
					systemArgumentList = at + strlen(post);
				}
				*systemArgumentList++ = ')';	//   end of call setup
				*systemArgumentList = 0;
				
				unsigned int result;
				globalDepth = 2;
				DoFunction(currentFunctionDefinition->word,macroArgumentList,currentOutputBase,result);
				globalDepth = 0;
				if (!multi) break;
			}
		}
	}
	FreeBuffer();
	jumpIndex = oldjump; //   return to  old error handler

	currentFunctionDefinition->systemFlags &= -1LL ^ FUNCTION_NAME;
	currentFunctionDefinition->v.argumentCount = 0;
	currentFunctionDefinition->w.fndefinition = NULL;
	AddSystemFlag(currentFunctionDefinition,DELETED_WORD);
	currentFunctionDefinition = NULL; 
	return ptr;
}


static char* ReadTopic(char* ptr, FILE* in,unsigned int zone)
{
	char word[MAX_WORD_SIZE];
	char* data = (char*) malloc(MAX_TOPIC_SIZE);
	StartRemaps(); //   spot remaps were when we started
	unsigned int TopLevelRules = 0;
	++topicCount;
	*data = 0;
	FACT* base = factFree;
	char* pack = data;
	*readTopicName = 0;
	uint64 flags = 0;
	bool topicFlagsDone = false;
	bool keywordsDone = false;
	int parenLevel = 0;
	bool quoted = false;
	MEANING topic;
	WORDP D = NULL;
	//   EXAMPLE--- topic: ~readTopicName system noerase [key1 'key2 "key words"]
	//			  r: LABEL (pattern data) This is output
	//			  t: This is more output
	//				a: (* ) a rejoinder responder
	//			  ?: () a question responder
	//			  s: () a statement responder
	//			  u: () a dual responder


	//   if error occurs lower down, flush to here
	jumpIndex = 1;
	int holdDepth = globalDepth;
	if (setjmp(scriptJump[1]))
	{
		globalDepth = holdDepth;
		//   flush ALL data
		pack = data;
		*pack = 0;

		//   eat tokens to use up current unit being read
		while (1)
		{
			ptr = ReadNextSystemToken(in,ptr,word,false);
			if (!*word) break;
			MakeLowerCopy(lowercaseForm,word);
			if (TopLevelWord(word) || TopLevelRule(lowercaseForm)) //   ok, let the loop below handle this now
			{
				ptr -= strlen(word);
				break;
			}
		}
	}
	WORDP topicName = NULL;
	++tcount;
	while (1) //   read as many tokens as needed to complete the definition
	{
		ptr = ReadNextSystemToken(in,ptr,word,false);
		if (!*word) break;

		if (!*readTopicName) //   get the topic name
		{
			if (*word != '~') BADSCRIPT("Topic name - %s must start with ~",word);
			strcpy(readTopicName,word);
			Log(STDUSERLOG,"Reading topic %s\r\n",readTopicName);
			topicName = FindWord(readTopicName);
			if (topicName && topicName->systemFlags & CONCEPT && !(topicName->systemFlags & TOPIC_NAME)) BADSCRIPT("Concept already defined with this topic name %s",readTopicName);
			topicName = StoreWord(readTopicName);
			topic = MakeMeaning(topicName);
			// handle potential multiple topics of same name
			duplicateCount = 0;
			while (topicName->systemFlags & TOPIC_NAME)
			{
				++duplicateCount;
				char name[MAX_WORD_SIZE];
				sprintf(name,"%s.%d",readTopicName,duplicateCount);
				topicName = StoreWord(name);
				if (!*duplicateTopicName) strcpy(duplicateTopicName,readTopicName);
			}
			strcpy(readTopicName,topicName->word);
			AddSystemFlag(topicName,TOPIC_NAME|zone|CONCEPT);  
			topicName->w.topicRestriction = NULL;
	
			//   empty the verify file
			sprintf(word,"VERIFY/%s.txt",readTopicName+1); 
			FILE* in = fopen(word,"wb");
			if (in) fclose(in);

			continue;
		}

		unsigned int len = strlen(word);
		if (TopLevelWord(word)) //   definition ends when another major unit starts
		{
			ptr -= len; //   let someone else see this starter also
			break; 
		}

		switch(*word)
		{
		case '#':
			if (*word == '#' && word[1] == '!')  NoteTester(data);//   special comment
			continue;
		case '(': case '[':
			topicFlagsDone = true; //   topic flags must occur before list of keywords
			++parenLevel;
			break;
		case ')': case ']':
			--parenLevel;
			if (parenLevel == 0) keywordsDone = true;
			break;
		default:
			MakeLowerCopy(lowercaseForm,word);
			if (!topicFlagsDone) //   do topic flags
			{
				if (!strnicmp(word,"bot=",4)) 
				{
					char botlist[MAX_WORD_SIZE];
					MakeLowerCopy(botlist,word);
					strcat(botlist," ");
					topicName->w.topicRestriction = AllocateString(botlist+3,strlen(botlist+3));//  strip off "bot" in the thing limited to a specific bot(s), can do bot=harry,georgia,roger
					topicName->w.topicRestriction[0] = ' '; // change = to space
					char* x;
					while ((x = strchr(topicName->w.topicRestriction,','))) *x = ' ';	// change comma to space. all bot names have spaces on both sides
				}
				else if (!stricmp(word,"user"));
				else if (!stricmp(word,"system")) flags |= TOPIC_SYSTEM | TOPIC_NOERASE | TOPIC_NOSTAY;
				else if (!stricmp(word,"random")) flags |= TOPIC_RANDOM;
                else if (!stricmp(word,"noerase")) flags |= TOPIC_NOERASE; //   sy+		o->word	0x0b5a936c " friz_accordion"	char *
				else if (!stricmp(word,"nostay")) flags |= TOPIC_NOSTAY; ///   system default
				else if (!stricmp(word,"repeat")) flags |= TOPIC_REPEAT; //   system default
 				else if (!stricmp(word,"norandom")) flags &= -1 ^TOPIC_RANDOM;
				else if (!stricmp(word,"erase")) flags &= -1 ^TOPIC_NOERASE;
				else if (!stricmp(word,"stay")) flags &= -1 ^TOPIC_NOSTAY;
				else if (!stricmp(word,"norepeat")) flags &= -1 ^TOPIC_REPEAT;
				else if (!stricmp(word,"priority"))  flags |= TOPIC_PRIORITY; //   system default
				else if (!stricmp(word,"normal")) flags &= -1 ^TOPIC_PRIORITY;
                else if (!stricmp(word,"deprioritize")) flags |= TOPIC_LOWPRIORITY; 
                else BADSCRIPT("Bad topic flag %s for topic %s",word,readTopicName);
			}
			else if (!keywordsDone) //   absorb keyword list
			{
				if (*word == '\'') //   comes back separated from next token, merge them by not spacing after this
				{
					quoted = true; //   even if next word repeats something we have, we MUST list it since we have put the '
					continue;
				}
				char* at;
				MEANING M;
				if (word[1] && (at = strchr(word+1,'~'))) //   wordnet meaning request, confirm definition exists
				{
					char level[1000];
					strcpy(level,at);
					M = ReadMeaning(word);	// returns synset
					if (!M)  BADSCRIPT("WordNet word doesn't exist %s",word);
					WORDP D = Meaning2Word(M);
					if (D->meaningCount == 0 && !(M & BASIC_POS))  
						BADSCRIPT("WordNet word has no such meaning %s",word);
					unsigned int index = Meaning2Index(M);
					if (index && !strcmp(word,D->word) && index > D->meaningCount)
						BADSCRIPT("WordNet word has no such meaning index %s",word);
				}
				else M = ReadMeaning(word);			
				uint64 flags = 0;
				if (quoted) flags |= ORIGINAL_ONLY;
				quoted = false;
				CreateFact(M,Mmember,topic,flags); 
			}
			else if (TopLevelRule(lowercaseForm))//   absorb a responder/gambit and its rejoinders
			{
				if (pack == data)
				{
					strcpy(pack,ENDUNITTEXT+1);	//   init 1st rule
					pack += strlen(pack);
				}
				++TopLevelRules;
				ptr = ReadRule(lowercaseForm,ptr,in,pack,data); //u: (_* ) '_#0 who? 
				pack += strlen(pack);
				if ((pack - data) > (MAX_TOPIC_SIZE - 2000)) BADSCRIPT("Topic %s data too big. Split it by calling another topic using u: () respond(~subtopic) and putting the rest of the rules in that subtopic",readTopicName);
			}
			else BADSCRIPT("Expecting responder for topic %s, got %s",readTopicName,word);
		}
	}

	jumpIndex = 0; //   revert to top level compile handler

	if (parenLevel) BADSCRIPT("Failure to balance (");
	if (!topicName) BADSCRIPT("No topic name?");

	int len = pack-data;
	if (!len) 
	{
		Log(STDUSERLOG,"*** Warning- No data in topic %s\r\n",readTopicName);
		++hasWarnings;
	}

	bool hasUpper;
	uint64 checksum = Hashit((unsigned char*) data, len, hasUpper);

	//   trailing blank after jump code
    NextinferMark();
    SetJumpOffsets(data);
	if (len >= MAX_TOPIC_SIZE-100) BADSCRIPT("Too much data in one topic");
	FILE* out = fopen(zone == BUILD0 ? "TOPIC/script0.txt" : "TOPIC/script1.txt","ab");

	char* restriction = (topicName->w.topicRestriction) ? topicName->w.topicRestriction : (char*)"all";
	fprintf(out,"TOPIC: %s %llu %llu %d \" %s \" ",readTopicName,flags,checksum,TopLevelRules,restriction); //   extra space after restriction is deliberate
	fwrite(data,1,strlen(data),out);
	fwrite("\r\n",1,2,out);
	fclose(out);
	char* ptr1 = data + JUMP_OFFSET; // at a responder heading
	while (ptr1)  ptr1 = FindNextResponder(NEXTRESPONDER,ptr1); //   prove layout can be walked

	//   remove local remaps
	ClearRemaps();
	free(data);
	return ptr;
}

static char* ReadConcept(char* ptr, FILE* in,unsigned int zone)
{
	char word[MAX_WORD_SIZE];
	MEANING concept;
	char conceptName[MAX_WORD_SIZE];
	*conceptName = 0;
	bool quoted = false;
	int parenLevel = 0;
	MEANING comment = 0;
	WORDP D;
	FACT* base = factFree; //   spot we started
	++ccount;

	//   Example---  concept: ~words (word1 'word2 "phrase of words" ~another_concept  )
	//   It is not illegal to repeat words, or have words stated AND implicitly inherited.

	while (1) //   read as many tokens as needed to complete the definition (must be within same file)
	{
		ptr = ReadNextSystemToken(in,ptr,word,false);
		if (!*word) break;	//   file ran dry

		if (!*conceptName) //   get the concept name, will be ~xxx or :xxx 
		{
			if (*word != '~' ) BADSCRIPT("Concept name must begin with ~ or : - %s",word);
			strcpy(conceptName,word);
			D = FindWord(conceptName);
			if (D && D->systemFlags & CONCEPT) BADSCRIPT("Concept/topic already defined %s",conceptName);
			D = StoreWord(conceptName,0);
			AddSystemFlag(D,zone|CONCEPT); 
			concept = MakeMeaning(D);
			Log(STDUSERLOG,"Reading concept %s\r\n",conceptName);

			//   check for comment
			ReadNextSystemToken(in,ptr,word,true); //   lookahead only
			if (*word == '"')
			{
				ptr = ReadNextSystemToken(in,ptr,word,false);
				char* at;
				while ((at = strchr(word,' '))) *at = '_'; //   BUG why does this be needed
				comment = MakeMeaning(StoreWord(word));
				CreateFact(comment,MakeMeaning(StoreWord("comment")),concept);
			}

			continue;
		}

		unsigned int len = strlen(word);
		if (TopLevelWord(word)) //   definition ends when another major unit starts
		{
			ptr -= len; //   let someone else see this starter 
			break; 
		}
		char* at;
		MEANING M;
		switch(*word) //   THE MEAT OF CONCEPT DEFINITIONS
		{
			case '(':  case '[':
				parenLevel++;
				break;
			case ')': case ']':
				--parenLevel;
				if (parenLevel < 0) BADSCRIPT("Missing ( for concept definition %s",conceptName);

				break;
			case '\'': //   comes back separated from next token, merge them by not spacing after this
				quoted = true;	//   since we emitted the ', we MUST emit the next token
				continue;
			default:
				if (*word == '~' || (*word == '\'' && word[1] == '~')) MakeLowerCase(word); //   set are always lower case
				if (word[1] && (at = strchr(word+1,'~'))) //   wordnet meaning request, confirm definition exists
				{
					char level[1000];
					strcpy(level,at);
					M = ReadMeaning(word);
					if (!M)  BADSCRIPT("WordNet word doesn't exist %s",word);
					WORDP D = Meaning2Word(M);
					if (D->meaningCount == 0 && !(M & BASIC_POS))  
						BADSCRIPT("WordNet word does not have such meaning %s",word);
					unsigned int index = Meaning2Index(M);
					if (index && !strcmp(word,D->word) && index > D->meaningCount)
						BADSCRIPT("WordNet word has no such meaning index %s",word);
				}
				else M = ReadMeaning(word);
				if (Meaning2Index(M)) M = GetMaster(M); //   a named wordnet subtree, use head
				//   pointless to refer to self like concept: ~car (car ~car) but might have declared
				//   via table a bunch of stuff. This indicates that there is already stuff (just a comment)
				if (M != concept) CreateFact(M,Mmember,concept, quoted ? ORIGINAL_ONLY : 0);  //   unique choices only (skip repeats) -- BUG caution- dont write out dictionary with this word in it
				quoted = false;
				break;
		}
		if (parenLevel == 0) break;
	}
	if (parenLevel) BADSCRIPT("Failure to close (in concept %s",conceptName);

	return ptr;
}

FILE* FOpen(char* name, char* unused)
{
	strcpy(currentFileName,name);
	currentFileLine = 0;
	FILE* f = fopen(name,"rb");
	char* at = strrchr(currentFileName,'/');
	if (at) strcpy(currentFileName,at+1);
	at = strrchr(currentFileName,'\\');
	if (at) strcpy(currentFileName,at+1);
	return f;
}

static void ReadTopicFile(char* name,unsigned int zone) //   read contents of a topic file (.top or .tbl)
{
	char word[MAX_WORD_SIZE];
	*currentOutput = *priorOutput = 0;
	FILE* in = FOpen(name,"rb");
	if (!in) 
	{
		Log(STDUSERLOG,"*** Warning- Missing file %s\r\n",name);
		++hasWarnings;
		return;
	}
	Log(STDUSERLOG,"\r\n----Reading file %s\r\n",currentFileName);
	char* ptr = "";

	//   if error occurs lower down, flush to here
	int holdDepth = globalDepth;
	if (setjmp(scriptJump[0]))
	{
		globalDepth = holdDepth;
		while (1)
		{
			ptr = ReadNextSystemToken(in,ptr,word,false);
			if (!*word) break;
			if (TopLevelWord(word)) //   ok, let the loop below handle this now
			{
				ptr -= strlen(word);
				break;
			}
		}
	}

	while (1) 
	{
		ptr = ReadNextSystemToken(in,ptr,word,false); //   eat tokens (should all be top level)
		if (!*word) break;						//   no more tokens found

		currentFunctionDefinition = NULL; //   can be set by ReadTable or ReadMacro
		if (*word == ':' && word[1])
		{
			DoCommand(readBuffer);
			*readBuffer = 0;
			*ptr = 0;
			continue;
		}
		if (!stricmp(word,"concept:")) ptr = ReadConcept(ptr,in,zone);
		else if (!stricmp(word,"topic:"))  ptr = ReadTopic(ptr,in,zone);
		else if (!stricmp(word,"table:")) ptr = ReadTable(ptr,in,zone);
		else if (!stricmp(word,"patternMacro:") || !stricmp(word,"outputMacro:")) ptr = ReadMacro(ptr,in,word,zone);
		else 
		{
			int x = 0;
			x = x / x;
			BADSCRIPT("Unknown top-level declaration %s in %s",word,name);
		}
	}
	fclose(in);
}

void EraseTopicFiles(unsigned int zone)
{
	int i = -1;
	while (topicFiles[++i])
	{
		char file[MAX_WORD_SIZE];
		strcpy(file,topicFiles[i]);
		char* ptr = strchr(file,'.') - 1;
		if (*ptr == 'x') *ptr = (zone == BUILD0) ? '0' : '1'; // unique name per file zone
		remove(file);
	}
}

void DoubleCheckReuse()
{
	FILE* in = fopen("TOPIC/missingLabel.txt","rb");
	if (!in) return;

	char word[MAX_WORD_SIZE];
	char file[MAX_WORD_SIZE];
	char topic[MAX_WORD_SIZE];
	unsigned int line;
	while (ReadALine(readBuffer,in))
	{
		char *ptr = ReadCompiledWord(readBuffer,word); //   reuse goal
		ptr = ReadCompiledWord(ptr,topic);	//   from topic
		ptr = ReadCompiledWord(ptr,file);	//   from file
		ReadInt(ptr,line);			//   from line
		WORDP D = FindWord(word);
		char* at = strchr(word,'.');
		if (!D)
		{
			if (!strcmp(topic,word)) 
			{
				++hasErrors;
				Log(STDUSERLOG,"** Missing local label %s for reuse/unerase in topic %s in File: %s Line: %d Topic:%s\r\n",at+1,topic,file,line);
			}
			else 
			{
				++hasWarnings;
				Log(STDUSERLOG,"** Warning- Missing cross-topic label %s for reuse in File: %s Line: %d\r\n",word,file,line);
			}
		}
	}
	fclose(in);
}

static void WriteConcepts(WORDP D, void* flag)
{
	uint64 zone = (long long) flag;
	if (D->word[0] != '~') return; // not a topic or concept
	if (!(D->systemFlags & zone)) return; // not defined during this build
	D->systemFlags &= -1LL ^ (BUILD0|BUILD1);
		
	// write out keywords 
	FILE* out = fopen(zone == BUILD0 ? "TOPIC/keywords0.txt" : "TOPIC/keywords1.txt","ab");
	fprintf(out,"%s ( ",D->word);
	FACT* F = GetObjectHead(D);
	if (F)
	{
		unsigned int len = 0;
		while (F) //   now have unique non-repeating members
		{
			if (F->properties & FACT_MEMBER) //   one of them
			{
				char word[MAX_WORD_SIZE];
				WORDP D = Meaning2Word(F->subject);
				if (D->word[0] == '"') sprintf(word,"%s ",JoinWords(BurstWord(D->word)));
				else if (F->properties & ORIGINAL_ONLY) sprintf(word,"'%s ",WriteMeaning(F->subject));
				else sprintf(word,"%s ",WriteMeaning(F->subject));
				unsigned int wlen = strlen(word);
				len += wlen;
				fwrite(word,1,wlen,out);
				if (len > 500)
				{
					fprintf(out,"\r\n    ");
					len = 0;
				}
				F->properties |= DEADFACT;
			}
			F = GetObjectNext(F);
		}
	}
	fprintf(out,")\r\n");
	fclose(out);
}

FILE* UTF8Open(char* filename,char* mode)
{
	FILE* in = fopen(filename,"rb");
	if (in) fclose(in);
	FILE* out = fopen(filename,mode);
	if (out && !in) // prepare file for UTF8 data if it doesnt yet have any content
	{
		unsigned char smarker[3];
		smarker[0] = 0xEF;
		smarker[1] = 0xBB;
		smarker[2] = 0xBF;
		fwrite(smarker,1,3,out);
	}
	return out;
}

bool ReadTopicFiles(char* name,unsigned int zone,FACT* base,int spell)
{
	generalMap.clear();
	initedGeneralizer = false;
	scriptSpellCheck = spell;
	FILE* in = fopen(name,"rb");
	if (!in)
	{
		printf("%s not found\r\n",name);
		return false;
	}
	int oldtokenControl = tokenControl;
	tokenControl = 0;
	buildID = zone;
	duplicateTopicName[0] = 0;
	ResetWarnings();
	*newBuffer = 0;
	ccount = tcolon = rcolon = scolon = qcolon = ucolon = acolon = tcount = 0;
	ClearUserVariables();

	//   erase facts and dictionary to appropriate level
	while (factFree > base) FreeFact(factFree--); //   restore back to facts alone
	if (zone == BUILD1) ReturnDictionaryToBuild0(); // rip dictionary back to start of zone (but props and systemflags can be wrong)
	else ReturnDictionaryToWordNet();
	EraseTopicFiles(zone);
	hasWarnings = hasErrors = 0;
	echo = true;
	ReadWordsOf("LIVEDATA/allownonwords.txt",PATTERN_ALLOWED,true);

	//   store known pattern words in pattern file 
	NextMatchStamp(); //   for marking which patternwords we have written
	patternFile = fopen(zone == BUILD0 ? "TOPIC/patternWords0.txt" : "TOPIC/patternWords1.txt","wb");
	if (!patternFile)
	{
		printf("Unable to create patternfile  in the TOPIC subdirectory? Make sure this directory exists and is writable.\r\n");
		tokenControl  = oldtokenControl;
		return false;
	}

	// predefine builtin sets
	unsigned int i = 0;
	if (zone == BUILD0) 
	{
		while (predefinedSets[i]) 
		{
			WORDP D = StoreWord((char*) predefinedSets[i++]);
			AddSystemFlag(D,CONCEPT|zone);
			fprintf(patternFile,"%s\r\n",D->word); //   predefined vocabulary for pattern matching
			++ccount;
		}
		for (unsigned int i = 0; i <= 63; ++i) 
		{
			fprintf(patternFile,"%s\r\n",posWords[i]->word); //   predefined vocabulary for pattern matching
			++ccount;
		}
	}
	currentOutputBase = AllocateBuffer();	//   for all output processing

	FILE* out = UTF8Open(zone == BUILD0 ? (char*)"TOPIC/script0.txt" : (char*)"TOPIC/script1.txt","wb");
	fprintf(out,"0    \r\n"); //   reserve 5-digit count for number of topics
	fclose(out);

	//   read file list to service
	topicCount = 0;
	char word[MAX_WORD_SIZE];
	char file[MAX_WORD_SIZE];
	while (ReadALine(readBuffer,in))
	{
		if (strstr(readBuffer,"bot=")) continue; 
		char* ptr = SkipWhitespace(readBuffer);
		ReadCompiledWord(ptr,word);
		if (*word == '#' || !*word) continue;
		if (!stricmp(word,"stop")) break; //   fast abort
		if (*word == ':' && word[1]) //   process command
		{
			DoCommand(readBuffer);
			*readBuffer = 0;
			*ptr = 0;
			continue;
		}
		sprintf(file,"%s",word);
		ReadTopicFile(file,zone);
	}
	if (in) fclose(in);
	*newBuffer = 0;

	FreeBuffer();

	DoubleCheckSet(); //   prove all sets  he used were defined
	DoubleCheckReuse();
	fclose(patternFile);
	remove("TOPIC/missingSets.txt");
	remove("TOPIC/missingLabel.txt");
	scriptSpellCheck = 0;
	buildID = 0;
	if (duplicateTopicName[0]) 	Log(STDUSERLOG,"Be advised you have duplicate topic names, e.g., %s\r\n",duplicateTopicName);
	Log(STDUSERLOG,"topics:%d concepts:%d rules:%d t:%d s:%d ?:%d u:%d a:%d \r\n",tcount,ccount,tcolon+scolon+qcolon+ucolon+acolon,
			tcolon,scolon,qcolon,ucolon,acolon);

	//   write how many topics were found (for when we preload during normal startups)
	out = fopen(zone == BUILD0 ? "TOPIC/script0.txt" : "TOPIC/script1.txt","rb+");
	fseek(out,0,SEEK_SET);
	char tmp[100];
	sprintf(tmp,"%05d",topicCount);
	fwrite(tmp,1,5,out);
	fclose(out);

	// we delay writing out keywords til now, allowing multiple accumulation across tables and concepts
	WalkDictionary(WriteConcepts,(void*)zone);

	//   now dump the facts created by reading topic files (which includes dictionary changes) and variables
	WriteExtendedFacts(fopen(zone == BUILD0 ? "TOPIC/facts0.txt" : "TOPIC/facts1.txt","wb"),base,zone); //   write FROM HERE to end
	tokenControl  = oldtokenControl;

	if (hasErrors) //   kill of the data
	{
		EraseTopicFiles(zone);
		Log(STDUSERLOG,"%d errors in build - press Enter to quit. Fix and try again.\r\n",hasErrors);
		ReadALine(readBuffer,stdin);
		return false;
	}
	else if (hasWarnings) 
	{
		DumpWarnings();
		Log(STDUSERLOG,"%d warnings in build - press a key and Enter to continue\r\n    ",hasWarnings);
		ReadALine(readBuffer,stdin);
	}
	else Log(STDUSERLOG,"No errors or warnings in build\r\n\r\n");

	if (zone == BUILD0) Build0LockDictionary();
	else LockDictionary();

	return true;
}

void SetJumpOffsets(char* data)
{
   //   store jump offset
    char* end = data;
    char* at = data;
	unsigned int units = 0;
	bool show = false;
    while (*at && *++at) //   find each responder end
    {
        if (*at == ENDUNIT) 
        {
			++units;
            int diff = at+1-end; //   jump from last table start to this table start
            
			if (show) //   for retry debugging
			{
				char sample[MAX_WORD_SIZE];
				strncpy(end,sample,40);
				sample[40] = 0;
				if (diff < 40) sample[diff] = 0;
				printf("%s\r\n",sample);
			}
			if (diff > MAX_JUMP_OFFSET) 
				BADSCRIPT("Jump offset too far - %d but limit %d near %s",diff,MAX_JUMP_OFFSET,readBuffer); //   limit 2 char (12 bit) 
            Encode(diff,end);
            end = at+1;  //   next starter (pair code after marker)
        }
    }
 }


char* CompileString(char* ptr)
{ // incoming will start with "
	static char data[MAX_BUFFER_SIZE];		//   topic script data found
	char* pack = data;
	*pack++ = '"';
	*pack++ = '`';
	unsigned int len = strlen(ptr);
	if (ptr[len-1] == '"') ptr[len-1] = 0;	// remove trailing quote
	else BADSCRIPT("string not terminated with doublequote %s",ptr);
	ReadOutput(ptr+2,NULL,pack,NULL);
	*pack++ = '"';
	*pack = 0;
	return data;
}
#endif
