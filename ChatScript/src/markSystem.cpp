// markSystem.cpp - annotates the dictionary with what words/concepts are active in the current sentence


#include "common.h"

#ifdef INFORMATION

Adjectives occur before nouns EXCEPT:
	1. object complement (with some special verbs)
	2. adjective participle (sometimes before and sometimes after)

In a pattern, an author can request:
	1. a simple word like bottle
	2. a form of a simple word non-canonicalized like bottled or apostrophe bottle
	3. a WordNet concept like bottle~1 
	4. a set like ~dead or :dead

For #1 "bottle", the system should chase all upward all sets of the word itself, and all
WordNet parents of the synset it belongs to and all sets those are in. 

Marking should be done for the original and canonical forms of the word.

For #2 "bottled", the system should only chase the original form.

For #3 "bottle~1", this means all words BELOW this in the wordnet hierarchy not including the word
"bottle" itself. This, in turn, means all words below the particular synset head it corresponds to
and so instead becomes a reference to the synset head: "0173335n" or some such.

For #4 "~dead", this means all words encompassed by the set ~dead, not including the word ~dead.

So each word in an input sentence is scanned for marking. 
the actual word gets to see what sets it is in directly. 
Thereafter the system chases up the synset hierarchy fanning out to sets marked from synset nodes.

#endif

bool strict = true;

WORDP wordFormat[MAX_SENTENCE_LENGTH];
char* wordCanonical[MAX_SENTENCE_LENGTH]; //   current sentence tokenization
static unsigned int markLength = 0; // prevent long lines in mark listing trace
WORDP originalLower[MAX_SENTENCE_LENGTH];
static WORDP originalUpper[MAX_SENTENCE_LENGTH];
uint64 originalFlags[MAX_SENTENCE_LENGTH];
unsigned int roles[MAX_SENTENCE_LENGTH];
Bit64Map whereInSentence;

bool posTrace = false;

WORDP canonicalLower[MAX_SENTENCE_LENGTH];
WORDP canonicalUpper[MAX_SENTENCE_LENGTH];

#define SETLIMIT 500
char* topicsInSentence[MAX_TOPICS_IN_SENTENCE+1];
unsigned int topicInSentenceIndex;
unsigned int matchStamp = 1; //   first real is 2. not collide with 1 used for uservar list

//   NOTE member never has NOT, are always only TRUE
//   note meaning indices only matter at start. Thereafter we walk up sets which have no meaning
//   or wordnet synsets which already account for meaning

void ClearWhereInSentence()
{
	// clear any whereInSentence bits
    map< int, uint64 >::const_iterator it;
	unsigned int limit = dictionaryFree - dictionaryBase;
    for ( it = whereInSentence.begin(); it != whereInSentence.end(); ++it )
    {
		unsigned int index = (unsigned int) ((*it).first);
		if (index < limit)
		{
			WORDP D = dictionaryBase + index;
			D->systemFlags &= -1LL ^ IN_SENTENCE;
		}
	}
	whereInSentence.clear();
}

void SetWhereInSentence(WORDP D,uint64 bits)
{
	D->systemFlags |= IN_SENTENCE;
	whereInSentence[D-dictionaryBase] = bits;
}

uint64 GetWhereInSentence(WORDP D)
{
	return (D->systemFlags & IN_SENTENCE) ? whereInSentence[D-dictionaryBase] : 0;
}

void MarkWordHit(WORDP D, unsigned int start,unsigned int end)
{ //   keep closest to start at bottom, when run out, drop later ones 
    //   field is 64 bit. hold 7  x 9bit  -- currently a phrase can be 1-5 words and we ignore words past 63 for matching
    if (!D) return;
    if (start > PHRASE_MAX_START_OFFSET) return; //   leave room for bits on top -- using 6 bits for start and 4 bits for width
	if (!D->properties && !(D->systemFlags & PATTERN_WORD) && D->word[0] != '~' && D->word[0] != '%') return;
    if (end > wordCount) end = wordCount;    
	if (start > wordCount) 
	{
		ReportBug("save position is too big");
		return;
	}

    int diff = end - start;
	// diff < 0 means peering INSIDE a multiword token before last word
	// we label END as the word before it (so we can still see next word) and START as the actual multiword token
    if (diff < 0) diff = PHRASE_END_MAX; 
    else if (diff >= PHRASE_END_MAX) return;  //   refuse things too wide
 
    int realEnd = (diff == PHRASE_END_MAX) ? (start-1) : (start + diff); 
    if (realEnd > (int)wordCount) 
    {
        ReportBug("Position out of range as set");
        return;
    }

	uint64 mask = start | (diff << PHRASE_END_OFFSET);  // this unit looks like this
    uint64 val;
    if (D->v.patternStamp != matchStamp) //   accidently match means a bogus reference, dont care
    {
        val = 0;   
        D->v.patternStamp = matchStamp;
    }
	else val = GetWhereInSentence(D);

	//   see if data on this position is already entered
    int shift = 0;
    uint64 has;
	bool added = true;
    while (shift < PHRASE_STORAGE_BITS && (has = ((val >> shift) & PHRASE_FIELD )) != 0) 
    {
		unsigned int began = has & PHRASE_START; // his field starts here
		if ( began > start) // we need to go before him -- sorted matches
		{
			if ( val & PHRASE_LAST_FIELD) return;	// we cant insert it
			added = true;
			uint64 beforeMask = 0;
			for (unsigned int i = shift; i > 0; --i) beforeMask |= 0x01ULL << (i-1); // create mask for before
			uint64 before = val & beforeMask;
			before |= mask << shift;	// add in correct piece
			uint64 after = val & (-1 ^ beforeMask);	// all the others
			before |= after << PHRASE_BITS;
			SetWhereInSentence(D,before);
			shift = PHRASE_STORAGE_BITS + 1; // dont try to add later
			break;
		}

        if (has == mask) return; 
        shift += PHRASE_BITS;  // 9 bits per phrase: 
    }

	//   shift is left spot open slot exists (if one does)
    if (shift <= (PHRASE_STORAGE_BITS - PHRASE_BITS) )
    {
		added = true;
        SetWhereInSentence(D,val | (mask << shift));
		// trace an instance of a topic in this sentence
        if (!shift && D->word[0] == '~' && topicInSentenceIndex < MAX_TOPICS_IN_SENTENCE) topicsInSentence[topicInSentenceIndex++] = D->word; //   topics covered by this sentence - one instance per sentence
	}
	if (trace & PREPARE_TRACE && added)  
	{
		markLength += strlen(D->word);
		if (markLength > 80)
		{
			markLength = 0;
			Log(STDUSERLOG,"\r\n");
			Log(STDUSERTABLOG,"");
		}
		if (D->properties & WORDNET_ID) 
			Log(STDUSERLOG," %s-%s ",D->word,WriteMeaning(D->meanings[1]));
		else Log(STDUSERLOG," %s ",D->word);
		if (start != end) Log(STDUSERLOG,"(%d-%d)",start,end);
	}
}

unsigned int GetIthSpot(WORDP D,int i)
{
    if (!D || D->v.patternStamp != matchStamp) return 0; //   not in sentence
	uint64 spot = GetWhereInSentence(D);

	unsigned int start = (unsigned int) (spot & PHRASE_START);
	while (start && --i) //   none left or not the ith choice
	{
		spot >>= PHRASE_BITS;	//   look at next slot
		start = (unsigned int) (spot & PHRASE_START);
	}
	if (!start) return 0;	//   failed to find any matching entry
    if (start > wordCount) //   bad cache- timestamp rollover?
    {
        ReportBug("Position out of range ith1");
        return 0;
    }
 
    //   match found
    positionStart =  positionEnd = start;

	//   check to see if its a range value -
	unsigned int offset = (unsigned int)((spot >> PHRASE_END_OFFSET) & PHRASE_END_MAX); // end offset from start
    if (offset == PHRASE_END_MAX) --positionEnd; // we mark BEFORE the composite word
    else positionEnd += offset;
    if (positionEnd > (int)wordCount) ReportBug("Position out of range ith2");
    return positionStart;
}

