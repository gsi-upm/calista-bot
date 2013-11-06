 // tagger.cpp - used for pos tagging

#ifdef INFORMATION

Structures:

1. Object complement:    It is most often used with verbs of creating or nominating such as make, name, elect, paint, call, choosing, judging, appointing, etc. in the active voice,.. implied missing "to be"
	Subject Verb Object nounphrase.		He considered her his grandmother.
	Subject Verb Object adjective.		He drove her crazy.
	Subject Verb Object prep-phrase.	He loved her like a sister.???
	Subject verb object gerund	?		I saw the Prime Minister sleeping
	Subject verb object clause			I like the shawl *people wear

2. Appostive noun (at end will have a comma, lest it be object ocmplement)
	He considered Joe, a self-starter.	(comma helps)
	The commander stood his ground, a heroic figure. (delayed appositive of commander)
	It is a pleasure *to meet up (delayed appositive of it but fine as appossitive of pleasure)
	I, the leader of the rebellion that defines eternity, call this to order.

3. Clause-omitting starter
	The shawl *people wear is green (subject complement)
	Roger likes the shawl *people wear (object complement)

4. Prep phrase omitting prep
	Time phrases					He will walk *next week

5.	Adjective Object
	Subject verb adjective		The shawl is green

#endif




#include "common.h"

#define INCLUSIVE 1
#define EXCLUSIVE 2

bool useStats = false;
bool useDefault = false;

static unsigned roleStack[20]; // what we seek in direct object land or verb land
static unsigned char verbStack[20];
static unsigned char verbList[20];
static unsigned int verbIndex;

static unsigned int roleIndex;
static int lastClause;
static int lastVerbal;

// field1: index of result bits, part1&2 bit, keep/discard bit
#define RESULT_SHIFT 48		// for 3 - set only on 1st field 49.50
#define OFFSET_SHIFT 48		// for 3 - set only on 2nd field 49.50
#define GUESS_BIT	(1LL << 51)	// for 1 - set only on first field or 3rd field (guess level 2) or 4th field (guess level 3)  each level uses bits on prior level
#define PART1_BIT ( 1LL << 51) // 1bit -- set on 2nd field only

#define PART2_BIT	( 1LL << 52)	// 1bit -- set on [0] first field only 0x0020 000000000000
#define KEEP_BIT	( 1LL << 52) 	// for 1 - set only on [1] 2nd field
#define REVERSE_BIT ( 1LL << 52)	// for 1 - set only on [2] 3rd field
#define TRACE_BIT ( 1LL << 52)		// for 1  - set only on [3] 4th field


#define SKIP_OP	( 1LL << 55 )
#define STAY_OP	( 1LL << 56 )
#define NOT_OP	( 1LL << 57 )
#define CONTROL_SHIFT 53 // for 3 - set on all pattern fields
#define OP_SHIFT 56		// for 5 set on all pattern fields
#define CTRL_SHIFT ( OP_SHIFT - CONTROL_SHIFT )
#define PATTERN_BITS 0x8000FFFFFFFFFFFFULL 
#define MISC_BITS	 0x007F000000000000ULL	// 7 bits
#define CONTROL_BITS 0xEF80000000000000ULL // 8 control bits
//  48,49,50,51,52,53,54,	55,56,57,58,59,60,61,62,   63 (prepostional_phrase bit)
#ifdef JUNK
The top 16 bits of dictionary properties are not used in pos-tagging labeling of words
and are reused as control information as follows:
	8 bits name the control bits for a field  56-63
	3 bits name the offset where the pattern as a whole starts relative to the target  53-55
	1 bit  specified result keep or discard 52
	2 bits specified which rule field has the result bits to use  50-51
	1 bit specifies tracing or not  49
#endif

#define MAX_TAG_FIELDS 4
unsigned char phrases[MAX_SENTENCE_LENGTH];
static unsigned char prepBit;
unsigned char clauses[MAX_SENTENCE_LENGTH];
static unsigned char clauseBit;
unsigned char verbals[MAX_SENTENCE_LENGTH];
static unsigned char verbalBit;

unsigned char designatedObjects[MAX_SENTENCE_LENGTH];
unsigned char coordinates[MAX_SENTENCE_LENGTH];

static int particleLocation;

 // a tag rule consists of uint64's, representing 4 comparator words (result uses one of them also), and a uint64 control word
 // the control word represents 6 bytes (describing how to interpret the 4 patterns and result), and a 1-byte offset locator to orient the pattern
 // the result is to either discard named bits or to restrict to named bits on the PRIMARY location, using the bits of the primary include...

static unsigned int tagRuleCount = 0;
static uint64* tags = NULL;
static char** comments = NULL;
uint64 values[MAX_SENTENCE_LENGTH];			// current pos tags in this word position
uint64 originalValues[MAX_SENTENCE_LENGTH];			// starting pos tags in this word position
static unsigned int markers[MAX_SENTENCE_LENGTH];			// flags from postagger to analyzer
int bitCounts[MAX_SENTENCE_LENGTH]; // number of tags still to resolve in this word position
static bool reverseFlow = false;
static bool reverseWords = false;
static unsigned int rulesUsed[1000];
static unsigned int rulesUsedIndex;
static int posStatus = 0;

static void StartupMembership()
{
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		WORDP D = originalLower[i];
		if (D && D->systemFlags & OMITTABLE_TIME_PREPOSITION) // after it is timeword?  // omitted prepoistion time phrase
		{
			WORDP E = canonicalLower[i+1];
			if (E && E->systemFlags & TIMEWORD)
			{
				if ( i == 1 || i == (wordCount - 1)) // time phrase at start or end
				{
					values[i] &= ADJECTIVE_BITS|DETERMINER;// This is a determiner
					bitCounts[i] = BitCount(values[i]);
					phrases[i] = prepBit;
					values[i+1] &= NOUN_BITS;
					bitCounts[i+1] = BitCount(values[i+1]);
					phrases[i+1] = prepBit;
					prepBit <<= 1;
				}
				
				markers[i+1] |= BE_OBJECT2;
			}
		}
	}
}

int BitCount (uint64 n)  
{  
	int count = 0 ;  
    while (n)  
	{  
       count++ ;  
       n &= (n - 1) ;  
    }  
    return count;  
 } 

 void C_Posit(char* word1)
{
	char buffer[MAX_BUFFER_SIZE];
	char word[MAX_WORD_SIZE];
	*buffer = 0;
	FILE* out = fopen("out.txt","wb");
	FILE* in = fopen("tagged.txt","rb");
	char* ptr = buffer;
	bool quote = false;
	while (1)
	{
		if (*ptr)
		{
			ptr = SkipWhitespace(ptr);
			if (!*ptr) continue;
			char* at = word;
			char c;
			while ((c = *ptr++) && c != ' ' && c) *at++ = c; 
			*at = 0;
			fprintf(out,"%s ",word);
			if ( !strcmp(word,"\"/\"")) quote = !quote;
			if (!stricmp(word,"./.") && !quote ) fprintf(out,"\r\n");
		}
		else 
		{
			if (!ReadALine(buffer,in)) break;
			ptr = buffer;
		}
	}

	fclose(in);
	fclose(out);
}

 void C_Posit1(char* word1)
{
	char sentence[MAX_BUFFER_SIZE];
	char pos[MAX_BUFFER_SIZE];
	char buffer[MAX_BUFFER_SIZE];
	char word[MAX_WORD_SIZE];
	*buffer = 0;
	FILE* out = fopen("splittag.txt","wb");
	FILE* in = fopen("out.txt","rb");
	char* ptr = buffer;
	while (ReadALine(buffer,in))
	{
		*sentence = 0;
		strcpy(pos,"  ");
		ptr = buffer;
		unsigned int index = 0;
		unsigned int posindex = 2;
		while (*ptr)
		{
			ptr = SkipWhitespace(ptr);
			ptr = ReadCompiledWord(ptr,word);
			if (!*ptr) break;
			char* sep = strchr(word+1,'/');
			*sep = 0;
			strcat(sentence,word);
			strcat(sentence," ");

			while (posindex < index) {strcat(pos," "); ++posindex;}
			strcat(pos,sep+1);
			strcat(pos," ");

			index += strlen(word) + 1;
			posindex += strlen(sep+1) + 1;
		}
		fprintf(out,"%s\r\n",sentence);
		fprintf(out,"%s\r\n",pos);
		PrepareSentence(sentence,true,true);

		fprintf(out,"  ");
		for (unsigned int i = 1; i <= wordCount; ++i) fprintf(out,"%s ",wordStarts[i]);
		fprintf(out,"\r\n");
	
	}

	fclose(in);
	fclose(out);
}

static int TestTag(int &i, int control, uint64 bits,int direction)
{
	uint64 val;
	bool notflag = false;
	if (control & NOTCONTROL) notflag = true;
	if ( i <= 0 || (unsigned int)i > wordCount) 
	{
		if (control & SKIP) return false;	// cannot stay any longer
		return (notflag) ? true : false;
	}
	control >>= CTRL_SHIFT;
	int answer = false;
	switch(control)
	{
		case HAS: // the bit is among those present 
			if (!answer) answer = (values[i] & bits) != 0;
			break;
		case IS: // The bit is what is given 
			if (!answer)
			{
				val = values[i] & bits; // bits in common
				if (val)
				{
					val = values[i] ^ val; // bits left over
					answer = (!val); // given choice or choice set, that matches all bits available
				}
				else answer = false;
			}
			break;

		case POSITION:
			if ( bits == 1 && i == 1) answer = true;
			else if ( bits > 100 && i == wordCount) answer = true;
			break;
		case ENDSWITH:
			{
				WORDP D = Meaning2Word((int)bits);
				unsigned int wordlen = strlen(wordStarts[i]);
				answer = !stricmp(wordStarts[i] - wordlen - 1,D->word);
			}
			break;
		case HASPROPERTY:
			{
				uint64 result =  originalLower[i] ? (originalLower[i]->systemFlags & bits ) : false; 
				answer = result != 0; 
			}
			break;
		case HASCANONICALPROPERTY:
			{
				uint64 result =  canonicalLower[i] ? (canonicalLower[i]->systemFlags & bits ) : false; 
				answer = result != 0; 
			}
			break;
		case LACKSROLE:
			answer = ((posStatus & bits) != 0) ; // NOT is not meaningful. only true after a pass
			break;
		case ISROLE:
			answer = ((roles[i] & bits) != 0) ; // NOT is not meaningful - here accept multiple roles
			break;
		case ISWORD:
			{
				WORDP D = Meaning2Word((int)bits);
				answer = (!stricmp(wordStarts[i],D->word));
			}
			break;
		case ISCANONICAL:
			{
				WORDP D = Meaning2Word((int)bits);
				answer =  canonicalLower[i] == D ;
			}
			break;
		case ISCLAUSE:
			answer = clauses[i];
			break;
		case ISADJACENT: // always use with stay
			if ( (unsigned int) i < wordCount)
			{
				val = values[i+1] & bits; // bits in common
				if (val) // exactly bits in common?
				{
					val = values[i+1] ^ val; // bits left over
					answer = (!val); // given choice or choice set, that matches all bits available
				}
				else answer = false; // no bits in common
			}
			if (!answer && i > 1)
			{
				val = values[i-1] & bits; // bits in common
				if (val)
				{
					val = values[i-1] ^ val; // bits left over
					answer = (!val); // given choice or choice set, that matches all bits available
				}
				else answer = false;
			}
			break;
		case ISMEMBER:
			{
				WORDP D = Meaning2Word((int)bits);
				FACT* F = (D) ? GetObjectHead(D) : NULL;
				MEANING M = MakeMeaning(canonicalLower[i]);
				while (F)
				{
					if (false)
					{
						TraceFact(F);
					}
					if (F->subject == M)
					{
						answer = true;
						break;
					}
					F = GetObjectNext(F);
				}
			}
			break;
		case PARTICLEVERB:
			{
				// dont allow contiguous particle to verb
				if (values[i] & (VERB_TENSES|NOUN_INFINITIVE|NOUN_GERUND) && i < (particleLocation-1)) // BUG  Some French names are hard to pronounce-- French is a verb? (lower case?)
				{
					char word[MAX_WORD_SIZE];
					if (canonicalLower[i]) 
					{
						sprintf(word,"%s_%s",canonicalLower[i]->word,wordStarts[particleLocation]);
					}
					else if (canonicalUpper[i]) 
					{
						sprintf(word,"%s_%s",canonicalUpper[i]->word,wordStarts[particleLocation]);
					}
					WORDP D = FindWord(word);
					answer = D && D->systemFlags & PHRASAL_VERB;
					if (answer && !notflag) answer = true;
				}
			}
			break;
		case INCLUDE:
			answer = values[i] & bits && values[i] & (-1LL ^ bits); // there are bits OTHER than what we are testing for
			break;
		default: // unknown control
			return 2;
	}
	if (notflag) answer = !answer;
	return answer;
}

