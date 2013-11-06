// infer.cpp - handles queries into fact data

#include "common.h"
#define MAX_PARENTS 9980
static MEANING tomParents[MAX_PARENTS+20];
bool blockMeaning = false;
static int tomParentIndex = 0;
static int tomParentWalk = 0;
static unsigned int ignoremark = 0;

#define NORMAL 1    //   class and set are a group (recursive)
#define QUOTED 2    //   class and set are simple words
#define PREMARK 4   //   marking but save word not meaning so wont use as scan
#define NOQUEUE 8   //   just marked with mark, not queued
#define QUEUE 16	//    add to q, 
static  FACT* set[MAX_FIND]; 
static  FACT* set1[MAX_FIND]; 
static char subject[MAX_WORD_SIZE];
static char object[MAX_WORD_SIZE];

static unsigned int chainCounter = 0;
unsigned int inferMark = 0;
unsigned int otherMark = 0;
unsigned int saveMark = 0;

FACT* factSet[MAX_FIND_SETS][MAX_FIND+1]; //   set of five is max
unsigned int foundIndex; //   current index in factSet[0] - 1based

static unsigned int wordCounter;
static void AddSet(WORDP D,int byClass);

#define MAX_QUEUE 10000
static MEANING queue[MAX_QUEUE+20]; 
static unsigned int queueIndex;
static float floatValues[MAX_FIND+1];

unsigned int NextinferMark()
{
    return ++inferMark;
}

void Sort(char* set) //   sort low to high ^sort(@1subject) which field we sort on (subject or object)
{
    unsigned int n = set[1] -'0';
	bool subject = set[2] != 'o';
    bool swap = true;
    unsigned int i;
    unsigned int size = (ulong_t) factSet[n][0];
    for (i = 1; i <= size; ++i)
    {
        FACT* F =  factSet[n][i];
        floatValues[i] = (float) atof((subject) ? Meaning2Word(F->subject)->word : Meaning2Word(F->object)->word); 
    }
    while (swap)
    {
        swap = false;
        for (i = 1; i <= size-1; ++i) 
        {
            if (floatValues[i] > floatValues[i+1])
            {
                float tmp = floatValues[i];
                floatValues[i] = floatValues[i+1];
                floatValues[i+1] = tmp;
                FACT* F = factSet[n][i];
                factSet[n][i] = factSet[n][i+1];
                factSet[n][i+1] = F;
                swap = true;
            }
        }
        --size;
    }
}

static void AddChainMember(MEANING T, int byClass)
{
    if (queueIndex >= MAX_QUEUE || !T) return; //   just dont do more
    WORDP D = Meaning2Word(T);
    if (byClass & PREMARK) 
	{
		if (D->stamp.inferMark == otherMark) return;	//   done already
		D->stamp.inferMark = otherMark; //   saving as word means wont scan but will erase mark later. Mark it for finding.
	}
    else
    {
 		if (D->stamp.inferMark == saveMark) return;	//   done already
        D->stamp.inferMark = saveMark;
        if (!(byClass & NOQUEUE)) queue[queueIndex++] = T;
    }

    //   check all equivalences as well
    FACT* F = GetSubjectHead(D);
    while (F)
    {
  		WORDP v = Meaning2Word(F->verb);
        if (v->properties & FACT_MEMBER) // a kind of REFINE?
		{
			WORDP D = Meaning2Word(F->object); 
			if (D->word[0] != '~')
			{
				if (trace & INFER_TRACE) TraceFact(F);
				AddChainMember(F->object,byClass);
			}
		}
        F = GetSubjectNext(F);
    }
}

static bool AddFoundFact(FACT* F,unsigned int store)
{
    if (foundIndex == MAX_FIND - 1) //   ignore excess
    {
        return true; 
    }
    factSet[store][++foundIndex] = F;
    if (trace & INFER_TRACE ) 
    {
        Log(STDUSERLOG,"    Found:");
        TraceFact(F);
    }
    return true;
}

static bool AddFollow(int flags,MEANING T,MEANING from) //   flags==QUEUE means put on list  or 0 means nothing 
{
    if (queueIndex >= MAX_QUEUE || !T) return false; //   just dont do more
    WORDP D = Meaning2Word(T);
	if (D->stamp.inferMark == saveMark || (ignoremark &&  D->stamp.inferMark == ignoremark)) return false;
	D->stamp.inferMark = saveMark; 
	char buf[1000];
	if (trace & INFER_TRACE) 
	{
		if (D->properties & WORDNET_ID) 
		{
			WORDP E = NULL;
			if (D->meaningCount ) E =  Meaning2Word(D->meanings[1]); // swap generic to specific instance
			if (E) sprintf(buf,"%s(%s) ",D->word,E->word);
			else strcpy(buf,D->word);
		}
		else 
		{
			strcpy(buf,D->word);
			if (from)
			{
				WORDP E = Meaning2Word(from);
				char* p = buf + strlen(buf);
				sprintf(p,"<-%s ",E->word);
			}
		}
	}
	if (flags & QUEUE) 
	{
		queue[queueIndex++] = T;
		if (trace & INFER_TRACE) Log(STDUSERLOG," %s+",buf);
	}
	else if (trace & INFER_TRACE) Log(STDUSERLOG," %s.",buf);

    //auto  check all equivalences as well
    FACT* F = GetSubjectHead(D);
    while (F)
    {
        if (F->properties & FACT_MEMBER) // a kind of refine
		{
			WORDP D = Meaning2Word(F->object);
			if (D->word[0] != '~') AddFollow(flags,F->object,F->subject);
		}
        F = GetSubjectNext(F);
    }
	return true;
}