unsigned int GetNextSpot(WORDP D,int start,unsigned int &positionStart,unsigned int& positionEnd)
{//   spot can be 1-31,  range can be 0-7 -- 7 means its a string, set last marker back before start so can rescan
	//   BUG - we should note if match is literal or canonical, so can handle that easily during match eg
	//   '~shapes matches square but not squares (whereas currently literal fails because it is not ~shapes
    if (!D || D->v.patternStamp != matchStamp) return 0; //   not in sentence
	uint64 spot = GetWhereInSentence(D);

	int value = (spot & PHRASE_START); // start of the match (1st choice)
	while (value && value <= start) //   no match, shift positions
	{
		spot >>= PHRASE_BITS;	//   look at next slot
		value = spot & PHRASE_START;
	}
	if (!value) return 0;	//   failed to find any matching entry
    if (value > (int)wordCount) //   bad cache- timestamp rollover?
    {
        ReportBug("Position out of range1");
        return 0;
    }
 
    //   match found
    positionStart =  positionEnd = (int) value;

	//   check to see if its a range value -
	// NORMAL phrases have a length offset past start.
	// A phrase which is MAX length is special and marks itself as ending BEFORE it starts, so it ca
	value = ((spot >> PHRASE_END_OFFSET) & PHRASE_END_MAX);
    if (value == PHRASE_END_MAX) --positionEnd;
    else positionEnd += (int) value;
    if (positionEnd > (int)wordCount) 
    {
        ReportBug("Position out of range");
        positionEnd = wordCount;
    }
    return positionStart;
}

unsigned int GetNthNextSpot(WORDP D,unsigned int start)
{//   spot can be 1-31,  range can be 0-7 -- 7 means its a string, set last marker back before start so can rescan
    if (!D || D->v.patternStamp != matchStamp) return 0; //   not in sentence
	uint64 spot = GetWhereInSentence(D);

	unsigned int value = (unsigned int) (spot & PHRASE_START);
	while (value && (value <= start)) //   no match, shift positions
	{
		spot >>= PHRASE_BITS;	//   look at next slot
		value = (unsigned int) (spot & PHRASE_START);
	}
	if (!value) return 0;	//   failed to find any matching entry
    if (value > wordCount) //   bad cache- timestamp rollover?
    {
        ReportBug("Position out of range original2");
        return 0;
    }

    //   match found
    positionStart =  positionEnd = value;

	//   check to see if its a range value -
	value = (unsigned int)((spot >> PHRASE_END_OFFSET) & PHRASE_END_MAX);
    if (value == PHRASE_END_MAX) --positionEnd;
    else positionEnd += value;
    if (positionEnd > (int)wordCount) 
    {
        ReportBug("Position out of range ended");
        positionEnd = wordCount;
    }
    return positionStart;
}

static bool MarkSetPath(MEANING T, unsigned int start, unsigned  int end, unsigned int depth) //   walks set hierarchy
{//   travels up concept/class sets, though might start out on a synset node or a regular word
	unsigned int flags = T & ESSENTIAL_FLAGS;
	if (!flags) flags = ESSENTIAL_FLAGS;
	WORDP D = Meaning2Word(T);
	if (!(D->properties & WORDNET_ID)) MarkWordHit(D,start,end); //   if we want the synset marked, RiseUp will do it.

	unsigned int index = Meaning2Index(T); //   always 0 for a synset or set, 
	
	//   check for any repeated accesses of this synset or set or word
	uint64 offset = 1 << index;
 	if (D->stamp.inferMark == inferMark) //   been thru this word recently
	{
		if (D->tried & offset)	return true;	//   done this branch already
	}
	else //   first time accessing, note recency and clear tried bits
	{
		D->stamp.inferMark = inferMark;
		D->tried = 0;
	}
 	D->tried |= offset;

	FACT* F = GetSubjectHead(D); 
	while (F)
	{
		//TraceFact(F);
		if (F->properties & FACT_MEMBER) //   ~concept facts and refinement
		{
			char word[MAX_WORD_SIZE];
			char* fact;
			if (trace == HIERARCHY_TRACE)  
			{
				fact = WriteFact(F,false,word); // just so we can see it
				unsigned int hold = globalDepth;
				globalDepth = depth;
				Log(STDUSERTABLOG,"%s\r\n",fact);
				globalDepth = hold;
			}
			unsigned int restrict = F->subject & TYPE_RESTRICTION;
			if (restrict) // type restriction in effect for this concept member
			{
				if (!( restrict & flags )) // not allowed
				{
					F = GetSubjectNext(F);
					continue;
				}
			}

			//   index meaning restriction (0 means all)
			if (index == Meaning2Index(F->subject)) //   must match generic or specific precisely
			{
				MarkSetPath(F->object,start,end,depth+1);
			}
		}
		F = GetSubjectNext(F);
	}
	return false;
}

static void RiseUp(MEANING T,unsigned int start, unsigned int end,unsigned int depth) //   walk wordnet hierarchy above a synset node
{//   T is always a synset head (index 0)
	//   mark this synset head (if appropriate)
	WORDP D = Meaning2Word(T);
	if (trace == HIERARCHY_TRACE) 
	{
		unsigned int hold = globalDepth;
		globalDepth = depth;
		WORDP E = D;
		if ( E->properties & WORDNET_ID) E = Meaning2Word(D->meanings[1]);
		Log(STDUSERTABLOG,"%s^\r\n",E->word);
		globalDepth = hold;
	}

	//   mark only if user might want it (he is presumed to want all sets and all words, but not synset nodes)
	if (!(D->properties & WORDNET_ID) || D->systemFlags & PATTERN_WORD) 
	{
		MarkWordHit(D,start,end); 
	}

	//   mark all sets it participates in - but if we've done this synset head already, abort further fanout
	if (MarkSetPath(T,start,end,depth)) return;	//   set memberships of this synset head, all the way to top of those sets

	//   now go to next levels of wordnet synset hierarchy
	FACT* F = GetSubjectHead(D); 
	while (F)
	{
		if (F->properties & FACT_IS) RiseUp(F->object,start,end,depth+1); 
		F = GetSubjectNext(F);
	}
}

void MarkFacts(MEANING M,unsigned int start, unsigned int end) //   show what matches up
{ //   T is always a word from a sentence

    if (!M) return;
	WORDP D = Meaning2Word(M);
	MarkSetPath(M,start,end,0);	//   generic membership of this word all the way to top

	//   now follow out the allowed synset hierarchies
	unsigned int size = D->meaningCount;
	for  (unsigned int k = 1; k <= size; ++k) 
	{
		uint64 flags = M & ESSENTIAL_FLAGS;
		if (!flags) flags = NOUN|VERB|ADJECTIVE|ADVERB|PREPOSITION; // unmarked ptrs can rise all
		MEANING T = D->meanings[k]; // it is a flagged meaning unless it self points
		if (T & flags) RiseUp(T,start,end,0); // allowed meanings (self ptrs need not rise up)
	}
}

 //   if membership is restricted by meaning or part of speech this enforces it
 //   if (!AllowedMember(F,i,0,index)) continue; //   restricted to a pos that isnt available in parse

static void SetSequenceStamp() //   mark words in sequence, original and canonical (but not mixed)
{
    //   mark only if we find it in dictionary (someone might be looking for it)
    char buffer[MAX_SENTENCE_BYTES];
    char buffer1[MAX_SENTENCE_BYTES];
	bool started = false;
	
	//   consider all sets of up to 5-in-a-row
	int limit = ((int)wordCount) - 1;
	for (int i = 1; i <= limit; ++i)
	{
		//   set base phrase
		buffer[0] = 0;
		buffer1[0] = 0;
		strcat(buffer,wordStarts[i]);
		strcat(buffer1,wordCanonical[i]);
       
		//   fan out for addon pieces, mark only ones we recognize as already in dictionary
		unsigned int k = 0;
		int index = 0;
		while ((++k + i) <= wordCount)
		{
			strcat(buffer,"_");
			strcat(buffer1,"_");
			strcat(buffer,wordStarts[i+k]);
			strcat(buffer1,wordCanonical[i+k]);
			NextinferMark();

			// special hack for infinitive phrases: to swim  etc
			if (k == 1 && buffer[2] == '_' && buffer[0] == 't' && buffer[1] == 'o')
			{
				WORDP D = FindWord(wordCanonical[i+1],0,LOWERCASE_LOOKUP);
				if (D && D->properties & VERB)
				{
					MarkFacts(MakeMeaning(FindWord("~noun_infinitive")),i,i+k);
				}
			}

			//   for now, accept both upper and lower case forms of the decomposed words for matching
			WORDP D = FindWord(buffer,0,PRIMARY_CASE_ALLOWED); 
			if (D) 
			{
				if (trace & PREPARE_TRACE) 
				{
					if (!started) Log(STDUSERLOG,")\r\n    sequences=( "); 
					Log(STDUSERLOG,")\r\n");
					started = true;
					Log(STDUSERLOG,"%s (%d-%d) ",D->word,i,i+k);
				}
				MarkFacts(MakeMeaning(D),i,i+k);
			}
			D = FindWord(buffer,0,SECONDARY_CASE_ALLOWED);
			if (D) 
			{
				if (trace & PREPARE_TRACE) 
				{
					if (!started) Log(STDUSERLOG,")\r\n    sequences=( "); 
					Log(STDUSERLOG,")\r\n");
					started = true;
					Log(STDUSERLOG,"%s (%d-%d) ",D->word,i,i+k);
				}
				MarkFacts(MakeMeaning(D),i,i+k);
			}
			D = FindWord(buffer1,0,PRIMARY_CASE_ALLOWED); 
			if (D) 
			{
				if (trace & PREPARE_TRACE)
				{
					if (!started) Log(STDUSERLOG,")\r\n    sequences=( "); 
					Log(STDUSERLOG,")\r\n");
					started = true;
					Log(STDUSERLOG,"%s (%d-%d) ",D->word,i,i+k);
				}
				MarkFacts(MakeMeaning(D),i,i+k);
			}
			D = FindWord(buffer1,0,SECONDARY_CASE_ALLOWED);
			if (D) 
			{
				if (trace & PREPARE_TRACE) 
				{
					if (!started) Log(STDUSERLOG,")\r\n    sequences=( "); 
					Log(STDUSERLOG,")\r\n");
					started = true;
					Log(STDUSERTABLOG,"%s (%d-%d) ",D->word,i,i+k);
				}
				MarkFacts(MakeMeaning(D),i,i+k);
			}
			if (++index > 5) break; //   up to 5 in a row
		}
	}
	if (trace & PREPARE_TRACE && started) Log(STDUSERLOG,")\r\n ");
}

