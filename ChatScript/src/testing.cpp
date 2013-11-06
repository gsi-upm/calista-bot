 // testing.cpp - used to test the engine

#include "common.h"
#ifndef NOTESTING
bool wasCommand;

typedef void (*FILEWALK)(char* name, unsigned int flag);

static void WalkDirectory(char* directory,FILEWALK function, unsigned int flags) 
{
#ifndef XMPP
#ifdef WIN32 // do all files in src directory
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError;
	LPSTR DirSpec;
	DirSpec = (LPSTR) malloc (MAX_PATH);
   
	  // Prepare string for use with FindFile functions.  First, 
	  // copy the string to a buffer, then append '\*' to the 
	  // directory name.
	strcpy(DirSpec,directory);
	strcat(DirSpec,"/*");
	char name[MAX_WORD_SIZE];
	// Find the first file in the directory.
	hFind = FindFirstFile(DirSpec, &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE) 
	{
		printf ("Invalid file handle. Error is %u.\n", GetLastError());
		return;
	} 
	else 
	{
		if (FindFileData.cFileName[0] != '.' && stricmp(FindFileData.cFileName,"bugs.txt"))
		{
			sprintf(name,"%s/%s",directory,FindFileData.cFileName);
			(*function)(name,flags);
		}
		while (FindNextFile(hFind, &FindFileData) != 0) 
		{
			if (FindFileData.cFileName[0] == '.' || !stricmp(FindFileData.cFileName,"bugs.txt")) continue;
			sprintf(name,"%s/%s",directory,FindFileData.cFileName);
			(*function)(name,flags);
		}
		dwError = GetLastError();
		FindClose(hFind);
		if (dwError != ERROR_NO_MORE_FILES) 
		{
			printf ("FindNextFile error. Error is %u.\n", dwError);
			return;
		}
	}
	free(DirSpec);
#endif
#endif
}

static void RegressSubstitutes1(WORDP D, void* unused)
{
	char expectedText[MAX_WORD_SIZE];
	char resultText[MAX_WORD_SIZE];
	if (!(D->systemFlags & SUBSTITUTE)) return;
	*readBuffer = 0;
	unsigned int n;

	//   see if word has start or end markers. Remove them.
	bool start = false;
	if (D->word[0] == '<')
	{
		start = true;
		n = BurstWord(D->word+1);
	}
	else n = BurstWord(D->word);
	bool end = false;
	char* last = GetBurstWord(n);
	unsigned int len = strlen(last);
	if (last[len-1] == '>')
	{
		end = true;
		last[len-1] = 0;
	}

	//   now composite an example, taking into account start and end markers
	unsigned int i;
	if (!start) strcat(readBuffer,"x ");	//   so match is not at start
	for (i = 1; i <= n; ++i)
	{
		strcat(readBuffer,GetBurstWord(i));
		strcat(readBuffer," ");
	}
	if (!end) strcat(readBuffer,"x "); //   so match is not at end

	//   generate what it results in
	PrepareSentence(readBuffer,true,true);

	*resultText = 0;
	if (!end) --wordCount;	//   remove the trailing x
	for (i = 1; i <= wordCount; ++i) //   recompose what tokenize got
	{
		if (!start && i == 1) continue;	//   remove the leading x
		strcat(resultText,wordStarts[i]);
		strcat(resultText," ");
	}

	WORDP S = D->substitutes;
	if (!S && wordCount == 0) return;	//   erased just fine
	if (!S)
	{
		Log(1,"Substitute failed: %s didn't erase itself, got %s\r\n",D->word,resultText);
		return;
	}
	strcpy(expectedText,S->word);
	strcat(expectedText," ");	//   add the trailing blank we get from concats above
	char* at;
	while ((at = strchr(expectedText,'+'))) *at = ' '; //   break up answer
	if (!stricmp(resultText,expectedText)) return;	//   got what was expected
	Log(1,"Substitute failed: %s got %s not %s\r\n",D->word,resultText,expectedText);
}

void RegressSubstitutes() //   see if substitutes work...
{
	WalkDictionary(RegressSubstitutes1);
}

static void DumpCommands()
{
	CommandInfo *fn;
	int i = 0;
	while ((fn = &commandSet[++i]) && fn->word )
	{
		if (fn->comment && *fn->comment) Log(STDUSERLOG,"%s - %s\r\n",fn->word,fn->comment);
	}
}

void C_Help(char* x)
{
	if (!*x || !stricmp(x,"command")) DumpCommands();
	else if ( !stricmp(x,"variable")) DumpSystemVariables();
	else if ( !stricmp(x,"function")) DumpFunctions();
}

void InitCommandSystem() //   set dictionary to match builtin functions
{
	unsigned int k = 0;
	CommandInfo *fn;
	while ((fn = &commandSet[++k]) && fn->word )
	{
		WORDP D = StoreWord((char*) fn->word);
		D->x.codeIndex = k;
	}
}

static void CleanIt(char* word,unsigned int junk)
{
	FILE* in = fopen(word,"rb");
	if (!in) 
	{
		printf("missing %s\r\n",word);
		return;
	}
	fseek (in, 0, SEEK_END);
    unsigned long size = ftell(in);
	char* buf = (char*) malloc(size+2);
	fseek (in, 0, SEEK_SET);
	int val = fread(buf,1,size,in);
	fclose(in);
	if ( val != size)
	{
		printf("bad sizing?");
		return;
	}
	buf[size] = 0;	// force an end

	FILE* out = fopen(word,"wb");
	for (unsigned int i = 0; i < size; ++i)
	{
		if (buf[i] == '\r' || buf[i] == 26) continue;	// ignore cr and EOF
		fwrite(buf+i,1,1,out);
	}
	if (buf[size-1] != '\n') fwrite("\n",1,1,out); // force ending line
	fclose(out);
	free(buf);
}

static void C_Clean(char* word) // remove CR for LINUX
{
	WalkDirectory("src",CleanIt,0);
}


static void DumpX(WORDP D,void* unused) // TTG
{
	char word[MAX_WORD_SIZE];
	strcpy(word,D->word);
	if (strchr(word,'_')) return; // no composite words
	int len = strlen(word);
	if (word[len-1] == '>') return;	// no ending specials
	if (!IsAlpha(word[0]) ) // no numbers or wordnet id's or things that are numerically part words
	{
		if (*word == '<') strcpy(word,word+1); // allow interjection to pass
		else return;
	}
	else if (D->properties & PART_OF_SPEECH);
	else if (D->systemFlags & SUBSTITUTE) // only swallow substitiutes for ' values and emotions and multiwords. Definitely not explicit typos remapping to valid
	{
		if (strchr(word,'\'')); // has apostrophe
		else if (D->substitutes && D->substitutes->word[0] == '~'); // emotion
		else if (D->substitutes && strchr(D->substitutes->word,'+')); // single goes multi, like wanna to want+to
		else return;
	}
	else return;
//	if (D->word[0] < 'a' || D->word[0] > 'z') return; // no proper nouns
	Log(1,"%s\r\n",word);
}

static void VerifySubstitutes1(WORDP D, void* unused)
{
	char expectedText[MAX_WORD_SIZE];
	char resultText[MAX_WORD_SIZE];
	if (!(D->systemFlags & SUBSTITUTE)) return;
	*readBuffer = 0;
	unsigned int n;

	//   see if word has start or end markers. Remove them.
	bool start = false;
	if (D->word[0] == '<')
	{
		start = true;
		n = BurstWord(D->word+1);
	}
	else n = BurstWord(D->word);
	bool end = false;
	char* last = GetBurstWord(n);
	unsigned int len = strlen(last);
	if (last[len-1] == '>')
	{
		end = true;
		last[len-1] = 0;
	}

	//   now composite an example, taking into account start and end markers
	unsigned int i;
	if (!start) strcat(readBuffer,"x ");	//   so match is not at start
	for (i = 1; i <= n; ++i)
	{
		strcat(readBuffer,GetBurstWord(i));
		strcat(readBuffer," ");
	}
	if (!end) strcat(readBuffer,"x "); //   so match is not at end

	//   generate what it results in
	PrepareSentence(readBuffer,true,true);

	*resultText = 0;
	if (!end) --wordCount;	//   remove the trailing x
	for (i = 1; i <= wordCount; ++i) //   recompose what tokenize got
	{
		if (!start && i == 1) continue;	//   remove the leading x
		strcat(resultText,wordStarts[i]);
		strcat(resultText," ");
	}

	WORDP S = D->substitutes;
	if (!S && wordCount == 0) return;	//   erased just fine
	if (!S)
	{
		Log(STDUSERLOG,"Substitute failed: %s didn't erase itself, got %s\r\n",D->word,resultText);
		return;
	}
	strcpy(expectedText,S->word);
	strcat(expectedText," ");	//   add the trailing blank we get from concats above
	char* at;
	while ((at = strchr(expectedText,'+'))) *at = ' '; //   break up answer
	if (!stricmp(resultText,expectedText)) return;	//   got what was expected
	Log(STDUSERLOG,"Substitute failed: %s got %s not %s\r\n",D->word,resultText,expectedText);
}

static void C_Bot(char* ptr)
{
	strcpy(computerID,ptr);
	computerIDwSpace[0] = ' ';
	MakeLowerCopy(computerIDwSpace+1,computerID);
	strcat(computerIDwSpace," ");
	ReadUserData();		//   now bring in user state
	char buffer[MAX_WORD_SIZE];
	StartConversation(buffer);
	wasCommand = false;	// lie so system will save revised user file
}

static void C_VerifySub(char* ptr) //   see if substitutes work...
{
	//	WalkDictionary(DumpX);

	WalkDictionary(VerifySubstitutes1);
}

static void DoAssigns(char* ptr) // format:  $var=value
{
	while (1)
	{
		char* at = strchr(ptr,'$'); 
		if (at && IsDigit(at[1])) at = 0;	//   cant handle var and money in same context
		char* sysat = strchr(ptr,'%'); 
		if (sysat && !IsAlpha(sysat[1])) sysat = NULL;
		char* wildat;
		char* spot = ptr;
		while ( (wildat = strchr(spot,'_')))  // may not be real yet, might be like New_year's_eve 
		{
			if (IsDigit(wildat[1])) break;
			spot = wildat + 1;
		}

		if (!at && !sysat && !wildat) break;

		if (at && sysat)
		{
			if (at < sysat) sysat = 0;
			else at = 0;
		}
		if (at && wildat)
		{
			if (at < wildat) wildat = 0;
			else at = 0;
		}
		if (wildat && sysat)
		{
			if (wildat < sysat) sysat = 0;
			else wildat = 0;
		}

		//   user variable
		if (at)
		{
			char word[MAX_WORD_SIZE];
			char* eq = strchr(at,'=');
			ptr = ReadSystemToken(SkipWhitespace(eq+1),word); //   get the value
			*eq = 0; //   make variable isolated
			while (IsWhiteSpace(*--eq)) *eq = 0;	//   remove trailing blanks from variable name
			SetUserVariable(at,word); //   rest is the single value
			*at = 0;		//   hide the assignment from the actual test
		}

		//   system var assign
		if (sysat && IsAlpha(sysat[1]))
		{
			char word[MAX_WORD_SIZE];
			char* eq = strchr(sysat,'=');
			ptr = ReadSystemToken(SkipWhitespace(eq+1),word); //   get the value
			*eq = 0; //   make systemvar isolated
			while (IsWhiteSpace(*--eq)) *eq = 0;	//   remove trailing blanks from sysvar name
			OverrideSystemVariable(sysat,word);
			*sysat = 0;		//   hide the assignment from the actual test
		}

		//   _ var assign
		if (wildat)
		{
			char word[MAX_WORD_SIZE];
			char* eq = strchr(wildat,'=');
			ptr = ReadSystemToken(SkipWhitespace(eq+1),word); //   get the value
			*eq = 0; //   make systemvar isolated
			while (IsWhiteSpace(*--eq)) *eq = 0;	//   remove trailing blanks from sysvar name
			SetWildCard(word,word,wildat+1,0);
			*wildat = 0;		//   hide the assignment from the actual test
		}
	}
}

static bool RuleTest(char* ptr)
{
	unsigned int gap = 0;
	globalDepth = 0;
	unsigned int wildcardSelector = 0;
	wildcardIndex = 0;
	unsigned int junk1;
	return  Match(ptr,0,0,'(',true,gap,wildcardSelector,junk1,junk1);
}

char* FindPatternSection(char* rule)
{
	char* base = rule+3; // at jump field if it exists, or at pattern (
 	char c = *base;
	if (c== '(');
	else if (c == ' ') base = 0; // no pattern
	else //   there is a label here - &aLABEL (
	{
		int len = c - '0';	//  size of label word + jump field + space
		base += len;		//   past jumpoffset + label and space-- points to (
	}
	return base;
}