static void SaveSetMembers(char* word,int byClass)
{
	if (!(byClass & QUOTED) && *word == '~' ) //   class OR generic meaning - EITHER created by topic system OR by hierarchy down search - MARK ALL ITS MEANINGS
    {
        char name[MAX_WORD_SIZE];
        strcpy(name,word);    //   named set
        WORDP D = FindWord(name,0);
        AddSet(D,byClass);
    }
	else 
    {
        chainCounter = 0;
        AddChainMember(ReadMeaning(word,false),byClass);
    }
}

static void AddSet(WORDP D,int byClass)
{
	FACT* F = (D) ? GetObjectHead(D) : NULL;
	while (F)
	{
		if (F->properties & FACT_MEMBER )
        {
            WORDP D = Meaning2Word(F->subject);
            if (F->properties & ORIGINAL_ONLY) SaveSetMembers(D->word,byClass | QUOTED); 
            else SaveSetMembers(D->word,byClass); 
        }
		F = GetObjectNext(F);
	}
}

static void QueryFacts(WORDP D,unsigned int index,unsigned int store,char* kind)
{//   analog of MarkFacts, but gets the set of topics
    if (!D || D->stamp.inferMark == inferMark) return;
    D->stamp.inferMark = inferMark; 
    FACT* F;
    FACT* G = GetSubjectHead(D); 
    unsigned int count = 20000;
    unsigned int restriction = 0;
	if (kind) //   limitation on translation of word as member of a set
	{
		if (!strnicmp(kind,"subject",7)) restriction = NOUN;
		else if (!strnicmp(kind,"verb",4)) restriction = VERB;
		else if (!strnicmp(kind,"object",7)) restriction = NOUN;
	}
	while (G)
    {
        F = G;
        G = GetSubjectNext(G);
        if (trace & INFER_TRACE ) TraceFact(F);

        if (!--count) ReportBug("matchfacts infinite loop");
        uint64 flags = F->properties;
   		WORDP v = Meaning2Word(F->verb);
        unsigned int fromindex = Meaning2Index(F->subject);
        if (fromindex == index || !fromindex); //   we allow penguins to go up to bird, then use unnamed bird to go to ~topic
        else if (index ) continue;  //   not following this path
        else if (flags & ORIGINAL_ONLY ) continue; //   you must match exactly- generic not allowed to match specific wordnet meaning- hierarchy BELOW only

		if (v->properties & FACT_MEMBER  && !AllowedMember(F,0,restriction,0)) continue; //   wrong POS
        if ((F->properties & FACT_MEMBER && !(flags & ORIGINAL_ONLY) )
             )
        {
            WORDP object = Meaning2Word(F->object);
            if (object->stamp.inferMark != inferMark) 
            {
                if (object->word[0] == '~') //   note topics and concepts, keeping only NATURAL topcics (eraseable) if a topic
                {
					WORDP D = StoreWord("1");
					FACT* F = CreateFact(MakeMeaning(D,0),MakeMeaning(FindWord("is"),0),MakeMeaning(object,0),TRANSIENTFACT);
					factSet[store][++foundIndex] = F;
                }
                QueryFacts(object,0,store,kind);
            }
         }
    }
}

unsigned int QueryTopicsOf(char* word,unsigned int store,char* kind)
{
    foundIndex = 0;
    NextinferMark(); 
    WORDP D = FindWord(word,0);
    QueryFacts(D,0,store,kind);
    factSet[store][0] = (FACT*)foundIndex;
    if (trace & INFER_TRACE) Log(STDUSERLOG,"QueryTopics: %s %d ",word,foundIndex);
    return  foundIndex;
}

