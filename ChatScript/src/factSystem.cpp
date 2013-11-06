// factSystem.cpp - represents triples of information (but not queries)

#include "common.h"

#ifdef INFORMATION

Facts are added as layers. You remove facts by unpeeling a layer. You can "delete" a fact merely by
marking it dead.

Layer1:  facts resulting from wordnet dictionary  (dictionaryFacts)
Layer2:	 facts resulting from topic system build0
Layer3:	 facts resulting from topic system build1
Layer4:	 facts created by user

Layer 4 is always unpeeled after an interchange with the chatbot, in preparation for a new
user who may chat with a different persona.

Layers 2 and 3 are unpeeled if you want to restart the topic system to read in new topic data on the fly.
Layer 3 is unpeeled for a new build 1
Layer 3 and 2 are unpeeled for a new build 0

Layer 1 is never unpeeled. If you want to modify the dictionary, you either restart the chatbot entirely
or patch in data piggy backing on facts (like from the topic system).

Unpeeling a layer implies you will also reset dictionary/stringspace pointers back to levels at the
start of the layer since facts may have allocated dictionary and string items. 
This is ReturnToDictionaryFreeze for unpeeling 3/4 and ReturnDictionaryToWordNet for unpeeling layer 2.

#endif



FACT* factBase = NULL; 
FACT* factEnd = NULL;
FACT* currentFact = NULL;
FACT* topicFacts = NULL;	//   end of topic facts, start of user facts
FACT* build0Facts = NULL;	//   end of build0 facts, start of build1 facts
FACT* dictionaryFacts = NULL;
FACT* chatbotFacts = NULL;
FACT* factFree = NULL;

//   values of verbs to compare against
MEANING Mmember;
MEANING Mis;


int Fact2Index(FACT* F) 
{ 
	return  (F) ? ((int)(F - factBase)) : 0;
}

void ResetFactSystem()
{
	while (factFree > topicFacts) FreeFact(factFree--); //   restore to end of basic facts
    chatbotFacts = factFree;
	for (unsigned int i = 1; i < MAX_FIND_SETS; ++i) factSet[i][0] = (FACT*) 0; //   empty all facts sets
}

FACT* GetFactRefDecode(char* word)
{
	char hold[MAX_WORD_SIZE];
	strcpy(hold,word);
	char* comma;
	while ((comma = strchr(hold,','))) 
	{
		char* altcomma = (comma - hold) + word;
		strcpy(comma,altcomma+1); // remove std number commas
	}
	int n = atoi(hold);
	if (n <= 0) return NULL;
	return (n > (factFree-factBase)) ? NULL : GetFactPtr(n);
}

FACT* FindFact(MEANING subject, MEANING verb, MEANING object, uint64 properties)
{//   accepts subject=0 or object=0 and finds fact ignoring that field (a fast query)
    FACT* F;
	FACT* G;
    //   see if FACT* of factalready exists. if so, just return it 
	if (properties & FACTSUBJECT)
	{
		F  = GetFactPtr(subject);
		G = GetSubjectHead(F);
		while (G)
		{
			if (G->properties & DEADFACT);
			else if (G->verb == verb &&  G->object == object && G->properties == properties) return G;
			G  = GetSubjectNext(G);
		}
		return NULL;
	}
 	else if (properties & FACTOBJECT)
	{
		F  = GetFactPtr(object);
		G = GetObjectHead(F);
		while (G)
		{
			if (G->properties & DEADFACT);
			else if (G->subject == subject && G->verb == verb &&  G->properties == properties) return G;
			G  = GetObjectNext(G);
		}
		return NULL;
	}
  	else if (properties & FACTVERB)
	{
		F  = GetFactPtr(verb);
		G = GetVerbHead(F);
		while (G)
		{
			if (G->properties & DEADFACT);
			else if (G->subject == subject && G->object == object &&  G->properties == properties) return G;
			G  = GetVerbNext(G);
		}
		return NULL;
	}   
	//   simple FACT* based on dictionary entries
	int which;
	if (!(properties & FACTSUBJECT) && subject) 
	{
		F = GetSubjectHead(Meaning2Word(subject));
		which = 0;
	}
	else if (!(properties & FACTOBJECT) && object) 
	{
		F = GetSubjectHead(Meaning2Word(object));
		which = 1;
	}
    else 
	{
		F = GetVerbHead(Meaning2Word(verb));
		which = 2;
	}
    while (F)
    {
		if (F->properties & DEADFACT);
		else if ((!subject || F->subject == subject) && (!verb || F->verb ==  verb) && (!object || F->object == object) && properties == F->properties)  return F;
 		
		if (which == 0) F = GetSubjectNext(F);
		else if (which == 1) F = GetObjectNext(F);
		else  F = GetVerbNext(F);
    }
    return NULL;
}