static bool HasKeywords(char* topic)
{
	WORDP D = FindWord(topic);
	if (!D) return false;
	FACT* F = GetObjectHead(D);
	while (F)
	{
		if (F->properties & FACT_MEMBER) return true;
		F = GetObjectNext(F);
	}
	return false;
}

void VerifyAccess(char* topic,char kind) //   prove patterns match comment example, kind is o for outside, r for rule, t for topic
{
	WORDP topicWord = FindWord(topic); // must find it
	bool testOutside = kind == 'o';
	bool testPattern = kind == 'p' || kind == 'r';
	bool testTopic = kind == 't';
 	unsigned int topicIndex = FindTopicIDByName(topic);
	if (!topicIndex) 
	{
		printf("%s not found\r\n",topic);
		return;
	}
	int flags = GetTopicFlags(topicIndex);
	if (flags & TOPIC_BLOCKED) return;
	if (testOutside) 	// has no keyword into here so dont test
	{
		FACT* F = GetObjectHead(topicWord);
		while (F)
		{
			if (F->properties & FACT_MEMBER) break; // has a keyword member
		}
		if (!F) return;
	}
	char name[100];
	static unsigned int n = 0;
	static unsigned int err = 0;
	if (*topic == '~') ++topic;
	if (duplicateCount) sprintf(name,"VERIFY/%s.%d.txt",topic,duplicateCount);
	else sprintf(name,"VERIFY/%s.txt",topic);
	FILE* in = fopen(name,"rb");
	if (!in) 
	{
		printf("%s verification data not found\r\n",name);
		return;
	}
	unsigned int oldtrace = trace;
	trace = 0;
	Log(STDUSERLOG,"VERIFYING %s ......\r\n",topic-1);
	char copyBuffer[MAX_BUFFER_SIZE];
	AddInterestingTopic(topicIndex);
	while (ReadALine(readBuffer,in))
	{
		if (!*readBuffer) break;
		if (!strnicmp(readBuffer,":trace",10))
		{
			trace = atoi(readBuffer+11);
			continue;
		}
		if (!strnicmp(readBuffer,":exit",5)) myexit(0);

		int offset = atoi(readBuffer);
		char* test = strchr(readBuffer,'!');	//   the input sentence (skipping offset and #! marker
		char junk[MAX_WORD_SIZE];
		test = ReadCompiledWord(test,junk);
		strcpy(copyBuffer,test);

		// the comment headers are:
		// #!x  - description header for :abstract
		// #!!R  - expect to fail RULE
		// #!!T - expect to fail TOPIC (be masked by earlier rule)
		if (junk[1] == 'x') continue; 
		bool wantfailRule = false;
		bool wantfailTopic = false;
		if (junk[1] == '!' && (junk[2] == 'r' || junk[2] == 'R')) wantfailRule = true;
		if (junk[1] == '!' && (junk[2] == 't' || junk[2] == 'T')) wantfailTopic = true;

		//   test pattern
		char* ruleBase = GetTopicData(topicIndex) + offset;
		char type = *ruleBase;	//   the code kind
		if (type == 't' || type == 'r' || type == 's' || type == '?' || type == 'u');
		else if (testTopic) continue;	//   a rejoinder not matchable from outside the topic
	
		//   var assign?
		DefineSystemVariables(); // clear system variables to default
		DoAssigns(test);
		PrepareSentence(test,true,true);	
		topicID = topicIndex;
		strcpy(test,copyBuffer); // sentence prep may have altered test data and we might want to redo it

		char* base = FindPatternSection(ruleBase); // ( of pattern returned
		lastwordIndex = wordCount; //   current context
		if (!base) 
		{
			ReportBug("No pattern here? %s %s\r\n",topic,ruleBase);
			continue;
		}

		bool result;

		if (testOutside)
		{
			char* ptr = base;
			if ( type == 's' || type == '?' || type == 'u') // can it be found from outside?
			{
				unsigned int pStart = 0;
				unsigned int pEnd = 0;
				if (!GetNextSpot(topicWord,0,pStart,pEnd)) // not findable topic
				{
					Log(STDUSERTABLOG,"  %s => %s\r\n",test,GetText(base,0,60,junk,false));
				}
			}
		}

		//   inside the pattern, test this rule
		if (testPattern)
		{
			result = RuleTest(base+2);// past the paren
			if ((!result && !wantfailRule) || (result && wantfailRule) ) //   didnt do what we expected
			{
				if (!testOutside)
				{
					char junk[MAX_WORD_SIZE];
					Log(STDUSERTABLOG,"VerifyFail %d ~%s: %s => %s\r\n",++err,topic,test,GetText(base,0,60,junk,false));

					// redo with tracing on if selected
					if (oldtrace)
					{
						trace = oldtrace;
						PrepareSentence(test,true,true);	
						strcpy(test,copyBuffer); // sentence prep may have altered test data and we might want to redo it
						bool match = RuleTest(base+2);
						trace = 0;
					}
				}
			}
			else if ( type == 's' || type == '?' || type == 'u') // can it be found from outside?
			{
				unsigned int pStart = 0;
				unsigned int pEnd = 0;
				if (!GetNextSpot(topicWord,0,pStart,pEnd) && HasKeywords(topicWord->word)) // not findable
				{
					Log(STDUSERTABLOG,"         Not findable outside of topic:  %s => %s\r\n",test,GetText(base,0,60,junk,false));
				}
			}
		}
		else if (testTopic)
		{
			if (*ruleBase >= 'a' && *ruleBase <= 'q') continue;	// dont do rejoinder matching
			if (wantfailTopic || wantfailRule) continue;	// not expected to match because earlier will block or it fails on its own
			char* data = GetTopicData(topicIndex);
			// test all rules BEFORE this one
			while (data && data < ruleBase)
			{
				char type = *data;	//   the code kind
				if (type != 't' && type != 'r')
				{
					char* newbase = FindPatternSection(data); // ( of pattern returned
					unsigned int result = RuleTest(newbase+2);// past the paren
					if (result)	break; // he matched, blocking us
				}
				data = FindNextResponder(NEXTTOPLEVEL,data);
			}
			if (data && data < ruleBase) // blocked by earlier
			{
				char junk[MAX_WORD_SIZE];
				char junkearlier[MAX_WORD_SIZE];
				GetText(data,0,60,junkearlier,false);
				Log(STDUSERTABLOG,"VerifyFail %d ~%s: %s =>\r\n   %s \r\n   blocks %s\r\n",++err,topic,test,junkearlier,GetText(ruleBase,0,60,junk,false));
			}
		}
	}
	fclose(in);
	RemoveInterestingTopic(topicIndex);
	trace = oldtrace;
}

void VerifyAllTopics(char kind)
{
	for (unsigned int i = 1; i <= topicNumber; ++i)
	{
		VerifyAccess(GetTopicName(i),kind);
	}
}

void DisplayTables()
{
	WORDP D = FindWord("^abstract_facts");
	if (!D) return;
	if (!(D->systemFlags & FUNCTION_NAME)) return;
	char* old = currentOutputBase;
	unsigned int oldtrace = trace;
	trace = 0;
	currentOutputBase = AllocateBuffer();
	unsigned int result;
	DoFunction(D->word,"",currentOutputBase,result);
	FreeBuffer();
	currentOutputBase = old;
	trace = oldtrace;
}


static int GetNextOffset(FILE* in)
{ //   implicit return of readBuffer as well
	int offset = 0x7fffffff;
	while (in) //   find first test line
	{
		*readBuffer = 0;
		ReadALine(readBuffer,in);
		if (!*readBuffer) break;
		if (!strnicmp(readBuffer,":trace",10)) continue;
		if (!strnicmp(readBuffer,":exit",5)) continue;
		offset = atoi(readBuffer);
		break;
	}
	return offset;
}

static int ShowHeader(FILE* in,int offset,int at,char* set)
{
	while (at > offset) //   cach up, scan is beyond display ability
	{
		offset = GetNextOffset(in);
	}
	if (at < offset) return offset; //   not there yet

	char* test = strchr(readBuffer,'!');	//   the input sentence (skipping offset)
	char junk[MAX_WORD_SIZE];
	test = SkipWhitespace(test);
	test = ReadCompiledWord(test,junk); //   skipping marker
	while (*test == ' ') ++test;	//   skip leading white space
	char* tail = test+strlen(test) - 1;
	while (*tail == ' ') --tail; 
	if (*++tail == ' ')  //   clear trailing blanks
	{
		*tail = 0;
		Log(STDUSERLOG,"\"%s\" =>   ",test);
		*tail = ' ';
	}
	else Log(STDUSERLOG,"\"%s\" =>   ",test);

	offset = GetNextOffset(in);
	if (offset == at) offset = ShowHeader(in,offset,at,set);
	return offset;
}

static void PurifyPattern(char* pattern)  // only keep simple tests 
{
	char token[MAX_WORD_SIZE];
	char* original = pattern;
	pattern += 2;	// 1st token
	if ( *pattern == '$') // keep a simple variable
	{
		pattern = ReadCompiledWord(pattern,token);
		strcpy(pattern,")");	// close this simple
	}
	else if (*pattern == '=' && pattern[2] == '$') // variable compare
	{
		memcpy(pattern,pattern+2,strlen(pattern)-1); // skip jump
		pattern = ReadCompiledWord(pattern,token);
		strcpy(pattern,")");	// close this simple
	}
	else *original = 0; // erase
}