unsigned int HierarchyIntersect(char* storeid,char* from, char* to)
{//   this will match a STRAIGHT fact list OR a multi set list as generated by wordnet using QueryHierary.
 //   The multiset marks START of a zone using verb IGNOREF
    unsigned int store = storeid[1] - '0';  
    factSet[store][0] = 0;
    factSet[store+1][0] = 0;
	WORDP ignore = FindWord("ignore");
    WORDP D;
    FACT* F;

    //   mark to set
    otherMark = NextinferMark();
    unsigned int i;
    unsigned toset = to[1] - '0';
    unsigned int limit = (ulong_t)factSet[toset][0];
    if (!limit) return 0;
    bool toAsSubject = (to[2] == 's'); //   the set is of subjects. Take subject as value. 
    for (i = 1; i <= limit; ++i)
    {
        F = factSet[toset][i];
        D = Meaning2Word((toAsSubject) ? F->subject : F->object);
        D->stamp.inferMark = otherMark;
    }

    //   walk FROM set for match - match will be at i if not exceed limit
    bool fromAsSubject = (from[2] == 's'); //   the set is of subjects. Take subject as value. 
    unsigned int where = from[1] - '0';  
    limit = (ulong_t)factSet[where][0];
    for (i = 1; i <= limit; ++i)
    {
        F = factSet[where][i];
        D = Meaning2Word((fromAsSubject) ? F->subject : F->object);
        if (D->stamp.inferMark == otherMark) 
        {
            D->stamp.inferMark = 0; //   mark intersection point
            break;    //   found intersection point
        }
    }
    if (i > limit) return 0; //   failed

    //   build set (the FROM intersect)
    foundIndex = 0;
    for (unsigned int j = 1; j <= i; ++j) 
    {
        F = factSet[where][j];
		WORDP v = Meaning2Word(F->verb);
        D = Meaning2Word((fromAsSubject) ? F->subject : F->object);
        if (v == ignore) foundIndex = 0;   //   restart of a set
        factSet[store][++foundIndex] = F;
    }
    factSet[store][0] = (FACT*)foundIndex;

    //   build set+1 (the TO intersect)
    foundIndex = 0;
    limit = (ulong_t) factSet[toset][0];
    for (unsigned int j = 1; j <= limit; ++j) 
    {
        FACT* F = factSet[toset][j];
        WORDP E = Meaning2Word((toAsSubject) ? F->subject : F->object);
		WORDP v = Meaning2Word(F->verb);
        if (v == ignore) foundIndex = 0;   //   start of a set
        factSet[store+1][++foundIndex] = F;
        if (E->stamp.inferMark != otherMark) break; //   saved up to common point
    }
    factSet[store+1][0] = (FACT*)foundIndex;
    return foundIndex;
}

unsigned int TopicIntersect(char* fromset, char* toset)
{
    unsigned int from = fromset[1]-'0';
    unsigned int to = toset[1]-'0';
    unsigned int setIndex = 0;

    unsigned int i;
    FACT* F;
    unsigned int limit = (ulong_t) factSet[from][0];
    factSet[to][0] = 0;
    foundIndex = 0;
    if (limit > MAX_FIND) 
    {
        factSet[from][0] = 0;
        return 0; //
    }
    for (i = 1; i <= limit; ++i) //   mark all basic facts
    {
        FACT* G = factSet[from][i];
		if (trace & INFER_TRACE) TraceFact(G);
        G->properties |= MARKED_FACT; //   mark facts for detection
    }
    //   find intersect with facts of each topic.
    for (i = 0; i < topicInSentenceIndex; ++i) //   for every topic indexed by sentence
    {
        WORDP D = FindWord(topicsInSentence[i],0);
        if (trace & INFER_TRACE) Log(STDUSERLOG,"topic: %s\r\n",D->word);
        unsigned int total = 0;
        F = GetObjectHead(D);   //   facts xrefed by this topic
        while (F)
        {
            FACT* G = F;
            if (trace & INFER_TRACE) TraceFact(G);
            if (G->properties & MARKED_FACT)
            {
                factSet[to][++total] = G;
                if (total > setIndex && setIndex) //   in a worse set
                {
                    total = 0;
                    break;
                }
            }
            F = GetObjectNext(F);
        }
        if (total && (total < setIndex || !setIndex))
        {
            //   FIRST, mark previous facts. we want intersection if we can get it
			unsigned int j;
			for (j = 1; j <= setIndex; ++j) set[j]->properties |= MARKED_FACT2;  //   mark known facts
            unsigned int set1Index = setIndex;
            memcpy(&set1[1],&set[1],sizeof(FACT*) * set1Index); //   dup facts

            //   now transfer relevant facts across
            setIndex = 0;
            bool marked = false;
            for (j = 1; j <= total; ++j)
            {
                FACT* F = factSet[to][j];
                if (F->properties & MARKED_FACT2) //   a marked fact, add to collection
                {
                    if (!marked) 
                    {
                        marked = true;
                        setIndex = 0; 
                    }
                    set[++setIndex] = F;
                }
                else if (!marked) set[++setIndex] = F;
                //   if we are saving marked and this is not, ignore it
            }
            if (trace & INFER_TRACE) 
            {
                Log(STDUSERLOG,"new set\r\n");
                for (j = 1; j <= setIndex; ++j) TraceFact(set[j]);
            }
            for (j = 1; j <= set1Index; ++j) set1[j]->properties ^= MARKED_FACT2; 
       }
    }

    //   clean  up
    for (i = 1; i <= limit; ++i) 
    {
        FACT* G = factSet[from][i];
        G->properties ^= MARKED_FACT; //   unmark facts
    }
    foundIndex = setIndex;
    factSet[to][0] = (FACT*)setIndex;
    memcpy(&factSet[to][1],&set[1],sizeof(FACT*) * setIndex);

    return foundIndex;
}