static void UseDefaultPos(unsigned int guessAllowed)
{
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		if (bitCounts[i] == 1) continue; // no problem
		WORDP D = originalLower[i];
		if (!D) continue;
		uint64 v = values[i];	// current choices;
		// static default priority by class first
		uint64 flags = 0;
		if ( guessAllowed == 3 && useStats)
		{
			if (D->systemFlags & NOUN && v & NOUN_BITS) 
			{
				flags |= NOUN_BITS;
				if (trace) Log(STDUSERLOG," %d) %s: statistically noun-priority\r\n",i,D->word);
			}
			if (D->systemFlags & VERB && v && (VERB_TENSES|VERB_INFINITIVE)) 
			{
				flags |= VERB_TENSES|VERB_INFINITIVE|NOUN_INFINITIVE;
				if (trace) Log(STDUSERLOG," %d) %s: statistically verb-priority\r\n",i,D->word);
			}
			if (D->systemFlags & ADJECTIVE && v & ADJECTIVE_BITS) 
			{
				flags |= ADJECTIVE_BITS;
				if (trace) Log(STDUSERLOG," %d) %s: statistically adjective-priority\r\n",i,D->word);
			}
			if (D->systemFlags & ADVERB && v & ADVERB_BITS) 
			{
				flags |= ADVERB_BITS;
				if (trace) Log(STDUSERLOG," %d) %s: statistically adverb-priority\r\n",i,D->word);
			}
			if (D->systemFlags & PREPOSITION && v & PREPOSITION|CONJUNCTION_SUBORDINATE) 
			{
				flags |= PREPOSITION|CONJUNCTION_SUBORDINATE;
				if (trace) Log(STDUSERLOG," %d) %s: statistically preposition-subordconjunct-priority\r\n",i,D->word);
			}
		}
		else if (!useDefault);

		// static default priority by subclass next
		else if (v & PREPOSITION) // rare- most likely
		{
			v &= PREPOSITION;
			if (trace) Log(STDUSERLOG," %d) %s: guess preposition-default\r\n",i,D->word);
		}	
		else if (v & NOUN_BITS)
		{
			v &= NOUN_BITS;
			if (trace) Log(STDUSERLOG," %d) %s: guess noun-default\r\n",i,D->word);
		}
		else if (v & VERB_TENSES)
		{
			v &= VERB_TENSES|VERB_INFINITIVE| NOUN_INFINITIVE;
			if (trace) Log(STDUSERLOG," %d) %s: guess verb-default\r\n",i,D->word);
		}
		else if (v & ADJECTIVE_BITS)
		{
			v &= ADJECTIVE_BITS;
			if (trace) Log(STDUSERLOG," %d) %s: guess adjective-default\r\n",i,D->word);
		}
		else if (v & ADVERB_BITS)
		{
			v &= ADVERB_BITS;
			if (trace) Log(STDUSERLOG," %d) %s: guess adverb-default\r\n",i,D->word);
		}
		else if (v & PRONOUN_SUBJECT)
		{
			v &= PRONOUN_SUBJECT;
			if (trace) Log(STDUSERLOG," %d) %s guess pronounsubject-default\r\n",i,D->word);
		}
		if (flags) v &= flags;	// only keep these
		if (v && v != values[i]) // make the change on this one word and return
		{
			values[i] = v;
			bitCounts[i] = BitCount(v);
			return;
		}
	}
}

static bool ProcessSplitNoun(unsigned int verb1)
{
	// 1 when we have dual valid verbs, maybe we swallowed a noun into a prep phrase that shouldnt have been.  (sequence: noun noun (ad:v) verb)
	// 2 Or We may have an inner implied clause -- the shawl people often wear isn't yellow ==  I hate the shawl people wear
	// 3 or a clause in succession when object side- to sleep is the thing eli wanted
	// 4 or a clause on subject side -  what they found thrilled them

	// 1 For 1st verb, see if we have  noun noun {adv} verb  formats (which implies noun should have been separated,
	unsigned int before = verb1;
	if (values[verb1] & VERB_TENSES) // past tense needs to go to participle
	{
		while (--before > 0) 
		{
			if (values[before] & ADVERB_BITS) continue; // ignore this
			// find the subject just before our 2nd verb
			if (values[before] & (NOUN_BITS - NOUN_GERUND - NOUN_CARDINAL - NOUN_ORDINAL + PRONOUN_SUBJECT + PRONOUN_OBJECT) &&
				values[before-1] & (NOUN_BITS - NOUN_GERUND - NOUN_CARDINAL - NOUN_ORDINAL  + PRONOUN_SUBJECT + PRONOUN_OBJECT))
			{ // misses, the shawl the people wear
				if (trace) Log(STDUSERLOG,"split noun @ %s(%d) to %s(%d)\r\n",wordStarts[before],before,wordStarts[verb1],verb1);

				if (values[before] & (PRONOUN_SUBJECT|PRONOUN_OBJECT)) // pronoun will already be split, but might be a clause starter
				{
					if (canonicalLower[before-1] && canonicalLower[before-1]->systemFlags & POTENTIAL_CLAUSE_STARTER)
					{
						roles[before] = SUBJECT2;
						phrases[before] = 0;
						roles[verb1] = VERB2;
						if (designatedObjects[verb1]) roles[designatedObjects[verb1]] = OBJECT2;
						--before; // subsume starter for clause
					}
					else
					{
						roles[before] = SUBJECT2;
						phrases[before] = 0;
						roles[verb1] = VERB2;
						if (designatedObjects[verb1]) roles[designatedObjects[verb1]] = OBJECT2;
					}
				}
				else if (values[before-1] & (PRONOUN_SUBJECT|PRONOUN_OBJECT))// pronoun will already be split
				{
					roles[before] = SUBJECT2;
					phrases[before] = 0;
					roles[verb1] = VERB2;
					if (designatedObjects[verb1]) roles[designatedObjects[verb1]] = OBJECT2;
				}
				else 
				{
					roles[before-1] = roles[before];
					roles[before] = SUBJECT2;
					phrases[before] = 0;
					roles[verb1] = VERB2;
					if (designatedObjects[verb1]) roles[designatedObjects[verb1]] = OBJECT2;
			}

				// if we find an entire given clause with starter, add the starter "what they found thrilled them"

				while (before <= verb1) clauses[before++] = clauseBit;
				clauseBit <<= 1;
				return true;	
			}
			else break;
		}
	}
	return false;
}

static bool ProcessImpliedClause(unsigned int verb1) 
{
	if (clauses[verb1]) return false;	// already in a clause
	if (roles[verb1] == MAINVERB) return false;	// assume 1st is main verb

	unsigned int subject = verb1 - 1;
	if ( values[subject] & (AUX_VERB_TENSES|ADVERB_BITS)) --subject;

	if (roles[subject] == MAINOBJECT) // implied clause as direct object  "I hope (Bob will go)"
	{
		if (trace) Log(STDUSERLOG,"implied direct object clause @ %s(%d)\r\n",wordStarts[subject],subject);
		roles[subject] = SUBJECT2;
		roles[verb1] = VERB2;
		while (subject <= verb1) clauses[subject++] = clauseBit;
		clauseBit <<= 1;
		return true;	
	}
	return false;
}

static bool ProcessCommaClause(unsigned int verb1)
{
	if (!(roles[verb1] & CLAUSE)) return false; 
	if (trace) Log(STDUSERLOG,"Comma clause\r\n");
	roles[verb1] ^= CLAUSE;
	unsigned int subject = verb1;
	while (values[--subject] != COMMA);
	while (++subject <= verb1) clauses[subject] = clauseBit;
	clauseBit <<= 1;
	return true;	
}

static bool ProcessOmittedClause(unsigned int verb1) // They were certain (they were happy)
{
	if (clauses[verb1]) return false;	// already done
	unsigned int subject = verb1;
	while (subject)
	{
		if (values[--subject] & (NOUN_BITS | PRONOUN_BITS)) break;
		if (values[subject] & (AUX_VERB_TENSES|ADVERB_BITS)) continue;
		return false;
	}
	if (!subject) return false;
	roles[verb1] = VERB2;
	roles[subject] = SUBJECT2;
	while (subject <= verb1) clauses[subject++] = clauseBit;
	clauseBit <<= 1;
	return true;
}

static bool ProcessReducedPassive(unsigned int verb1,bool allowObject)
{
	if (clauses[verb1]) return false;	// already in a clause
	if (designatedObjects[verb1] && !allowObject)  // be pessimistic, we dont want the main verb taken over
	{
		// the man given the ball is ok.
		// but the man dressed the cow is not - AND normal verbs might have objects
		// but "called" "named" etc create subjects of their objects and are ok.
		return false;
	}

	// or a past participle clause immediately after a noun --  the woman dressed in red
	// Or past particple clause starting sentence (before subject)  Dressed in red the woman screamed.   OR  In the park dressed in red, the woman screamed
	// OR -- the men driven by hunger ate first (directly known participle past)
	char* base = GetInfinitive(wordStarts[verb1]);
	if (!base) return false;
	char* pastpart = GetPastParticiple(base);
	if (!pastpart) return false;
	if (stricmp(pastpart,wordStarts[verb1])) return false;	// not a possible past particple

	unsigned int before = verb1;
	if (values[verb1] & (VERB_PAST | VERB_PAST_PARTICIPLE)) 
	{
		while (--before > 0) // past tense needs to go to participle
		{
			if (values[before] & ADVERB_BITS) continue; // ignore this
			if (values[before] & (NOUN_BITS - NOUN_GERUND - NOUN_CARDINAL - NOUN_ORDINAL + PRONOUN_SUBJECT + PRONOUN_OBJECT)) 
			{
				if (phrases[before]) before = 0;	// cannot take object of prep and make it subject of clause directly
				break;
			}
			if (values[before] & COMMA) continue;
			return false;	// doesnt come after a noun or pronoun
		}
		// before might be 0 when clause at start of sentence
		if (trace) Log(STDUSERLOG,"reduced passive @ %s(%d)\r\n",wordStarts[verb1],verb1);
		roles[verb1] = VERB2;
		if (designatedObjects[verb1]) roles[designatedObjects[verb1]] = OBJECT2;
		values[verb1] = VERB_PAST_PARTICIPLE;
		clauses[verb1] = clauseBit;
		clauseBit <<= 1;
		return true;	
	}
	// alternative is we have coordinating sentences we didnt know about
	// defines the object, we come after main verb - We like the boy Eli *hated -   but "after he left home *I walked out
	// clause defines the subject, we come before main verb - The boy Eli hated *ate his dog - boy was marked as subject, and hated as mainverb.
	return false;
}


// commas occur after a phrase at start, after an inserted clause or phrase, and in conjunction lists, and with closing phrases