void NextMatchStamp() //   used as timestamp on an entire sentence
{
    ++matchStamp;   
}

static void PrimaryMark(MEANING M, unsigned int start, unsigned int end) 
{
	if (!M) return;
	MarkFacts(M,start,end);		//   the basic word
          
	//   check ORIGINAL word for proper names, because canonical may mess it up, like james -->  jam
	WORDP D = Meaning2Word(M);
	if (D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL) && strlen(D->word) >= 3) 
	{
		MarkFacts(MakeMeaning(Dpropername),start,end);
		if (D->systemFlags & SPACEWORD)  MarkFacts(MakeMeaning(Dspacename),start,end);
		if (D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL) && D->properties & NOUN_HUMAN)  MarkFacts(MakeMeaning(Dhumanname),start,end);
		if (D->properties & NOUN_HUMAN && D->properties & NOUN_HE)  MarkFacts(MakeMeaning(Dmalename),start,end);
		if ( D->properties & NOUN_HUMAN && D->properties & NOUN_SHE)  MarkFacts(MakeMeaning(Dfemalename),start,end);
		if ( D->properties & NOUN_FIRSTNAME)  MarkFacts(MakeMeaning(Dfirstname),start,end);
		
	}

	if (D->systemFlags & SPACEWORD && !(D->properties & PREPOSITION)) MarkFacts(MakeMeaning(Dlocation),start,end);
	if (D->systemFlags & TIMEWORD && !(D->properties & PREPOSITION)) MarkFacts(MakeMeaning(Dtime),start,end);
}

static void CanonicalMark(WORDP D, unsigned int start, unsigned int end) 
{
	if (!D || !(D->properties & PART_OF_SPEECH)) return; //   has no meaning

	uint64 age =  (D->systemFlags & AGE_LEARNED);
	if (age  & (KINDERGARTEN|GRADE1_2)) MarkFacts(MakeMeaning(Dchild),start,end);
	else if (D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL | ADJECTIVE_ORDINAL | ADJECTIVE_CARDINAL | NOUN_CARDINAL | NOUN_ORDINAL) || D->word[1] == 0 ); 
	else if (age & (GRADE3_4|GRADE5_6)) MarkFacts(MakeMeaning(Dchild),start,end);
}
	
static void SetCanonical()
{
	bool tense = false;

	// now set canonical lowercase forms
	for (unsigned int i = 1; i <= wordCount; ++i)
    {
		char* original =  wordStarts[i];
		if (originalLower[i]) original = originalLower[i]->word;
		uint64 pos = values[i] & (TAG_TEST|PART_OF_SPEECH);
		if (!pos && !(original[0] == '~')) values[i] = pos = NOUN;		// default it back to something
		WORDP D = FindWord(original);
		char* canon =  GetCanonical(D);
		if (canon) canonicalLower[i] = FindWord(canon);
		else if (pos & NUMBER_BITS); // must occur before verbs and nouns, since "second" is a verb and a noun
		else if (pos & (VERB_TENSES | NOUN_GERUND | NOUN_INFINITIVE|ADJECTIVE_PARTICIPLE) ) 
		{
			canonicalLower[i] = FindWord(GetInfinitive(original));
		}
		else if (pos & (NOUN_BITS - NOUN_GERUND) ) 
		{
			if (canonicalLower[i] && canonicalLower[i]->properties & NUMBER_BITS);
			else if (IsAlpha(original[0]) &&  canonicalLower[i] && !strcmp(canonicalLower[i]->word,"unknown-word"));	// keep unknown-ness
			else canonicalLower[i] = FindWord(GetSingularNoun(original));
		}
		else if (pos & (ADJECTIVE_BITS - ADJECTIVE_PARTICIPLE)) 
		{
			if (canonicalLower[i] && canonicalLower[i]->properties & NUMBER_BITS);
			else canonicalLower[i] = FindWord(GetAdjectiveBase(original));

			// for adjectives that are verbs, like married, go canonical to the verb if adjective is unchanged
			if (canonicalLower[i] && !strcmp(canonicalLower[i]->word,original))
			{
				char* infinitive = GetInfinitive(original);
				if (infinitive) canonicalLower[i] = FindWord(infinitive);
			}
		}
		else if (pos & ADVERB_BITS) 
		{
			if (canonicalLower[i] && canonicalLower[i]->properties & NUMBER_BITS);
			else canonicalLower[i] = FindWord(GetAdverbBase(original));
			// for adverbs that are adjectives, like faster, go canonical to the adjective if adverb is unchanged
			if (canonicalLower[i] && !strcmp(canonicalLower[i]->word,original))
			{
				char* adjective = GetAdjectiveBase(original);
				if (adjective) canonicalLower[i] = FindWord(adjective);
			}
		}
		else if (pos & (PRONOUN|CONJUNCTION_BITS|PREPOSITION|DETERMINER|PUNCTUATION|COMMA|PAREN)) canonicalLower[i] = FindWord(original);
		else if (original[0] == '~') canonicalLower[i] = FindWord(original);
		else if (!IsAlpha(original[0])) canonicalLower[i] = FindWord(original);
		
		if (canonicalLower[i]) wordCanonical[i] = canonicalLower[i]->word;
		else if (canonicalUpper[i]) wordCanonical[i] = canonicalUpper[i]->word;
		else wordCanonical[i] = wordStarts[i];

		// determine sentence tense
		if (tense);
		else if ( pos & AUX_VERB_TENSES && originalLower[i])
		{
			if (originalLower[i]->properties & AUX_VERB_FUTURE) tokenFlags |= FUTURE;
			else if (originalLower[i]->properties & AUX_VERB_PAST) tokenFlags |= PAST;
			tense = true;
		}
		else if ( roles[i] & MAINVERB && values[i] & VERB_PAST)
		{
			tokenFlags |= PAST;
			tense = true;
		}
	}
}