void DisplayTopic(char* name)
{
	char* ptr;
	int id = FindTopicIDByName(name);
	char word[MAX_WORD_SIZE];
	if (!id)
	{
		printf("%s not found\r\n",name);
		return;
	}
	if (topicRestrictionMap[id] && !strstr(topicRestrictionMap[id],computerIDwSpace)) return;
	if (GetTopicFlags(id) & TOPIC_BLOCKED) return;

	Log(STDUSERLOG,"\r\n****** Topic: %s\r\n",name);
	char fname[MAX_WORD_SIZE];
	sprintf(fname,"VERIFY/%s.txt",name+1);
	FILE* in = fopen(fname,"rb");
	int offset = GetNextOffset(in); //   readBuffer set with this display
	ptr = ReadCompiledWord(readBuffer,word);
	ptr = ReadCompiledWord(ptr,word);
	if (word[2] == 'x') //   #!x comment - describes topic as a whole
	{
		Log(STDUSERLOG,"\r\n%s\r\n",ptr);
		offset = GetNextOffset(in);
	}
	Log(STDUSERLOG,"\r\n");
	bool deadend;
	bool preprint;
	char* data = GetTopicData(id); //   inset +3
	char* base = data;
	char* hold = name;
	char* set = base;
	bool transition = false;
	char pattern[MAX_WORD_SIZE];
	*pattern = 0;
	while (data && *data)
	{
		deadend = false;
		preprint = false;
		*pattern = 0;

		char* ptr = data+3;  //   now pointing at first useful data
 		char c = *ptr;
		char* pbase = ptr;
		char* patternstart = ptr;
		if (c== '(') 
		{
			ptr = BalanceParen(ptr+1); //   go past pattern to new token
		}
		else if (c == ' ') 
		{
			patternstart = NULL;
			++ptr; //   no pattern
		}
		else //   there is a label here - &aLABEL (
		{
			int len = c - '0'; //   size of label word + jump field + space
			ptr += len;			//   past jumpoffset + label and space-- points to (
			patternstart  = ptr;
			ptr = BalanceParen(ptr+1); // points to new token
		}
		int patternlen = ( patternstart) ? (ptr-patternstart) : 0;
		if (*data == 't' || *data == 'r') //   gambits
		{
			if (patternlen > 4)
			{
				strncpy(pattern,patternstart,patternlen);
				pattern[patternlen] = 0;
			}
		}
		else //   swallow pattern of responders
		{
			if (*data == '?' || *data == 's' || *data == 'u') //   top level responders
			{
				if (!transition) Log(STDUSERLOG,"\r\n\r\n"); //   gap after main story
				transition = true;
			}
			else // rejoinder
			{
				int i = 0;
			}
			if (patternlen > 4)
			{
				strncpy(pattern,patternstart,patternlen);
				pattern[patternlen] = 0;
				PurifyPattern(pattern);
			}
		}

		//   DISPLAY all data words in this
		char basic[MAX_WORD_SIZE];
		basic[0] = *data;
		basic[1] = ':';
		basic[2] = ' ';
		basic[3] = 0;
		if (*pattern) 
		{
			char* compare = pattern;
			while ((compare = strstr(compare," =")))
			{
				if (compare[2] != ' ')
				{
					compare[1] = ' '; // erase =
					compare[2] = ' '; // erase accel
				}
				compare += 3;
			}
			strcat(basic,pattern);
		}
		bool unsafe = false;
		
		int count = 0;
		if (*data >= 'a' && *data <= 'p') 
			count = (*data - 'a' + 1) * 2;
		bool closed = true;
		int inbracket = 0;
		bool dumped = false;
		bool firstbracket = true;
		char output[MAX_WORD_SIZE];
		*output = 0;
		char* outputPtr = output;
		while (*ptr && *ptr != '`')
		{
			ptr = ReadCompiledWord(ptr,word);
			if (*word == '$' && !IsDigit(word[1])) 
			{
				if (*ptr == '=' || ptr[1] == '=')	//   $name = 5  or some such assignment
				{
					ptr = ReadCompiledWord(ptr,word);
					ptr = ReadCompiledWord(ptr,word);
					if (*ptr == '+' || *ptr == '-' || *ptr == '*' || *ptr == '/' || *ptr == '%') //   secondary operator: $name = 5 + 3
					{
						ptr = ReadCompiledWord(ptr,word);
						ptr = ReadCompiledWord(ptr,word);
					}
					continue;
				}
			}
			if (*word == '`' || !*word || *word == '+') break;	//   next thing   skip assignment statemtn
			if (*word == '[') 
			{
				++inbracket;
				if (*output) 
				{
					Log(STDUSERLOG,"%s\r\n",output);
					*output = 0;
					outputPtr = output;
					closed = true;
				}
				else
				{
					closed = true;
				}
				if (closed) //   start new line
				{
					closed = false;
					for (int j = 0; j < count; ++j) Log(STDUSERLOG,"  ");
					Log(STDUSERLOG,"%s",basic);
					strcpy(basic,"   ");
					int oldoffset = offset;
					offset = ShowHeader(in,offset,set-base,set);
					if (offset != oldoffset) unsafe = true; //   added example
				}
				if (!*outputPtr && unsafe && firstbracket) //   indent 1st thing when an inset
				{
					Log(STDUSERLOG,"\r\n");
					for (int j = 0; j < count; ++j) Log(STDUSERLOG,"  ");
					Log(STDUSERLOG,"%s",basic);
					strcpy(basic,"   ");
				}
				unsafe = false;
				firstbracket = false;
				sprintf(outputPtr,"%s ",word);
				outputPtr += strlen(outputPtr);
				char word1[MAX_WORD_SIZE];
				ptr = ReadCompiledWord(ptr,word1);
				if (*word1 == ']') ptr -= 2; //   empty [], let normal close happen
				else 
				{
					sprintf(outputPtr,"%s ",word1);
					outputPtr += strlen(outputPtr);
				}
				continue;
			}
			if (*word == ']') 
			{
				--inbracket;
				sprintf(outputPtr,"%s\r\n",word);
				Log(STDUSERLOG,"%s",output);
				*output = 0;
				outputPtr = output;
				closed = true;
				continue;
			}
			if (*word == '_') //   is it assignment statement
			{
				char junk[MAX_WORD_SIZE];
				char* at = ReadCompiledWord(ptr,junk);
				if (*junk == '=' || junk[1] == '=') //   assignment
				{
					ptr = ReadCompiledWord(at,junk);
					continue;
				}
			}

			if (*word == ')' && preprint) //   closing ) for preprint
			{
				preprint = false;
				continue;
			}

			//   remove trailing punctuation
			if (*word == '^') //   swallow function call or function argumnet
			{
				if (!stricmp(word,"^preprint")) //   show preprint content
				{
					ptr = ReadCompiledWord(ptr,word);
					preprint = true;
					continue;
				}
				if (!stricmp(word,"^gambit") || !stricmp(word,"^reuse")|| !stricmp(word,"^respond")|| !stricmp(word,"^topic")) deadend = true;
				if (*ptr == '(') ptr = BalanceParen(ptr+1);
			}
			else if (word[1] == ':'); //   c: or its ilk INSIDE a [] 
			else if (*word == '$' && !IsDigit(word[1]));
			else if ( word[0] == '=' ) // assignment
			{
				char* old = outputPtr;
				while (*old && *--old && *old != ' ');
				if (*old == ' ') // erase left hand of assignment
				{
					outputPtr = old + 1;
					if (*outputPtr == '$' || *outputPtr == '_')	*outputPtr = 0;
				}
				if (*ptr != '^') ptr = ReadCompiledWord(ptr,word);	// swallow next arguement when not a function call
			}
			else if (word[0] == '%'  || word[1] == '='   || word[0] == '~'); //   var
			else if (word[0] == '\\')
			{
				if (word[1] == '"')
				{
					if (closed) //   start new line
					{
						closed = false;
						for (int j = 0; j < count; ++j) Log(STDUSERLOG,"  ");
						Log(STDUSERLOG,basic);
						strcpy(basic,"   ");
						offset = ShowHeader(in,offset,set-base,set);
					}
					sprintf(outputPtr,"%s ",word+1);
					outputPtr += strlen(outputPtr);
				}
			}
			else 
			{
				if (closed) //   start new line
				{
					closed = false;
					for (int j = 0; j < count; ++j) Log(STDUSERLOG,"  ");
					Log(STDUSERLOG,"%s",basic);
					strcpy(basic,"   ");
					offset = ShowHeader(in,offset,set-base,set);
				}
				sprintf(outputPtr,"%s ",word);
				outputPtr += strlen(outputPtr);
			}
		}
		if (*output) Log(STDUSERLOG,"%s\r\n",output);
		*output = 0;
		outputPtr = output;
	
		if (closed && (set-base) == offset && !deadend) //   we are at marker, it had no output but had an input
		{
			for (int j = 0; j < count; ++j) Log(STDUSERLOG,"  ");
			Log(STDUSERLOG,basic);
			strcpy(basic,"   ");
			offset = ShowHeader(in,offset,set-base,set);
			Log(STDUSERLOG,"...\r\n");
		}
		while ((set-base) >= offset) offset = GetNextOffset(in); //   swallow unused stuff

		closed = true;
		if (data) set = data = FindNextResponder(NEXTRESPONDER,data);
	}
	if (in) fclose(in);
}

unsigned int basestamp = 0;
static int itemcount = 0;
static bool DumpOne(WORDP S,int all,int depth);

int CountSet(WORDP D) //   full recursive referencing
{
	if (!D) return 0;

	int count = 0;
	FACT* F = GetObjectHead(D);
	FACT* G;
	while (F) //   do all atomic members of it
	{
		G = F;
		F = GetObjectNext(F);
		WORDP S = Meaning2Word(G->subject);
		int index = Meaning2Index(G->subject);
		if (!(G->properties & FACT_MEMBER)  || G->properties & DEADFACT) continue;
		if (S->word[0] == '~' ) continue;
		else if (S->properties & WORDNET_ID) //   word~2 reference become a synset header
			count += CountDown(G->subject,-1,-1); //   follow IS_A submembers
		else //   simple atomic member -- or POS specificiation
		{
			if (S->stamp.inferMark <= basestamp) //   count once
			{
				S->stamp.inferMark = inferMark;
				++count;
			}
		}
	}
	F = GetObjectHead(D);
	while (F) //   do all set members of it
	{
		G = F;
		F = GetObjectNext(F);
		WORDP S = Meaning2Word(G->subject);
		if (!(G->properties & FACT_MEMBER)  || G->properties & DEADFACT) continue;
		if (S->word[0] == '~')  count += CountSet(S);
	}
	return count;
}

int CountDown(MEANING T,int all,int depth)
{ //   a synset header
	if (all == 5) return 0;
	int count = 0;

	//   show each word in synset
    WORDP D = Meaning2Word(T);
	D->stamp.inferMark = inferMark;
	for (unsigned int i = 1; i <= D->meaningCount; ++i) //   for every word in the synset
    {
		WORDP S = Meaning2Word(D->meanings[i]);
		if (S->stamp.inferMark != inferMark) 
		{
			if (S->stamp.inferMark <= basestamp) ++count;
			S->stamp.inferMark = inferMark;	
			if (depth != -1) DumpOne(S,all,depth); //   display it
		}
    }

	//   down go down the synsets from this one
	FACT* F = GetObjectHead(T); 
	while (F)
	{
		WORDP S = Meaning2Word(F->subject); //   dinosaur~1 is 01699831n
		if (S->properties & WORDNET_ID && F->properties & FACT_IS)  
			count += CountDown(F->subject,all,depth);
		F = GetObjectNext(F);
	}
	return count;
}

static int CountDirectSet(WORDP D) //   who is direclty referenced -- not via a set
{
	bool did = false;
	NextinferMark();
	FILE* out = NULL;
	if (!D) return 0;
	int count = 0;
	FACT* F = GetObjectHead(D);
	FACT* G;
	while (F) //   do all atomic members of it
	{
		G = F;
		F = GetObjectNext(F);
		if (!(G->properties & FACT_MEMBER)  || G->properties & DEADFACT) continue;
		WORDP S = Meaning2Word(G->subject);
		int index = Meaning2Index(G->subject);
		if (S->word[0] == '~' ) continue; //   ignore set and class members
		else if (S->stamp.inferMark == inferMark) continue;
		else if (S->properties & WORDNET_ID) //   a reference to a verb~1 set - as synset
		{
			count += CountDown(G->subject,-1,-1); //   follow IS_A submembers
		}
		else { // word or word with pos restriction
			++count;
			S->stamp.inferMark = inferMark;
			if (!did)
			{
				out = fopen("sets.txt","ab");
				fprintf(out,"%s ",D->word);
				did = true;
			}
			fprintf(out,"%s ",S->word);
		}
	}
	if (did) 
	{
		fprintf(out,"\r\n");
		fclose(out);
	}
	return count;
}

static char* GetComment(WORDP D)
{
	WORDP C = FindWord("comment");
	MEANING T = MakeMeaning(C,0);
	FACT* F = GetObjectHead(D);
	while (F)
	{
		if (F->verb == T) return Meaning2Word(F->subject)->word;
		F = GetObjectNext(F);
	}
	return "";
}

static void Indent(int count)
{
	Log(STDUSERLOG,"%d.",count);
	while (count--) Log(STDUSERLOG,"    ");
}

static bool DumpOne(WORDP S,int all,int depth)
{
	bool did = false;
	if (all) 
	{
			if ( all == 3) return false;
			if (itemcount == 0 && all != 2) Indent(depth);
			if (all == 1) SetWhereInSentence(S,GetWhereInSentence(S)+1);
			if (all == 1 && GetWhereInSentence(S) > 1) Log(STDUSERLOG,"+%s  ",S->word); //   multiple occurences
			else  //   first occurence of word
			{
				if (all == 1 && !(S->systemFlags & VERB_DIRECTOBJECT)) //   generate a list of intransitive verbs
				{
					FILE* out = fopen("intransitive.txt","ab");
					fprintf(out,"%s 1\r\n",S->word);
					fclose(out);
				}
				if (all == 1 && (S->systemFlags & VERB_INDIRECTOBJECT)) //   generate a list of dual transitive verbs
				{
					FILE* out = fopen("intransitive.txt","ab");
					fprintf(out,"%s 2\r\n",S->word);
					fclose(out);
				}
				Log(STDUSERLOG,"%s  ",S->word);
			}
			++itemcount;
			if (itemcount == 10 && all != 2)
			{
				Log(STDUSERLOG,"\r\n");
				itemcount = 0;
			}
			did = true;
	}
	return did;
}

static void ClearEntry(WORDP D,void* unused)
{
	D->v.patternStamp = 0;
	D->stamp.inferMark = 0;
}

static void MultipleEntry(WORDP D,void* unused)
{
	if (GetWhereInSentence(D) > 1 && IsAlpha(D->word[0]))
	{
		Log(STDUSERLOG,"%s %d\r\n",D->word,GetWhereInSentence(D));
	}
}

static void MissingVerbEntry(WORDP D, void* unused)
{
	static int count = 0;
	if (D->properties & NOUN) return;	//   ignore confusion for now
	if (!(D->properties & VERB) || D->properties & AUX_VERB) return;
	if (!(D->properties & VERB_INFINITIVE)) return;
	if (D->v.patternStamp || D->stamp.inferMark) return;
	if (strchr(D->word,'_')) return;	//   ignore composites
	if (strchr(D->word,'-')) return;	//   ignore composites
	Log(STDUSERLOG,"%d  %s\r\n",++count,D->word);
}

static bool VerbMember(WORDP D, char* set)
{
	MEANING S = MakeMeaning(FindWord(set));
	FACT* F = GetSubjectHead(D);
	while (F)
	{
		if (F->object == S) return true;
		F = GetSubjectNext(F);
	}
	return false;
}