static void FinishSentenceAdjust(bool resolved,unsigned int hasSubject, unsigned int hasVerb, unsigned int hasObject,unsigned int hasIndirectObject,unsigned int hasIndirectObject2, unsigned int hasObject2)
{
	if (resolved)
	{
		int i = 0;
	}
	while (roleStack[roleIndex] & (OBJECT2|MAINOBJECT)) // close of sentence proves we get no final object... 
	{
		// indirects w/o direct, migrate them to new position
		if (roleStack[roleIndex] & OBJECT2 && hasIndirectObject2) // apparent indirect - fix it to be direct
		{
			hasObject2 = hasIndirectObject2;
			roles[hasObject2] = OBJECT2;
			hasIndirectObject2 = 0;
		}
		else if (roleStack[roleIndex] & MAINOBJECT && hasIndirectObject) // apparent indirect - fix it to be direct
		{
			hasObject = hasIndirectObject;
			roles[hasObject] = MAINOBJECT;
			hasIndirectObject = 0;
		}
		--roleIndex; // close out infinitive
		if (roleIndex == 0) // put back now.
		{
			++roleIndex;
			break;
		}
	}

	int commas = 0;
	for (unsigned int i = 1; i <= wordCount; ++i) if (values[i] == COMMA) ++commas;
	if (commas > 2) // presumed AND... ignore dual for now
	{
		verbIndex = 0;
	}

	if (verbIndex > 1)
	{
		unsigned int i;
		unsigned int mainverb = 0;
		unsigned int leftover = 0;
		for (i = 1; i <= verbIndex; ++i)
		{
			int verb = verbList[i];
			if ( i == verbIndex  && !mainverb) 	// have no main verb, let the final one be it
			{
				mainverb = verb;
				break;
			}
			ProcessCommaClause(verb); // clause bounded by commas?
			ProcessSplitNoun(verb); // see if is clause instead
			ProcessReducedPassive(verb,false); // we carefully avoid verbs with object in this pass
			ProcessImpliedClause(verb);
			if (!mainverb && !clauses[verb]) mainverb = verb;
			else if (!leftover && !clauses[verb]) leftover = verb;
		}

		// mark found main verb
		if (mainverb)
		{
			hasVerb = mainverb;
			roles[mainverb] = MAINVERB; 
			if ( designatedObjects[mainverb] ) roles[designatedObjects[mainverb]] = MAINOBJECT;
		}
		if (leftover) 
		{
			ProcessReducedPassive(leftover,true); // allow objects to be accepted
			ProcessOmittedClause(leftover);		  // whole seemingly second sentence
		}
	

		// found no main verb? resurrect a clause into main verb - prefer removal of clause modifying prep object
		if (!mainverb)
		{
			for (i = 1; i <= verbIndex; ++i)
			{
				int verb = verbList[i];
				if (clauses[verb] && phrases[verb-1])
				{
					hasVerb = verb;
					roles[verb] = MAINVERB; 
					if ( designatedObjects[verb] ) roles[designatedObjects[verb]] = MAINOBJECT;
					clauses[verb] = 0;
					break;
				}
			}
		}
	}	

	// sentence starting in qword as subject, flip subject and object
	if (canonicalLower[1] && canonicalLower[1]->properties & QWORD && roles[1] & MAINSUBJECT && hasSubject && hasObject)
	{
		// what (mainsubject) can i (object2) call you (mainobject)
		// will you (mainsubject)  throw me the ball-- normal order has no qword
		unsigned int subject = 0;
		while ( ++subject <= wordCount && !(values[subject] & AUX_VERB_TENSES));	// find helper verb
		if (subject < wordCount) // found
		{
			while (!roles[++subject] || !(values[subject] & (NOUN_BITS | PRONOUN_BITS))) 
			{
				if (subject == wordCount) // failed to find subject
				{
					subject = 0;
					break; // find marked noun,pronoun
				}
			}
		}
		else subject = 0;
		if (subject && subject != hasObject) // here is the real subject when we have indirect object as well
		{
			roles[subject] = MAINSUBJECT;
			roles[hasObject] = MAININDIRECTOBJECT;
			roles[hasSubject] = MAINOBJECT;
			hasSubject = subject;
		}
		else 
		{
			roles[hasObject] = MAINSUBJECT;
			roles[hasSubject] = MAINOBJECT;
			hasSubject = hasObject;
		}
	}
	// Flip subject/object - who broke the window -- object before subject    and  what is your name  
	else if (hasSubject && hasObject && hasObject < hasVerb)
	{
		roles[hasObject] = MAINSUBJECT;
		roles[hasSubject] = MAINOBJECT;
	}

	// migrate direct objects into clauses and verbals....
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		char* word = wordStarts[i];
		if (!designatedObjects[i]) continue; // nothing interesting
		unsigned int object;
		unsigned int at;
		if (verbals[i])
		{
			unsigned char verbal = verbals[i];
			unsigned char phrase = phrases[i];	// verbal might be object of a phrase
			at = i;

			// see if it has an object also...spread to cover that...
			while ((object = designatedObjects[at]))
			{
				for (unsigned int j = at+1; j <= object; ++j) 
				{
					verbals[j] = verbal;	//  "to eat *rocks"
					if (phrase) phrases[j] = phrase;
				}
				at = designatedObjects[at]; // extend to cover HIS object if he is gerund or infintiive
			}
		}
		if (clauses[i])
		{
			unsigned char clause = clauses[i];
			at = i;
			// see if it has an object also...spread to cover that...
			while ((object = designatedObjects[at]))
			{
				for (unsigned int j = at+1; j <= object; ++j) clauses[j] = clause;	// "when I ate *rocks"
				at = designatedObjects[at]; // extend to cover HIS object
			}
		}
		if (phrases[i])
		{
			unsigned char phrase = phrases[i];
			at = i;
			// see if it has an object also...spread to cover that... "after eating *rocks"
			while ((object = designatedObjects[at]))
			{
				for (unsigned int j = at+1; j <= object; ++j) phrases[j] = phrase;
				at = designatedObjects[at]; // extend to cover HIS object
			}
		}
	}

	
	// assign appositive nouns - near my cat Bill children played  ... but "the shawl people wear is green NOT appositive
	// done after objects are covered by verbal/clause marks
	unsigned int lastnoun = 0;
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		char* ptr = wordStarts[i];
		if (values[i] & (NOUN_SINGULAR|NOUN_PLURAL|NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL) && roles[i]) // a noun with a role (not a modifier of a noun)
		{
			// if this noun is starting a clause, it cannot be appositive
			if (clauses[i] && clauses[i-1] != clauses[i]) lastnoun = 0;
			if (roles[i] & (MAININDIRECTOBJECT|INDIRECTOBJECT2)) lastnoun = 0; // after I threw the red robin of the field the ball
			if (roles[i] & (MAINOBJECT)) lastnoun = 0; 
			if (lastnoun)
			{
				if  (roles[lastnoun] == MAININDIRECTOBJECT && roles[i] == MAINOBJECT)  lastnoun = i;
				else if (roles[lastnoun] == INDIRECTOBJECT2 && roles[i] == OBJECT2)  lastnoun = i;
				else if (verbals[i] && !verbals[lastnoun]) lastnoun = i; // cannot jump across
				else if (clauses[i] && !clauses[lastnoun]) lastnoun = i; // cannot jump across
				else
				{
					// dual nouns acting as indirectobject/object are not appositive
					if (roles[i] != OBJECT2 && roles[lastnoun] == OBJECT2) roles[lastnoun] = roles[i];
					roles[i] = APPOSITIVE;
					lastnoun = 0;
				}
			}
			else if (phrases[i]) lastnoun = 0; // Dont do them in phrases for now--- after I threw the red robin of the field the *ball
			else lastnoun = i;
		}
		else if (values[i] & PRONOUN_BITS) lastnoun = i;
		else if (values[i] & (VERB_TENSES|PREPOSITION|CONJUNCTION_COORDINATE|PAREN|CONJUNCTION_SUBORDINATE|TO_INFINITIVE|NOUN_INFINITIVE|NOUN_GERUND)) lastnoun = 0;
		else if (lastnoun && phrases[i] && phrases[lastnoun] != phrases[i]) lastnoun = 0;	// dont search INTO a non-prep time phrase. Can search OUT of a phrase though
		else if (lastnoun && clauses[i] && clauses[lastnoun] != clauses[i]) lastnoun = 0;	// dont search INTO a non-conjunct clause. Can search OUT of a clause though
	}

	// if we have OBJECT2 not in a clause or phrase.... maybe we misfiled it. "Hugging the ground, Nathan peered."
	if (resolved) for (unsigned int i = 1; i <= wordCount; ++i)
	{
		if (roles[i] != OBJECT2 || phrases[i] || clauses[i] || verbals[i]) continue;
		if (hasSubject && hasSubject < i && values[hasSubject] == NOUN_GERUND) // the Verbal will be an adjective phrase on the main subject
		{
			roles[i] = MAINSUBJECT;
			roles[hasSubject] = SUBJECT2;
		}
		// if subject is 1st word, after it is clause, and comma, then our phrase, migrate into clause -- those dressed in red, the men ate first
		if (hasSubject == 1 && clauses[2] && i < hasVerb) // just assume it is right for now if is before main verb
		{
			roles[hasSubject] = SUBJECT2;
			clauses[1] = clauses[2];
			roles[i] = MAINSUBJECT;
			hasSubject = i;
		}
	}

	// assign adjective objects
	if (hasVerb && canonicalLower[hasVerb] && canonicalLower[hasVerb]->systemFlags & VERB_ADJECTIVE_OBJECT) 
	{
		for (unsigned int i = hasVerb+1; i <= wordCount; ++i)
		{
			if (clauses[i] || verbals[i] || phrases[i]) continue;	// not relevant (though detecting these in a clause would be nice)
			if (values[i] & (POSSESSIVE|PRONOUN_POSSESSIVE|DETERMINER)) break;	// not "he is a green man"
			if (values[i] & ADJECTIVE_BITS) // he is green. but not they are green and sticky men
			{
				if (values[i-1] & (NOUN_BITS | PRONOUN_BITS) && roles[i-1] == MAINOBJECT ) 
				{
					roles[i] = OBJECT_COMPLEMENT;
					continue; // not for object complements:  they considered him crazy  
				}
				unsigned int j = i;
				while (++j <= wordCount) // prove no nouns in our way after this -- are you the scared cat  vs  are you afraid of him
				{
					if (clauses[j] || verbals[j] || phrases[j]) break;	// not relevant (though detecting these in a clause would be nice)
					if ( values[j] & (PAREN | COMMA | CONJUNCTION_BITS)) break;
					if (values[j] & NOUN_BITS)  //  but not they are green men 
					{
						i = wordCount + 1; // fall off
						break;
					}
				}
				if ( i <= wordCount) roles[i] = ADJECTIVE_AS_OBJECT;
			}
		}
	}
}

int FindCoordinate(int i) // i is on comma
{
	bool comma2 = false;
	while ((unsigned int) ++i < wordCount)
	{
		if (values[i] & COMMA) comma2 = true; // there is a comma
		if (values[i] == CONJUNCTION_COORDINATE)
		{
			if (comma2) return i; // conjunction after a second comma
		}
	}
	return 0; // didnt find
}

void HandleConjunct(int i,int hasSubject, int hasVerb) // determine kind of conjunction
{
	unsigned int before = i - 1;
	unsigned int after = i + 1;
	if ( values[after] == CONJUNCTION_COORDINATE) ++after;	// comma then conjunct, skip conjunct
	int deepAfter = after;
	while (values[deepAfter] & ADVERB_BITS) ++deepAfter;	// ignore adverbs see what comes after..
	int deepBefore = before;
	while ( ( values[deepBefore] & NOUN_BITS) ) // back before any prep phrase
	{
		int at = deepBefore;
		while (values[at] & (DETERMINER|PREDETERMINER|NOUN_BITS|ADJECTIVE_BITS|ADVERB_BITS)) --at;
		if ( values[at] & PREPOSITION) deepBefore = at - 1;	// before this phrase (presuming 
		else break;
	}

	if (values[before] & (ADJECTIVE_BITS|ADVERB_BITS) && values[after] & (ADJECTIVE_BITS|ADVERB_BITS)) 
	{
		roles[i] = CONJUNCT_WORD;	
		coordinates[i] = before;
	}
	else if ( values[after] & ADJECTIVE_BITS && values[deepBefore] & ADJECTIVE_BITS) 
	{
		roles[i] = CONJUNCT_WORD;	
		coordinates[i] = before;
	}
	else if (values[before] & NOUN_BITS && values[after] & (NOUN_BITS|DETERMINER|ADJECTIVE_BITS)) 
	{
		roles[i] = CONJUNCT_WORD;
		while (!(values[after] & NOUN_BITS) && after <= wordCount) ++after;
		unsigned char phrase = phrases[before];
		if (after <= wordCount) 
		{
			roles[after] = roles[before];	// duplicate the role info
			coordinates[i] = before;
		}
	}
	else if (values[before] & (VERB_TENSES|NOUN_INFINITIVE) && values[after] & (VERB_TENSES|NOUN_INFINITIVE))  
	{
		roles[i] = CONJUNCT_WORD;
		coordinates[i] = before;
		roles[after] = roles[before];	// duplicate the role info
	}
	else if (values[before] & PARTICLE && values[after] & (VERB_TENSES|NOUN_INFINITIVE))  // he stood up and ran down the street
	{
		roles[i] = CONJUNCT_WORD;
		while (--before > 2) if ( values[before] & (VERB_TENSES|NOUN_INFINITIVE)) break;
		roles[after] = roles[before];	// duplicate the role info
		coordinates[i] = before;
	}
	else if ( values[after] & PREPOSITION && values[before] & (NOUN_BITS|PRONOUN_BITS))  
	{
		int r = before;
		while (--r) // prove phrase matching going on
		{
			if (! (values[r] & (NOUN_BITS|DETERMINER|PREDETERMINER|ADJECTIVE_BITS|ADVERB_BITS))) break;
		}
		if ( values[r] & PREPOSITION) 
		{
			roles[i] = CONJUNCT_PHRASE;
			coordinates[i] = r;
		}
		if (trace) Log(STDUSERLOG,"DuplicatePhrase");
	}
	else if (values[deepAfter] & VERB_TENSES && hasVerb) // we and to a verb... expect this to be WORD
	{
		roles[i] = CONJUNCT_WORD;
		unsigned int verb = i;
		while (--verb && !(values[verb] & VERB_TENSES)); // find verb before
		roles[deepAfter] = roles[verb];	// copy verb role from before
		coordinates[i] = verb;
		if (roles[deepAfter] == MAINVERB && canonicalLower[i] && canonicalLower[i]->systemFlags & (VERB_DIRECTOBJECT|VERB_INDIRECTOBJECT)) // change to wanting object(s)
		{
			roleStack[1] |= MAINOBJECT;
			if ( canonicalLower[i]->properties & VERB_INDIRECTOBJECT) roleStack[1] |= MAININDIRECTOBJECT;
			if (trace) Log(STDUSERLOG,"DuplicateVerb");
		}
	}
	else if (hasSubject && hasVerb)
	{
		roles[i] = CONJUNCT_SENTENCE;
		if (trace) Log(STDUSERLOG,"DuplicateSentence");
	}
	if (trace) Log(STDUSERLOG,"Conjunct=%s(%d) ",wordStarts[i],i);
}