uint64 GetPosData(char* original,WORDP &entry,WORDP &canonical,bool firstTry,uint64 bits) // case sensitive, may add word to dictionary, will not augment flags of existing words
{
	uint64 flags = 0;
	entry = FindWord(original,0,PRIMARY_CASE_ALLOWED);

	// mark numeric fractions
	char* fraction = strchr(original,'/');
	if ( fraction && IsDigit(fraction[1]))
	{
		char number[MAX_WORD_SIZE];
		strcpy(number,original);
		number[fraction-original] = 0;
		if (IsNumber(number) && IsNumber(fraction+1))
		{
			int x = atoi(number);
			int y = atoi(fraction+1);
			float val = (float)((float)x / (float)y);
			sprintf(number,"%2.2f",val);
			flags = ADJECTIVE|NOUN|ADJECTIVE_CARDINAL|NOUN_CARDINAL|NOUN_ORDINAL;
			if (!entry) entry = StoreWord(original,flags);
			canonical = FindWord(number,0,PRIMARY_CASE_ALLOWED);
			if (canonical) flags |= canonical->properties;
			else StoreWord(number,flags);
			return flags;
		}
	}
	else if (IsNumber(original))
	{
		char number[MAX_WORD_SIZE];
		uint64 baseflags = (entry) ? entry->properties : 0;
		if (IsPlaceNumber(original))
		{
			sprintf(number,"%d",Convert2Integer(original));
			flags = ADJECTIVE|ADJECTIVE_ORDINAL|NOUN|NOUN_ORDINAL| (baseflags & TAG_TEST);
		}
		else if (original[0] == '$') // money
		{
			int n = Convert2Integer(original+1);
			float fn = (float)atof(original+1);
			if ((float)n == fn) sprintf(number,"%d",n);
			else sprintf(number,"%2.2f",fn);
			flags = NOUN|NOUN_CARDINAL;
		}
		else
		{
			if (strchr(original,'.')) sprintf(number,"%2.2f",atof(original));
			else 
			{
				int val = Convert2Integer(original);
				uint64 val64;
				ReadInt64(original,val64);
				char valid[MAX_WORD_SIZE];
				sprintf(valid,"%I64d",val64);	
				if (!stricmp(valid,original) && val64 != (uint64) val ) // can be 64bit represented
				{
					strcpy(number,valid);  // maintain full value
				}
				else sprintf(number,"%d",val);
			}
			flags = ADJECTIVE|NOUN|ADJECTIVE_CARDINAL|NOUN_CARDINAL;
		}
		entry = StoreWord(original,0);
		canonical = StoreWord(number,flags);
	//	if (!entry) entry = canonical; // unknown word, use canonical for it
		return flags;
	}
	if (entry && entry->properties & (PART_OF_SPEECH|TAG_TEST)) // we know this usefully
	{
		flags = entry->properties;
		char* canon = GetCanonical(entry);
		canonical = NULL;
		if ( canon) canonical = FindWord(canon);
		// possessive pronoun-determiner like my is always a determiner, not a pronoun
		if (entry->properties & DETERMINER) flags &= -1 ^ PRONOUN;
	}


	if (!(flags & VERB) && bits & VERB) // could it be a verb we dont know?
	{
		char* verb =  GetInfinitive(original); 
		if (verb)  
		{
			flags |= VERB | verbFormat;
			if (verbFormat & (VERB_PRESENT_PARTICIPLE|VERB_PAST_PARTICIPLE)) flags |= ADJECTIVE|ADJECTIVE_PARTICIPLE;
			if (!entry) entry = StoreWord(original,VERB | verbFormat);
			canonical =  FindWord(verb,0,PRIMARY_CASE_ALLOWED);
		}
	}
	else if (!canonical && bits & VERB) canonical = FindWord(GetInfinitive(original));
	
	if (!(flags & NOUN) && bits & VERB) // could it be a noun
	{
		char* noun = GetSingularNoun(original); 
		if (noun) 
		{
			flags |= NOUN | nounFormat;
			if (!entry) entry = StoreWord(original,NOUN | nounFormat);
			if (!canonical) canonical = FindWord(noun);
		}
	}
	else if (!canonical && bits & VERB) // EVEN if we do know it... flies is a singular and needs canoncial for fly BUG
	{
		canonical = FindWord(GetSingularNoun(original));
	}
	else // even if it is a noun, if it ends in s and the root is also a noun, make it plural as well (e.g., rooms)
	{
		int len = strlen(original);
		if (original[len-1] == 's') 
		{
			char* singular = GetSingularNoun(original);
			if (singular)
			{
				canonical = StoreWord(singular);
				flags |= NOUN_PLURAL;
			}
		}
	}
	if (!(flags & ADJECTIVE) && bits & ADJECTIVE) 
	{
		char* adjective = GetAdjectiveBase(original); 
		if (adjective) 
		{
			flags |= ADJECTIVE | adjectiveFormat;
			if (!entry) entry = StoreWord(original,ADJECTIVE | adjectiveFormat);
			if (!canonical) canonical = FindWord(adjective);
		}
	}
	else if (  bits & ADJECTIVE)// even if marked as adjective, we still need to test for Adjective_participle--  walking is both regular adjective AND adjective participle
	{
		unsigned int len = strlen(original);
		if ( len > 3 && !stricmp(original+len-3,"ing"))
		{
			WORDP D = FindWord(original,len-3);
			if (D && D->properties & VERB) 
			{
				flags |= ADJECTIVE_PARTICIPLE;
				canonical = D;
			}
		}
		else if (!canonical) canonical = FindWord(GetAdjectiveBase(original));
	}
	if (!(flags & ADVERB) &&  bits & ADVERB) 
	{
		char* adverb = GetAdverbBase(original); 
		if (adverb)  
		{
			flags |= ADVERB | adverbFormat;
			if (!entry) entry = StoreWord(original,ADVERB | adverbFormat);
			if (!canonical) canonical = FindWord(adverb);
		}
	}
	else if (!canonical  &&  bits & ADVERB) canonical = FindWord(GetAdverbBase(original));
	if (!canonical) canonical = entry; //  for all others

	if (!flags && firstTry) // auto try for opposite case
	{
		WORDP D = FindWord(original,0,SECONDARY_CASE_ALLOWED);
		if (D) return GetPosData(D->word,entry,canonical,false);
	}

	if (!flags) // we dont know this word
	{
		unsigned int len = strlen(original);
		if (IsUpperCase(original[0])) 
		{
			flags |= (original[len-1] == 's') ? NOUN_PROPER_PLURAL : NOUN_PROPER_SINGULAR;
		}
		else 
		{
			flags |= (original[len-1] == 's') ? NOUN_PLURAL : NOUN_SINGULAR;
		}
		bool mixed = false;
		for (unsigned int i = 0; i < len; ++i) if (!IsAlpha(original[i]) && original[i] != '-' && original[i] != '_') mixed = true; // has non alpha in it

		if (mixed || IsUpperCase(original[0]));  // non-real word OR proper name
		else if (len > 2 && original[len-2] == 'e' && original[len-1] == 'd') flags |= VERB_PAST | VERB_PAST_PARTICIPLE|ADJECTIVE_PARTICIPLE;
		else if ( len > 3 && original[len-3] == 'i' && original[len-2] == 'n' && original[len-1] == 'g') flags |= NOUN_GERUND | VERB_PRESENT_PARTICIPLE;
		else if ( len > 1 && original[len-1] == 's') flags |= VERB_PRESENT_3PS | VERB_PRESENT;
		else flags |= VERB_PRESENT | VERB_INFINITIVE|NOUN_INFINITIVE;

		if (mixed || IsUpperCase(original[0])); // non-real word OR proper name
		else if (len > 3 && original[len-3] == 'e' && original[len-2] == 's' && original[len-1] == 't') flags |= ADJECTIVE_MOST;
		else if (len > 2 && original[len-2] == 'e' && original[len-1] == 'r') flags |= ADJECTIVE_MORE;
		else flags |= ADJECTIVE_BASIC;

		canonical = StoreWord("unknown-word");
	}
	if (flags & VERB_INFINITIVE) flags |= NOUN_INFINITIVE;
	return flags;
}

char* GetNounPhrase(unsigned int i,const char* avoid)
{
	unsigned int start = i;
	static char buffer[MAX_WORD_SIZE];
	*buffer = 0;
	bool front = false;
	while (values[start] & (NOUN_BITS | COMMA | CONJUNCTION_COORDINATE | ADJECTIVE_BITS | DETERMINER | PREDETERMINER | ADVERB_BITS | POSSESSIVE | PRONOUN_POSSESSIVE) && bitCounts[i] == 1) 
	{
		if (values[start] & COMMA && !(values[start-1] & ADJECTIVE_BITS)) break;	// NOT like:  the big, red, tall human
		if (values[start] & CONJUNCTION_COORDINATE)
		{
			if ( strcmp(canonicalLower[start]->word,"and")) break;	// not "and"
			if (!(values[start-1] & (ADJECTIVE_BITS|COMMA))) break;	// NOT like:  the big, red, and very tall human
			if (values[start-1] & COMMA && !(values[start-2] & ADJECTIVE_BITS)) break;	// NOT like:  the big, red, and very tall human
		}
		if (values[start] & (NOUN_GERUND|ADJECTIVE_PARTICIPLE)) break; 
		if (values[start] & ADVERB_BITS && !(values[start+1] & ADJECTIVE_BITS)) break;
		if (front && values[start] & NOUN_BITS && !(values[start-1] & POSSESSIVE)) break;	// maybe appostive like: Jesus his son
		if (values[start] & (PRONOUN_POSSESSIVE|ADJECTIVE_BITS)) front = true; // dont accept nouns now
		WORDP canon = canonicalLower[start];
		WORDP orig = originalLower[start];
		if (orig && (!strcmp("here",orig->word) || !strcmp("there",orig->word))) break;
		if (orig && (!strcmp("this",orig->word) || !strcmp("that",orig->word) || !strcmp("these",orig->word) || !strcmp("those",orig->word))) break;
		if (canon && canon->properties & PRONOUN) // avoid recursive pronoun expansions... like "their teeth"
		{
			if (!strcmp(canon->word,avoid)) break;	
		}
		if (values[start--] & (DETERMINER|PRONOUN_POSSESSIVE|NOUN_PROPER_SINGULAR)) break; // proper singular blocks appostive revisition
	}
	// start is NOT a member
	while (++start <= i)
	{
		char* word = wordStarts[start]; // we say "my name is:"  it binds to what HE might say- "it is a good name"
		if (tokenFlags & USERINPUT) strcat(buffer,word);			// things are stored from HIS perspective
		else if (!stricmp(word,"my")) strcat(buffer,"your");			// invert when WE say it
		else if (!stricmp(word,"your")) strcat(buffer,"my");	// invert when WE say it
		else strcat(buffer,word);
		if (start != i) strcat(buffer," ");
	}
	return buffer;
}