static void FileAnswer(const char* file, char* data)
{
	if (!*file) file = "nouns.txt"; //default
	char name[MAX_WORD_SIZE];
	sprintf(name,"RAWDATA/TTNOUNS/%s",file);
	FILE* out;

	// if first open, put in header
	FILE* in = fopen(name,"rb");
	if (!in)
	{
		in = fopen("RAWDATA/NOUNS/header.txt","rb");
		out = fopen(name,"wb");
		while (ReadALine(readBuffer,in)) fprintf(out,"%s\r\n",readBuffer);
		fclose(out);
		fclose(in);
	}
	else fclose(in);

	out = fopen(name,"ab");
	fprintf(out,"%s",data);
	fclose(out);
}

static void DumpNounPrimary(WORDP D, unsigned int depth,char* file)
{
	char* originalFile = file;
	if (!D->meaningCount) return;
	char output[MAX_WORD_SIZE];
	// D is a synset
	unsigned int n = 0;
	WORDP E = Meaning2Word(D->meanings[1]); // sample word of meaning (kindergarten or longest)
	unsigned int index = Meaning2Index(D->meanings[1]);
	if (IsUpperCase(E->word[0])) return;	// no proper nouns
	char* underscore = strchr(E->word,'_');
	if ( underscore && IsUpperCase(underscore[1])) return;	// genus_XXX

	// see if this is a terminal node
	bool hasChild = false;
	FACT* F = GetObjectHead(D);
	while (F)
	{
		if (F->properties & FACT_IS)
		{
			WORDP G = Meaning2Word(F->subject);
			if (G->properties & WORDNET_ID) 
			{
				hasChild = true;
				break;
			}
		}
		F = GetObjectNext(F);
	}

	if (hasChild)
	{
		//locate the file attribute if it has one
		F = GetSubjectHead(D);
		while (F)
		{
			WORDP V = Meaning2Word(F->verb);
			WORDP O = Meaning2Word(F->object);
			if (F->properties & (FACT_MEMBER|FACT_IS));
			else if (!stricmp(V->word,"file"))
			{
				file = O->word;
				break;
			}
			F = GetSubjectNext(F);
		}

		sprintf(output," %d ",depth);
		FileAnswer(file,output);
		Log(STDUSERLOG,"%s",output);
		while (n++ < depth) 
		{
			FileAnswer(file,"  ");
			Log(STDUSERLOG,"  ");
		}
		sprintf(output,"%s~%d ",E->word,index);
		Log(STDUSERLOG,"%s",output); // the primary meaning
		FileAnswer(file,output);

		// now show its properties
		F = GetSubjectHead(D);
		while (F)
		{
			WORDP V = Meaning2Word(F->verb);
			WORDP O = Meaning2Word(F->object);
			if (F->properties & (FACT_MEMBER|FACT_IS));
			else if (VerbMember(V,"~ttproperties"))
			{
				sprintf(output,"%s:%s ",V->word,O->word);
				Log(STDUSERLOG,"%s",output); 
				FileAnswer(file,output);
			}
			F = GetSubjectNext(F);
		}
		sprintf(output,"\r\n",E->word,index);
		Log(STDUSERLOG,"%s",output); // the primary meaning
		FileAnswer(file,output);
	}

	F = GetObjectHead(D);
	while (F)
	{
		if (F->properties & FACT_IS)
		{
			WORDP D = Meaning2Word(F->subject);
			if (D->properties & WORDNET_ID) DumpNounPrimary(D,depth+1,file);
		}
		F = GetObjectNext(F);
	}
	
	if ( file) // changing file boundary
	{
		if (!originalFile || stricmp(file,originalFile))
		{
			FileAnswer(file,"\r\n");
		}
	}
}

static void DumpBySystemFlag(WORDP D,void* data)
{
	if (D->systemFlags & (uint64)data) Log(STDUSERLOG,"%s\r\n",D->word);
}

static void C_DumpNouns(char* data)
{
	WORDP D = FindWord("entity");
	D = Meaning2Word(D->meanings[1]);
	DumpNounPrimary(D,0,"");
}

//   whereInSentence marks when we dump a word, allows us to mark duplicates
//   patternStamp marks that we have output a set (so we dont do it again even though hooked multiply) mode 2
void DumpSetHierarchy(WORDP D,unsigned int depth,int all) //   TTG 0=terse 1=full 2=tt file 3= CSV sets( no actual verbs )
{
	if (!D) return;

	if (depth == 0) //   kill the ~INANIMATE_VERBS path from verbs... we dont want them conflicting with animates
	{
		ClearWhereInSentence();
		FACT* F = NULL;//   GetSubjectHead(FindWord("~inanimate_verbs"));
		while (F)
		{
			F->properties |= DEADFACT; 
			F = GetSubjectNext(F);
		}
		
		WalkDictionary(ClearEntry,0);
	}

	//   dont dump out a set more than once
	if (D->v.patternStamp && all == 2) return;
	D->v.patternStamp = 1;

	//   all == 0 -> just the hierarchy
	//   all == 1 ->  the hierarchy + words at 10 per line
	//   all == 2 -> hierarchy + words 1 per line (TT layt)
	//   all == 3 -> CSV hierarchy but not the verbs
	if (all < 2) Indent(depth);
	FACT* G;
	FACT* F = GetObjectHead(D);
	++depth;
	int base = basestamp; 
	basestamp = inferMark;
	NextinferMark();
	int fc = CountSet(D); //   all members whose stamp mark becomes HIGHER than basestamp
	basestamp = inferMark;
	int direct = CountDirectSet(D); //   top level members only
	basestamp = base; //   restore OLD base
	char* comment = GetComment(D);
	char* p;
	while ((p = strchr(comment,'_'))) *p = ' ';
	char describe[MAX_WORD_SIZE];
	*describe = 0;
	if (comment && *comment) sprintf(describe,"\"%s\"",comment);
	if (all == 2) Log(STDUSERLOG,"%s ",D->word);
	else if ( all == 3) 
	{
		if (D->word[0] != '~') return;
		for (unsigned int i = 0; i < depth; ++i) Log(STDUSERLOG,",");
		Log(STDUSERLOG,"%s\r\n",D->word);
	}
	else if (! direct || fc == direct) Log(STDUSERLOG,"%s (%d) %s\r\n",D->word,fc,describe); //   all accounted for 
	else Log(STDUSERLOG,"%s (%d / %d) %s\r\n",D->word,fc,direct,describe);

	itemcount = 0;
	NextinferMark();
	F = GetObjectHead(D);
	while (F) //   do all atomic members of it (explicitly approved)
	{
		G = F;
		F = GetObjectNext(F);
		if (!(G->properties & FACT_MEMBER) || G->properties & DEADFACT ) continue;
		WORDP S = Meaning2Word(G->subject);
		if (S->word[0] == '~' ) continue;
		else if (S->stamp.inferMark == inferMark);
		else if (S->properties & WORDNET_ID) //   find all submeanings
		{
			CountDown(G->subject,all,depth);
		}
		else
		{
			S->stamp.inferMark = inferMark;
			DumpOne(S,all,depth);
		}
	}
	if (itemcount && all != 2) Log(STDUSERLOG,"\r\n");

	//   type=2  list all ~sets 
	F = GetObjectHead(D);
	if (all == 2) while (F) //   For type2 debuglist all ~sets (no :classes), non-recursive
	{
		G = F;
		F = GetObjectNext(F);
		if (!(G->properties & FACT_MEMBER)  || G->properties & DEADFACT) continue;
		WORDP S = Meaning2Word(G->subject);
		if (S->word[0] != '~') continue;
		Log(STDUSERLOG,"%s ",S->word);
	}
	if (all == 2) Log(STDUSERLOG,"\r\n");

	F = GetObjectHead(D);
	while (F) //   do all subsets
	{
		G = F;
		F = GetObjectNext(F);
		if (!(G->properties & FACT_MEMBER) || G->properties & DEADFACT) continue;
		WORDP S = Meaning2Word(G->subject);
		if (S->word[0] != '~') continue;
		DumpSetHierarchy(S,depth,all);
	}

	if (depth != 1) return;

	//   show multiple meaning words
	if (all == 1 && false)
	{
		Log(STDUSERLOG,"\r\nMultiply represented:\r\n");
		WalkDictionary(MultipleEntry,0);
	}
	if (all == 2 && false)
	{
		Log(STDUSERLOG,"\r\nMissing verbs:\r\n");
		WalkDictionary(MissingVerbEntry,0);
	}
}

static bool MarkedEntry(WORDP D,int index)
{
	uint64 offset = 1;
	if (index < 63) offset <<= index;
	if (D->stamp.inferMark == inferMark) 
	{
		if (D->tried & offset) return true;
	}
	else
	{
		D->stamp.inferMark = inferMark;
		D->tried = 0;
	}
 	D->tried |= offset;
	return false;
}

static MEANING FindChild(MEANING who,int n)
{ // GIVEN SYNSET
    WORDP D2 = Meaning2Word(who);
    FACT* chain = GetObjectHead(who);
    while (chain)
    {
        FACT* at = chain;
        chain = GetObjectNext(chain);
        if (!(at->properties & FACT_IS) ) continue;
		if (!(Meaning2Word(at->subject)->properties & WORDNET_ID)) continue; // not a wordnet down, must be within this synset
        if (--n == 0)   return at->subject;
    }
    return 0;
}

MEANING FindWordNetParent(MEANING who,int n)
{
    WORDP D = Meaning2Word(who);
    unsigned int index = Meaning2Index(who);
    FACT* chain = GetSubjectHead(D); //   facts involving the left side can go up hierarchy on right

    while (chain)
    {
        FACT* at = chain;
        chain = GetSubjectNext(chain);
        if (trace & INFER_TRACE) TraceFact(at);
        if (!(at->properties & (FACT_IS|FACT_MEMBER))) continue;
        if (index > 1 && !Meaning2Index(at->subject)) continue; //   generic only allowed for meaning 1
        if (at->subject != who) //   not an exact match. is is a related match
        {
            WORDP D1 = Meaning2Word(at->subject);
            WORDP D2 = Meaning2Word(who);
            if (D1 != D2) continue; 
            if (Meaning2Index(at->subject) != 0 &&  Meaning2Index(who) != 0) continue; //   both are specific
        }
        if (--n == 0)   return at->object;
    }
    return 0;
}

void DrawDownHierarchy(MEANING T,unsigned int depth,unsigned int limit,bool kid,bool sets)
{
	if (sets) limit = 1000;
    if (depth >= limit || !T) return;
    WORDP D = Meaning2Word(T);
    unsigned int index = Meaning2Index(T);
    unsigned int size = D->meaningCount;
    if (!size) size = 1; //   try for hierarchy globally

	if (*D->word == '~') // show members
	{
		FACT* F = GetObjectHead(D);
		for (unsigned int j = 0; j < (depth*2); ++j) Log(STDUSERLOG," "); 
		unsigned int i = 0;
		while (F)
		{
			MEANING M = F->subject;
			WORDP S = Meaning2Word(M);
			if ( S->properties & WORDNET_ID) M = S->meanings[1];
            Log(STDUSERLOG,"%s ",WriteMeaning(M)); // simple member
			if ( ++i >= 10)
			{
				Log(STDUSERLOG,"\r\n");
				for (unsigned int j = 0; j < (depth*2); ++j) Log(STDUSERLOG," ");
				i = 0;
			}
			F = GetObjectNext(F);
		}
		return;
	}

    for (unsigned int k = 1; k <= size; ++k) //   for each meaning of this dictionary word
    {
		if (D->properties & WORDNET_ID) size = 0; // we just have 1 real meaning as we are head
        else if (index)
		{
			if (k != index) continue; //   not all, just one specific meaning
			T = D->meanings[k]; // synset head
		}
		else T = (D->meaningCount) ? D->meanings[k] : MakeMeaning(D); //   did not request a specific meaning, look at each in turn

        //   for the current T meaning
        if (depth++ == 0 && size)  Log(STDUSERLOG,"\r\n<%s.%d => %s\r\n",D->word,k,WriteMeaning(T)); //   header for this top level meaning is OUR entry and MASTER
        int l = 0;
        while (++l) //   find the children of the meaning of T
        {
			WORDP E = Meaning2Word(T);
			if (!(E->properties & WORDNET_ID)) continue;	// local synset, not real
			MEANING child = (limit >= 1) ? FindChild(T,l) : 0; //   only headers sought
            if (!child) break;
			if (sets) //   no normal words, just a set hierarchy
			{
				WORDP D = Meaning2Word(child);
				if (D->word[0] != '~') continue;
			}

			 //   child and all syn names of child
            for (unsigned int j = 0; j < (depth*2); ++j) Log(STDUSERLOG," "); 
            Log(STDUSERLOG,"%d. %s\r\n",depth,WriteMeaning(child));
			DrawDownHierarchy(child,depth,limit,kid,sets);
        } //   end of children for this value
        --depth;
    }
}