static void CloseLevel(int  i)
{
	while (roleStack[roleIndex] && !(roleStack[roleIndex] & (-1 ^ KINDS_OF_PHRASES)) ) 
	{
		--roleIndex;	// level complete (not looking for anything anymore)
		if (roleIndex == 0)
		{
			++roleIndex;
			break;
		}
	}
}

#ifdef INFORMATION
A roleStack value indicates what we are currently working on (nested).
Base level is always the main sentence. Initially subject and verb. When we get a verb, we may add on looking for object and indirect object.
	Other levels may be:
1. phrase (from start of phrase which is usually a preposition but might be omitted) til noun ending phrase
but-- noun might be gerund in which case it extends to find objects of that.
2. clause (from start of clause which might be omitted til verb ending clause
but -- might have objects which extend it, might have gerunds which extend it (and it will include prep phrases attached at the end of it - presumed attached to ending noun)
3. Infinitive and gerund - adds a level to include potential objects. 

The MarkPrep handles finding and marking basic phrases and clauses. AssignRoles extends those when the system "might" find an object.
It will backpatch later if it finds it swalllowed a noun into the wrong use.
#endif

static bool AssignRoles()
{
	if ((tokenControl & DO_PARSE) == 0) return false;	// dont assume it is sentences

	if (trace) Log(STDUSERLOG,"---- Assign roles\r\n");
	bool resolved = true;
	int beVerb = 0;
	// main sentence components
	int hasSubject = false;		
	int hasVerb = false;	
	int hasObject = false;	
	int hasIndirectObject = false;	

	// components of phrases/clauses/gerunds/infinitives
	int hasObject2 = false;
	int hasIndirectObject2 = false;

	// punctuations
	int startComma = 0;
	int endComma = 0;
	int commalist = false;
	verbIndex = 0;

	// After hitting Sue, who is  the one person that I love among all the people I know who can walk, the ball struck a tree.
	memset(roles,0,(wordCount+2) * sizeof(int));
	roleStack[0] = COMMA_PHRASE;		// a non-zero level
	roleIndex = 0;
	roleStack[++roleIndex] = MAINVERB|MAINSUBJECT;	// we want this
	verbStack[roleIndex] = 0;
	lastClause = lastVerbal = 0;
	for (int i = 1; i <= (int)wordCount; ++i)
	{
		char* word = wordStarts[i];
		if (phrases[i]) 
		{
			if (roleStack[roleIndex] & CLAUSE) 
			{
				--roleIndex;	// clause will never have direct object after this
				if (roleIndex == 0) ++roleIndex;
			}
			if (phrases[i] != phrases[i-1])  
			{
				roleStack[++roleIndex] = OBJECT2|PREPPHRASE; // start of phrase seeking object
				verbStack[roleIndex] = 0;
			}
		}
		if (clauses[i]) 
		{
			if (clauses[i] != clauses[i-1])  
			{
				roleStack[++roleIndex] = SUBJECT2|VERB2|CLAUSE; // start of a clause triggers a new level for it, looking for subject and verb (which will be found).
				verbStack[roleIndex] = 0;
			}
			lastClause = i;				// where last clause was, if any
		}
		if (verbals[i]) 
		{
			if (verbals[i] != verbals[i-1])  
			{
				roleStack[++roleIndex] = VERB2|INFINITIVE; // start of a clause triggers a new level for it, looking for subject and verb (which will be found).
				verbStack[roleIndex] = 0;
			}
			lastVerbal = i;				// where last verbal was, if any
		}

		if (bitCounts[i] == 1) // we know what POS it is unambiguously
		{
//----------------------------------------
			if (values[i] &  VERB_TENSES) // does not include NOUN_INFINITIVE or NOUN_GERUND or ADJECTIVE_PARTICIPLE - its a true verb (not aux)
			{
				verbStack[roleIndex] = i;	// current verb
				
				// when an infinitive is waiting to close because it wants an object, this MAIN verb
				// this might be wrong if you can insert a clause in there somewhere 
				if (roleStack[roleIndex] & OBJECT2)
				{
					roleStack[roleIndex] &= -1 ^ ( OBJECT2 | INDIRECTOBJECT2);	// we wont find these any more
					CloseLevel(i-1);
				}

				if (canonicalLower[i] && !stricmp(canonicalLower[i]->word,"be"))	beVerb = i;

				// a real verb will terminate a bunch of things
				//To sleep *is (no objects will be found)

				if (roles[i]); // already have a role for this verb.
				else if (roleStack[roleIndex] & VERB2) // verb is inside a clause, probably clausal verb we expected
				{
					roles[i] = VERB2;
					roleStack[roleIndex] &=  -1 ^ (SUBJECT2|VERB2);
				}
				else if ( hasSubject < startComma && i < endComma) // saw a subject, now in a comma phrase, wont be main verb (except it might be- hugging the wall, nathan *peered 
				{
					verbList[++verbIndex] = i;	// we KNOW it will be a phrase, backwards to comma
					roles[i] = VERB2;
					roleStack[++roleIndex] = COMMA_PHRASE;
					verbStack[roleIndex] = 0;
				}
				else if (roleStack[roleIndex] & MAINVERB)
				{
					hasVerb = i;
					verbList[++verbIndex] = i;
					roles[i] = MAINVERB;
					roleStack[roleIndex] ^=  MAINVERB;
					// "are you" questions do not abandon subject yet, same for non-aux have -- but not are as AUX
					if (roleStack[roleIndex] & MAINSUBJECT && canonicalLower[i] && (!stricmp(canonicalLower[i]->word,"be")||!stricmp(canonicalLower[i]->word,"have"))); // no subject yet
					else roleStack[roleIndex] &= -1 ^ MAINSUBJECT;	// can no longer be seeking a subject, maybe it was implied. -- but "be cool" wont work - bug
				}
				else 
				{
					if ( verbIndex) verbList[++verbIndex]  = i;
					if (roleStack[roleIndex] & (MAINOBJECT)) // another verb?
					{
						roleStack[++roleIndex] = CLAUSE;
						verbStack[roleIndex] = i;
					}
				}

				// see if verb might take an object
				if (values[i] & VERB_PAST_PARTICIPLE && values[i-1] & AUX_VERB_TENSES && clauses[i]); // after the dog had been walked (passive takes no objects) vs After he walked the dog
				else if (canonicalLower[i] && canonicalLower[i]->systemFlags & (VERB_DIRECTOBJECT|VERB_INDIRECTOBJECT)) // change to wanting object(s)
				{
					if (roles[i] & MAINVERB)
					{
						if (canonicalLower[i]->systemFlags & VERB_DIRECTOBJECT)  roleStack[roleIndex] |= MAINOBJECT;
					}
					else roleStack[roleIndex] |=  OBJECT2;

					if (canonicalLower[i]->systemFlags & VERB_INDIRECTOBJECT) 
					{
						if ( !(values[i+1] & DETERMINER | PREDETERMINER | ADJECTIVE | ADVERB | PRONOUN_OBJECT | NOUN_BITS)); // must come immediately
						else if (roles[i] & MAINVERB) roleStack[roleIndex] |= MAININDIRECTOBJECT;
						else roleStack[roleIndex] |= INDIRECTOBJECT2;
					}
				}
			}
//----------------------------------------
			else if (values[i] & (NOUN_BITS|NOUN_INFINITIVE|PRONOUN_BITS)) // nouns & gerunds, pronouns, infinitives
			{
				bool finalNoun = false;
				if (values[i] & (PRONOUN_BITS)) finalNoun = true;
				else if (values[i] & (NOUN_PROPER_SINGULAR || NOUN_PROPER_PLURAL) && !(values[i+1] & POSSESSIVE)) finalNoun = true; 
				else if (values[i] & ( NOUN_INFINITIVE|NOUN_GERUND)) finalNoun = true; // can also accept objects
				else if (!(values[i+1] & (POSSESSIVE|(NOUN_BITS-NOUN_GERUND)))) finalNoun = true;
				else if (canonicalUpper[i+1]) // no noun will precede a proper name? 
				{
					WORDP D = canonicalUpper[i+1];
					if (D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL)) finalNoun = true;	// this will be noun appositive or omitted preposition - "the thing Eli wanted" or "my brother Billy"
				}
				else if (!clauses[i] && clauses[i+1]) finalNoun = true;	// we break on clause boundary
				else if (bitCounts[i+1] != 1) finalNoun = true;	// we must assume it for now...  "eating children tastes good

				// something wanting object will not get it, since this is a subject pronoun
				if (roleStack[roleIndex] & OBJECT2 && values[i] & PRONOUN_SUBJECT) 
				{
					int object = designatedObjects[verbStack[roleIndex]];
					if (object) roles[object] = OBJECT2; // change back
					roleStack[roleIndex] &= -1 ^ ( OBJECT2 | INDIRECTOBJECT2 );
					--roleIndex;
					if (roleIndex == 0) ++roleIndex;
					lastClause = 0;
					lastVerbal = 0;
				}
				
				if (!finalNoun);
				else if (roles[i]);	// role assigned by conjunction-word( but what happens to level changes?bug)
				else if (markers[i] & BE_OBJECT2) roles[i] = OBJECT2; // omitted prep phrase (time)
				else if (values[i] & NOUN_INFINITIVE && roleStack[roleIndex] & MAINVERB) // COMMAND-  "be happy"  but not "are you able to drive a car
				{
					verbList[++verbIndex]  = i;
					roles[i] = MAINVERB;
					roleStack[roleIndex] &= -1 ^ (MAINVERB|MAINSUBJECT);
				}
				// sentence ending in preposition, with Qword at front, qword becomes OBJECT2--- Eg. who are you afraid of -- but not what dog are you afraid of
				else if (i == 1 && values[1] & PRONOUN_BITS && values[wordCount] == PREPOSITION  && canonicalLower[i] && canonicalLower[i]->properties & QWORD )
				{
					roles[i] = OBJECT2;
				}
				// sentence starting with qword determiner (which and what for example) on this noun
				else if ( i == 2 && values[1] & DETERMINER && canonicalLower[1]->properties & QWORD)
				{
					roles[i] = OBJECT2;	// a question...
				}
				else if (roleStack[roleIndex] & INDIRECTOBJECT2 )
				{
					roles[i] = INDIRECTOBJECT2;
					hasIndirectObject2 = i; // we need direct also
					hasObject2 = false;
					roleStack[roleIndex] ^= INDIRECTOBJECT2;
					if (verbStack[roleIndex]) designatedObjects[verbStack[roleIndex]] = i; // dont disable geenric, in case we find object also
				}
				else if (roleStack[roleIndex] & MAININDIRECTOBJECT )
				{
					if (verbStack[roleIndex]) designatedObjects[verbStack[roleIndex]] = i;
					roles[i] = MAININDIRECTOBJECT;
					hasIndirectObject = i;
					roleStack[roleIndex] ^= MAININDIRECTOBJECT;
				}
				else if (roleStack[roleIndex] & VERB2 && values[i] & (NOUN_GERUND|NOUN_INFINITIVE)) // complete verb in verb phrase, maybe we wanted it as object?
				{
					int toLoc;
					if (values[i] & NOUN_INFINITIVE) // find the "to"
					{
						toLoc = i;
						while (--toLoc && values[toLoc] != TO_INFINITIVE);	// back up.
						if (toLoc) --toLoc;
					}
					// need to know if this is supplying an object or is describing a noun...
					// EG  I want to eat   vs  I want chitlins to eat ...
					if (values[i] & NOUN_INFINITIVE && roles[toLoc] & (OBJECT2|MAINOBJECT)) // this is describing a noun, not being one
					{
						roles[i] = VERB2;
					}
					else if (roleIndex > 1 && roleStack[roleIndex-1] & (MAINOBJECT|OBJECT2)) // as an object, it cant ever be an indirect object
					{
						// attach this as the object of the phrase, or of the verb...  I like after my shift *waiting tables to eat
						if ( roleStack[roleIndex-1] & PREPPHRASE && roleStack[roleIndex-1] & OBJECT2) roles[i] = OBJECT2;	// completes prep phrase underneath of verbal
						else if ( phrases[i-1]) roles[i] = SUBJECT2; // attaches to noun before in the phrase before
						else if (verbStack[roleIndex-1]) // filling in a verb from a while ago
						{
							designatedObjects[verbStack[roleIndex-1]] = i; // dont disable geenric, in case we find object also
							roles[i] = roleStack[roleIndex-1] & (MAINOBJECT|OBJECT2);
							if ( roleStack[roleIndex-1] & MAINOBJECT) hasObject = i;
							else hasObject2 = i;
							roleStack[roleIndex-1] &= -1 ^ (INDIRECTOBJECT2|OBJECT2|MAINOBJECT|MAININDIRECTOBJECT);
						}
						else roles[i] = SUBJECT2;	// generic verb clause describing something before (like the preceeding noun)
						beVerb = 0; // dont end "beverb" because of possible inverted subject-- "are you xxx"
					}
					else if (roleIndex > 1 && roleStack[roleIndex-1] & (MAINSUBJECT|SUBJECT2)) // as a subject
					{
						if (verbStack[roleIndex-1]) designatedObjects[verbStack[roleIndex-1]] = i; // dont disable geenric, in case we find object also
						roles[i] = roleStack[roleIndex-1] & (MAINSUBJECT|SUBJECT2);
						if ( roleStack[roleIndex-1] & MAINSUBJECT) hasSubject = i;
						roleStack[roleIndex-1] &= -1 ^ (MAINSUBJECT|SUBJECT2);
						
						beVerb = 0; // dont end "beverb" because of possible inverted subject-- "are you xxx"
					}
					else roles[i] = VERB2;	// boring role
					roleStack[roleIndex] &= -1 ^ VERB2;
				}
				else if (roleStack[roleIndex] & OBJECT2 )
				{
					roles[i] = OBJECT2;
					hasObject2 = i;
					roleStack[roleIndex] &= -1 ^ (INDIRECTOBJECT2|OBJECT2);
					beVerb = 0; // dont end "beverb" because of possible inverted subject-- "are you xxx"
					if (verbStack[roleIndex])  designatedObjects[verbStack[roleIndex]] = i; // verb knows it has an object
				}
				else if (roleStack[roleIndex] & MAINOBJECT && values[i] & PRONOUN_BITS && canonicalLower[i] && canonicalLower[i]->properties & QWORD)	// is a question pronoun (eg what) so it not a subject, though WHO might be
				{ // becomes main object even w/o subject yet
					hasObject = i;
					roles[i] = MAINOBJECT;
					roleStack[roleIndex] ^= MAINOBJECT;
					beVerb = 0; // dont end "beverb" because of possible inverted subject-- "are you xxx"
				}
				else if (roleStack[roleIndex] & SUBJECT2)
				{
					roles[i] = SUBJECT2;
					roleStack[roleIndex] ^= SUBJECT2;
				}
				else if (roleStack[roleIndex] & MAINSUBJECT)
				{
					hasSubject = i;
					roles[i] = MAINSUBJECT;
					roleStack[roleIndex] ^= MAINSUBJECT;
				}
				else if (roleStack[roleIndex] & MAINOBJECT)
				{
					hasObject = i;
					roles[i] = MAINOBJECT;
					roleStack[roleIndex] &= -1 ^ (MAINOBJECT|MAININDIRECTOBJECT);
					beVerb = 0; // dont end "beverb" because of possible inverted subject-- "are you xxx"
				}
				else if (values[i] & NOUN_INFINITIVE) 
				{
					roles[i] = VERB2;
					roleStack[++roleIndex] = INFINITIVE;
					verbStack[roleIndex] = i;
				}
				else // probably  omitted prep time phrase
				{
					roles[i] = OBJECT2;
					lastClause = 0;
					hasObject2 = i;
				}

				// extend reach for supplemental objects
				if (values[i] & ( NOUN_INFINITIVE|NOUN_GERUND)) 
				{
					if (canonicalLower[i] && !stricmp(canonicalLower[i]->word,"be"))	beVerb = i;
					
					if (canonicalLower[i] && canonicalLower[i]->systemFlags & (VERB_DIRECTOBJECT|VERB_INDIRECTOBJECT)) // change to wanting object(s)
					{
						verbStack[roleIndex] = i;	// this is the verb for this
						if (roles[i] != MAINVERB) roleStack[roleIndex] |= OBJECT2; // mark this kind of level (infinitive or gerund) UNLESS this is actually the main verb
							// are you able to drive a car wants object2 for car
						else roleStack[roleIndex] |=  MAINOBJECT; // "Be a man"
						if (canonicalLower[i]->systemFlags & VERB_INDIRECTOBJECT) 
						{
							if ( !(values[i+1] & DETERMINER | PREDETERMINER | ADJECTIVE | ADVERB | PRONOUN_OBJECT | NOUN_BITS)); // must come immediately
							else if (roles[i] == MAINVERB) roleStack[roleIndex] |= MAININDIRECTOBJECT;
							else roleStack[roleIndex] |= INDIRECTOBJECT2;
						}
					}
					else lastClause = lastVerbal = 0;
				}
			}
//----------------------------------------
			else if (values[i] & AUX_VERB_TENSES && !(roleStack[1] & MAINSUBJECT)) // see if this is an inverted subject question where we think it was filled in already
			{
				//  question words at start of sentence mean we didnt get a subject yet, we got the object of something-- which are you attached to?
			}
//----------------------------------------
			else if (values[i] & PREPOSITION)
			{
				beVerb = 0;
				if (roleStack[roleIndex-1] & MAINOBJECT && hasSubject > hasVerb); // if main subject comes after main verb (question), then prep phrase cancels nothing
				else if (!(roleStack[roleIndex] & PREPPHRASE) && roleStack[roleIndex-1] & (OBJECT2|MAINOBJECT) && !designatedObjects[verbStack[roleIndex-1]]) // phrase should not intervene before direct object unless describing indirect object
				{
					roleStack[roleIndex-1] &= -1 ^ (INDIRECTOBJECT2|MAININDIRECTOBJECT|OBJECT2|MAINOBJECT);
				}
			}
//----------------------------------------
			else if (values[i] & CONJUNCTION_COORDINATE)
			{
				beVerb = 0;
				if (roles[i-1] && values[i-1] & COMMA);	// conjuct put on the comma instead
				else
				{
					HandleConjunct(i,hasSubject,hasVerb);
					if (roles[i] == CONJUNCT_SENTENCE) // we want a whole new sentence
					{
						FinishSentenceAdjust(false,hasSubject,hasVerb,hasObject,hasIndirectObject,hasIndirectObject2,hasObject2);
						hasSubject = hasVerb = hasObject = hasIndirectObject = 0;
						lastClause = 0;
						lastVerbal = 0;
						verbIndex = 0;
						roleIndex = 0;
						roleStack[++roleIndex] = MAINVERB|MAINSUBJECT|MAINOBJECT;	// we want this
						verbStack[roleIndex] = 0;
					}
				}
			}
			else if (values[i] & CONJUNCTION_SUBORDINATE) // new clause
			{
				beVerb = 0;
				lastVerbal = 0;
				if (roleStack[roleIndex] & (OBJECT2|MAINOBJECT) && !designatedObjects[verbStack[roleIndex]]) // clause should not intervene before direct object unless describing indirect object
				{
					roleStack[roleIndex] &= -1 ^ (INDIRECTOBJECT2|MAININDIRECTOBJECT|OBJECT2|MAINOBJECT);
				}
			}
//----------------------------------------
			else if (values[i] & ADJECTIVE_PARTICIPLE && values[i-1] & NOUN_BITS) // note- adjective-participles may occur AFTER the noun and have objects but not when before - we have the people *taken care of
			{
					if (canonicalLower[i] && canonicalLower[i]->systemFlags & (VERB_DIRECTOBJECT|VERB_INDIRECTOBJECT)) // change to wanting object(s)
					{
						// wont have indirect object we presume
						verbStack[roleIndex] = i;	// this is the verb for this
						roleStack[roleIndex] |= OBJECT2; // mark this kind of level (infinitive or gerund) UNLESS this is actually the main verb
					}
					else lastClause = lastVerbal = 0;
			}
			else if (values[i] & ADJECTIVE_BITS && beVerb) // adjective object completes a verb
			{
				designatedObjects[beVerb] = i; // EXTEND Phrases and clauses and verbals to cover adjective object
				roles[i] = roleStack[roleIndex] & (OBJECT2|INDIRECTOBJECT2);
				roleStack[roleIndex] &= -1 ^ (MAINOBJECT|MAININDIRECTOBJECT|OBJECT2|INDIRECTOBJECT2); 
				beVerb = 0;
			}
			else if (values[i] & (DETERMINER|PREDETERMINER|ADJECTIVE_BITS|TO_INFINITIVE))
			{
				beVerb = 0;
			}
//----------------------------------------
			else if (values[i] & (ADVERB_BITS | PARTICLE) ) // adverb or particle binds to PRIOR verb, not next verbFR
			{
				if (clauses[i-1]) clauses[i] = clauses[i-1];
				if (verbals[i-1]) verbals[i] = verbals[i-1];
				if ( phrases[i-1]  && !phrases[i] && values[i-1] & NOUN_GERUND) phrases[i] = phrases[i-1];
			}
//----------------------------------------
			else if ( values[i] & COMMA && values[i-1] & ADJECTIVE_BITS && values[i+1] & (NOUN_BITS|ADVERB_BITS|ADJECTIVE_BITS,CONJUNCTION_COORDINATE)) // MAY NOT close needs for indirect objects, ends clauses because adjectives separated by commas) - hugging the cliff, Nathan follows the narrow, undulating road.
			{
				int j = 0;
			}
			else if ( values[i] & COMMA) // close needs for indirect objects, ends clauses e.g.   after being reprimanded, she went home
			{
				beVerb = 0;
				if (roleStack[roleIndex] & OBJECT2) roleStack[roleIndex] &= -1 ^ (INDIRECTOBJECT2 | OBJECT2);
				// one might have a start of sentence commas off from rest, like: After I arrived, he left and went home.
				// one might have a middle part like: I find that often, to my surprise, I like the new food.
				// one might have a tail part like:  I like you, but so what?
				// or comma might be in a list:  I like apples, pears, and rubbish.
				if (!startComma) // we have a comma, find a close.  if this is a comma list, potential 3d comma might exist
				{
					startComma = i;
					for (unsigned int j = i+1; j < wordCount; ++j) 
					{
						if (values[j] & COMMA) // this comma ends first here
						{
							endComma = j;
							break;
						}
					}
				}
				// if we already had a comma and this is the matching end we found for it and we are in a comma-phrase, end it.
				else if (i == endComma && roleStack[roleIndex] & COMMA_PHRASE) 
				{
					--roleIndex; // end phrase
					if (roleIndex == 0) ++roleIndex;
				}

				// commas end some units like phrases, clauses and infinitives
				lastClause = 0;
				lastVerbal = 0;

				if (!commalist) // was this a comma list (1st time init)
				{
					if (FindCoordinate(i)) commalist = 1; // yes
					else commalist = -1;		// never happens
				}
				if (commalist == 1) // treat this as coordinate conjunction (but skip over possible adj conjunct)
				{
					HandleConjunct(i,hasSubject,hasVerb);
					if (roles[i] == CONJUNCT_SENTENCE) // start a new sentence now, after cleanup of old sentence
					{
						FinishSentenceAdjust(false,hasSubject,hasVerb,hasObject,hasIndirectObject,hasIndirectObject2,hasObject2);
						hasSubject = hasVerb = hasObject = hasIndirectObject = hasObject2 = hasIndirectObject2 = 0;
						verbIndex = roleIndex = 0;
						roleStack[++roleIndex] = MAINVERB|MAINSUBJECT;	// start new sentence
						verbStack[roleIndex] = 0;
					}
				}
				else if (roleStack[roleIndex] & OBJECT2) roleStack[roleIndex] &= -1 ^ (INDIRECTOBJECT2 | OBJECT2);
			}

			CloseLevel(i);
	
			if (trace)
			{
				Log(STDUSERLOG,"-> \"%s\" roleIndex:%d lastc:%d lastv:%d \r\n",word,roleIndex,lastClause,lastVerbal);
				Log(STDUSERLOG,"          subject:%d verb:%d indirectobject:%d object:%d\r\n",hasSubject,hasVerb,hasIndirectObject,hasObject);
				for (unsigned int x = roleIndex; x >= 1; --x)
				{
					if (roleStack[x] & PREPPHRASE) Log(STDUSERLOG," Phrase ");
					if (roleStack[x] & CLAUSE) Log(STDUSERLOG," Clause ");
					if (roleStack[x] & INFINITIVE) Log(STDUSERLOG," Infinitive ");
					if (!(roleStack[x] & (PREPPHRASE|CLAUSE|INFINITIVE))) Log(STDUSERLOG," Main ");

					if (roleStack[x] & MAINSUBJECT) Log(STDUSERLOG," MainSubject ");
					if (roleStack[x] & MAINVERB) Log(STDUSERLOG," MainVerb ");
					if (roleStack[x] & SUBJECT2) Log(STDUSERLOG," Subject2 ");
					if (roleStack[x] & VERB2) Log(STDUSERLOG," Verb2 ");
					if (roleStack[x] & INDIRECTOBJECT2) Log(STDUSERLOG," Io2 ");
					if (roleStack[x] & OBJECT2) Log(STDUSERLOG," Obj2 ");
					if (roleStack[x] & MAININDIRECTOBJECT) Log(STDUSERLOG," MainIO ");
					if (roleStack[x] & MAINOBJECT) Log(STDUSERLOG," Mainobj ");
					Log(STDUSERLOG,"\r\n");
				}
			}
			
			//----------------------------------------
		} // end if bitcounts == 1
		else // we are not sure // no more analysis is valid
		{
			resolved = false;
			lastClause = lastVerbal =  0;	
			break;
		}
	}
	
	if (trace)
	{
		Log(STDUSERLOG,"PreFinishSentence\r\n");
		DumpPrepositionalPhrases();
	}

 	FinishSentenceAdjust(resolved,hasSubject,hasVerb,hasObject,hasIndirectObject,hasIndirectObject2,hasObject2);
	posStatus = 0;
	if (!hasVerb) posStatus = MAINVERB;
	return resolved;
}