static void AssignPronouns()
{
	int itAssigned = 0;
	int itRole = 0;
	for (unsigned int i = 1; i <= wordCount; ++i)
    {
		char* original =  wordStarts[i];
		if (!(values[i] & NOUN_BITS)) continue;
		WORDP canon = (canonicalUpper[i]) ? canonicalUpper[i] : canonicalLower[i];
		WORDP orig = (originalUpper[i]) ? originalUpper[i] : originalLower[i];
		if (!canon) continue;
		if (!roles[i] && values[i+1] & NOUN_BITS) continue;	// just a noun modifying a noun -  I'd like to be a great *art patron
		// But fails if referential within the sentence
		// bug-- check for "and" coordinating words of noun
		if (values[i+1] & CONJUNCTION_COORDINATE && roles[i+1] & CONJUNCT_WORD)
		{
			char buf1[MAX_WORD_SIZE];
			strcpy(buf1,GetNounPhrase(i,"they")); //
			strcat(buf1," and ");
			unsigned  int j = i;
			while (!(values[++j] & NOUN_BITS) && j <= wordCount);	// find next noun
			while (!roles[j] && j < wordCount) ++j; // find trailing noun that marks the role
			strcat(buf1,GetNounPhrase(j,"they"));
			SetUserVariable("$they_pronoun",buf1);  
		}
	
		// they
		if (values[i] & (NOUN_PLURAL | NOUN_PROPER_PLURAL )) SetUserVariable("$they_pronoun",GetNounPhrase(i,"they"));
		else if (orig && orig->properties & (NOUN_PLURAL | NOUN_PROPER_PLURAL | NOUN_THEY))  SetUserVariable("$they_pronoun",GetNounPhrase(i,"they"));  
		// she
		else if (orig && orig->properties & NOUN_SHE)  SetUserVariable("$she_pronoun",GetNounPhrase(i,"she"));  
		// he
		else if (orig && orig->properties & NOUN_HE)  SetUserVariable("$he_pronoun",GetNounPhrase(i,"he"));  
		// he-she
		else if (canon->properties & NOUN_HUMAN)  SetUserVariable("$he-she_pronoun",GetNounPhrase(i,""));  
		// he-she
		else if (canon->properties & NOUN_ROLE || canon->systemFlags & NOUN_BEING)  SetUserVariable("$he-she_pronoun",GetNounPhrase(i,""));  
		// there - proper name location
		else if (orig && orig->properties & NOUN_PROPER_SINGULAR && orig->systemFlags & SPACEWORD) 
		{
			char buf[MAX_WORD_SIZE];
			strcpy(buf,"in ");
			strcpy(buf+3,GetNounPhrase(i,"there"));
			SetUserVariable("$there_pronoun",buf);  
		}
		// it
		else   
		{
			if (itAssigned && roles[i] >= roles[itAssigned]) continue;// priority is MAINSUBJECT MAINOBJECT subject2 object2
			SetUserVariable("$it_pronoun",GetNounPhrase(i,"it"));  
			itAssigned = i;
		}
	}
}

void DumpPronouns()
{
	if (trace)
	{
		char* he = GetUserVariable("$he_pronoun");
		char* she = GetUserVariable("$she_pronoun");
		char* heshe = GetUserVariable("$he-she_pronoun");
		char* it = GetUserVariable("$it_pronoun");
		char* they = GetUserVariable("$they_pronoun");
		char* here = GetUserVariable("$here_pronoun");
		char* there = GetUserVariable("$there_pronoun");

		Log(STDUSERLOG,"  he= %s   she= %s  he-she= %s  it=%s  they= %s  here= %s  there= %s\r\n",he,she,heshe,it,they,here,there);
	}
}

void POSTag(bool postrace)
{
	// mark data off the ends, so dont have to test for ends
	wordStarts[0] = "";
	values[wordCount+1] = values[0] = 0;
	originalLower[wordCount+1] = originalLower[0] = NULL; // just something no one will be checking for

    for (unsigned int i = 1; i <= wordCount; ++i)
    {
		char* original =  wordStarts[i];
		originalLower[i] = originalUpper[i] = NULL;
		canonicalUpper[i] = canonicalLower[i] = NULL;
		values[i] = 0;

		WORDP entry = NULL,canonical = NULL;
		uint64 flags = GetPosData(original,entry,canonical);

		if (IsUpperCase(original[0]))
		{
			if (!canonicalUpper[i]) canonicalUpper[i] = canonical;
			originalUpper[i] = entry;
			if (original[1] == 0 && original[0] == 'I'); // dont lower case I
			else if ( canonical && !stricmp(canonical->word,"unknown-word")); // dont try to change
			else if (!entry)
			{
				char word[MAX_WORD_SIZE];
				strcpy(word,original);
				word[0] = toLowercaseData[(unsigned char) word[0]];
				uint64 uflags = GetPosData(word,entry,canonical);
				if (uflags && canonical)  // IGNORE the upper case word...lest we have multiple noun flags (proper and non) and get confused
				{
					flags = uflags;
					originalUpper[i] = canonicalUpper[i]  = NULL;
				}
				if (!canonicalLower[i]) canonicalLower[i] = canonical;
				originalLower[i] = entry;
			}
		}
		else // lower case
		{
			if (!canonicalLower[i]) canonicalLower[i] = canonical;
			originalLower[i] = entry;
			WORDP X;
			char word[MAX_WORD_SIZE];
			strcpy(word,original);
			MakeUpperCase(word);
			bool useUpper = false;
			if (entry && (entry->properties & NOUN) && ((entry->properties & (VERB|ADJECTIVE|ADVERB|PREPOSITION)) == 0 && (X = FindWord(word,0,UPPERCASE_LOOKUP))  &&  (X->properties & NOUN)))
			{
				// there is an uppercase interpreteation... see if we should be that -- Did superman do it
				if (!(values[i-1] & (ADJECTIVE_BITS|POSSESSIVE|PRONOUN_POSSESSIVE|DETERMINER|PREDETERMINER) )) 
				{
					useUpper = true;
					flags = 0;
					strcpy(wordStarts[i],X->word);
					canonicalLower[i] = originalLower[i] = NULL;
				}
			}
			if (!strict || useUpper)
			{
				uint64 uflags = GetPosData(word,entry,canonical);
				if (uflags) flags |= uflags;
				if (!canonicalUpper[i]) canonicalUpper[i] = canonical;
				originalUpper[i] = entry;
			}
		}
		values[i] = flags;
	}
	if (tokenControl & DO_POSTAG)  TagIt(postrace);
	SetCanonical();
	AssignPronouns();
	if (trace & PREPARE_TRACE || prepareMode == 2) 
	{
		Log(STDUSERLOG,"\r\n%s---\r\n",DumpAnalysis(values,"\r\nTagged POS",false));
		DumpPrepositionalPhrases();
		DumpPronouns();
	}
	if (debug && wordCount) // not on opening volley
	{
		int old = trace;
		bool oldecho = echo;
		echo = true;
		trace = 1;
		if (tokenFlags & USERINPUT) Log(STDUSERLOG,"User ");
		else Log(STDUSERLOG,"Bot ");
		DumpPrepositionalPhrases();
		DumpPronouns();
		trace = old;
		echo = oldecho;
	}
}