static char* EatTopicToken(char* ptr, char* spot) 
{
    char* start = spot;
	*spot = 0;
	tokenLength = 0;
	if (!ptr || *ptr == 0) return 0;
    ptr = SkipWhitespace(ptr);
	if (*ptr == '"' || (*ptr == '\\' && ptr[1] == '"')) //   "ddd" or \"ddd"
    {
		char* beginptr = ptr;		//   used to see if quote read happened ok
 		ptr = ReadQuote(ptr, spot);//   grab whole string - opening has been stored
	    if (ptr != beginptr) return ptr;
    }

    while (*ptr != ' ' &&  *ptr != '\t' && *ptr != 0 ) 
    {
        char c = *ptr;
		if (c == '`' && ptr == start) return ptr;
        //   [ allowed to be within a word, because of rules
		if (spot == start+1 && '_' == *start); //   allow _ before anything
        else if ((c == '`' || c == ')' || (c == ']' && *start != '\\') ||  (c == '[' && *start != '\\' ) || c == '(' || c == '{' ||  c == '}' ) && spot != start)  break; //   dont add to a word tail
        ptr++;
        *spot++ = c;  //   put char into word
        if ((c == '[' && *start != '\\') || c == '{' || (c == ']' && *start != '\\') || c == '}' ) break;       //   we MUST be done now
        if ((c == '(' ||  c == ')') && spot == (start+1))  break; //   opening or closing paren
      }
    *spot = 0;
	unsigned int len = spot-start - 1;
	//   no joined punctuation unless initials word 
	if (start[len] == '.' || start[len] == ',' || start[len] == '?' || start[len] == '!' || start[len] == ';' || start[len] == '-' ) 
	{
		if (IsMadeOfInitials(start,start+len+1) == ABBREVIATION); 
		else if (len)
		{
			--ptr;
			start[len--] = 0;
		}
	}
	tokenLength = len + 1;
		
	if (start[0] == '"' && start[1] == '"' && start[2] == 0) 
    {
        *start = 0;   //   NULL STRING
        tokenLength = 0;
    }
    return ptr; 
}

static char* ValidateText(char* ptr,bool& first,char* data,char* name,WORDP parent) //   ESL SYSTEM BUG
{
	char word[MAX_WORD_SIZE];
	//   validate all data words in this
	int inbracket = 0;
	bool barry = false;
	while (ptr)
	{
		ptr = EatTopicToken(ptr,word); //   returns an entire string if one is there... doesnt peek into formatting...should
		if (*word == '"') //   a string, not protected, will be formatted, look at its content
		{
			ptr -= strlen(word) - 1;	//   skip opening paren
			continue;
		}
		unsigned int len = strlen(word);
		if (*word != '\\' && word[len-1] == '"' && *word != '"') //   closing quote from when we peeked in, take it as independent token later
		{
			--ptr;
			word[len-1] = 0;
		}
		if (*word == '`' || !*word) break;	//   next thing
		if (*word == '[') 
		{
			++inbracket;
			char word1[MAX_WORD_SIZE];
			EatTopicToken(ptr,word1);
			if (!stricmp(word1,"$barry")) barry = true;
		}
		if (*word == ']') 
		{
			--inbracket;
			barry = false;
		}
		if (!stricmp(word,"loop")) continue; //   function
		if (!stricmp(word,"null")) continue; //   assignment value
		len = strlen(word);
		while (word[len-1] == '.' || word[len-1] == ','   || word[len-1] == ':' || word[len-1] == '-' ||word[len-1] == '!' ||  word[len-1] == '?') word[--len] = 0;
		bool upper = IsUpperCase(word[0]);
		if (upper && !first)
		{
			WORDP D = FindWord(word);
			if (D && D->properties & NOUN) continue;	//   we know this word as a proper name
			if (D &&IsUpperCase(D->word[0])) continue;	 //   it was found as proper
			if (IsUpperCase(word[0])) continue;	//   even if we dont know it
		}
		MakeLowerCase(word); //   lookup lower case form
		if (upper && first) //   upper case which has no LOWER case is fine, otherwise may just be capped word
		{
			WORDP D = FindWord(word);
			if (!D) continue;	//   we dont know it in lower case
		}
		if (first) upper = false;
		first = false;
		if (*word == '^') //   swallow function call or function argumnet
		{
			ptr = SkipWhitespace(ptr);
			if (*ptr == '(') ptr = BalanceParen(ptr+1);
		}
		else if (*word == '<' && word[1] == '<');
		else if (*word == '>' && word[1] == '>');
		else if (*word == '$') //   skip over variable assignment
		{
			ptr = SkipWhitespace(ptr);
			if (*ptr == '=' || ptr[1] == '=')	//   $name = 5  or some such assignment
			{
				ptr = EatTopicToken(ptr,word);  //   swallow op
				ptr = EatTopicToken(ptr,word);  //   swallow value
				ptr = SkipWhitespace(ptr);
				if (*ptr == '+' || *ptr == '-' || *ptr == '*' || *ptr == '/') 
				{
					ptr = EatTopicToken(ptr,word);  //   swallow op
					ptr = EatTopicToken(ptr,word);  //   swallow value
				}
			}
		}
		else if (word[0] == '$' || word[0] == '_' || word[0] == '%' || word[1] == '=' || word[0] == '='  || word[0] == '@' || word[0] == '~'); //   var
		else if (word[1] == 0);	//   some punctuation or whatever
		else if (IsDigit(word[0]));	//   some number
		else if (IsNumber(word));	//   some number
		else if (IsPlaceNumber(word));	//   some number
		else if (word[0] == '|'); //  parser fragment
		else if (word[0] == '*'); //  some wildcard
		else if (word[0] == '\\'); //   escaped quantity
		else //verify this
		{
			WORDP D = FindWord(word); 
			if (D && IsMember(D,parent)) continue;	//   extended vocabulary directly legal
			char* item = FindCanonical(word,0);
			if (!item) item = word;
			D = FindWord(item); 
			char value[100];
			strncpy(value,data,40);
			value[40] = 0;
			ptr = SkipWhitespace(ptr);
			if (*ptr == '\''); //   upcoming possessive
			else if (!D && upper); //   proper name (unknown)
			else if (D && IsMember(D,parent)) continue;	//   extended vocabulary directly legal
			else if (strchr(word,'\'')); //   contraction or possessive, allowed
			else if (!D) 
				Log(STDUSERLOG,"%s:  Unknown word %s :  %s\r\n",name,word,value);
			else if ((D->systemFlags & AGE_LEARNED) == KINDERGARTEN); //   fine word
			else if ((D->systemFlags & AGE_LEARNED) == GRADE1_2); //   fine word
			else if (IsUpperCase(D->word[0])); //   cap known
			else if ((D->systemFlags & AGE_LEARNED) == GRADE5_6 || ((D->systemFlags & AGE_LEARNED) == GRADE3_4))
			{
				if (!barry)	
					Log(STDUSERLOG,"%s:  advanced %s :  %s\r\n",name,word, value);
			}
			else
			{
				Log(STDUSERLOG,"%s:  unusable %s -> %s :  %s\r\n",name,word,D->word,value);
			}
		}
	}
	return ptr;
}

void ValidateTopicVocabulary(char* name) //   checks vocabulary of topic
{
	WORDP parent = StoreWord("~extravocabulary");
	int id = FindTopicIDByName(name);
	if (!id)
	{
		printf("%s not found\r\n",name);
		return;
	}
	char* data = GetTopicData(id);
	bool first = true;
	while (data && *data)
	{
		char* ptr = data;  //   now pointing at first useful data
 		char c = *ptr;
		if (c== '(') ptr = BalanceParen(ptr+1); //   go past pattern
		else if (c == ' ') ++ptr; //   no pattern
		else //   there is a label here - &aLABEL (
		{
			int len = c - '0'; //   size of label word + jump field + space
			ptr += len;			//   past jumpoffset + label and space-- points to (
			ptr = BalanceParen(ptr+1);
		}

		//   validate all data words in this
		ptr = ValidateText(ptr,first,data,name,parent);
		if (data) data = FindNextResponder(NEXTRESPONDER,data);
		first = true;
	}
}

void TestTables()
{
	WORDP parent = StoreWord("~extravocabulary");
	WORDP D = FindWord("verify");
	if (!D) return;
	FACT* F = GetObjectHead(D);
	while (F)
	{
		WORDP E = Meaning2Word(F->subject);
		D = Meaning2Word(F->verb);
		char text[MAX_WORD_SIZE];
		strcpy(text,E->word);
		char* at;
		while ((at = strchr(text,'_'))) *at = ' ';	//   back into words
		bool first = false;
		ValidateText(text,first,text,D->word,parent);
		F = GetObjectNext(F);
	}
}

void TestPattern(char* input)
{ //   input is a pattern and a sentence, and maybe a var assign e.g.,  (* food *)  Do you like food?  %hour = 20
#ifndef NOSCRIPTCOMPILER
	while (IsWhiteSpace(*input)) ++input;
	if (*input != '(') 
	{
		Log(STDUSERLOG,"Bad pattern");
		return;
	}

	char data[1000];
	char* pack = data;
	strcpy(readBuffer,input);
	jumpIndex = 1;
	if (setjmp(scriptJump[1])) return;
	char junk[MAX_WORD_SIZE];
	ReadNextSystemToken(NULL,NULL,junk,false,false); //   flush any cache data
	char* ptr = ReadPattern(readBuffer, NULL, pack,false);

	//   var assign?
	DoAssigns(ptr);
	if (!*ptr) return; //   forget example sentence

	PrepareSentence(ptr,true,true);	

	lastwordIndex = wordCount; //   current context
	unsigned int gap = 0;
	unsigned int wildcardSelector = 0;
	wildcardIndex = 0;
	unsigned int junk1;
	bool result =  Match(data+2,0,0,'(',true,gap,wildcardSelector,junk1,junk1);
	if (result) Log(STDUSERLOG," Matched\r\n");
	else Log(STDUSERLOG," Failed\r\n");
#endif
}


void C_TestOutput(char* input)
{ //   input is output content
#ifndef NOSCRIPTCOMPILER
	char data[1000];
	char* pack = data;
	strcpy(readBuffer,input);
	jumpIndex = 1;
	if (setjmp(scriptJump[1])) return;
	char junk[MAX_WORD_SIZE];
	ClearVariableSetFlags();
	ReadNextSystemToken(NULL,NULL,junk,false,false); //   flush any cache data
	char* ptr = ReadOutput(input, NULL, pack,false);
	unsigned int result;
	char* buffer = AllocateBuffer();
	FreshOutput(data,buffer,result, 0);
	Log(STDUSERLOG,"output: %s\r\n",buffer);
	ShowChangedVariables();
	FreeBuffer();
#endif
}

static void C_Verify(char* input)
{
	char word[MAX_WORD_SIZE];
	// may be :verify
	// or :verify ~topic
	// or :verify pattern/topic
	// or :verify pattern/topic ~topic
	char* ptr = ReadCompiledWord(input,word);
	ptr = SkipWhitespace(ptr);
	if (!*word) VerifyAllTopics('r');
	else if (*word == '~') VerifyAccess(word,'r');
	else if (!stricmp(word,"pattern"))
	{
		ReadCompiledWord(ptr,word);
		if (*word) VerifyAccess(word,'p');
		else VerifyAllTopics('p');
	}
	else if (!stricmp(word,"topic"))
	{
		ReadCompiledWord(ptr,word);
		if (*word) VerifyAccess(word,'t');
		else VerifyAllTopics('t');
	}
	else if (!stricmp(word,"outside"))
	{
		ReadCompiledWord(ptr,word);
		if (*word) VerifyAccess(word,'o');
		else VerifyAllTopics('o');
	}
}

static void C_Undo(char* input)
{
	char ptr[MAX_WORD_SIZE];
	sprintf(ptr,"USERS/topic_%s_%s.txt",loginID,computerID);
	char name[MAX_WORD_SIZE];
	sprintf(name,"USERS/fact_%s.txt",loginID);
	CopyFile2File(ptr,"TMP/topics.tmp",false);	// revert backup of current topic 
	CopyFile2File(name,"TMP/facts.tmp",false);	// revert backup of current topic facts 
	ResetResponses();
	ReadUserData();
	ResetSentence();
	strcpy(revertBuffer,input);
}