FACT* CreateFact(MEANING subject, MEANING verb, MEANING object, uint64 properties)
{
	currentFact = NULL; 

	//   get correct field values
    WORDP s = (properties & FACTSUBJECT) ? NULL : Meaning2Word(subject);
    WORDP v = (properties & FACTVERB) ? NULL : Meaning2Word(verb);
	WORDP o = (properties & FACTOBJECT) ? NULL : Meaning2Word(object);

	if (s && (s->word[0] == 0 || s->word[0] == ' '))
	{
		ReportBug("bad choice in fact subject");
		return NULL;
	}
	if (v && (v->word[0] == 0 || v->word[0] == ' '))
	{
		ReportBug("bad choice in fact verb");
		return NULL;
	}
	if (v) properties |= v->systemFlags & (FACT_MEMBER|FACT_IS);	//   this fact has a marked verb on it
	if (o && (o->word[0] == 0 || o->word[0] == ' '))
	{
		ReportBug("bad choice in fact object");
		return NULL;
	}

	//   insure fact is unique if requested
	currentFact = FindFact(subject,verb,object,properties); 
	if (currentFact) return currentFact; 

	if (!subject || !object || !verb)
	{
		ReportBug("Missing field in fact create");
		return NULL;
	}

	FACT* F;

	//   allocate a fact
    if (++factFree == factEnd) 
	{
		--factFree;
		ReportBug("out of fact space");
		return NULL;
    }
    currentFact = factFree;

	//   init the basics
	memset(currentFact,0,sizeof(FACT));
    currentFact->subject = subject;
	currentFact->verb = verb; 
    currentFact->object = object;
	currentFact->properties = properties;

    //   crossreference
    if (s) 
    {
        SetSubjectNext(currentFact,GetSubjectHead(subject));
        SetSubjectHead(subject,currentFact);
    }
    else 
    {
        F = GetFactPtr(currentFact->subject);
		SetSubjectNext(currentFact,F); 
		SetSubjectHead(F,currentFact);
	}
    if (v) 
    {
        SetVerbNext(currentFact, GetVerbHead(verb));
        SetVerbHead(verb,currentFact);
    }
	else 
    {
		F = GetFactPtr(currentFact->verb);
		SetVerbNext(currentFact,GetVerbHead(F));
		SetVerbHead(F,currentFact);
	}
    if (o) 
    {
        SetObjectNext(currentFact, GetObjectHead(object));
        SetObjectHead(object,currentFact);
    }
	else
	{
        F = GetFactPtr(currentFact->object);
		SetObjectNext(currentFact,GetObjectNext(F)); 
		SetObjectHead(F,currentFact);
    }
	
	if (trace & FACTCREATE_TRACE)
	{
		char* buffer = AllocateBuffer();
		buffer = WriteFact(currentFact,false,buffer);
		Log(STDUSERLOG,"create %s\r\n",buffer);
		FreeBuffer();
	}
	return currentFact;
}