void MarkAllImpliedWords()
{
	unsigned int olddepth = globalDepth;
	globalDepth = 5;
	unsigned int i;
 	POSTag(posTrace);
	if ( prepareMode) return;
	for (i = 1; i <= wordCount; ++i) 
	{
		originalFlags[i] = values[i]; // move back to primary
		if ( originalFlags[i] & (NOUN_BITS-NOUN_GERUND)) originalFlags[i] |= NOUN;
		if ( originalFlags[i] & (VERB_TENSES|VERB_INFINITIVE|NOUN_INFINITIVE|NOUN_GERUND|ADJECTIVE_PARTICIPLE)) originalFlags[i] |= VERB;
		if ( originalFlags[i] & AUX_VERB_TENSES) originalFlags[i] |= AUX_VERB;	// lest it report "can" as untyped, and thus become a noun
		if ( originalFlags[i] & (ADJECTIVE_BITS-ADJECTIVE_PARTICIPLE)) originalFlags[i] |= ADJECTIVE;
		if ( originalFlags[i] & ADVERB_BITS) originalFlags[i] |= ADVERB;
		if ( originalFlags[i] & (PRONOUN_BITS|PRONOUN_POSSESSIVE)) originalFlags[i] |= PRONOUN;
	}

	NextMatchStamp();
	markLength = 0;
	bool slash = false;
    topicInSentenceIndex = 0;

    if (trace & PREPARE_TRACE) Log(STDUSERLOG,"Concepts: \r\n");
 
	//   now mark every word in sentence
    for (i = 1; i <= wordCount; ++i) //   mark that we have found this word, either in original or canonical form
    {
		char* original =  wordStarts[i];
        //   now mark original word - dont create it, since we precreate all pattern words we need
        char* word = wordCanonical[i];
		unsigned int len = strlen(original);

		NextinferMark();
 		if (trace  & PREPARE_TRACE) Log(STDUSERLOG,"%s original=( ",original);

		switch(roles[i])
		{
			case MAINSUBJECT: MarkFacts(MakeMeaning(StoreWord("~mainsubject")),i,i); break;
			case MAINVERB: MarkFacts(MakeMeaning(StoreWord("~mainverb")),i,i); break;
			case MAINOBJECT: MarkFacts(MakeMeaning(StoreWord("~mainobject")),i,i); break;
			case MAININDIRECTOBJECT: MarkFacts(MakeMeaning(StoreWord("~mainindirectobject")),i,i); break;
			case SUBJECT2: MarkFacts(MakeMeaning(StoreWord("~subjec2")),i,i); break;
			case VERB2: MarkFacts(MakeMeaning(StoreWord("~verb2")),i,i); break;
			case OBJECT2: MarkFacts(MakeMeaning(StoreWord("~object2")),i,i); break;
			case INDIRECTOBJECT2: MarkFacts(MakeMeaning(StoreWord("~indirectobject2")),i,i); break;
		}

		// mark pos data
		uint64 bit = 0x8000000000000000ULL;
		for (int j = 63; j >= 0; --j)
		{
			if (originalFlags[i] & bit) 
				MarkFacts(MakeMeaning(posWords[j]),i,i);
			bit >>= 1;
		}

		// mark general number property
		if (originalFlags[i] & (NOUN_ORDINAL | NOUN_CARDINAL | ADJECTIVE_CARDINAL|ADJECTIVE_ORDINAL))  
		{
			MarkFacts(MakeMeaning(Dnumber),i,i); 
			if (wordStarts[i][0] == '$') MarkFacts(MakeMeaning(Dmoney),i,i); 
		}
		if (originalFlags[i] & (PRONOUN_BITS|PRONOUN_POSSESSIVE))  MarkFacts(MakeMeaning(FindWord("~pronoun")),i,i); 

        WORDP OL = originalLower[i];
		WORDP CL = canonicalLower[i];
		if (OL && OL->properties & QWORD) MarkFacts(MakeMeaning(FindWord("~qwords")),i,i);

 		WORDP OU = originalUpper[i]; 
        WORDP CU = canonicalUpper[i]; 
		if (!OU && !OL) OU = FindWord(original,0,UPPERCASE_LOOKUP);	// try to find an upper 
		if (!CU ) 
		{
			CU = FindWord(original,0,UPPERCASE_LOOKUP);	// try to find an upper to go with it, in case we can use that, but not as a human name
			if (OU); // it was originally uppercase or there is no lower case meaning
			else if (CU && CU->properties & (NOUN_FIRSTNAME|NOUN_HUMAN)) CU = NULL;	// remove accidental names
		}

		//   mark possessive apostrophe and other special decodes
		char* nextWord = wordStarts[i+1];
		if (IsUrl(original,0)) MarkFacts(MakeMeaning(Durl),i,i);

		if (!OL && !OU && !CL && !CU) MarkFacts(MakeMeaning(Dunknown),i,i); // unknown word
 		if ( originalFlags[i] & AUX_VERB_TENSES) originalFlags[i] |= VERB;	// lest it report "can" as untyped, and thus become a noun
		PrimaryMark(MakeTypedMeaning(OL,0,(unsigned int)(originalFlags[i] & TYPE_RESTRICTION)), i, i);
        if (trace & PREPARE_TRACE) Log(STDUSERLOG," // "); //   close original meanings lowercase
		markLength = 0;
		PrimaryMark(MakeTypedMeaning(OU,0,(unsigned int)(originalFlags[i] & TYPE_RESTRICTION)), i, i);

		if (trace & PREPARE_TRACE) Log(STDUSERLOG,")\r\n    canonical=( "); //   close original meanings uppercase

		//   canonical word
  		PrimaryMark(MakeTypedMeaning(CL,0, (unsigned int)(originalFlags[i] & TYPE_RESTRICTION)), i, i);
  		CanonicalMark(CL, i, i);
 		markLength = 0;
	    if (trace & PREPARE_TRACE) Log(STDUSERLOG," // "); //   close canonical form lowercase
 		PrimaryMark(MakeTypedMeaning(CU,0, (unsigned int)(originalFlags[i] & TYPE_RESTRICTION)), i, i);
  		CanonicalMark(CU, i, i);
		if (trace & PREPARE_TRACE) Log(STDUSERLOG,") "); //   close canonical form uppercase
		markLength = 0;
	
        //   peer into multiword expressions  (noncanonical), in case user is emphasizing something so we dont lose the basic match on words
        //   accept both upper and lower case forms . 
		// But DONT peer into something proper like "Moby Dick"
		unsigned int  n = BurstWord(wordStarts[i]); // peering INSIDE a single token....
		WORDP D,E;
		if (originalFlags[i] & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL)) n = 1;
        if (n > 1 && n < 6) //   longer than 5 is not emphasis, its a sentence - we do not peer into titles
        {
            for (unsigned int k = 1; k <= n; ++k)
            {
  				unsigned int prior = (k == n) ? i : (i-1); //   -1  marks its word match INSIDE a string before the last word, allow it to see last word still
                E = FindWord(GetBurstWord(k),0,PRIMARY_CASE_ALLOWED); 
                if (E)
				{
					if (!(E->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL))) PrimaryMark(MakeMeaning(E),i,prior);
					else MarkFacts(MakeMeaning(E),i,prior);
				}
                E = FindWord(GetBurstWord(k),0,SECONDARY_CASE_ALLOWED); 
				if (E)
				{
					if (!(E->properties &  (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL))) PrimaryMark(MakeMeaning(E),i,prior);
					else MarkFacts(MakeMeaning(E),i,prior);
				}
           }
        }
		
		D = (CL) ? CL : CU; //   best recognition
		char* last;
		if (D && D->properties & NOUN && (last = strrchr(D->word,'_')) && originalFlags[i] & NOUN) PrimaryMark(MakeMeaning(FindWord(last+1,0)), i, i); //   composite noun, store last word as referenced also

		//   handle finding fractions  mark as placenumber 
		if (slash && GetNextSpot(Dnumber,i-2,positionStart,positionEnd) == (i-2) && GetNextSpot(Dnumber,i,positionStart,positionEnd) == i)
		{
			MarkFacts(MakeMeaning(Dplacenumber),i-2,i-2);  
			if (trace & PREPARE_TRACE) Log(STDUSERLOG,"=%s/%s \r\n",wordStarts[i-2],wordStarts[i]);
		}
		slash = false;
		if (original[0] == '/' && !original[1]) slash = true; //   delay check for fractions

        if (trace & PREPARE_TRACE) Log(STDUSERLOG,"\r\n");
    }
 
	//   check for repeat input by user - but only if more than 2 words or are unknown (we dont mind yes, ok, etc repeated)
	//   track how many repeats, for escalating response
	bool brief = (wordCount > 2);
	if (wordCount == 1 && !FindWord(wordStarts[1])) brief = true;
    unsigned int counter = 0;
    if (brief && !repeatable) for (int j = 0; j < (int)(humanSaidIndex-1); ++j)
    {
        if (strlen(humanSaid[j]) > 5 && !stricmp(humanSaid[humanSaidIndex-1],humanSaid[j])) //   he repeats himself
        {
            ++counter;
            char buf[100];
			strcpy(buf,"~repeatinput");
			buf[12] = (char)('0' + counter);
			buf[13] = 0;
            MarkWordHit(FindWord(buf,0,PRIMARY_CASE_ALLOWED),1,1);  //   only marking ones we test for
        }
    }

	//   now see if he is repeating stuff I said
	counter = 0;
    if (wordCount > 2) for (int j = 0; j < (int)chatbotSaidIndex; ++j)
    {
        if (strlen(chatbotSaid[j]) > 5 && !stricmp(humanSaid[humanSaidIndex-1],chatbotSaid[j])) //   he repeats me
        {
           MarkFacts(MakeMeaning(FindWord("~repeatme",0,PRIMARY_CASE_ALLOWED)),counter,counter); //   you can see how many times
        }
    }

    //   the word you may be implied, check for that here
	markLength = 0;
    SetSequenceStamp(); //   squences of words
	globalDepth = olddepth;
}