static void C_Revert(char* input)
{
	char ptr[MAX_WORD_SIZE];
	sprintf(ptr,"USERS/topic_%s_%s.txt",loginID,computerID);
	char name[MAX_WORD_SIZE];
	sprintf(name,"USERS/fact_%s.txt",loginID);
	CopyFile2File(ptr,"TMP/topics.tmp",false);	// revert backup of current topic 
	CopyFile2File(name,"TMP/facts.tmp",false);	// revert backup of current topic facts 
	ResetResponses();
	ReadUserData();
	ResetSentence();
}

static void C_TestPattern(char* input)
{
	TestPattern(input);
}

static void C_Word(char* input)
{
	char word[MAX_WORD_SIZE];
	char junk[MAX_WORD_SIZE];
	while(1)
	{
		input = SkipWhitespace(input);
		input = ReadCompiledWord(input,word);
		if (!*word) break;
		input = SkipWhitespace(input);
		int limit= 0;
		if (IsDigit(*input))
		{
			input = ReadCompiledWord(input,word);
			limit = atoi(junk);
		}
		DumpDictionaryEntry(word,limit);  
	}
} 	

static void C_Variables(char* input)
{
	DumpVariables(); 
	DumpInterestingTopics();

} 	

static void C_Source(char* input)
{
	char word[MAX_WORD_SIZE];
	char* ptr = ReadCompiledWord(input,word);
	SetSource(word);  
	ptr = SkipWhitespace(ptr);
	ReadCompiledWord(ptr,word);
	echoSource = 0;
	if (!stricmp(word,"echo")) echoSource = 1;
	else if (!stricmp(word,"internal"))  echoSource = 2;
} 

static void C_ServerLog(char* input)
{
	serverLog = atoi(input) != 0;
	Log(STDUSERLOG,"server log to %d\r\n",serverLog);
}

static void C_Prepare(char* input)
{
	unsigned int oldtrace = trace;
	if (!trace) trace = -1;
	if (!stricmp(input,"all")) prepareMode = 1;
	else if (!stricmp(input,"none")) prepareMode = false;
	else PrepareSentence(input,true,true);	
	trace = oldtrace;
}

static void C_Pennbank(char* input)
{
	FILE* in = fopen("out.txt","rb");
	if (!in) return;
	char words[200][40];	// what sentence word is
	char tags[200][20];		// what pennbank value is
	char sentence[MAX_WORD_SIZE];
	unsigned int matchedTags = 0;
	unsigned int failedTags = 0;
	unsigned int badSentences = 0;
	while (ReadALine(readBuffer,in))
	{
		char word[MAX_WORD_SIZE];
		unsigned int count = 1;
		char* ptr = readBuffer;
		*sentence = 0;
		while (1)
		{
			ptr = SkipWhitespace(ptr);
			ptr = ReadCompiledWord(ptr,word);
			if (!*word) break; // done reading words
			char* sep = strchr(word,'/');
			if (!sep) 
			{
				Log(STDUSERLOG,"missing break %s\r\n",readBuffer);
				return;
			}
			*sep++ = 0;
			if (!stricmp(sep,"n't")) strcpy(sep,"not");
			if (!stricmp(sep,"ca")) strcpy(sep,"can"); // presume from can't
			strcpy(words[count],sep);
			strcpy(tags[count],word);
			strcat(sentence,words[count]);
			strcat(sentence," ");
			++count;
		}
		*words[count] = 0;
		*words[count+1] = 0;
		*words[count+2] = 0;
		*words[count+3] = 0;
		*words[count+4] = 0;
		*words[count+5] = 0;

		PrepareSentence(sentence,false,true);	
		POSTag((trace) ? true : false);
		unsigned int base = 1;
		for (unsigned int i = 1; i <= wordCount; ++i)
		{
			bool failed = false;
			char* word = wordStarts[i];
			char last = word[strlen(word)-1];
			if (!stricmp(words[base],"'d")) strcpy(words[base],word);	// match 'd to had or would or whatever we ask it to substitute
			if (!stricmp(words[base],"'re")) strcpy(words[base],word);	// match 're to are
			if (!stricmp(words[base],"'ll")) strcpy(words[base],word);	// match 'll to will
			if (!stricmp(words[base],"'s")) strcpy(words[base],word);	// match 's to is or has or 's as we did
			
			if (stricmp(words[base],word) && !stricmp(words[base+1],word)) ++base;	// we dropped 1st word (like "then a team of diggers came along")
			else if (stricmp(words[base],word)) // we merged stuff or changed it
			{
				char merge[MAX_WORD_SIZE];
				strcpy(merge,words[base]);	// initial word
				strcat(merge,"_");
				strcat(merge,words[base+1]);
				if (!stricmp(merge,word)) // we merged 2
				{
					if (values[i] & VERB_TENSES) strcpy(tags[base+1],tags[base]); // preserve tag
					strcpy(words[base+1],merge);
					++base;
				}
				else
				{
					strcat(merge,"_");
					strcat(merge,words[base+2]);
					if (!stricmp(merge,word)) // we merged 3
					{
						if (values[i] & VERB_TENSES) strcpy(tags[base+2],tags[base]); // preserve tag
						strcpy(words[base+2],merge);
						base += 2;
					}
					else
					{
						strcat(merge,"_");
						strcat(merge,words[base+3]);
						if (!stricmp(merge,word)) // we merged 4
						{
							if (values[i] & VERB_TENSES) strcpy(tags[base+3],tags[base]); // preserve tag
							strcpy(words[base+3],merge);
							base += 3;
						}
						else
						{
							strcat(merge,"_");
							strcat(merge,words[base+4]);
							if (!stricmp(merge,word)) // we merged 5
							{
								if (values[i] & VERB_TENSES) strcpy(tags[base+4],tags[base]); // preserve tag
								strcpy(words[base+4],merge);
								base += 4;
							}
							else
							{
								++badSentences;
								Log(STDUSERLOG,"At %s Skipping %s\r\n",word,readBuffer);
								Log(STDUSERLOG,"    %s---\r\n",DumpAnalysis(values,"POS",false));
								break;
							}
						}
					}
				}
			}

			if (bitCounts[i] != 1) failed = true;
			else if (!stricmp(words[base],word)) // same word, no change
			{
				if (!stricmp(tags[base],"CC"))
				{
					if (values[i] & CONJUNCTION_COORDINATE);
					else failed = true;
				}
				else if (!stricmp(tags[base],"CD"))
				{
					if (values[i] & (NOUN_CARDINAL));
					else if (values[i] & (NOUN_ORDINAL)); // they get this wrong
					else if (values[i] & (ADJECTIVE_CARDINAL)); // they get this wrong
					else failed = true;
				}
				else if (*tags[base] == '(');	// punctuation
				else if (*tags[base] == ')');	// punctuation
				else if (*tags[base] == '$');	// punctuation
				else if (*tags[base] == '.');	// punctuation
				else if (*tags[base] == ',');	// punctuation
				else if (!stricmp(tags[base],"DT"))
				{
					if (values[i] & DETERMINER); 
					else failed = true;
				}
				else if (!stricmp(tags[base],"EX"))
				{
					if (values[i] & THERE_EXISTENTIAL);
					else failed = true;
				}
				else if (!stricmp(tags[base],"IN"))
				{
					if (values[i] & (PREPOSITION|CONJUNCTION_SUBORDINATE));
					else failed = true;
				}
				else if (!stricmp(tags[base],"JJ"))
				{
					if (values[i] & (ADJECTIVE_PARTICIPLE|ADJECTIVE_BASIC|NOUN_ORDINAL)); 
					else failed = true;
				}
				else if (!stricmp(tags[base],"JJR"))
				{
					if (values[i] & ADJECTIVE_MORE);
					else failed = true;
				}
				else if (!stricmp(tags[base],"JJS"))
				{
					if (values[i] & ADJECTIVE_MOST);
					else failed = true;
				}
				else if (!stricmp(tags[base],"MD"))
				{
					if (values[i] & AUX_VERB_TENSES);
					else failed = true;
				}
				else if (!stricmp(tags[base],"NN"))
				{
					if (values[i] & (NOUN_SINGULAR|NOUN_PROPER_SINGULAR)); // they dont ship lc to uc when its wrong
					else if (values[i] & NOUN_GERUND);		// they believe this
					else 
					{
						failed = true;
					}
				}
				else if (!stricmp(tags[base],"NNS"))
				{
					if (values[i] & NOUN_PLURAL);
					else failed = true;
				}
				else if (!stricmp(tags[base],"NNP"))
				{
					if (values[i] & NOUN_PROPER_SINGULAR);
					else failed = true;
				}
				else if (!stricmp(tags[base],"NNPS"))
				{
					if (values[i] & NOUN_PROPER_PLURAL);
					else failed = true;
				}
				else if (!stricmp(tags[base],"PDT"))
				{
					if (values[i] & PREDETERMINER);
					else failed = true;
				}
				else if (!stricmp(tags[base],"POS"))
				{
					if (values[i] & POSSESSIVE);
					else failed = true;
				}
				else if (!stricmp(tags[base],"PRP"))
				{
					if (values[i] & PRONOUN_BITS);
					else failed = true;
				}
				else if (!stricmp(tags[base],"PRP$"))
				{
					if (values[i] & PRONOUN_POSSESSIVE);
					else failed = true;
				}
				else if (!stricmp(tags[base],"RB"))
				{
					if (values[i] & ADVERB_BASIC);
					else failed = true;
				}
				else if (!stricmp(tags[base],"RBR"))
				{
					if (values[i] & ADVERB_MORE);
					else failed = true;
				}
				else if (!stricmp(tags[base],"RBS"))
				{
					if (values[i] & ADVERB_MOST);
					else failed = true;
				}
				else if (!stricmp(tags[base],"RP"))
				{
					if (values[i] & PARTICLE);
					else failed = true;
				}
				else if (!stricmp(tags[base],"TO")) // cannot fail
				{
				}
				else if (!stricmp(tags[base],"VB"))
				{
					if (values[i] & (VERB_INFINITIVE|NOUN_INFINITIVE));
					else if (values[i] & AUX_VERB_TENSES); // they get this wrong
					else failed = true;
				}
				else if (!stricmp(tags[base],"VBG"))
				{
					if (values[i] & (NOUN_GERUND|VERB_PRESENT_PARTICIPLE) || (values[i] & ADJECTIVE_PARTICIPLE && last == 'g'));
					else failed = true;
				}
				else if (!stricmp(tags[base],"VBN")) // past participle
				{
					if (values[i] & VERB_PAST_PARTICIPLE || (values[i] & ADJECTIVE_PARTICIPLE && last == 'd') || (values[i] & ADJECTIVE_PARTICIPLE && last == 'n')); // taken, walked
					else if (values[i] & VERB_PAST);	// allow this for now
					else if (values[i] & AUX_VERB_TENSES);	// we disagree
					else if (values[i] & (VERB_INFINITIVE|NOUN_INFINITIVE));	// maybe we dont do this right
					else failed = true;
				}
				else if (!stricmp(tags[base],"VBP")) // verb present not 3rd person
				{
					if (values[i] & VERB_PRESENT);
					else if ( values[i] & AUX_VERB_TENSES);	// they get this wrong
					else failed = true;
				}
				else if (!stricmp(tags[base],"VBD")) // past tense verb 
				{
					if (values[i] & VERB_PAST);
					else if (values[i] & ADJECTIVE_PARTICIPLE && last == 'd');
					else if (values[i] & VERB_PAST_PARTICIPLE);	// they get this wrong often
					else if (values[i] & AUX_VERB_TENSES);	// they get this wrong often
					else failed = true;
				}
				else if (!stricmp(tags[base],"VBZ"))
				{
					if (values[i] & VERB_PRESENT_3PS);
					else if (values[i] & AUX_VERB_TENSES); // we disagree
					else failed = true;
				}
				else if (!stricmp(tags[base],"WDT")) // wh determiner
				{
					if (values[i] & DETERMINER && 
						(!stricmp(wordStarts[i],"what") || !stricmp(wordStarts[i],"which") || !stricmp(wordStarts[i],"when"))
						);
					else failed = true;
				}
				else if (!stricmp(tags[base],"WP$"))
				{
					if (values[i] & DETERMINER && !stricmp(wordStarts[i],"whose") );
					else failed = true;
				}
				else if (!stricmp(tags[base],"WRB")) // when, where, why,whence, whereby, wherein,  whereupon how  wh-adverb
				{
					if (values[i] & WH_ADVERB);
					else failed = true;
				}
				else if (!stricmp(tags[base],"WP")) // WHO, WHETHER  WHAT WHICH   wh-pronoun
				{
					if ((values[i] & (PRONOUN_SUBJECT|PRONOUN_OBJECT)) && 
						(!stricmp(wordStarts[i],"what") || !stricmp(wordStarts[i],"whether") || !stricmp(wordStarts[i],"who") || !stricmp(wordStarts[i],"which"))
						);
					else failed = true;
				}
				else if (!stricmp(tags[base],"UH")) // interjection
				{
					failed = true;
				}
				else if (!stricmp(tags[base],"SYM")) // symbol
				{
					failed = true;
				}
				else if (!stricmp(tags[base],"FW")) // foreign word
				{
					failed = true;
				}
				else if (!stricmp(tags[base],"LS")) // list item marker
				{
					failed = true;
				}
				else failed = true; // unknown pennbank
			}

			if (failed) 
			{
				++failedTags;
				Log(STDUSERLOG,"Failed %s (%s) in: %s\r\n",words[base],tags[base],sentence);
				Log(STDUSERLOG,"    %s---\r\n",DumpAnalysis(values,"POS",false));
			}
			else ++matchedTags;
			++base;
		} // end current word test
		int x = 0;
	} // end this sentence
	Log(STDUSERLOG,"Matched %d tags, failed %d tags, skipped %d sentences\r\n",matchedTags,failedTags,badSentences);
	fclose(in);
}