bool ExportFacts(char* name, int set)
{
	if (set < 0 || set >= MAX_FIND_SETS) return false;
	if ( *name == '"')
	{
		++name;
		unsigned int len = strlen(name);
		if (name[len-1] == '"') name[len-1] = 0;
	}
	FILE* out = fopen(name,"wb");
	if (!out) return false;

	char word[MAX_WORD_SIZE];
	unsigned int count = (ulong_t)factSet[set][0];
	for (unsigned int i = 1; i <= count; ++i)
	{
		FACT* F = factSet[set][i];
		uint64 properties = F->properties;
		if (F && !(properties & DEADFACT)) // allowed to write out transient facts 
		{
			F->properties &= -1 & TRANSIENTFACT;	// dont pass that out
			char* val = WriteFact(F,false,word);
			F->properties = properties;
			if (*val) fprintf(out,"%s\r\n",val);
		}
	}

	fclose(out);
	return true;
}

char* EatFact(char* ptr,uint64 flags)
{
	char word[MAX_WORD_SIZE];
	char word1[MAX_WORD_SIZE];
	char word2[MAX_WORD_SIZE];
	char word3[MAX_WORD_SIZE];
	ptr = SkipWhitespace(ptr); // could be user-formateed, dont trust
	unsigned int result = 0;
	//   subject
	if (*ptr == '(') //   nested fact
	{
		ptr = EatFact(ptr+1); //   returns after the closing paren
		flags |= FACTSUBJECT;
		sprintf(word,"%d",currentFactIndex() ); //   created OR e found instead of created
	}
	else  ptr = ReadCommandArg(ptr,word,result); //   subject
	ptr = SkipWhitespace(ptr); // could be user-formateed, dont trust
	if (result & ENDCODES) return ptr;

	//verb
	if (*ptr == '(') //   nested fact
	{
		ptr = EatFact(ptr+1);
		flags |= FACTVERB;
		sprintf(word1,"%d",currentFactIndex() );
	}
	else  ptr = ReadCommandArg(ptr,word1,result); //verb
	ptr = SkipWhitespace(ptr); // could be user-formateed, dont trust
	if (result & ENDCODES) return ptr;

	//   object
	if (*ptr == '(') //   nested fact
	{
		ptr = EatFact(ptr+1);
		flags |= FACTOBJECT;
		sprintf(word2,"%d",currentFactIndex() );
	}
	else  ptr = ReadCommandArg(ptr,word2,result); 
	ptr = SkipWhitespace(ptr); // could be user-formateed, dont trust
	if (result & ENDCODES) return ptr;

	//   flags
	char flaglist[MAX_WORD_SIZE];
	*flaglist = 0;
	while (*ptr && *ptr != ')' ) 
	{
		ptr = ReadCommandArg(ptr,word3,result); 
 		if (result & ENDCODES) return ptr;
		if (trace && TraceIt(OUTPUT_TRACE)) 
		{
			strcat(flaglist,word3);
			strcat(flaglist," ");
		}
		if (*word3 == '$') flags |= atoi(GetUserVariable(word3));
		else if (word3[0] == '0' && (word3[1] == 'x' || word3[1] == 'X'))
		{
			uint64 val = 0;
			ptr = ReadHex(ptr,val);
			flags |= val;
		}
		else if (IsDigit(word3[0])) flags |= atoi(word3);
		else  flags |= (int)FindValueByName(word3);
		ptr = SkipWhitespace(ptr);
	}

	MEANING subject;
	if ( flags & FACTSUBJECT)
	{
		FACT* G = GetFactRefDecode(word);
		subject = Fact2Index(G);
	}
	else subject =  MakeMeaning(StoreWord(word,AS_IS),0);
	MEANING verb;
	if ( flags & FACTVERB)
	{
		FACT* G = GetFactRefDecode(word1);
		verb = Fact2Index(G);
	}
	else verb =  MakeMeaning(StoreWord(word1,AS_IS),0);
	MEANING object;
	if ( flags & FACTOBJECT)
	{
		FACT* G = GetFactRefDecode(word2);
		object = Fact2Index(G);
	}
	else object =  MakeMeaning(StoreWord(word2,AS_IS),0);

	if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"%s %s %s %s) = ",word,word1,word2,flaglist);
	
	if (subject && verb && object) CreateFact(subject,verb,object,flags);
	else ReportBug("failed to create fact ");

	return (*ptr) ? (ptr + 2) : ptr; //   returns after the closing ) if there is one
}