bool AllowedMember(FACT* F, unsigned int i,unsigned int is,unsigned int index)
{
	if (trace & INFER_TRACE)  TraceFact(F);
    unsigned int localIndex = Meaning2Index(F->subject);
	unsigned int pos = GetMeaningType(F->subject);
	bool bad = false;
	if (!i && pos ) 
	{
            if (pos & VERB && is & NOUN)  
            {
				bad = true;
            }
            else if (pos & NOUN && is & VERB)  
            {
				bad = true;
            }
            else if ((pos & ADJECTIVE || localIndex & ADVERB) && is & (NOUN|VERB))  
            {
				bad = false;
            }
 	}
	else if (index && pos && pos != index) bad = true;
	return !bad;
}

static void Showit(char* buffer, const char* what,uint64 bits)
{
	if (bits) strcat(buffer,"+");
	strcat(buffer,what);
}

char* DumpAnalysis(uint64 flags[MAX_SENTENCE_LENGTH],const char* label,bool original)
{
	static char buffer[BIG_WORD_SIZE];
	*buffer = 0;
    if (wordCount == 0) return "";
	char* ambiguous = "";
	if (!original)
	{
		for (unsigned int i = 1; i <= wordCount; ++i)
		{
			if (bitCounts[i] > 1) 
			{
				ambiguous = "ambiguous ";
				break;
			}
		}
	}
	sprintf(buffer,"%s%s %d words: ",ambiguous,label,wordCount);
	int lenpre;

	for (unsigned int i = 1; i <= wordCount; ++i)
    {
		WORDP D = originalLower[i];
		uint64 tie = (D) ? D->systemFlags & (NOUN|VERB|ADJECTIVE|ADVERB|PREPOSITION) : 0;
		if (bitCounts[i] <= 1) tie = 0;
	
		if (originalLower[i]) strcat(buffer,originalLower[i]->word); // we know it as lower
		else if (originalUpper[i]) strcat(buffer,originalUpper[i]->word); // we know it as upper
		else  strcat(buffer,wordStarts[i]); // what he gave which we didnt know as lower or upper
		// canonical
		if (!original) strcat(buffer,"/");
		if (!original && canonicalLower[i]) strcat(buffer,canonicalLower[i]->word);
		else if (!original && canonicalUpper[i]) strcat(buffer,canonicalUpper[i]->word);
		strcat(buffer," (");
		lenpre = strlen(buffer);

		if (!original && phrases[i] && phrases[i-1] != phrases[i]) strcat(buffer,"<Phrase "); 
		if (!original && clauses[i] && clauses[i-1] != clauses[i]) strcat(buffer,"<Clause ");
		if (!original && verbals[i] && verbals[i-1] != verbals[i]) strcat(buffer,"<Verbal ");

		if (!original && roles[i])
		{
			if ( roles[i] == MAINSUBJECT) strcat(buffer,"MAINSUBJECT ");
			else if (roles[i] == SUBJECT2) strcat(buffer,"SUBJECT2 ");
			else if (roles[i] == MAINVERB) strcat(buffer,"MAINVERB ");
			else if (roles[i] == VERB2) strcat(buffer,"VERB2 ");
			else if (roles[i] == MAINOBJECT) strcat(buffer,"MAINOBJECT ");
			else if (roles[i] == OBJECT2) strcat(buffer,"OBJECT2 ");
			else if (roles[i] == MAININDIRECTOBJECT) strcat(buffer,"MAININDIRECTOBJECT ");
			else if (roles[i] == INDIRECTOBJECT2) strcat(buffer,"INDIRECTOBJECT2 ");
			else if (roles[i] == CONJUNCT_WORD) strcat(buffer,"CONJUNCT_WORD ");
			else if (roles[i] == CONJUNCT_PHRASE) strcat(buffer,"CONJUNCT_PHRASE ");
			else if (roles[i] == CONJUNCT_CLAUSE) strcat(buffer,"CONJUNCT_CLAUSE ");
			else if (roles[i] == CONJUNCT_SENTENCE) strcat(buffer,"CONJUNCT_SENTENCE ");
			else if (roles[i] == APPOSITIVE) strcat(buffer,"APPOSITIVE ");
			else if (roles[i] == OBJECT_COMPLEMENT) strcat(buffer,"OBJECT_COMPLEMENT ");
			else if (roles[i] == ADJECTIVE_AS_OBJECT) strcat(buffer,"ADJECTIVE_AS_OBJECT ");
		}

  		uint64 type  = flags[i];
		if (type & PUNCTUATION)  strcat(buffer,"Punctuation ");
		if (type & PAREN)  strcat(buffer,"Parenthesis ");
		if (type & COMMA)  strcat(buffer,"Comma ");
		if (type & (NOUN | NOUN_BITS|NOUN_INFINITIVE)) 
		{
			if (type & NOUN_PLURAL) Showit(buffer,"Noun_plural ",tie&NOUN);
			if (type & NOUN_GERUND) Showit(buffer,"Noun_gerund ",tie&NOUN);
			if (type & NOUN_INFINITIVE) Showit(buffer,"Noun_infinitive ",tie&NOUN);
			if (type & NOUN_SINGULAR) Showit(buffer,"Noun_singular ",tie&NOUN);
			if (type & NOUN_PROPER_SINGULAR) Showit(buffer,"Noun_proper_singular ",tie&NOUN);
			if (type & NOUN_PROPER_PLURAL) Showit(buffer,"Noun_proper_plural ",tie&NOUN);
			if (type & NOUN_CARDINAL) Showit(buffer,"Noun_cardinal ",tie&NOUN);
			if (type & NOUN_ORDINAL) Showit(buffer,"Noun_ordinal ",tie&NOUN);
			if ( !(type & (NOUN_BITS|NOUN_INFINITIVE))) Showit(buffer,"Noun_unknown ",tie&NOUN);
		}
		if (type & (AUX_VERB | AUX_VERB_TENSES)) 
		{
			if (type & AUX_VERB_FUTURE) strcat(buffer,"Aux_verb_future ");
			if (type & AUX_VERB_PAST) strcat(buffer,"Aux_verb_past ");
			if (type & AUX_VERB_PRESENT) strcat(buffer,"Aux_verb_present ");
			if ( !(type & AUX_VERB_TENSES)) strcat(buffer,"Aux_verb_unknown ");
		}
		if (type & (VERB|VERB_TENSES)) 
		{
			if (type & VERB_INFINITIVE) Showit(buffer,"Verb_infinitive ",tie&VERB);
			if (type & VERB_PRESENT_PARTICIPLE) Showit(buffer,"Verb_present_participle ",tie&VERB);
			if (type & VERB_PAST) Showit(buffer,"Verb_past ",tie&VERB);
			if (type & VERB_PAST_PARTICIPLE) Showit(buffer,"Verb_past_participle ",tie&VERB);
			if (type & VERB_PRESENT) Showit(buffer,"Verb_present ",tie&VERB);
			if (type & VERB_PRESENT_3PS) Showit(buffer,"Verb_present_3ps ",tie&VERB);
			if (!(type & VERB_TENSES)) Showit(buffer,"Verb_unknown ",tie&VERB);
		}
		if (type & PARTICLE) strcat(buffer,"Particle ");
		if (type & (ADJECTIVE|ADJECTIVE_BITS))
		{
			if (type & ADJECTIVE_PARTICIPLE) Showit(buffer,"Adjective_participle ",tie&ADJECTIVE); // can be dual kind of adjective
			if (type & ADJECTIVE_MORE) Showit(buffer,"Adjective_more ",tie&ADJECTIVE);
			if (type & ADJECTIVE_MOST) Showit(buffer,"Adjective_most ",tie&ADJECTIVE);
			if (type & ADJECTIVE_CARDINAL) Showit(buffer,"Adjective_cardinal ",tie&ADJECTIVE);
			if (type & ADJECTIVE_ORDINAL) Showit(buffer,"Adjective_ordinal ",tie&ADJECTIVE);
			if (type & ADJECTIVE_BASIC) Showit(buffer,"Adjective_basic ",tie&ADJECTIVE);
			if (!(type & ADJECTIVE_BITS))  Showit(buffer,"Adjective_unknown ",tie&ADJECTIVE);
		}
		if (type & (ADVERB|ADVERB_BITS))
		{
			if (type & ADVERB_MORE) Showit(buffer,"Adverb_more ",tie&ADVERB);
			if (type & ADVERB_MOST) Showit(buffer,"Adverb_most ",tie&ADVERB);
			if (type & ADVERB_BASIC) Showit(buffer,"Adverb_basic ",tie&ADVERB);
			if (type & WH_ADVERB) Showit(buffer,"Adverb_wh ",tie&ADVERB);
			if (!(type & ADVERB_BITS)) Showit(buffer,"Adverb_unknown ",tie&ADVERB);
		}
		if (type & PREPOSITION) Showit(buffer,"Preposition ",tie&PREPOSITION);
		if (type & TO_INFINITIVE) strcat(buffer,"To_infinitive ");
		if (type & (PRONOUN|PRONOUN_BITS|PRONOUN_POSSESSIVE))
		{
			if (type & PRONOUN_POSSESSIVE) strcat(buffer,"Pronoun_possessive ");
			if (type & PRONOUN_OBJECT) strcat(buffer,"Pronoun_object ");
			if (type & PRONOUN_SUBJECT) strcat(buffer,"Pronoun_subject ");
			if (!(type & PRONOUN_BITS|PRONOUN_POSSESSIVE)) strcat(buffer,"Pronoun_unknown ");
		}
		if (type & THERE_EXISTENTIAL)  strcat(buffer,"There_existential ");
		if (type & CONJUNCTION_COORDINATE) strcat(buffer,"Conjunction_coordinate ");
		if (type & CONJUNCTION_SUBORDINATE) Showit(buffer,"Conjunction_subordinate ",tie&PREPOSITION);
		if (type & PREDETERMINER) strcat(buffer,"Predeterminer ");
		if (type & DETERMINER) strcat(buffer,"Determiner ");
		if (type & POSSESSIVE)strcat(buffer,"Possessive ");

		if (original && originalLower[i])
		{
			if (originalLower[i]->systemFlags & OTHER_SINGULAR) strcat(buffer,"other_singular ");
			if (originalLower[i]->systemFlags & OTHER_PLURAL) strcat(buffer,"other_plural ");
		}
	
		if (!original && phrases[i] && phrases[i+1] != phrases[i]) strcat(buffer,"Phrase> ");
		if (!original && clauses[i] && clauses[i+1] != clauses[i]) strcat(buffer,"Clause> ");
		if (!original && verbals[i] && verbals[i+1] != verbals[i]) strcat(buffer,"Verbal> ");

		unsigned int len = strlen(buffer);
		if (len == lenpre) strcpy(buffer+len,")  ");
		else strcpy(buffer+len-1,")  ");
	}
	strcat(buffer,"\r\n");
	return buffer;
}