static void AddFollowSet(int flags,WORDP D);

static void Follow(int how, char* word)
{
	if (!(how & ORIGINAL_ONLY ) && *word == '~' ) //   recursive on set and all its members
    {
		WORDP D = FindWord(word,0);
		if (!ignoremark || !D ||  D->stamp.inferMark != ignoremark)
		{
 			AddFollow(how, MakeMeaning(D,0),0); //   mark the original name
			AddFollowSet(how,D); //   now follow it
		}
	}
	else 
	{
		if (word[0] == '\'') ++word;	//   shift past the quoted item
		AddFollow(how, ReadMeaning(word),0);
	}
}

static void AddFollowSet(int flags,WORDP D)
{
	if (!D) return;
	FACT* F = GetObjectHead(D);
	while (F)
	{
		if (F->properties & FACT_MEMBER )
        {
            WORDP D = Meaning2Word(F->subject);
			Follow(flags | (F->properties & ORIGINAL_ONLY) ,D->word);
        }
		F = GetObjectNext(F);
	}
}

unsigned int Query(char* kind, char* subjectword, char* verbword, char* objectword, unsigned int count, char* fromset, char* toset, char* propogate, char* match)
{
	if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\nQUERY: %s ",kind);
	WORDP C = FindWord(kind,0);
	if (!C || !(C->systemFlags & QUERY_KIND)) 
	{
		ReportBug("Illegal query control %s",kind);
		return 0;
	}
	char* control = NULL;
	if (C->w.userValue && *C->w.userValue) control = C->w.userValue;
	else 
	{
		ReportBug("query control lacks data %s",kind);
		return 0;
	}

    int n = BurstWord(subjectword);
	if (!n); //   _ is an empty thing, but legal
	else if (*subjectword) strcpy(subjectword,JoinWords(n,false));
    n = BurstWord(objectword);
	if (!n); //   _ is an empty thing, but legal
	else if (*objectword) strcpy(objectword,JoinWords(n,false));
    n = BurstWord(verbword);
	if (!n); //   _ is an empty thing, but legal
	else if (*verbword) strcpy(verbword,JoinWords(n,false));
    n = BurstWord(match);
	MakeLowerCase(verbword);
	strcpy(match,JoinWords(n,false));
	n = BurstWord(propogate);
	strcpy(propogate,JoinWords(n,false));
	if (trace & INFER_TRACE) Log(STDUSERLOG," ::%s  [%s %s %s] %d <%s >%s m:%s p:%s - ",control,subjectword,verbword,objectword,count,fromset,toset,propogate,match);   

	//   handle what sets are involved
	int store = (IsDigit(toset[1]) ) ? (toset[1]-'0') : 0;
	int from = (IsDigit(toset[1])) ? (fromset[1]-'0') : 0;
	foundIndex = 0; //   zero the starting facts
	bool upDictionary = false;
	queueIndex = 0; //   zero the starting set
	ignoremark = 0;
	unsigned int baseMark = inferMark; //   offsets of this value

	//   process initialization
nextsearch:  //   can do multiple searches, thought they have the same basemark so can be used across searchs (or not) up to 9 marks

#ifdef INFORMATION
# first segment describes what to initially mark and initially queue for processing (sources of facts)
#	Values:
#		1..9  = set global tag to this label - 0 means turn off global tag
#		i =  use argument tag on words to ignore during a  tag or queue operation 
#			Next char is tag label. 0 means no ignoremark
#		s/v/o/p/m/~set  = use subject/verb/object/progogate/match/factset argument as item to process or use named set 
#			This is automatically marked using the current mark and is followed by 
#				q  = queue items (sets will follow to all members recursively)
#				t  = tag (no queue) items
#				e  = expandtag (no queue) (any set gets all things below it tagged)
#				h  = tag propogation from base (such propogation might be large)
#					1ST char after h is mark on verbs to propogate thru
#					2nd char is t or q (for tag or mark/queue)
#					3rd char (< >) after h is whether to propogate up from left/subject to object or down/right from object to subject when propogating
#
#		f = use given facts in from as items to process -- f@n  means use this set
#			This is followed by 
#			s/v/o/f	= use corresponding field of fact or entire fact
#		the value to process will be marked AND may or may not get stored, depending on following flag being q or m
#endif

	//   ZONE 1 - mark and queue setups
	--control;
	int baseOffset = 0; //   facts come from this side, and go out the verb or other side
	char* choice;
	char* at;
	int qMark = 0;
	int mark = 0;
	int whichset = 0;
	blockMeaning = false;
	WORDP D;

	char maxmark = '0';
	while (*++control && *control != ':' )
	{
		choice = NULL;
		switch(*control)
		{
			case '_': case '.': continue;	//   just separators to make things easier to read
			case '0': saveMark = 0; continue;
			case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': //   set current marks
				saveMark = baseMark + *control - '0';
				if (*control > maxmark) maxmark = *control;
				continue;
			case '~': //   use named set
					choice = control; 
					at = strchr(control,'.'); //   MUST end with _ or .
					if (!at) at = strchr(control,'_');
					if (!at) return 0;
					*at = 0;
					control = at;	//   skip past to end
					break;
			case '\'': //   use prespecified word
					choice = control+1; 
					at = strchr(control,'.'); //   MUST end with _ or .
					if (!at) at = strchr(control,'_');
					if (!at) return 0;
					*at = 0;
					control = at;	//   skip past to end
					break;
			case 'i': ++control;
				ignoremark = (*control == '0') ? 0 : (baseMark + (*control - '0'));
				break;
			case 's': choice = subjectword; mark = 0; break;
			case 'o': choice = objectword; mark = 2; break;
			case 'v': choice = verbword; mark = 1; break;
			case 'p': choice = propogate; break;
			case 'm': choice = match; break;
			case 'f':  //   we have incoming facts to use 
				++control;
				if (*control == '@') 
				{
					whichset = *++control - '0';
					++control;
				}
				else whichset = from;
				if (trace & INFER_TRACE)  Log(STDUSERLOG,"\r\n FactField: %c(%d) ",saveMark-baseMark+'0',saveMark);
				for (unsigned int j = 1; j <= (ulong_t)factSet[whichset][0]; ++j) 
				{
					FACT* F = factSet[whichset][j];
					MEANING T ;
					if (*control == 'f') //   whole fact can be queued. It cannot be marked as on the queue
					{
						queue[queueIndex++] = Fact2Index(F);
						continue;
					}
					else if (*control == 's') {T = F->subject ; mark = 0; }
					else if (*control == 'o') {T = F->object; mark = 2; }
					else if (*control == 'v') {T = F->verb; mark = 1; }
					else 
					{
						ReportBug("bad control for query %s",control);
						return 0;
					}
					//   we have a field to store, do so
					if (trace & INFER_TRACE)  Log(STDUSERLOG," %s ",WriteMeaning(T));
					AddFollow((control[1] == 'q') ? QUEUE : 0,T,0);
				}
				++control;
				continue;
			default: 
				ReportBug("Bad control code for query init %s", control);
				return 0;
		}
		if (choice) //   we have something to follow that didnt come from a fact
		{
			++control;
			unsigned int flags = 0;
			if (choice[0] == '\'')  //dont expand this
			{
				++choice;
				flags = int(ORIGINAL_ONLY);
			}
			// dynamic choices
			if (choice[0] == '_') 
			{
				int wild = atoi(choice+WILDINDEX); //   which wildcard
				if (flags != 0)
				{
					flags = 0;
					choice = wildcardOriginalText[wild];
				}
				else choice = wildcardCanonicalText[wild];
			}
			else if (choice[0] == '$') 
			{
				choice = GetUserVariable(choice);
			}
			if (*control == 'q') 
			{
				if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\n Tag&Queue: %s #%c(%d) ",choice,saveMark-baseMark+'0',saveMark);
				qMark = saveMark;	//   if we q more later, use this mark by default
				if (*choice) Follow(QUEUE|flags,choice); //   mark and queue items
				if (*choice == '~' ) blockMeaning = true;	//   do not use ontology on a set
			}
			else  if (*control == 't') 
			{
				if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\n Tag: %s #%c(%d) ",choice,saveMark-baseMark+'0',saveMark);
				if (!*choice);
				else if (*choice == '\'') AddFollow(flags, ReadMeaning(choice+1),0); // ignore unneeded quote
				else AddFollow(flags, ReadMeaning(choice),0);
			}
			else  if (*control == 'e') 
			{
				if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\n ExpandTag: %s #%c(%d) ",choice,saveMark-baseMark+'0',saveMark);
				if (*choice) Follow(flags,choice); //   tag but dont queue
			}
			else if (*control == '<' || *control == '>') //   chase hierarchy (exclude VERB hierarchy-- we infer on nouns)
			{ //   macroArgumentList are:  flowverbs,  queue or mark, 
				if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\n Hierarchy %c: %s #%c(%d) ",*control,choice,saveMark-baseMark+'0',saveMark);
				char kind = *control;
				int flows = baseMark + *++control - '0';
				int flag = (*++control == 'q') ? QUEUE : 0;
				if (flag & QUEUE) blockMeaning = true;
				if (*choice) ScanHierarchy(ReadMeaning(choice),saveMark,flows,kind == '<',flag, (mark != 1) ? NOUN : VERB);
			}
			else 
			{
				ReportBug("bad follow argument %s",control);
				return 0;
			}
			if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\n");
		}
	}
	inferMark += maxmark - '0';	//   update to use up marks we have involved

	//   ZONE 2 - how to use contents of the queue
	//   given items in queue, what field from a queued entry to use facts from 
	bool factType = false;
	while (*control && *++control && *control != ':' )
	{
		switch (*control)
		{
		case 's': baseOffset = 0; break;
		case 'v': baseOffset = 1; break;
		case 'o': baseOffset = 2; break;
		case 'f': factType = true; break; //   queued items are facts, not tomptrs
		case '_': case '.':  break;
		default:
			ReportBug("Bad control code for query test %s",control);
			return 0;
		}
	}

	whichset = store;

	//   ZONE 3 - how to detect facts we can return as answers and where they go
	//   set marks to compare on (test criteria for saving an answer)
	bool sentences = false;
	bool sentencev = false;
	bool sentenceo = false;
	unsigned int marks = 0,markv = 0,marko = 0;
	unsigned int markns = 0,marknv = 0,markno = 0;
	unsigned int rmarkv = 0,rmarks = 0, rmarko = 0;
	unsigned int intersectMark = 0, propogateVerb = 0;
	bool up = false;
	bool findTopic = false;
	saveMark = qMark;	//   default q value is what we used before
	while (*control && *++control && *control != ':' )
	{
		switch (*control)
		{
			//   do NOT match this
		case '!':
			++control;
			if (*control == 's')  markns = baseMark + (*++control - '0');
			if (*control == 'v')  marknv = baseMark + (*++control - '0');
			if (*control == 'o')  markno = baseMark + (*++control - '0');
			break;
		case 'x': // field after has setnence mark
			++control;
			if (*control == 's')  sentences = true; break;
			if (*control == 'v')  sentencev = true; break;
			if (*control == 'o')  sentenceo = true; break;
			break;

			//   normal tests of fact fields
		case 's': marks = baseMark + (*++control - '0'); break;
		case 'v': markv = baseMark + (*++control - '0'); break;
		case 'o': marko = baseMark + (*++control - '0'); break;
			
			//   dont pay attention to this value during search (opposite the baseOffset)
		case 'i': ++control;
			ignoremark = (*control == '0') ? 0 : (baseMark + (*control - '0'));
			break;
			//   future queuing uses this mark (hopefully same as original queued)
		case 'q': saveMark = baseMark + (*++control - '0');  break;
		case '~': 
			intersectMark = baseMark + (*++control - '0'); //   label to intersect to 
			break;
		case 't': findTopic = true; break;
		case '<': 		case '>': 
			propogateVerb = baseMark + (*++control - '0'); //   label of verbs to propogate on
			up = (*control == '<'); //   direction of propogation Up is true
			break;
		case '@': //   where to put answers (default is store)
			whichset = *++control - '0';
			break;
		case '_': case '.': break;
		case '^': upDictionary = true; break;	// rise up dominant dictionary meaning 
		default: 
			ReportBug("Bad control code for query test %s",control);
			return 0;
		}
	}

	//   ZONE 4- how to migrate around the graph and save new queue entries
	//   now examine riccochet OR other propogation controls (if any)
	//   May say to match another field, and when it matches store X on queue
	int riccochetOffset = 0,riccochetField = 0;
	bool factRiccochet = false;
	while(*control && *++control && *control != '|' )
	{
		switch (*control)
		{
		//   tests on riccochet fields
		case 'S': rmarks = baseMark + (*++control - '0'); break;
		case 'V': rmarkv = baseMark + (*++control - '0'); break;
		case 'O': rmarko = baseMark + (*++control - '0'); break;

		//   fields to access as next element from a NORMAL tomptr
		case  's': riccochetField = 0; break; //   MEANING offsets into a fact to get to subject,verb,object
		case  'v': Log(STDUSERLOG,"bad riccochet field"); return 0; 
		case  'o': riccochetField = 2 ; break;
		case 'f': 
			factRiccochet = true; break;
			//   which facts of that element to use
		case  '<': 
			riccochetOffset= 0; 
			break;
		case  '>': 
			riccochetOffset = 2; 
			break;
		case '_':  break;
		default: ReportBug("Bad control code for query test");
			return 0;
		}
	}
	//   now perform the query
	FACT* F;
	unsigned int  scanIndex = 0;
	unsigned int pStart,pEnd;
	while (scanIndex < queueIndex)
	{
		MEANING next = queue[scanIndex++];
        unsigned int index;

		//   get node which has fact list on it
		if (factType)
		{
			F = GetFactPtr(next);
			if (baseOffset == 0) F = GetSubjectHead(F);
			else if (baseOffset ==1 ) F = GetVerbHead(F);
			else  F = GetObjectHead(F);
			index = 0;
		}
		else
		{
			WORDP X = Meaning2Word((ulong_t)next);
			if (baseOffset == 0) F = GetSubjectHead(X);
			else if (baseOffset ==1 ) F = GetVerbHead(X);
			else  F = GetObjectHead(X);
			index = Meaning2Index(next);
		}
		bool once = false;
		while (F)
		{
			if (trace & INFER_TRACE) TraceFact(F);

			//   prepare for next fact to walk
			FACT* G = F;
			MEANING INCOMING;
			MEANING OUTGOING;
			if (baseOffset == 0) 
			{
				F = GetSubjectNext(F);
				INCOMING = G->subject;
				OUTGOING = G->object;
			}
			else if (baseOffset == 1)  
			{
				F = GetVerbNext(F);
				INCOMING = G->verb;
				OUTGOING = G->object;
			}
			else 
			{
				F = GetObjectNext(F);
				INCOMING = G->object;
				OUTGOING = G->subject;
			}
			//   is this fact based on what we were checking for? (we store all specific instances on the general dictionary)
			if (index &&  INCOMING != next) continue;
			WORDP OTHER = Meaning2Word(OUTGOING);

			WORDP S = Meaning2Word(G->subject);
			WORDP V = Meaning2Word(G->verb);
			WORDP O = Meaning2Word(G->object);

			//   if this is part of ignore set, ignore it (not good if came via verb BUG)
			if (ignoremark && OTHER->stamp.inferMark == ignoremark ) 
			{
				if (trace & INFER_TRACE) Log(STDUSERLOG,"i");
				continue;
			}

			bool match = true;

			// follow dictionary path?
			if (upDictionary && G->properties & FACT_IS && !once)
			{
				once = true;
				if (AddFollow(QUEUE,OUTGOING,INCOMING)); // add object onto queue
				continue;
			}

			//   field validation fails on some field?
			if (marks && S->stamp.inferMark != marks)  match = false;
			else if (markns && S->stamp.inferMark == marks)  match = false;
			if (markv && V->stamp.inferMark != markv) match = false;
			else if (marknv && V->stamp.inferMark == markv)  match = false;
			if (marko && O->stamp.inferMark != marko)  match = false;
			else if (markno && O->stamp.inferMark == marko)  match = false;
			if (sentences && !GetNextSpot(S,0,pStart,pEnd)) match = false;
			if (sentencev && !GetNextSpot(V,0,pStart,pEnd)) match = false;
			if (sentenceo && !GetNextSpot(O,0,pStart,pEnd)) match = false;

			//   if search is riccochet, we now walk facts of riccochet target
			if (match && rmarkv )
			{
				WORDP D1;
				if (!factRiccochet)
				{
					D1 = Meaning2Word((riccochetField == 0)  ? G->subject : G->object);
				}
				else D1 = (WORDP) G;
				FACT* H  = ((riccochetOffset == 0)  ? GetSubjectHead(D1)  : GetObjectHead(D1) );
				while (H)
				{
					if (trace & INFER_TRACE) TraceFact(H);
					FACT* I = H;
					H = ((riccochetField == 0)  ? GetSubjectNext(H)  : GetObjectNext(H) );
					if (rmarkv)
					{
						D = Meaning2Word(I->verb);
						if (D->stamp.inferMark != rmarkv) continue;
					}
					if (rmarks)
					{
						D = Meaning2Word(I->subject);
						if (D->stamp.inferMark != rmarks) continue;
					}
					if (rmarko)
					{
						D = Meaning2Word(I->object);
						if (D->stamp.inferMark != rmarko) continue;
					}
					if (!(I->properties & MARKED_FACT)) //   find unique fact (no repeats)
					{
						I->properties |= MARKED_FACT;
						AddFoundFact(I,whichset); 
						if (foundIndex >= count) 
						{
							scanIndex = queueIndex; //   end outer loop 
							F = NULL;
							break;
						}
					}
				}
			}
			else if (match && !(G->properties & MARKED_FACT)) //   find unique fact -- it was not rejected by anything
			{
				G->properties |= MARKED_FACT;
				AddFoundFact(G,whichset);
				if (foundIndex >= count) 
				{
					scanIndex = queueIndex; //   end outer loop 
					F = NULL;
					break;
				}
			}

			//   if propogation is enabled, queue appropriate choices
			if (!match && propogateVerb && V->stamp.inferMark == propogateVerb) //   is this is not a fact to check, this is a fact to propogate on
			{
				if (findTopic && OTHER->systemFlags & TOPIC_NAME && !(G->properties & MARKED_FACT)) // supposed to find a topic
				{
					G->properties |= MARKED_FACT;
					AddFoundFact(G,whichset); 
					if (foundIndex >= count) 
					{
						scanIndex = queueIndex; //   end outer loop 
						F = NULL;
						break;
					}
				}
				else if (AddFollow(QUEUE,OUTGOING,INCOMING)); // add object onto queue
			}
		}
	}
	
	//   clear marks for duplicates
	for (unsigned int i = 1; i <= foundIndex; ++i) factSet[whichset][i]->properties &= -1 ^ MARKED_FACT;
	factSet[whichset][0] = (FACT*)foundIndex;
    if (trace & INFER_TRACE) 
	{
		char word[MAX_WORD_SIZE];
		if (foundIndex) Log(STDUSERLOG," result: @%d[%d] e.g. %s\r\n",whichset,foundIndex,WriteFact(factSet[whichset][1],false,word));
		else Log(STDUSERLOG," result: @%d none found \r\n",whichset);
	}

	if (*control++ == '|') goto nextsearch; //   chained search, do the next

	return foundIndex;
}