bool ImportFacts(char* name, char* set, char* erase, char* transient)
{
	if (*set != '@') return false;
	int store = set[1] - '0';
	unsigned int count = (ulong_t) factSet[store][0];

	if ( *name == '"')
	{
		++name;
		unsigned int len = strlen(name);
		if (name[len-1] == '"') name[len-1] = 0;
	}
	FILE* in = fopen(name,"rb");
	if (!in) return false;
	uint64 flags = 0;
	if (!stricmp(erase,"transient") || !stricmp(transient,"transient"))
	{
		flags |= TRANSIENTFACT;
	}
	while (ReadALine(readBuffer, in))
    {
        if (*readBuffer == 0 || *readBuffer == '#'); //   empty or comment
		char* ptr = strchr(readBuffer,'(');
		if (!ptr) continue; // ignore non-fact lines
        EatFact(ptr+1,flags);
		if (++count > MAX_FIND) --count;
		factSet[store][count] = currentFact;
	}
	fclose(in);
	if (!stricmp(erase,"erase") || !stricmp(transient,"erase")) remove(name);
	factSet[store][0] = (FACT*) count;
	if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"[%d] => ",count);
	return true;
}

void WriteFacts(FILE* out,FACT* F) //   write out from here to end
{ 
	char word[MAX_WORD_SIZE];
	if (!out) return;
	int n = 0;
    while (++F <= factFree) 
	{
		++n;
		if (!(F->properties & (TRANSIENTFACT|DEADFACT))) 
		{
			char* val = WriteFact(F,true,word);
			if (*val) fprintf(out,"%s\r\n",val);
		}
	}
    fclose(out);
}

static char* DecodeField(MEANING T, uint64 flags,char* buffer)
{
    if (flags ) //   fact reference
    {
		FACT* G = GetFactPtr(T);
		if (!*WriteFact(G,false,buffer)) 
			return "";
		buffer += strlen(buffer);
    }
	else if (!T) 
	{
		ReportBug("Missing fact fieldt");
		*buffer++ = '?';
	}
    else 
	{
		WORDP D = Meaning2Word(T);
		if (D->systemFlags & DELETED_WORD) return buffer; //   cancels print
		char* answer = WriteMeaning(T);
		unsigned int len = strlen(answer);
		bool embedded = strchr(answer,' ') != NULL && *answer != '"';

		// be careful with an entry like "(ice) is good" which becomes (ice)_is_good
		if (strchr(answer,'(') && *answer != '"') // convert to string with blanks
		{
			buffer[0] = '"';
			strcpy(buffer+1,answer);
			char* underscore;
			while ((underscore = strchr(buffer,'_'))) *underscore = ' ';
			int len = strlen(buffer);
			buffer += len;
			*buffer++ = '"';
			*buffer = 0;
		}
		else if (embedded) // has blanks but not a quoted string
		{
			ReportBug("embedded space not in string");
			strcpy(buffer,"bad");
		}
		else strcpy(buffer,answer);
		buffer += strlen(buffer);
	}
	*buffer++ = ' ';
	*buffer = 0;
	return buffer;
}