void DumpPrepositionalPhrases()
{
	int id = 0;
	char buffer[MAX_BUFFER_SIZE];
	*buffer = 0;
	strcat(buffer,"  MainSentence: ");
	// show main sentence
	unsigned int subjects[200];
	unsigned int verbs[200];
	unsigned int objects[200];
	unsigned int indirectobjects[200];
	unsigned int subject = 0, verb = 0, indirectobject = 0, object = 0;
	unsigned int i;
	bool notFound = false;
	for ( i = 1; i <= wordCount; ++i) // show phrases
	{
		if (roles[i] == MAINSUBJECT) subjects[subject++] = i;
		if (roles[i] == MAINVERB) verbs[verb++] = i;
		if (roles[i] == MAININDIRECTOBJECT) indirectobjects[indirectobject++] = i;
		if (roles[i] == MAINOBJECT) objects[object++] = i;
		if (roles[i] == ADJECTIVE_AS_OBJECT) objects[object++] = i;
		if (roles[i] == NOT) notFound = true;
	}
	if (subject) 
	{
		for (i = 0; i < subject; ++i)
		{
			unsigned int s = subjects[i];
			if (values[s] & NOUN_INFINITIVE) strcat(buffer,"to "); // doesnt display DO NOT here
			char* phrase = GetNounPhrase(s-1,"");
			if (*phrase) 
			{
				strcat(buffer,"(");
				strcat(buffer,phrase);
				strcat(buffer,") ");
			}
			strcat(buffer,wordStarts[s]);
			if (i < subject-1) strcat(buffer,"+");
		}
		strcat(buffer," ");
	}
	if (verb) 
	{
		if (notFound) strcat(buffer,"(NOT!) ");
		for (i = 0; i < verb; ++i)
		{
			unsigned int j = verbs[i];
			unsigned int clause = clauses[j];
			for (unsigned int k = 0; k < j; ++k)
			{
				if (clauses[k] == clause && values[k] & AUX_VERB_TENSES) // has  aux
				{
					strcat(buffer,wordStarts[k]);
					strcat(buffer," ");
				}
			}
			if (j != 1 && values[j] & NOUN_INFINITIVE) strcat(buffer,"to "); // doesnt display DO NOT here
			strcat(buffer,wordStarts[j]);
			if (i < verb-1) strcat(buffer,"+");
		}
		strcat(buffer," ");
	}
	if (indirectobject) 
	{
		for (i = 0; i < indirectobject; ++i)
		{
			strcat(buffer,wordStarts[indirectobjects[i]]);
			if (i < verb-1) strcat(buffer,"+");
		}
		strcat(buffer," ");
	}
	if (object) 
	{
		for (i = 0; i < object; ++i)
		{
			unsigned int o = objects[i];
			char* phrase = GetNounPhrase(o-1,"");
			if (*phrase) 
			{
				strcat(buffer,"(");
				strcat(buffer,phrase);
				strcat(buffer,") ");
			}
			if (values[o] & NOUN_INFINITIVE) strcat(buffer,"to "); // doesnt display DO NOT here
			strcat(buffer,wordStarts[o]);
			if (i < verb-1) strcat(buffer,"+");
		}
		strcat(buffer," ");
	}

	if (tokenFlags & QUESTIONMARK) 
	{
		if (!stricmp(wordStarts[1],"when")) strcat(buffer,"(when) ");
		if (!stricmp(wordStarts[1],"where")) strcat(buffer,"(where) ");
		if (!stricmp(wordStarts[1],"why")) strcat(buffer,"(why) ");
		if (!stricmp(wordStarts[1],"who")) strcat(buffer,"(who) ");
		if (!stricmp(wordStarts[1],"what")) strcat(buffer,"(what) ");
		if (!stricmp(wordStarts[1],"how")) strcat(buffer,"(how) ");
		strcat(buffer,"? ");
	}
	if (tokenFlags & PAST) strcat(buffer," PAST ");
	if (tokenFlags & FUTURE) strcat(buffer," FUTURE ");

	for (unsigned int i = 1; i <= wordCount; ++i) // show phrases
	{
		if (coordinates[i])
		{
			strcat(buffer,"\r\n Coordinate");
			if (roles[i] == CONJUNCT_WORD) strcat(buffer,"Word: ");
			else if (roles[i] == CONJUNCT_PHRASE) strcat(buffer,"Phrase: ");
			else strcat(buffer,": ");
			strcat(buffer,wordStarts[i]);
			strcat(buffer," (");
			strcat(buffer,wordStarts[coordinates[i]]);
			strcat(buffer,") ");
		}
		if ( phrases[i])
		{
			if (phrases[i] != id)
			{
				id = phrases[i];
				strcat(buffer,"\r\n  Phrase: ");
			}
			strcat(buffer,wordStarts[i]);
			strcat(buffer," ");
		}
	}
	id = 0;
	for (unsigned int i = 1; i <= wordCount; ++i) // show clauses
	{
		if ( clauses[i])
		{
			if (clauses[i] != id)
			{
				id = clauses[i];
				strcat(buffer,"\r\n  Clause: ");
			}
			strcat(buffer,wordStarts[i]);
			strcat(buffer," ");
		}
	}
	id = 0;
	for (unsigned int i = 1; i <= wordCount; ++i) // show verbals
	{
		if ( verbals[i])
		{
			if (verbals[i] != id)
			{
				id = verbals[i];
				strcat(buffer,"\r\n  Verbal: ");
			}
			strcat(buffer,wordStarts[i]);
			strcat(buffer," ");
		}
	}
	Log(STDUSERLOG,"%s\r\n\r\n",buffer);
 }