static char* BitLabels(uint64 properties)
{
	static char buffer[MAX_WORD_SIZE];
	*buffer = 0;
	char* ptr = buffer;
	uint64 bit = 0x8000000000000000LL;		//   starting bit
	while (properties)
	{
		if (properties & bit)
		{
			properties ^= bit;
			char* label = FindNameByValue(bit);
			sprintf(ptr,"%s+",label);
			ptr += strlen(ptr);
		}
		bit >>= 1;
	}
	if (ptr > buffer) *--ptr = 0;
	return buffer;
}

void MarkPrepPhrases() // FindPrep
{// finds the MINIMAL  clause or verbal (ending at verb) but the maximal phrase (ending at last consecutive noun).
	// These are not redone each time; they are permanent once found
	// AssignRoles is responsible for extending things for trailing adjectives, objects of gerunds, etc and
	// some other place will remove extra nouns not part of a phrase.
	unsigned int start = 0;
	unsigned int end = 0;
	int startClause = 0;
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		char* word = wordStarts[i];
		if (bitCounts[i] != 1)
		{
			startClause = start = 0; // cancel
			continue;
		}

		// find phrases (preposition start, ending in noun)
		if (!phrases[i])
		{
			//  (assumes no prep phrase can occur within a prep phrase)
			if ( values[i] == PREPOSITION) 
			{
				if ( i == wordCount) // sentences ends in prep (maybe continues to start)
				{
					phrases[i] = prepBit;
					prepBit <<= 1;
				}
				else start = i;
			}
			else if (!start);
			else if (values[i] & (PRONOUN_BITS|NOUN_BITS)) // cant have "after to eat" pseudo noun -- cannot to "to_infintiive" after preposition
			{
				// extend noun for now on a completed phrase.... if we need to separate them, this will happen in FinishSentence
				if (values[i+1] & ((NOUN_BITS|POSSESSIVE_BITS) - NOUN_GERUND))
				{
					if (bitCounts[i+1] != 1) start = 0;	// cancel and wait for better data
					// wait for a better choice -- we have "after *bob's dog" or "after the *bank teller" - if we swallow too much consecutive nouns, we will patch later
				}
				else end = i; // end of prep phrase

				if (end == i) // mark this zone as belong to this prep phrase 
				{
					roles[i] = OBJECT2;	// role is always OBJECT2
					for (unsigned int j = start; j <= end; ++j)
					{
						phrases[j] = prepBit;
					}
					prepBit <<= 1;
					start = end = 0;
				}
			}
			else if (values[i] & (ADVERB_BITS|DETERMINER|PREDETERMINER|ADJECTIVE_BITS|POSSESSIVE|PRONOUN_POSSESSIVE));	// normal
			else start = 0;	// cancel it, its no good for some reason
		}

		// find clauses  ( markers start, ending with a verb)
		if (!clauses[i])
		{
			if (i == 1 && canonicalLower[i] && canonicalLower[i]->properties & QWORD && values[2] & (VERB_TENSES|AUX_VERB_TENSES)) continue; // qwords are not clauses but main sentence-- what is my name, EXCEPT "when the"

			//  (assumes no prep phrase can occur within a prep phrase)
			if ( values[i] == CONJUNCTION_SUBORDINATE)
			{
				if (trace) Log(STDUSERLOG,"%s clause - subordinate conjunction?\r\n",wordStarts[i]);
				startClause = i;
			}
			// SUBJECT clause starters immediately follow a noun they describe
			// OBJECT clause starters arise in any object position and may even be ommited  -This is the man (whom/that) I wanted to speak to . The library didn't have the book I wanted
			// The book whose author won a Pulitzer has become a bestseller.
			// clause starter CANNOT be used as adjective... can be pronoun or can be possessive determiner (like whose)
			// USE originalLower because "that" becomes "a" when canonical
			else if (originalLower[i] && originalLower[i]->systemFlags & POTENTIAL_CLAUSE_STARTER && !(values[i] & ADJECTIVE_BITS) && values[i-1] & (NOUN_BITS|PRONOUN_BITS)) // relative clause after a noun or pronoun?
			{
				if (trace) Log(STDUSERLOG,"%s clause - potential clause starter?\r\n",wordStarts[i]);
				startClause = i;
			}
			else if (originalLower[i] && originalLower[i]->systemFlags & POTENTIAL_CLAUSE_STARTER  && !(values[i] & ADJECTIVE_BITS) && values[i-1] & COMMA && values[i-2] & (NOUN_BITS|PRONOUN_BITS)) // relative clause after comma after a noun or pronoun?
			{
				if (trace) Log(STDUSERLOG,"%s clause - potential clause starter after comma?\r\n",wordStarts[i]);
				startClause = i;
			}
			else if (originalLower[i] && values[i] & WH_ADVERB && !(values[i+1] & AUX_VERB_TENSES) && !(values[i+2] & AUX_VERB_TENSES)  ) // I have no idea how it works, but not How can it work?
			{
				if (trace) Log(STDUSERLOG,"%s clause - potential clause starter on whword?\r\n",wordStarts[i]);
				startClause = i;
			}
			else if (!startClause);
			else if (values[i] & VERB_TENSES) 
			{
				if (trace) Log(STDUSERLOG,"     %s clause verb found \r\n",wordStarts[i]);
				for (unsigned int j = startClause; j <= i; ++j)
				{
					clauses[j] = clauseBit;
				}
				clauseBit <<= 1;
				startClause = 0;
			}
		}

		if (!verbals[i]) // infinitive start (backpedaled to a TO if one exists) and ends there
		{
			if (values[i] & (NOUN_GERUND|NOUN_INFINITIVE))  
			{
				// infinitive after aux verb is likely some funny tense
				if (values[i-1] & AUX_VERB_TENSES) continue;
				if (i > 1 && values[i-2] & AUX_VERB_TENSES && values[i] & NOUN_GERUND)  continue; 

				verbals[i] = verbalBit;
				if (values[i] & NOUN_INFINITIVE)
				{
					unsigned int j = i;
					while (--j && !(values[j] & TO_INFINITIVE)); // find it even before it knows it is?
					if (values[j] & TO_INFINITIVE) // we react here in case something KNOWS it is to-infinitive but verb doesnt yet know it is infinitive.
					{
						while (j < i) verbals[j++] = verbalBit;
					}
				}
				verbalBit <<= 1;
			}
			else if (values[i] & ADJECTIVE_PARTICIPLE && values[i-1] & NOUN_BITS) // note- adjective-participles may occur AFTER the noun and have objects but not when before - we have the people *taken care of
			{
				verbals[i] = verbalBit;
				verbalBit <<= 1;
			}
			
		}
	}
}
	
 void TagIt(bool trace) // get the set of all possible tags.
 {
	uint64* data;
	roleIndex = 0;
	values[0] = values[wordCount+1] = 0;
	posStatus = 0;
	prepBit = 1;
	clauseBit = 1;
	verbalBit = 1;

	memset(phrases,0,wordCount+2);
	memset(verbals,0,wordCount+2);
	memset(clauses,0,wordCount+2);
	memset(designatedObjects,0,wordCount+2);
	memset(roles,0,(wordCount+2) * sizeof(int));
	memset(markers,0,(wordCount+2) * sizeof(int));

	 // get all possible flags first
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		originalValues[i] = values[i];	// for dual verb to know what was possible
		values[i] &= TAG_TEST;  // only the bits we care about for pos tagging
		if ( wordStarts[i][0] == '~') values[i] = 0;	// interjection
		if ( (values[i] & (ADJECTIVE_BASIC|ADJECTIVE_PARTICIPLE)) == (ADJECTIVE_BASIC|ADJECTIVE_PARTICIPLE))
			values[i] ^= ADJECTIVE_BASIC;	// accept the particple over the basic. only want one bit active and participle is more usable
		if ( (values[i] & (WH_ADVERB|ADVERB_BASIC)) == (WH_ADVERB|ADVERB_BASIC))
			values[i] ^= ADVERB_BASIC;	// accept the wh over regular
		// generator's going down... rewrite here
		if (values[i] & POSSESSIVE && wordStarts[i][1] && values[i-1] & NOUN_SINGULAR && values[i+1] & VERB_PRESENT_PARTICIPLE)
		{
			wordStarts[i] = AllocateString("is");
			originalLower[i] = FindWord(wordStarts[i]);
			values[i] = AUX_VERB|AUX_VERB_PRESENT;
			canonicalLower[i] = FindWord("be");
		}
		originalFlags[i] = values[i];	// just for testpos display of incoming

		bitCounts[i] = BitCount(values[i]);
	}
	//if (wordCount <= 2) return;	// DONT try to sort out microscopic sentence
	if (trace && prepareMode != 2) Log(STDUSERLOG,"%s",DumpAnalysis(values,"\r\nOriginal POS: ",true));
	StartupMembership();		// set markers as appropriate

	bool changed = true;
	rulesUsedIndex = 0;

	bool keep;
	int offset;
	int start;
	int direction;
	int retryCount = 0;
	int guessAllowed = 0;
	bool tracex;
	int pass = 0;
	// bug note--- dont need to execute a rule for an output that is already a single value
	// execute all rules while data changes
	unsigned int ambiguous = wordCount;
	while (changed && ambiguous)
	{
		ambiguous = wordCount;
		++pass;
		changed = false;
		if (!tags) return;
		if (guessAllowed) // done as much as normal, now do role analysis as well
		{
			bool resolved =  AssignRoles(); // but need to have marked preps first
			if (trace) 
			{
				Log(STDUSERLOG,"%s\r\n",DumpAnalysis(values,"Roled SemiTagged POS",false));
				DumpPrepositionalPhrases();
				Log(STDUSERLOG,"\r\n");
			}
		}

		// test all items in the sentence
		if (trace) Log(STDUSERLOG,"-------------   Pass %d rule application: \r\n",pass);
		for (unsigned int k = 1; k <= wordCount; ++k)
		{
			unsigned int j;
			if (reverseWords) j = wordCount + 1 - k ;
			else j = k;
			char* word = wordStarts[j];

			if (bitCounts[j] != BitCount(values[j]))
				Log(STDUSERLOG,"Bitcount inconsistency\r\n");

			if (bitCounts[j] == 1) 
			{
				--ambiguous;
				continue;			// already fixed, cannot be changed by this rule
			}

			// now execute all rules to reduce the possible flags
			for (unsigned int n = 0; n < tagRuleCount; ++n)
			{
				unsigned int i;
				if (reverseFlow) i = tagRuleCount - n - 1;
				else i = n;
				data = tags + (i * MAX_TAG_FIELDS);
				uint64 basic = *data;

				// reasons this rule doesnt apply
				if (basic & PART2_BIT) continue; // skip 2nd part
				int resultOffset = (basic >> RESULT_SHIFT) & 0x0003;
				uint64 resultBits = data[resultOffset] & PATTERN_BITS; 
				if ( !(values[j] & resultBits)) continue; // cannot help  this- no bits in common
				if ( !(values[j] & (-1LL ^ resultBits))) continue;	//  cannot help this - all bits would be kept or erased

				// does rule require guessing and is it allowed
				if (!guessAllowed)
				{
					if (basic & GUESS_BIT) continue; // ignore guesses
				}
				else  if (guessAllowed == 1)
				{
					if (data[2] & GUESS_BIT || data[3] & GUESS_BIT || data[4] & GUESS_BIT) continue; 
				}
				else if (guessAllowed == 2) 
				{
					if (data[3] & GUESS_BIT) continue;
				}
				else if (guessAllowed == 3) 
				{
					if (data[4] & GUESS_BIT) continue;
				}

				unsigned int limit;
				tracex = (data[3] & TRACE_BIT ) != 0;
				limit = (data[1] & PART1_BIT) ? (MAX_TAG_FIELDS * 2) : MAX_TAG_FIELDS;
				offset = ((data[1] >> OFFSET_SHIFT) & 0x00000007) - 3; // where to start pattern match relative to current field
				keep =  (data[1] & KEEP_BIT ) != 0;
				char* comment = comments[i];
				direction =  (data[2]  & REVERSE_BIT) ? -1 : 1;
				offset *= direction;	
				if (tracex) 
					Log(STDUSERLOG,"  Trace rule:%d %s  \r\n",i,comment);

				start = j + offset - direction;
				unsigned int k;
				int result;
				for (k = 0; k < limit; ++k) // test  fields
				{
					uint64 field = data[k];
					unsigned int control = (unsigned int) ( (field >> CONTROL_SHIFT) &0x00ff);
					if (!(control & STAY)) start += direction;
					if (k == resultOffset) 
					{
						result = 1; // the result include is known to match if we are here
						if (resultBits == PARTICLE) // memorize where tested
							particleLocation = j;	
					}
					else if (control) // if we care
					{
						if (control & SKIP) // check for any number of matches until fails.
						{
							result = TestTag(start,control,field & PATTERN_BITS,direction); // might change start to end or start of PREPOSITIONAL_PHRASE if skipping
							if (tracex) 
								Log(STDUSERLOG,"    %s(%d) field:%d result:%d\r\n",wordStarts[start],start,k,result);
							if ( result == 2)
							{
								Log(STDUSERLOG,"unknown control %d) %s Rule #%d %s\r\n",j,word,i,comment);
								result = false;
							}
							while (result) 
							{
								start += direction;
								result = TestTag(start,control,field,direction);
								if (tracex) 
									Log(STDUSERLOG,"    Trace skip field:%d result:%d @%s(%d)\r\n",k,result,wordStarts[start],start);
							}
							start -= direction; // ends on non-skip, back up so will see again
						}
						else
						{
							result = TestTag(start,control,field & PATTERN_BITS,direction); // might change start to end or start of PREPOSITIONAL_PHRASE if skipping
							if (tracex) 
								Log(STDUSERLOG,"    %s(%d) field:%d result:%d\r\n",wordStarts[start],start,k,result);
							if ( result == 2)
							{
								Log(STDUSERLOG,"unknown control %d) %s Rule #%d %s\r\n",j,word,i,comment);
								result = false;
							}
							if (!result) break;	// fails to match // if matches, move along. If not, just skip over this (used to align commas for example)
						}
					}
				} // end test on fields

				// pattern matched, apply change if we can
				if (k >= limit) 
				{
					if (tracex) Log(STDUSERLOG," => matched\r\n");
					uint64 old = values[j] & ((keep) ? resultBits : ( -1LL ^ resultBits));
					if (trace)
					{
						char* which = (keep) ? (char*) "KEEP" : (char*)"DISCARD";
						uint64 properties = old; // old is what is kept
						if (!keep) properties = values[j] - old;	// what is discarded
						if (!properties) // SHOULDNT HAPPEN
						{
							ReportBug("bad result in pos tag %s\r\n",comment);
						}

						char* name = FindNameByValue(resultBits);
						if (!name) name = BitLabels(properties);
						Log(STDUSERLOG," %d) %s: %s %s using Rule #%d %s\r\n",j,word,which,name,i,comment);
					}
					if (old && values[j] != old) // ONLY change if it really leaves something behind
					{
						values[j] = old;
						if ( rulesUsedIndex >= 1000)
						{
							Log(STDUSERLOG,"unbounded tag analysis\r\n");
							return;
						}
						rulesUsed[rulesUsedIndex++] = (i<<16) | j; // note what we changed, for regression test display

						bitCounts[j] = BitCount(values[j]);
						changed = true;
						if (bitCounts[j] == 1) 
						{
							--ambiguous;
							break; // ending loop on rules
						}
					}
					else 
						Log(STDUSERLOG,"failed to change %d) %s Rule #%d %s\r\n",j,word,i,comment);
				} // end result change k > limit
				else if (tracex) Log(STDUSERLOG," => unmatched\r\n");
			if (bitCounts[j] != BitCount(values[j]))
				Log(STDUSERLOG,"Bitcount inconsistency\r\n");

			} // end loop on rules 
		}	// end loop on words

		if (trace && changed) Log(STDUSERLOG,"  results: %s",DumpAnalysis(values,"POS",false));
		
		if (!changed && ambiguous) // no more rules changed anything and we still need help
		{
			if (!guessAllowed) 
			{
				changed = true;
				guessAllowed = 1;
				if (trace) Log(STDUSERLOG," *** enable GUESS \r\n");
			}
			else if (guessAllowed == 1) 
			{
				changed = true;
				guessAllowed = 2;
				if (trace) Log(STDUSERLOG," *** enable GUESS1\r\n");
			}
			else if (guessAllowed == 2) 
			{
				guessAllowed = 3;
				if (trace) Log(STDUSERLOG," *** enable GUESS2\r\n");
				UseDefaultPos(guessAllowed); // 3 = known probablity (2500 words not nouns- 3500 words) + 1500 words we infer and could choose wrong. 
				changed = true;
			}
			else if (guessAllowed == 3) 
			{
				if (trace) Log(STDUSERLOG," *** enable GUESS3\r\n");
				changed = true;
				UseDefaultPos(++guessAllowed); //  4 = default probablity
			}
		}
		MarkPrepPhrases(); // shade the prep phrases
		if (trace) 	DumpPrepositionalPhrases();
	} // end main loop
	if (trace) 
	{
		Log(STDUSERLOG,"%s\r\n",DumpAnalysis(values,"PreFinalRoles POS",false));
		DumpPrepositionalPhrases();
		Log(STDUSERLOG,"\r\n");
	}
	AssignRoles();
	if (trace) Log(STDUSERLOG,"\r\n");


	if (posStatus & MAINVERB) // FOUND no verb, not a sentence
	{
		if (trace) Log(STDUSERLOG,"Not a sentence\r\n");
	}
	else // see if question or statement
	{
		int oldFlags = tokenFlags;
		unsigned int i;
		for (i = 1; i <= wordCount; ++i) 
		{
			if (roles[i] & MAINSUBJECT) break;
			if (phrases[i] || clauses[i] || verbals[i]) continue;
			if ( bitCounts[i] != 1) break;	// all bets about structure are now off
			if (values[i] & AUX_VERB_TENSES) // its a question because AUX or VERB comes before MAINSUBJECT
			{
				tokenFlags |= QUESTIONMARK;
				break;
			}
		}
		if (i <= wordCount) tokenFlags = oldFlags; // no subject found
	}

	// handle particles and NOT
	for (unsigned int i = 1; i <= wordCount; ++i) 
	{
		if (!strcmp(wordStarts[i],"not"))
		{
			if (roles[i+1] == MAINVERB || roles[i+2] == MAINVERB || roles[i+3] == MAINVERB) 
			{
				// I won't often eat  // won't you eat // will the people eat
				roles[i] = NOT;
			}
		}

		if (values[i] != PARTICLE) continue;
		for (unsigned int j = i-1; j > 0; --j)
		{
			if (!(values[j] & (VERB_TENSES|NOUN_GERUND|NOUN_INFINITIVE))) continue; // find matching verb

			char word[MAX_WORD_SIZE];
			if (canonicalLower[j])  sprintf(word,"%s_%s",canonicalLower[j]->word,wordStarts[i]);
			else if (canonicalUpper[j])  sprintf(word,"%s_%s",canonicalUpper[j]->word,wordStarts[i]);
			WORDP D = FindWord(word);
			if ( D && D->systemFlags & PHRASAL_VERB)
			{
				if (canonicalLower[j]) canonicalLower[j] = D; // store canonical
				else if (canonicalUpper[j]) canonicalUpper[j] = D;
				sprintf(word,"%s_%s",wordStarts[j],wordStarts[i]);
				originalLower[j] = StoreWord(word,VERB);
				break;
			}
		}
	}
}