static void C_Pos(char* input)
{
	if (!stricmp(input,"all")) prepareMode = 2;
	else if (!stricmp(input,"none")) prepareMode = false;
	else
	{
		PrepareSentence(input,false,true);	
		POSTag((trace) ? true : false);
		if (!(trace & PREPARE_TRACE)) 
		{
			Log(STDUSERLOG,"\r\n%s---\r\n",DumpAnalysis(values,"\r\nTagged POS",false)); // show anyway
			DumpPrepositionalPhrases();
		}
	}
}

static void C_TestTopic(char* input)
{
	static char word[MAX_WORD_SIZE];
	unsigned int oldtrace = trace;
	trace = -1;
	if (*input == '~')
	{
		input = ReadCompiledWord(input,word);
		input = SkipWhitespace(input);
	}
	unsigned int topic = FindTopicIDByName(word);
	if (!topic) 
	{
		printf("%s topic not found\r\n",word);
		return;
	}
	PrepareSentence(input,true,true);	
	bool pushed = PushTopic(topic); 
	globalDepth++;
	char* buffer = AllocateBuffer();
	ClearVariableSetFlags();
	PerformTopic(0,buffer);
	ShowChangedVariables();
	globalDepth--;
	FreeBuffer();
	if (pushed) PopTopic();
	trace = oldtrace;
}

static void C_PostProcess(char* input)
{
	unsigned int oldtrace = trace;
	trace = -1;

	while (*input)
	{
		input = SkipWhitespace(input);
		char* old = input;
		input = Tokenize(input,wordCount,wordStarts);   //   only used to locate end of sentence but can also affect tokenFlags (no longer care)
			
		//   strip trailing blanks
		char* at = input;
		while (*--at == ' '); 
		char c = *++at;
		*at = 0;

		//   save sentences as facts
		WORDP D = StoreWord(old);
		MEANING M = MakeMeaning(Dchatoutput);
		CreateFact(MakeMeaning(D),M,M,TRANSIENTFACT);

		*at = c;
	}
	ClearVariableSetFlags();
	OnceCode("$control_post");
	ShowChangedVariables();
	trace = oldtrace;
}

static void C_Trace(char* input)
{
	char word[MAX_WORD_SIZE];
	unsigned int flags = 0;
	while (input)
	{
		input = ReadCompiledWord(input,word); // if using trace in a table, use closer "end" if you are using named flags
		if (!*word) break;
		input = SkipWhitespace(input);
		if (!stricmp(word,"all")) flags = -1;
		else if (!stricmp(word,"none")) flags = 0;
		else if (IsNumberStarter(*word)) 
		{
			ReadInt(word,flags);
			break; // there wont be more flags -- want :trace -1 in a table to be safe from reading the rest
		}
		else if (!stricmp(word,"basic")) flags |= BASIC_TRACE;
		else if (!stricmp(word,"prepare")) flags |= PREPARE_TRACE; 
		else if (!stricmp(word,"match")) flags |= MATCH_TRACE;
		else if (!stricmp(word,"output")) flags |= OUTPUT_TRACE;
		else if (!stricmp(word,"pattern")) flags |= PATTERN_TRACE;
		else if (!stricmp(word,"infer")) flags |= INFER_TRACE;
		else if (!stricmp(word,"substitute")) flags |= SUBSTITUTE_TRACE;
		else if (!stricmp(word,"hierarchy")) flags |= HIERARCHY_TRACE;
		else if (!stricmp(word,"fact")) flags |= FACTCREATE_TRACE;
		else if (!stricmp(word,"end")) break; // safe end
		else if (*word == '!') // NOT tracing a topic 
		{
			if (word[1]) // ! jammed against topic, separate it
			{
				input -= strlen(word+1); 
				word[1] = 0;
			}
			input = SkipWhitespace(input);
			input = ReadCompiledWord(input,word);
			unsigned int topic = FindTopicIDByName(word);
			if (!topicDebugMap[topic]) topicDebugMap[topic] = NO_TRACE;
			else topicDebugMap[topic] = 0;
			Log(STDUSERLOG," !trace %s = %d\r\n",word,topicDebugMap[topic]);
			return;
		}
		else if (*word == '~') // tracing a topic or rule
		{
			char* period = strchr(word,'.');
			if (period) *period = 0;
			unsigned int topic = FindTopicIDByName(word);
			if (!period) 
			{
				if (!topicDebugMap[topic]) topicDebugMap[topic] = BASIC_TRACE;
				else topicDebugMap[topic] = 0; // turn off (toggle)
				Log(STDUSERLOG," trace %s = %d\r\n",word,topicDebugMap[topic]);
			}
			else // find ALL labelled statement and mark them
			{
				unsigned int id = 0;
				char* which = GetTopicData(topic);
				bool found = false;
				while (which && (which = FindNextLabel(period+1,which,id,true)))
				{
					SetDebugMark(id,topic);
					found = true;
					which = FindNextResponder(NEXTRESPONDER,which);
				}
				if (!found) printf("cannot find %s.%s\r\n",word,period+1);
			}
			return;
		}
	}
	trace = flags;
    Log(STDUSERLOG," trace set to %d\n",trace);
} 


static unsigned int GetDebugValue(char* input)
{
	char word[MAX_WORD_SIZE];
	unsigned int flags = 0;
	while (input)
	{
		input = ReadCompiledWord(input,word); // if using trace in a table, use closer "end" if you are using named flags
		if (!*word) break;
		input = SkipWhitespace(input);
		if (!stricmp(word,"all")) flags = -1;
		else if (!stricmp(word,"true")) flags = -1;
		else if (!stricmp(word,"none")) flags = 0;
		else if (!stricmp(word,"false")) flags = 0;
		else if (IsNumberStarter(*word)) 
		{
			ReadInt(word,flags);
			break; // there wont be more flags -- want :trace -1 in a table to be safe from reading the rest
		}
	}
	return flags;
}

static void C_Debug(char* input)
{
	debug = (bool)GetDebugValue(input);
    Log(STDUSERLOG," debug set to %d\n",debug);
} 

static void C_PosTrace(char* input)
{
	posTrace = (bool)GetDebugValue(input);
    Log(STDUSERLOG," postrace set to %d\n",posTrace);
} 

static void C_Repeat(char* input)
{
	repeatable = GetDebugValue(input) ? 1000 : 0;
    Log(STDUSERLOG," repeatable to: %d\n",repeatable);
}

static void C_All(char* input)
{
	all = (bool)GetDebugValue(input);
    Log(STDUSERLOG," all set to %d\n",all);
} 

static void C_Echo(char* input)
{
	echo = (bool)GetDebugValue(input);
    Log(STDUSERLOG," echo set to %d\n",echo);
} 

static void C_EchoServer(char* input)
{
	echoServer = (bool)GetDebugValue(input);
    Log(STDUSERLOG," echoServer set to %d\n",echoServer);
} 

static void C_Noreact(char* input)
{
	noreact = (bool)GetDebugValue(input);
    Log(STDUSERLOG," noreact set to %d\n",noreact);
} 

static void C_RegressSub(char* input)
{
	RegressSubstitutes();
} 

static void C_FakeReply(char* input)
{
	regression = 1;
	if  (*input == '1' || *input == 'O' || *input == 'o') oktest = OKTEST;
	else if  (*input == '2' || *input == 'W' || *input == 'w') oktest = WHYTEST;
	else
	{
		oktest = 0;
		regression = 0;
	}
	if (oktest) Log(STDUSERLOG,"Fake input set to %s\r\n",(oktest == OKTEST) ? "oktest" : "whytest");
}  

static void C_Generate(char* input)
{
#ifndef NOSCRIPTCOMPILER
	AIGenerate(input);
#endif
}

static void C_Reset(char* input)
{
	char word[MAX_WORD_SIZE];
	unsigned int old = inputCount; // preserve count so regress will show full number of lines swallowed
	ReadNewUser(); 
	inputCount = old;
	userFirstLine = 1;
	strcpy(word,"I have forgotten all about you.");
	AddResponse(word);
}

static void C_Restart(char* input)
{
	trace = 0;
	CreateSystem();
	systemReset = true;	// drop this user, start over a new conversation with him as a new user
}

static void C_Shutdown(char* input)
{
	exit(0);
}

static void C_Build(char* input)
{
#ifndef NOSCRIPTCOMPILER
	char file[MAX_WORD_SIZE];
	input = ReadCompiledWord(input,file);
	input = SkipWhitespace(input);
	int spell = 1;
	if (!stricmp(input,"nospell")) spell = 0;
	else if (!stricmp(input,"output")) spell = 2;

	unsigned int oldtrace = trace;
	trace = 0;	//   in case persona turned it on, we want it off by default
	if (!*file) printf("missing build label");
	else
	{
		sprintf(logbuffer,"USERS/build%s_log.txt",file); //   all data logged here by default
		FILE* in = fopen(logbuffer,"wb");
		if (in) fclose(in);
		char word[MAX_WORD_SIZE];
		sprintf(word,"files%s.txt",file);
		if (ReadTopicFiles(word,(*file == '0') ? BUILD0 : BUILD1,dictionaryFacts,spell)) 
		{
			CreateSystem();
			systemReset = true;	// drop this user, start over a new conversation with him as a new user
		}
	}
	trace = oldtrace;
	if (!trace) echo = 0;
#endif
}  

static void C_DumpSet(char* input)
{
	char word[MAX_WORD_SIZE];
	input = SkipWhitespace(input);
	input = ReadCompiledWord(input,word);
	input = SkipWhitespace(input);
	fclose(fopen("intransitive.txt","wb")); //   clear inransitive verb list
	fclose(fopen("sets.txt","wb")); //   clear inransitive verb list
	DumpSetHierarchy(FindWord(word,0),0,atoi(input));
}     

static void C_Down(char* input)
{
	char word[MAX_WORD_SIZE];
	input = SkipWhitespace(input);
	input = ReadCompiledWord(input,word);
	input = SkipWhitespace(input);
    int limit = atoi(input);
    if (!limit) limit = 1; //   top 2 level only (so we can see if it has a hierarchy)
	input = SkipWhitespace(input);
	NextinferMark();
    DrawDownHierarchy(ReadMeaning(word,false,false),0,limit,false,!stricmp(input,"sets"));
}

static void C_Up(char* input)
{
 	char word[MAX_WORD_SIZE];
	ReadCompiledWord(input,word);
	DumpUpHierarchy(ReadMeaning(word,false,false),0);
}