// used by query setup - scans noun hierarchies for inference
void ScanHierarchy(MEANING T,int savemark,unsigned int flowmark,bool up,int flag, unsigned int type)
{
	if (!T) return;
	if (!AddFollow(flag,T,0)) return;

	int baseOffset = (up) ? 0 : 2;		//   up uses subject field and verb to match.  down uses object field and verb to match
	tomParentIndex = tomParentWalk  = 0;
	tomParents[tomParentIndex++] = T;

	WORDP A = Meaning2Word(T);

	if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\nHierarchy: %s ",WriteMeaning(T));
	int index = Meaning2Index(T);
	int start = 1;

	//   find its wordnet ontological meanings, they then link synset head to synset head
	//   automatically store matching synset heads to this also -- THIS APPLIES ONLY TO THE ORIGINAL WORDP
	MEANING* onto = GetMeaningsFromMeaning(T);
	unsigned int size = A->meaningCount;
	if (!up || blockMeaning ) size = 0;//   we are a specific meaning already, so are the synset head of something - or are going down
	else if (index) start=size = index;	//   do JUST this one
	else if (!size) size = 1; //   even if no ontology, do it once for 
	for  (unsigned int k = start; k <= size; ++k) // for each meaning of this word, mark its synset heads
	{
		MEANING T1;
		if (A->meaningCount) 
		{
			T1 = (MEANING)(ulong_t)onto[k]; //   this is the synset ptr.
			MEANING next = GetMaster(T1); //   wordnet uses synset head for its hierarchy facts
			WORDP Q = Meaning2Word(next);
			if (!(Q->properties & type)) continue;	
			if (! AddFollow(flag,next,T)) continue; //   either already marked OR to be ignored
			tomParents[tomParentIndex++] = next;	
		}
	}

	while (tomParentWalk < tomParentIndex) //   walk up its chains in stages
	{
		T = tomParents[tomParentWalk++];
		if (!T) continue;
		if (tomParentIndex > MAX_PARENTS) break;	//   overflow may happen. give up
		WORDP D = Meaning2Word(T);
		unsigned int index = Meaning2Index(T);
		if (D->properties & WORDNET_ID) // need to consult words of the synset to see if THEY are members of concepts
		{
			unsigned int limit = D->meaningCount;
			MEANING* onto = GetMeaningsFromMeaning(T);
			for ( unsigned int i = 1; i <= limit; ++i)
			{
				MEANING M = onto[i]; // a word member of the synset
				WORDP SYN = Meaning2Word(M);
				FACT* F = GetSubjectHead(SYN);
				unsigned int index = Meaning2Index(M); // we are this definition of the word
				while (F)
				{
					//if (trace & INFER_TRACE) TraceFact(F);
					FACT* G = F;
					F = GetSubjectNext(F);
					if (!(G->properties & FACT_MEMBER)) continue; // is relation takes us back to synset head, dont care

					//   if the incoming ptr is generic, it can follow out any generic or pos_generic reference.
					//   It cannot follow out a specific reference of a particular meaning.
					//   An incoming non-generic ptr is always specific (never pos_generic) and can only match specific exact.
					unsigned int index = Meaning2Index(G->subject);
					if (index && M !=  G->subject) continue; // generic can run all meanings out of here
					if (G->subject & TYPE_RESTRICTION && !(G->subject & type)) continue;	// only allowed to chase nouns
					MEANING x = G->object; 
					WORDP A = Meaning2Word(x); // for debugging
					if (!AddFollow(flag,x,G->subject)) continue;	//   either already marked OR to be ignored -- he can be marked
					tomParents[tomParentIndex++] = x;	// he can be followed
				}
			}
		}
		// now follow facts of the synset head itself or the word itself.
		FACT* F = GetSubjectHead(D);
		while (F)
		{
			WORDP verb = Meaning2Word(F->verb); 
			FACT* G = F;
			//if (trace & INFER_TRACE) TraceFact(F);
			F = GetSubjectNext(F);
			if (verb->stamp.inferMark !=  flowmark ) continue;

			//   if the incoming ptr is generic, it can follow out any generic or pos_generic reference.
			//   It cannot follow out a specific reference of a particular meaning.
			//   An incoming non-generic ptr is always specific (never pos_generic) and can only match specific exact.
			if (index && T !=  G->subject) continue; //   generic can run all meanings out of here
			if (G->subject & TYPE_RESTRICTION && !(G->subject & type)) continue;	// only allowed to chase nouns
			MEANING x = G->object; 
			WORDP A = Meaning2Word(x); 
			if (!AddFollow(flag,x,G->subject)) continue;	//   either already marked OR to be ignored
			tomParents[tomParentIndex++] = x;
		}
	}

	if (trace & INFER_TRACE) Log(STDUSERLOG,"\r\n");
}