char* WriteFact(FACT* F,bool comments,char* buffer)
{ //   if fact is junk, return the null string
	char* start = buffer;
	if (!F) return "";
	if (F->properties & DEADFACT) 
	{
		strcpy(buffer,"DEAD ");
		buffer += strlen(buffer);
	}
	
	F->properties &= -1 ^ (MARKED_FACT|MARKED_FACT2); //   remove temporary flags maybe left over

	//   fact opener
	*buffer++ = '(';
	*buffer++ = ' ';

	//   do subject
	char* base = buffer;
	buffer = DecodeField(F->subject,F->properties & FACTSUBJECT,buffer);
	if (base == buffer ) return ""; //    word itself was removed from dictionary

	base = buffer;
	buffer = DecodeField(F->verb,F->properties & FACTVERB,buffer);
	if (base == buffer) return ""; //    word itself was removed from dictionary

	base = buffer;
	buffer = DecodeField(F->object,F->properties & FACTOBJECT,buffer);
	if (base == buffer ) return ""; //    word itself was removed from dictionary

	//   add properties
    if (F->properties)  
	{
		sprintf(buffer,"0x%llx ",F->properties); 
		buffer += strlen(buffer);
	}

	//   close fact
	*buffer++ = ')';

	*buffer = 0;

	//   add comments?  -- no more
	return start;
}

void TraceFact(FACT* F)
{
	char word[MAX_WORD_SIZE];
	Log(STDUSERLOG,WriteFact(F,false,word));
	Log(STDUSERLOG,"\r\n");
}

char* ReadField(char* ptr,char* field)
{
	if (*ptr == '(')
	{
		FACT* G = ReadFact(ptr);
		ptr += 2; // skip close and space
		if (!G)
		{
			ReportBug("Missing fact field");
			return NULL;
		}
		sprintf(field,"%d",Fact2Index(G)); 
	}
    else ptr = ReadCompiledWord(ptr,field); 
	if (field[0] == '~') MakeLowerCase(field);	// all concepts/topics are lower case
	return ptr; //   return at new token
}

FACT* ReadFact(char* &ptr)
{
    char word[MAX_WORD_SIZE];
    MEANING subject = 0;
	MEANING verb = 0;
    MEANING object = 0;
 	//   fact may start indented.  Will start with (or be 0 for a null fact
	ptr = SkipWhitespace(ptr);
    ptr = ReadCompiledWord(ptr,word);  //   BUG reconsider spacing need this?
    if (word[0] == '0') return 0; //   unless it is the null fact

	//   look at subject-- nested fact?
	char subjectname[MAX_WORD_SIZE];
	ptr = ReadField(ptr,subjectname);

    char verbname[MAX_WORD_SIZE];
	ptr = ReadField(ptr,verbname);

    char objectname[MAX_WORD_SIZE];
	ptr = ReadField(ptr,objectname);

	if (!ptr) return NULL;
	
    //   handle the flags on the fact
    uint64 properties = 0;
    if (!*ptr || *ptr == ')'); //   end of fact
    else if (*ptr == '0') ptr = ReadHex(ptr,properties);
	else
	{
		char flag[MAX_WORD_SIZE];
		while (*ptr && *ptr != ')')
		{
			ptr = ReadCompiledWord(ptr,flag);
			properties |= FindValueByName(flag);
			ptr = SkipWhitespace(ptr);
		}
	}

	while (*ptr == ' ') ++ptr;

    if (properties & FACTSUBJECT) subject = (MEANING) atoi(subjectname);
    else  subject = ReadMeaning(subjectname);
	if (properties & FACTVERB) verb = (MEANING) atoi(verbname);
	else  verb = ReadMeaning(verbname);
	if (properties & FACTOBJECT) object = (MEANING) atoi(objectname);
	else  object = ReadMeaning(objectname);

    FACT* F = FindFact(subject, verb,object,properties);
    if (!F)   F = CreateFact(subject,verb,object,properties); 
    return F;
}