static void TrimIt(char* name,unsigned int flag) // flag== 0, both.  flag == 1 only human  5 = sort to file
{
	char buffer[MAX_MESSAGE];
	getcwd(buffer, MAX_MESSAGE);
	FILE* in = fopen(name,"rb");
	if (!in) return;

	FILE* out;
	if (flag != 5)
	{
		out = fopen("TMP/tmp.txt","ab");
		fprintf(out,"-----------------\r\n");
	}
	char file[MAX_WORD_SIZE];
	*file = 0;
	char* at;
	unsigned int n = 0;
	if (in) while (fgets(readBuffer, MAX_MESSAGE, in) ) 
	{
		unsigned int len = strlen(readBuffer);
   		if (!len) continue;

		if (flag == 5) // sort to users
		{
			char* ptr = strchr(readBuffer,'.'); // find ip stuff
			if (!ptr) continue;
			if (!IsDigit(*(ptr-1))) continue;
			if (!IsDigit(ptr[1])) continue;

			ptr = strchr(ptr,' ');	// find space before username
			if (!ptr) continue;
			char word[MAX_WORD_SIZE];
			char filename[MAX_WORD_SIZE];
			ReadCompiledWord(ptr+1,word);
			ptr = strchr(word,'/'); // remove trailing slash if any
			if (ptr) *ptr = 0;
			sprintf(filename,"USERS/%s",word);
			out = fopen(filename,"ab");
			if (out)
			{
				fprintf(out,readBuffer);
				fclose(out);
			}
			continue;
		}

		//   echo separations
		if (strstr(readBuffer,":relogin") || strstr(readBuffer,":login") || *readBuffer == '\r')  continue;

		at = strstr(readBuffer,"msg:");
		if (!at) continue;
		at += 5;
		char* output = strstr(at,"=> ");
		*output = 0;
		output += 4;

        if (!flag) fprintf(out,"%s => %s\r\n",at,output);
        else  fprintf(out,"%s\r\n",at);
    }
    if (in) fclose(in);
    if (flag != 5) fclose(out);
}

static void C_Trim(char* input) // create simple file of user chat from directory
{   //   0 shows input and output yielded   1 is input only   5 is sort to users
 	char word[MAX_WORD_SIZE];
	ReadCompiledWord(input,word);
	unsigned int flag = atoi(word); 
	fclose(fopen("TMP/tmp.txt","wb"));
	WalkDirectory("LOGS",TrimIt,flag);
}

static void C_ESLTopic(char* input)
{
	char word[MAX_WORD_SIZE];
 	ReadCompiledWord(input,word); //   topic name
	if (!*word)
	{
		for (unsigned int i = 1; i <= topicNumber; ++i)
		{
			ValidateTopicVocabulary(topicNameMap[i]);
		}
		TestTables();
	}
	else ValidateTopicVocabulary(word);
}

static void C_Abstract(char* input)
{
	if (*input == '~')
	{
		DisplayTopic(input);
		return;
	}

	for (unsigned int i = 1; i <= topicNumber; ++i)
	{
		DisplayTopic(topicNameMap[i]);
	}
	DisplayTables();
}

static void ESLOutput(WORDP D,void* data) //   display esl dictionary (basic + advanced)
{
	FILE* out = (FILE*)data;
		
	char c = D->word[0]; 
	uint64 n = D->systemFlags & AGE_LEARNED;
	if (IsUpperCase(c)); //   ignore proper names
	else if (!(n & KINDERGARTEN)); //    ignore too complex vocab
	else
	{
		c = (n & GRADE1_2) ? '+' : ' '; 
		fprintf(out,"%s %c\r\n",D->word,c);
	}
}

static void POSOverride(WORDP D,void* data) //   display esl dictionary (basic + advanced)
{
	char c = D->word[0]; 
	uint64 n = D->systemFlags & AGE_LEARNED;
	if (IsUpperCase(c)); //   ignore proper names
	else if (!(n & KINDERGARTEN)); //    ignore too complex vocab
	else
	{
		if (D->systemFlags & (NOUN|VERB|ADJECTIVE|ADVERB|PREPOSITION ));
		else // does it need it?
		{
			uint64 mflags = D->properties & (ADJECTIVE|ADVERB|NOUN|VERB|PREPOSITION);
			int count = BitCount(mflags);
			if (count > 1)
				Log(STDUSERLOG,"%s\r\n",D->word);
		}
	}
}
static void C_POSOverride(char* input)
{
	WalkDictionary(POSOverride,NULL);
}

static void C_ESL(char* input)
{
	FILE* out = fopen("eslwords.txt","wb");
	WalkDictionary(ESLOutput,out);
	fclose(out);
}


void C_Facts(char* input)
{
	WriteFacts(fopen("TMP/facts.txt","wb"),factBase);
}

static void C_Used(char* input)
{
	int topic = FindTopicIDByName(input);
	if (!topic) return;
	unsigned int i = 0;
	Log(1,"Disabled: ");
	unsigned int limit = topicRespondersMap[topic] * 8;
	for (unsigned int i = 0; i < limit; ++i)
	{
		if (!UsableResponder(i,topic)) Log(1,"%d ",i);
	}
	Log(1,"\r\n");
}

static void C_Execute(char* input)
{
#ifndef NOSCRIPTCOMPILER
	int oldindex = jumpIndex;
	jumpIndex = -1;		// dont react to bad function calls
	char* answer = AllocateBuffer();
	unsigned int result;
	char data[1000];
	*data = 0;
	char* out = data;
	ReadOutput(input, NULL,out,NULL);
	FreshOutput(data,answer,result);
	printf("output: %s result: %d\r\n",answer,result);
	FreeBuffer();
	jumpIndex =  oldindex;
#endif
}

static void C_Real(char* input)
{
	C_Execute(input);
}

static void Topic(WORDP D,void* junk)
{
	if (!(D->systemFlags & (BUILD0|BUILD1))) return; // ignore internal system concepts
	if (!(D->systemFlags & TOPIC_NAME)) return;
	Log(STDUSERLOG,"%s\r\n",D->word);
}

static void C_Topics(char* input)
{
	WalkDictionary(Topic,0);
}


static void C_SortTopic(char* input)
{
	WalkDictionary(SortTopic0,0);
	AddSystemFlag(StoreWord("~_dummy",AS_IS),TOPIC_NAME|BUILD0);
	fclose(fopen("cset.txt","wb"));
	WalkDictionary(SortTopic,(void*)input[10]);
	if (*input) system("sort /rec 63000 c:/chatscript/cset.txt >x.txt");
}

static void C_SortConcept(char* input)
{
#ifdef INFORMATION
To get concepts in a file sorted alphabetically (both by concept and within)l, do 
	0. :build 0 when only the file involved is the only active one in files0.txt
	1. :sortconcept x		-- builds one concept per line and sorts the file by concept name
	2. take the contents of x.txt and replace the original file, putting dummy from end at top
	3. :build 0
	4. :sortconcept			-- maps concepts neatly onto multiple lines
	5. take the contents of x.txt, delete ~_dummy concept, and replace the original file
#endif
	WalkDictionary(SortConcept0,0); // stores names of concepts on dummy concept, to lock position in dictionary
	AddSystemFlag(StoreWord("~_dummy",AS_IS),BUILD0);
	fclose(fopen("cset.txt","wb"));
	WalkDictionary(SortConcept,(void*)input[12]);
	if (*input) system("sort /rec 63000 c:/chatscript/cset.txt >x.txt");
}

bool Command(char* input)
{
	char word[MAX_WORD_SIZE];
	input = ReadCompiledWord(input,word);
	WORDP D = FindWord(word);
	if (D) 
	{
		CommandInfo* info;
		if (D->x.codeIndex) 
		{
			bool oldecho = echo;
			echo = true;	// see outputs sent to log file on console also
			info = &commandSet[D->x.codeIndex];
			input = SkipWhitespace(input);
			wasCommand = true;
			(*info->fn)(input);
			if (info->resetEcho) echo = oldecho;
			return wasCommand;
		}
	}
	printf("Unknown command %s\r\n",input);
	return false; 
}

//void AmazonSort(uint64 id,int depth,int limit);

void C_A(char* buffer)
{
	uint64 id;
	unsigned int limit;
	char* ptr = ReadInt64(buffer,id);
	ptr = SkipWhitespace(ptr);
	ReadInt(ptr,limit);
	if (limit == 0) limit = 100000;
	sprintf(logbuffer,"USERS/amazon.tbl");
	FILE* out = fopen(logbuffer,"wb");
	if (out) fclose(out);

	Log(STDUSERLOG,"table: AMAZON( ^l1 ^l2 ^l3 ^l4 ^l5 ^l6 ^l7 ^l8 ^l9 ^browseid ^keyword) \r\n");
	Log(STDUSERLOG,"Amazon(^l1 ^l2 ^l3 ^l4 ^l5 ^l6 ^l7 ^l8 ^l9 ^browseid ^keyword ) DATA:\r\n");
	showBadUTF = false;
	echo = false;
//	AmazonSort(id,0,limit);
	sprintf(logbuffer,"USERS/junk.txt");
}

CommandInfo commandSet[] =
{//   1st arg must always be all lower case
	{"",0,false,""},
	{":a",C_A,true,""},
	{":abstract",C_Abstract,true,"Display overview of chat script data"}, // overview all topics in simpler form
	{":all",C_All,true,"Set to see all possible responses to an input"},
	{":bot",C_Bot,true,"Change focus to this bot"}, // change to this bot
	{":build",C_Build,true,"Compile a script"}, // compile a level of script
	{":clean",C_Clean,true,"Convert source files to NL instead of CR/LF for unix"},	// prepare c source file for unix by removing CR
	{":crash",0,true,"Simulate a crash"},
	{":debug",C_Debug,false,"Set debug variable (easier to test a bot)"},
	{":down",C_Down,true,"Show wordnet items inheriting from here"}, // show words down from this
	{":dumpset",C_DumpSet,true,"Display designed POS tree"}, // export a word-class tree
	{":dumpnouns",C_DumpNouns,true,"Display noun tree"}, // export primary nouns
	{":echo",C_Echo,false,"Set echo variable"},
	{":echoserver",C_EchoServer,false,"Set echo server variable"},
	{":esl",C_ESL,true,""},
	{":esltopic",C_ESLTopic,true,""},
	{":execute",C_Execute,true,""}, // run a macro
	{":facts",C_Facts,true,"Dump all facts to TMP/facts.tmp"}, // write all facts into tmp
	{":fakereply",C_FakeReply,true,""},  // use OK or WHY as all input
	{":generate",C_Generate,true,""},
	{":prepare",C_Prepare,true,"Show results of tokenization and tagging on a sentence"}, // show tokenization and matching for a sentence
	{":noreact",C_Noreact,false,"Set noreact variable"},
	{":pennbank",C_Pennbank,true,"Compare chatscript postagger to pennback output file given"},// show pos tagging
	{":pos",C_Pos,true,"Display pos tag of sentence"},// show pos tagging
	{":posdata",ReadTaggerPos,false,"Generate posinfo.txt from pos-analyzed sentences"}, //("CORPUS/pos.txt");
	{":posdump",C_POSOverride,true,"Show kindergarten words that have no pos override and need it"},
	{":posit",C_Posit,true,""},
	{":posit1",C_Posit1,true,""},
	{":postrace",C_PosTrace,false,"Set postrace variable"},
	{":postprocess",C_PostProcess,true,""}, // show what system would do with this output from it
	{":real",C_Real,true,""},	// actually save state of this into user world, prefix as before some other command
	{":repeat",C_Repeat,true,"allow system to repeat outputs"},
	{":restart",C_Restart,true,"Read dictionary and all again"},
	{":reset",C_Reset,true,"Start user all over again"},
	{":revert",C_Revert,true,"Redo the last input sentence in context- eg turn on trace and then repeat some bad result"},
	{":retry",C_Undo,true,"back up and try replacement user input"},
	{":serverlog",C_ServerLog,true,"enable or disable server logging"},
	{":shutdown",C_Shutdown,true,"Stop the server"},
	{":sortconcept",C_SortConcept,true,""},
	{":sorttopic",C_SortTopic,true,""},
	{":source",C_Source,true,"Switch input to a file"},
	{":testpos",TagTest,true,"Perform a regression test on pos-tagging using default file or named file"},
	{":testpattern",C_TestPattern,true,"See if a pattern works with an input"},
	{":testoutput",C_TestOutput,true,"See what output script would generate"},
	{":testtopic",C_TestTopic,true,"See what a topic does given specific input"},
	{":trim",C_Trim,true,"Strip excess off chatlog file to make simple file"},
	{":trace",C_Trace,false,"Set trace variable"},
	{":topics",C_Topics,false,"List all topics"},
	{":up",C_Up,true,"Display concept structure above a word"},
	{":used",C_Used,true,"For a topic, list the responder id's that are used up"},
	{":variables",C_Variables,true,"Display current values of user variables"},
	{":verify",C_Verify,true,"Test that topic rules are accessible"},
	{":verifysub",C_VerifySub,true,"Regress test substitutes.txt"},
	{":word",C_Word,true,"Display information about given word"},
	
	{":brownpos",ReadBrown,false,"Get pos stats from Brown Corpus"},
	{":medpostpos",ReadMedpost,false,"Get pos stats from Medpost Corpus"},
	{":ancpos",ReadANC,false,"Get pos stats from ANC Corpus"},

	{":help",C_Help,false,"Show all commands (command) or system variables (variable)"},
	{0,0,false,""}	
};
#endif