#define MAX_POS_RULES 500

void ReadPosPatterns(const char* file)
{
	uint64 data[MAX_POS_RULES * MAX_TAG_FIELDS];
	char word[MAX_WORD_SIZE];
	char* commentsData[MAX_POS_RULES];
	unsigned int count = 0;
	FILE* in = fopen(file,"rb");
	if (!in)
	{
		printf("missing %s\r\n",file);
		return;
	}
	uint64 val;
	uint64 flags = 0;
	uint64 v;
	uint64 offsetValue;
	uint64* dataptr;
	tagRuleCount = 0;
	char comment[MAX_WORD_SIZE];
	while (ReadALine(readBuffer,in)) // read new rule or comments
	{
		char* ptr = SkipWhitespace(readBuffer);
		ptr = ReadCompiledWord(ptr,word);
		if (!*word) continue;
		if (*word == '#' ) 
		{
			strcpy(comment,readBuffer);
			continue;
		}
		if (!stricmp(word,"INVERT")) // run rules backwards
		{
			reverseFlow = true;
			continue;
		}
		if (!stricmp(word,"ENABLE_STATS")) // run best guesses of word types during GUESS2
		{
			useStats = true;
			continue;
		}
		if (!stricmp(word,"ENABLE_DEFAULT")) // run default kinds of word types during GUESS3
		{
			useDefault = true;
			continue;
		}
		if (!stricmp(word,"INVERTWORDS")) // run sentences backwards
		{
			reverseWords = true;
			continue;
		}
		int c = 0;
		int offset;
		bool reverse = false;
		unsigned int limit = MAX_TAG_FIELDS;
		bool big = false;
		if (!stricmp(word,"BIG")) // fat rule
		{
			ptr = SkipWhitespace(ptr);	
			ptr = ReadCompiledWord(ptr,word);
			limit += limit;
			big = true;
		}
		if (!stricmp(word,"reverse"))
		{
			reverse = true;
			ptr = SkipWhitespace(ptr);	
			ptr = ReadCompiledWord(ptr,word);
			c = 0;
			if (!IsDigit(word[0]) && word[0] != '-')
			{
				printf("Missing reverse offset  %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
				return;
			}
			c = atoi(word);
			if ( c < -3 || c > 3) // 3 bits
			{
				printf("Bad offset (+-3 max)  %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
				return;
			}
		}
		else if (!IsDigit(word[0]) && word[0] != '-') continue; // not an offset start of rule
		else 
		{
			c = atoi(word);
			if ( c < -3 || c > 3) // 3 bits
			{
				printf("Bad offset (+-3 max)  %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
				return;
			}
		}
		offset = (reverse) ? (c + 1) : (c - 1); 
		offsetValue = (uint64) ((c+3) & 0x00000007); // 3 bits offset
		offsetValue <<= OFFSET_SHIFT;
		int resultIndex = -1;
		dataptr = data + (tagRuleCount * MAX_TAG_FIELDS);
		uint64* base = dataptr;
		unsigned int includes = 0;
		bool doReverse = reverse;
		for (unsigned int i = 1; i <= limit; ++i)
		{
			unsigned int kind = 0;
			if (reverse) 
			{
				reverse = false;
				--offset;
			}
			else ++offset;
			flags = 0;
resume:
			// read control for this field
			ptr = SkipWhitespace(ptr);	
			ptr = ReadCompiledWord(ptr,word);
			if (!*word || *word == '#')
			{
				ReadALine(readBuffer,in);
				ptr = readBuffer;
				--i;
				continue;
			}
			if (!stricmp(word,"stay")) 
			{ 
				if ( doReverse) ++offset;
				else --offset;
				kind |= STAY;
				goto resume;
			}
			if (!stricmp(word,"SKIP")) // cannot do wide negative offset and then SKIP to fill
			{ 
				if ( resultIndex == -1)
				{
					if (!reverse) printf("Cannot do SKIP before the primary field -- offsets are unreliable (need to use REVERSE) Rule: %d comment: %s\r\n",tagRuleCount,comment);
					else printf("Cannot do SKIP before the primary field -- offsets are unreliable Rule: %d comment: %s\r\n",tagRuleCount,comment);
					return;
				}

				if ( doReverse) ++offset;
				else --offset;
				kind |= SKIP;
				goto resume;
			}

			if (*word == 'x') val = 0; // no field to process
			else if (*word == '!')
			{
				if (!stricmp(word+1,"STAY"))
				{
					printf("Cannot do !STAY (move ! after STAY)  Rule: %d comment: %s\r\n",tagRuleCount,comment);
					return;
				}
				if (!stricmp(word+1,"ISCLAUSE"))
				{
					printf("Cannot do !ISCLAUSE rule: %d comment: %s\r\n",tagRuleCount,comment);
					return;
				}
				val = FindValueByName(word+1);
				if ( val == 0)
				{
					printf("Bad notted control word %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
					return;
				}
				if (!stricmp(word+1,"include") || !stricmp(word+1,"is"))
				{
					printf("Use !has instead of !include or !is-  %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
					return;
				}
				kind |= NOTCONTROL;
				if (!stricmp(word+1,"start")) flags = 1;
				else if (!stricmp(word+1,"first")) flags = 1;
				else if (!stricmp(word+1,"end")) flags = 10000;
				else if (!stricmp(word+1,"last")) flags = 10000;
			}
			else
			{
				val = FindValueByName(word);
				if ( val == 0 || val > LASTCONTROL)
				{
					printf("Bad control word %s rule: %d comment: %s \r\n",word,tagRuleCount,comment);
					return;
				}
				if (!stricmp(word,"include")) ++includes;
				if (!stricmp(word,"start")) flags |= 1;
				else if (!stricmp(word,"end")) flags |= 10000;
			}
			flags |= val << OP_SHIFT;	// top byte
			flags |= ((uint64)kind) << CONTROL_SHIFT;
			if (i == 2 && big) flags |= PART1_BIT;			// marker on 1st half
			if (i == ( MAX_TAG_FIELDS + 1)) flags |= PART2_BIT;	// big marker on 2nd
			if (flags) // there is something to test
			{
				// read flags for this field
				bool subtract = false;
				while (1) 
				{
					ptr = SkipWhitespace(ptr);	
					ptr = ReadCompiledWord(ptr,word);
					if (!*word || *word == '#') break;	// end of flags
					uint64 baseval = flags >> OP_SHIFT;
					if (IsDigit(word[0])) v = atoi(word); // for POSITION Or dummy for PRIORPARTICLEVERB
					else if (!stricmp(word,"ALL")) v = TAG_TEST;
					else if ( word[0] == '-')
					{
						subtract = true;
						v = 0;
					}
					else if ( *word == '*')
					{
						if (offset != 0)
						{
							printf("INCLUDE * must be centered at 0 rule: %d comment: %s\r\n",tagRuleCount,comment);
							return;
						}
						if ( resultIndex != -1)
						{
							printf("Already have pattern result bits %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
							return;
						}
						resultIndex = i-1;
						v = 0;
					}
					else if (  baseval == ISMEMBER ) 
					{
						v = MakeMeaning(FindWord(word));
						if (!v)
						{
							printf("Failed to find set %s - POS tagger incomplete because build 0 not yet done.\r\n",word);
						}
					}
					else if ( baseval == ISCANONICAL || baseval == ISWORD || baseval == ENDSWITH) 
					{
						v = MakeMeaning(StoreWord(word));
					}
					else
					{
						if (!stricmp("NOUN_DESCRIPTION_BITS",word))
						{
							uint64 z = NOUN_DESCRIPTION_BITS;
							z = z + 0;
						}
						v = FindValueByName(word);
						if ( v == 0)
						{
							printf("Bad flag word %s rule: %d %s\r\n",word,tagRuleCount,comment);
							return;
						}
						if (v & BASIC_POS)
						{
							printf("Bad flag word overlaps BASIC bits %s rule: %d %s\r\n",word,tagRuleCount,comment);
							return;
						}
						if ( subtract ) 
						{
							flags &= -1 ^ v;
							v = 0;
							subtract = false;
						}
					}
					flags |= v;
				}
			}
			if (includes > 1) 
			{
				printf("INCLUDE should only be on primary field - use HAS Rule: %d %s\r\n",tagRuleCount,comment);
				return;
			}
			if (i == 2) 
				flags |= offsetValue;	// 2nd field gets offset shift
			*dataptr++ = flags; 
			ReadALine(readBuffer,in);
			ptr = readBuffer;
		} // end of fields loop

		// now get results data
		ptr = SkipWhitespace(ptr);	
		ptr = ReadCompiledWord(ptr,word);
		if (!stricmp(word,"stay") || !stricmp(word,"skip") || *word == '!' ) 
		{
			printf("Too many fields before result %s: %d %s\r\n",word,tagRuleCount,comment);
			return;
		}
		if (doReverse) base[2] |=  REVERSE_BIT;
		while (1)
		{
			if (!stricmp(word,"GUESS")) *base |= GUESS_BIT;
			else if (!stricmp(word,"GUESS1")) 
			{
				*base |= GUESS_BIT;
				base[2] |= GUESS_BIT;
			}
			else if (!stricmp(word,"GUESS2")) 
			{
				*base |= GUESS_BIT;
				base[3] |= GUESS_BIT;
			}
			else if (!stricmp(word,"GUESS3")) 
			{
				*base |= GUESS_BIT;
				base[3] |= GUESS_BIT;
				base[4] |= GUESS_BIT;
			}
			else if (!stricmp(word,"TRACE")) 
				base[3] |= TRACE_BIT;
			else if (!stricmp(word,"DISABLE")) base[1] =  ((uint64)HAS) << CONTROL_SHIFT; // 2nd pattern becomes HAS with 0 bits which cannot match
			else break;
			ptr = SkipWhitespace(ptr);	
			ptr = ReadCompiledWord(ptr,word);
		}
		if (!stricmp(word,"KEEP")) base[1] |= KEEP_BIT;
		else if (!stricmp(word,"DISCARD"));
		else
		{
			printf("Too many fields before result? %s  rule: %d comment: %s\r\n",word,tagRuleCount,comment);
			return;
		}
		if ( resultIndex == -1)
		{
			printf("Needs * on result bits %s rule: %d comment: %s\r\n",word,tagRuleCount,comment);
			return;
		}

		*base |= ((uint64)resultIndex) << RESULT_SHIFT;
		
		commentsData[tagRuleCount] = AllocateString(comment);
		++tagRuleCount;
		if (big) 
		{
			commentsData[tagRuleCount] = AllocateString(" ");
			++tagRuleCount;		// double-size rule
		}
	}
	
	tags = (uint64*) AllocateString((char*)data,tagRuleCount * MAX_TAG_FIELDS * sizeof(uint64),true);
	comments = (char**)  AllocateString((char*)commentsData,tagRuleCount * sizeof(char*));

	printf("Read %d pos rules\r\n",tagRuleCount);
}

void TagTest(char* file)
{
	if (!*file) file = "REGRESS/postest.txt";
	FILE* in = fopen(file,"rb");
	if (!in) return;
	unsigned int count = 0;
	unsigned int fail = 0;
	char sentence[MAX_WORD_SIZE];
	while (ReadALine(readBuffer,in))
	{
		char* ptr =  SkipWhitespace(readBuffer);
		if (!*ptr || *ptr == '#') continue;
		++count;
		strcpy(sentence,ptr);
		PrepareSentence(ptr,false,true);
		POSTag(trace != 0);
		char* answer = DumpAnalysis(originalFlags,"Original POS",true);
		char basic[BIG_WORD_SIZE];
		strcpy(basic,answer);
		answer = DumpAnalysis(values,"Tagged POS",false);
		while (ReadALine(readBuffer,in))
		{
			char* answer = SkipWhitespace(readBuffer);
			if (!strnicmp(answer,"Tagged",6)) break;
		}
		if (!*readBuffer) break;	// all done
		char* cr = strchr(answer,'\r');
		if (cr) *cr = 0;

		// remove all trailing blanks.
		int len = strlen(answer);
		while (answer[len-1] == ' ') answer[--len] = 0;
		len = strlen(readBuffer);
		while (readBuffer[len-1] == ' ') readBuffer[--len] = 0;

		if (strcmp(answer,readBuffer))
		{
			unsigned int i;
			for (i = 0; i < strlen(readBuffer); ++i)
			{
				if (answer[i] != readBuffer[i]) break;
				if (!answer[i] || !readBuffer[i]) break;
			}
			while (i && answer[--i] != '(');	// back up to a unit start
			if (i) --i;
			while (i && answer[--i] != ' ');	// start of word
			char hold[BIG_WORD_SIZE];
			strcpy(hold,answer+i);
			answer[i] = 0;
			strcat(answer,"\r\n--> ");
			strcat(answer,hold);
			char hold1[BIG_WORD_SIZE];
			*hold1 = 0;
			unsigned int len = strlen(readBuffer);
			if ( len > i) strcpy(hold1,readBuffer+i);
			readBuffer[i] = 0;
			strcat(readBuffer,"\r\n--> ");
			strcat(readBuffer,hold1);
			Log(STDUSERLOG,"\r\nMismatch: for: %s\r\n",sentence);
			Log(STDUSERLOG,"%s\r\n",basic);

			for (unsigned int i = 0; i < rulesUsedIndex; ++i)
			{
				unsigned int rule = rulesUsed[i] >> 16;
				unsigned int word = rulesUsed[i] & 0x0000ffff;
				unsigned int index = rule * MAX_TAG_FIELDS;
				uint64* data = tags + (rule * MAX_TAG_FIELDS);
				unsigned int resultOffset = (*data >> RESULT_SHIFT) & 0x0003;
				bool keep =  (data[1] & KEEP_BIT ) != 0;
				char* which = (keep) ? (char*)"KEEP" : (char*) "DISCARD";
				uint64 properties = data[resultOffset] & PATTERN_BITS; 
				char* name = FindNameByValue(properties);
				if (!name) name = BitLabels(properties);
				Log(STDUSERLOG,"  %d) %s: %s %s using Rule #%d %s\r\n",word,wordStarts[word],which,name,rule,comments[rule]);
			}

			Log(STDUSERLOG,"          got: %s\r\n",answer);
			Log(STDUSERLOG,"         want: %s\r\n",readBuffer);
			++fail;
		}
	}

	fclose(in);
	Log(STDUSERLOG,"%d sentences tested, %d failed.\r\n",count,fail);
}