void ReadDeadFacts(const char* name) //   kill these WordNet facts
{
    FILE* in = fopen(name,"rb");
    if (!in) return;
    while (ReadALine(readBuffer, in))
    {
        if (*readBuffer == 0 || *readBuffer == '#') continue;
		//   facts are direct copies of other facts, are ReadCompiledWord safe
		char* ptr = readBuffer;
        FACT* F = ReadFact(ptr);
        if (F) F->properties |= DEADFACT;   
		else printf("ReadDeadFacts fact not found: %s\r\n",readBuffer);
    }
    fclose(in);
}

void ReadFacts(const char* name,uint64 zone) //   a facts file may have dictionary augmentations and variable also
{
    FILE* in = fopen(name,"rb");
    if (!in) return;
	char word[MAX_WORD_SIZE];
    while (ReadALine(readBuffer, in))
    {
		ReadCompiledWord(readBuffer,word);
        if (*word == 0 || *word == '#'); //   empty or comment
		else if (*word == '+') //   dictionary entry
		{
			char word[MAX_WORD_SIZE];
			char* at = SkipWhitespace(readBuffer+2);
			at = ReadCompiledWord(at,word); //   start at valid token past space
			WORDP D = StoreWord(word);
			ReadDictionaryFlags(D,at);
			AddSystemFlag(D,zone);
			if (word[0] == '~') AddSystemFlag(D,PATTERN_WORD); // legal as a pattern also
		}
		else if (*word == '$') // variable
		{
			 char* ptr = strchr(readBuffer,'=');
			 if (*ptr)
			 {
				*ptr = 0;
				SetUserVariable(readBuffer,ptr+1);
			 }
			 else ReportBug("Bad fact file user var assignment");
		}
        else 
		{
			char* ptr = readBuffer;
			ReadFact(ptr);
		}
    }
   fclose(in);
}

void InitFacts()
{
	if ( factBase == 0) 
	{
		factBase = (FACT*) malloc(MAX_FACT_NODES * sizeof(FACT)); // only on 1st startup, not on reload
		if ( factBase == 0)
		{
			printf("failed to allocate fact space\r\n");
			ReportBug("Failed to allcoate fact space\r\n");
			exit(0);
		}
		memset(factBase,0,sizeof(FACT) * MAX_FACT_NODES); // not strictly necessary
	}
    factFree = factBase;
	factEnd = factBase + MAX_FACT_NODES;
}

void InitFactWords()
{
	//   special internal fact markers
	WORDP D = StoreWord("member");
	D->systemFlags |= FACT_MEMBER;
	Mmember = MakeMeaning(D);
	D = StoreWord("is");
	D->systemFlags |= FACT_IS;
	Mis = MakeMeaning(D);
}

void CloseFacts()
{
    free(factBase);
}

void FreeFact(FACT* F)
{ //   most recent facts are always at the top of any xref list. Can only free facts sequentially backwards.
    if (!(F->properties & FACTSUBJECT)) SetSubjectHead(F->subject,GetSubjectNext(F));
	else SetSubjectHead(GetFactPtr(F->subject),GetSubjectNext(F));

	if (!(F->properties & FACTVERB)) SetVerbHead(F->verb,GetVerbNext(F));
	else  SetVerbHead(GetFactPtr(F->verb),GetVerbNext(F));

    if (!(F->properties & FACTOBJECT)) SetObjectHead(F->object,GetObjectNext(F));
	else SetObjectHead(GetFactPtr(F->object),GetObjectNext(F));
 }

void WriteExtendedFacts(FILE* factout,FACT* base,unsigned int zone)
{
	if (!factout) return;
	WriteVariables(factout);
	WriteDictionaryChange(factout,zone);
	WriteFacts(factout,base);
}

bool IsMember(WORDP who,WORDP parent)
{
    FACT* F = GetSubjectHead(who);
    while (F)
    {
        if (F->properties & FACT_MEMBER) 
        {
            WORDP obj = Meaning2Word(F->object);
            if (obj == parent) return true;
        }
        F = GetSubjectNext(F);
    }
    return false;
}

