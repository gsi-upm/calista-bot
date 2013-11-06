// dictionarySystem.cpp - represents the dictionary of words


#include "common.h"

#ifdef DESIGN

This file covers routines that create and access a "dictionary entry" (WORDP) and the "meaning" of words (MEANING).

The dictionary acts as the central hash mechanism for accessing various kinds of data.

The dictionary consists of data imported from WORDNET 3.0 (copyright notice at end of file) 
+ augmentations + system and script pseudo-words.

The usual WORDP entry has flags (->properties) describing its part of speech and its properties. 
A separate set of flags covers system properties (->systemFlags).

A word also gets the WordNet meaning ontology (->meanings & ->meaningCount). The definition of meaning in WordNet 
is words that are synonyms in some particular context. Such a collection in WordNet is called a synset. 

Since words can have multiple meanings (and be multiple parts of speech), the flags of a word are a summary
of all of the properties it might have and it has a list of entries called "meanings". Each entry is a MEANING 
and points to the synset header, a word like 02350055n, which is the number that represents the synset (noun) 
in the WordNet system and has a bit marking it as a WordNet_ID and its part of speech. This is referred to as the 
"master" meaning and has the gloss (definition) of the meaning. The meaning list of a master node points back to 
all the real words which comprise it.


Since WordNet has an ontology, its synsets are hooked to other synsets in various relations, particular that
of parent and child. ChatScript represents these as facts. The hierarchy relation uses the verb "is" and
has the child as subject and the parent as object. Such a fact runs from the master entries, not any of the actual 
word entries. So to see if "dog" is an "animal", you could walk every meaning list of the word animal and
mark the master nodes they point at. Then you would search every meaning of dog, jumping to the master nodes,
then look at facts with the master node as subject and the verb "is" and walk up to the object. If the object is 
marked, you are there. Otherwise you take that object node as subject and continue walking up. Eventually you arrive at
a marked node or run out at the top of the tree.

Some words DO NOT have a master node. Their meaning is defined to be themselves (things like pronouns or determiners), so
their meaning value for a meaning merely points to themselves.

The meaning system is established by building the dictionary and NEVER changes thereafter by chatbot execution.
New words can transiently enter the dictionary for various purposes, but they will not have "meanings".

A MEANING is a reference to a specific meaning of a specific word. It is an index into the dictionary 
(identifying the word) and an index into that words meaning list (identifying the specific meaning).
An meaning list index of 0 refers to all meanings of the word. A meaning index of 0 can also be type restricted
so that it only refers to noun, verb, adjective, or adverb meanings of the word.

Since there are only two words in WordNet with more than 63 meanings (break and cut) we limit all words to having no
more than 63 meanings by discarding the excess meanings. Since meanings are stored most important first,
these are no big loss. This leaves room for the 5 essential type flags we use for restricting a generic meaning.

The Wordnet ontology allows multiple synset parents (e.g., Canis_familiaris~1 is under canid and domesticated_animal)

Space for dictionary words, strings, and the meanings of words come from a common pool. Dictionary words are
allocated linearly forward in the pool, while strings and meanings are allocated linearly backwards. Thus
all dictionary entries are indexable as a giant array.

The dictionary separately stores uppercase and lowercase forms of the same words (since they have different meanings).
There is only one uppercase form stored, so United and UnItED would be saved as one entry. The system will have
to decide which form a user intended, since they may not have bothered to capitalize a proper noun, or they 
may have shouted a lowercase noun, and a noun at the start of the sentence could be either proper or not.

Dictionary words are hashed as lower case, but if the word has an upper case letter it will be stored
in the adjacent higher bucket. Words of the basic system are stored in their appropriate hash bucket.
After the basic system is read in, the dictionary is frozen. This means it remembers spot the allocation
pointers are for the dictionary and string space and is using mark-release memory management.

Words created on the fly (after a freeze) by interacting with a user are always stored in bucket 0. 
This allows the system to instantly discard them when the interaction has been processed merey by 
zeroing bucket 0. The string space and dictionary space allocated to those on-the-fly words are merely 
"released" back to the values at the time of dictionary freeze.

#endif


#ifndef WIN32
#define MAX_STRING_SPACE 20000000  // transient string space
#elif IOS
#define MAX_STRING_SPACE 30000000 //   20M bytes#else
#else
#define MAX_STRING_SPACE 30000000 //   20M bytes
#endif

WordMap canonicalMap;

WORDP posWords[64];

WORDP dictionaryBase = 0;			//   base of allocated space that encompasses dictionary, string space, and meanings
WORDP dictionaryFree;		//   current next dict space available going forward (not a valid entry)
static WORDP dictionaryLocked;
static WORDP dictionaryPreTopics;
static WORDP dictionaryPreBuild1;
static char* secondaryBase;
static char* secondaryFreeze;
char* stringSpaceFreeze = 0;
char* stringSpacePreTopics = 0;
char* stringSpacePrebuild1 = 0;
static map<string,uint64> defineValues;
static map<uint64,string> defineNames;
uint64 verbFormat;
uint64 nounFormat;
uint64 adjectiveFormat;
uint64 adverbFormat;
int ndict = 0;

WORDP Dnot;
WORDP Dnever;
WORDP Dnumber;
WORDP Dmoney;
WORDP Dplacenumber;
WORDP Dpropername;
WORDP Dspacename;
WORDP Dmalename,Dfemalename,Dhumanname,Dfirstname;
WORDP Dlocation,Dtime;
WORDP Durl;
WORDP Dunknown;
WORDP Dchild,Dadult;
WORDP Dyou;
WORDP Dslash;
WORDP Dchatoutput;
WORDP Dburst;
WORDP Dtopic;

char* GetCanonical(char* word)
{
	if (!word) return NULL;
	return canonicalMap[word];
}

char* GetCanonical(WORDP D)
{
	if (!D || !(D->systemFlags & HAS_CANONICAL)) return NULL;
	return canonicalMap[D->word];
}


void AcquireDefines(char* fileName)
{
	defineValues.clear();
	defineNames.clear();
	FILE* in = fopen(fileName,"rb");
	if (!in) return;
	char label[MAX_WORD_SIZE];
	char word[MAX_WORD_SIZE];
    bool orop = false;
    bool shiftop = false;
	bool xorop = false;
	bool plusop = false;
	bool minusop = false;
	while (ReadALine(readBuffer, in))
	{
		uint64 result = 0;
        uint64 value;
		char* ptr = SkipWhitespace(readBuffer);
   		ptr = ReadCompiledWord(ptr,word);
		if (stricmp(word,"#define")) continue;
		//   accept lines line #define NAME 0x...
		ptr = SkipWhitespace(ptr);
		ptr = ReadCompiledWord(ptr,word); //   the #define name 
		unsigned int len = strlen(word);
        if (ptr == 0 || *ptr == 0 || strchr(word,'(')) continue; //   if a paren is IMMEDIATELY attached to the name, its a macro, ignore it.
		else if (*ptr == ' ' && ptr[1] == '(' && ptr[2] == '(') continue;	//   special macros we want to ignore LIST_BY_SIZE
		while (ptr) //   read value of the define
        {
            ptr = SkipWhitespace(ptr);
            if (ptr == 0) break;
			char c = *ptr;
			if (!c) break;

            if (c == ')' || c == '/') break; //   a form of ending
			++ptr;
			if (c == '+' && *ptr == ' ') plusop = true;
			else if (c == '-' && *ptr == ' ') minusop = true;
            else if (IsDigit(c))
            {
				if (*ptr == 'x') ptr = ReadHex(ptr-1,value);  //   HEX NUMBER
                else ptr = ReadInt64(ptr-1,value);   //   the value
                if (orop) result |= value;
                else if (shiftop) result <<= value;
                else result = value;
                plusop = minusop = xorop = orop = shiftop = false;
            }
			else if (c == '(');	//   start of a (expression in a define, ignore it
            else if (c == '|')   orop = true;
            else if (c == '<') //    <<
            {
                ++ptr; 
                shiftop = true;
            }
           else if (c == '^') 
            {
                ++ptr; //   ^^
                xorop = true;
            }
           else //   reusing word
            {
                ptr = ReadCompiledWord(ptr-1,label);
                value = FindValueByName(label);
                if (!value) 
					ReportBug("missing modifier value for %s\r\n",label);
                if (orop) result |= value;
                else if (shiftop) result <<= value;
                else if (xorop) result ^= value;
				else if (plusop) result += value;
				else if (minusop) result -= value;
                else result = value;
                plusop = minusop = xorop = orop = shiftop = false;
            }
        }
		char* name = AllocateString(word,0);
		defineValues[name] = result;

		if (defineValues.find(name) == defineValues.end()) 
		{
			printf("failed to store define %s\r\n",name);
		}

		string x = defineNames[result];
		if ( x.size() != 0) 
			continue;	//   already have something for this value
		defineNames[result] = name;
	}
	fclose(in);
}

uint64 FindValueByName(char* name)
{
	if (defineValues.find(name) == defineValues.end()) return 0; //   no such

	return defineValues[name];
}

char* FindNameByValue(uint64 val)
{
	if (defineNames.find(val) == defineNames.end()) return 0; //   no such
	return (char*) defineNames[val].c_str();
}

void InitDictionary()
{
	//   dictionary and meanings and strings share space, running from opposite ends of a common pool
	unsigned int size = sizeof(WORDENTRY) * MAX_ENTRIES;
	size += MAX_STRING_SPACE;
	size /= sizeof(WORDENTRY);
	unsigned int space = ((size * sizeof(WORDENTRY) + 3) / 4) * 4;	//   word boundary allocation
	if ( dictionaryBase == 0) // 1st startup allocation -- not needed on a reload
	{
		dictionaryBase = (WORDP) malloc(space);
		if (!dictionaryBase)
		{
			printf("Out of space for dictionary\r\n");
			ReportBug("Cannot allocate space for dictionary %d\r\n",space);
			exit(0);
		}
		memset(dictionaryBase,0,space);
	}
	dictionaryFree =  dictionaryBase + MAX_HASH_BUCKETS + HASH_EXTRA ;		//   prededicate hash space within the dictionary itself
	secondaryBase = ((char*)dictionaryBase) + space;						//   the end of the allocated space

	//   Lower case words hash to 1 and above.  Uppercase words hash to 1 beyond lower case hash.
	//   [0] is for temporary words added user
	//   The bucket list is threaded thru WORDP nodes, and consists of indexes, not addresses.

	dictionaryPreTopics = 0; // in initial dictionary

	// make sets for the part of speech data
	uint64 value = 0x8000000000000000ULL;
	char name[MAX_WORD_SIZE];
	name[0] = '~';
	AcquireDefines("src/dictionarySystem.h"); //   get dictionary defines (must occur before loop that decodes properties into sets (below)
	for (int i = 63; i >= 0; --i)
	{
		char* word = FindNameByValue(value);
		if (word) 
		{
			MakeLowerCopy(name+1,word);
			WORDP D = posWords[i] = StoreWord(name);
			AddSystemFlag(D,CONCEPT);
			char* n = D->word;
		}
		else 
			posWords[i] = StoreWord(","); // unknown value
		value >>= 1;
	}
}

unsigned int TotalFreeSpace()
{
	return ((char*)secondaryBase) - ((char*) dictionaryFree);
}

char* AllocateString(char* word,unsigned int len,bool align64)
{ //   string allocation moves BACKWARDS from end of dictionary space (as do meanings)
	//   if word == null, dont initialize the space
	if (len == 0) len = strlen(word);
	if (word) ++len;	//   null terminate string

	//   always allocate on word boundary (since we share with meaning space)
	unsigned int allocate = ((len + 3) / 4) * 4;

	secondaryBase -= allocate;
 	if (align64) // force 64bit alignment alignment
	{
		uint64 base = (uint64) secondaryBase;
		base &= 0xFFFFFFFFFFFFFFC0ULL;
		secondaryBase = (char*) base;
	}
	char* newword =  secondaryBase;
    if (secondaryBase <= (char*) dictionaryFree) 
    {
		secondaryBase += allocate - 4; 
        ReportBug("Out of transient string space");
        len = 2;
#ifndef WIN32
		exit(0);
#endif
    }
    if (word) 
	{
		memcpy(newword,word,--len);
		newword[len] = 0;
	}
    return newword;
}

static void SetIdiomHeader(WORDP D)
{
	//   internal use, do not allow idioms on words from #defines or user variables or  sets.. but allow substitutes to do it?
	unsigned int n = BurstWord(D->word);
	if (n != 1) 
	{
		D = StoreWord(JoinWords(1),0);		//   create the 1-word header
		if (n > D->multiwordHeader) D->multiwordHeader = n;	//   mark it can go this far for an idiom
	}
}

void AddSystemFlag(WORDP D, uint64 flag)
{
	D->systemFlags |= flag;
}

void AddProperty(WORDP D, uint64 flag)
{
	if (!strcmp(D->word,"Space_Age_Power_Cleaner"))
	{
		int i = 0;
	}
	if (flag & PART_OF_SPEECH) 
	{
		SetIdiomHeader(D); //   make composite real words auto find in input
		if (flag & NOUN && D->systemFlags & UPPERCASE_HASH && !(flag & NOUN_PROPER_PLURAL)) flag |= NOUN_PROPER_SINGULAR;
	}
	D->properties |= flag;
}

void RemoveProperty(WORDP D, uint64 flags)
{
	D->properties &= -1LL ^ flags;
}

void RemoveSystemFlag(WORDP D, uint64 flags)
{
	D->systemFlags &= -1LL ^ flags;
}

void MeasureBuckets() //if you want to measure how long buckets are generally, shows the worst
{
	echo = true;
	int worst[10];
	memset(worst,0,sizeof(worst));
	unsigned int worstBucket = 0;
	unsigned int holes = 0;
	WORDP badBucket = 0;
	for (unsigned int i = 1; i < MAX_HASH_BUCKETS+HASH_EXTRA; ++i)
	{
		WORDP D = dictionaryBase + i; //   list is of index values
		if (!D->word) ++holes;
		unsigned int count = 0;
		while (D != dictionaryBase)
		{
			D = dictionaryBase + D->nextNode;
			++count;
		}
		if (count > worstBucket) 
		{
			worstBucket = count;
			badBucket = dictionaryBase + i;
		}
	}
	Log(STDUSERLOG,"Worst bucket %d %s  -- %d holes of %d\r\n",worstBucket,badBucket->word,holes,MAX_HASH_BUCKETS+2);

	for (unsigned int i = 1; i < MAX_HASH_BUCKETS+HASH_EXTRA; ++i)
	{
		WORDP D = dictionaryBase + i; //   list is of index values
		unsigned int counter = 0;
		while (D != dictionaryBase)
		{
			D = dictionaryBase + D->nextNode;
			++counter;
		}
		if (counter == worstBucket) ++worst[0];
		if (counter == worstBucket-1) ++worst[1];
		if (counter == worstBucket-2) ++worst[2];
		if (counter == worstBucket-3) ++worst[3];
		if (counter == worstBucket-4) ++worst[4];
		if (counter == worstBucket-5) ++worst[5];
		if (counter == worstBucket-6) ++worst[6];
		if (counter == worstBucket-7) ++worst[7];
	}
	Log(STDUSERLOG,"worst 8 from %d: %d %d %d %d %d %d %d %d\r\n",worstBucket,worst[0],worst[1],worst[2],worst[3],worst[4],worst[5],worst[6],worst[7]);
}

WORDP FindWord(const char* word, int len,unsigned int caseAllowed) 
{
	if (word == NULL || *word == 0) return NULL;

	if (len == 0) len = (int) strlen(word);

	bool hasUpperCharacters = false;
	uint64 fullhash = Hashit((unsigned char*) word,len,hasUpperCharacters); //   sets hasUpperCharacters as well if appropriate
	unsigned int hash  = (fullhash % MAX_HASH_BUCKETS) + 1; //   mod the size of the table
	if (caseAllowed & LOWERCASE_LOOKUP);
	else if (hasUpperCharacters || (caseAllowed & UPPERCASE_LOOKUP)) ++hash;

	//   normal or fixed case bucket
	WORDP D;
	if (caseAllowed & (PRIMARY_CASE_ALLOWED|LOWERCASE_LOOKUP|UPPERCASE_LOOKUP))
	{
		D = dictionaryBase + hash;
		while (D != dictionaryBase)
		{
			if (fullhash == D->hash) return D;
			D = dictionaryBase + D->nextNode;
		}
	}

    //    alternate case bucket
	if (caseAllowed & SECONDARY_CASE_ALLOWED)
	{
		D = dictionaryBase + hash + ((hasUpperCharacters) ? -1 : 1);
		while (D != dictionaryBase)
		{
			if (fullhash == D->hash)  return D;
			D = dictionaryBase + D->nextNode;
		}
	}

    return NULL;
}

static WORDP AllocateEntry()
{
	WORDP  D = dictionaryFree++; 
    memset(D,0,sizeof(WORDENTRY));
	return D;
}

WORDP StoreWord(char* word, uint64 properties)
{
    if (*word <= 32) //   we require something visible
	{
		ReportBug("entering null word to dictionary");
		return StoreWord("badword");
	}
	if (word[1] < 0)
	{
		ReportBug("entering strange word to dictionary %s",word);
		return NULL;
	}

	//   make all words normalized with no blanks in them.
	if (*word == '"' || *word == '_'); // dont change any quoted things or things beginning with _ (we use them in facts for a "missing" value
	else if (!(properties & AS_IS)) word = JoinWords(BurstWord(word,0)); //   when reading in the dictionary, BurstWord depends on it already being in, so just use the literal text here
	properties &= -1 ^ AS_IS;
	unsigned int len = strlen(word);
	bool hasUpperCharacters = false;
	uint64 fullhash = Hashit((unsigned char*)word,len,hasUpperCharacters); //   this sets hasUpperCharacters as well if needed
	unsigned int hash = (fullhash % MAX_HASH_BUCKETS) + 1; //   mod the size of the table (saving 0 to mean no pointer and reserving an end upper case bucket)
	if (hasUpperCharacters)  ++hash;
	WORDP base = dictionaryBase + hash;
 
	//   locate spot existing entry goes
    WORDP D = base; 
	while (D != dictionaryBase)
    {
 		if (fullhash == D->hash)
		{
			AddProperty(D,properties);
			return D;
		}
		D = dictionaryBase + D->nextNode;
    }  

    //   not found, add entry 
	if (base->word == 0 && !dictionaryPreTopics) D = base; // add into hash zone initial dictionary entries (nothing allocated here yet)
	else  
	{
		D = AllocateEntry();
		if ((char*)D >= secondaryBase) ReportBug("Out of dictionary nodes");
		D->nextNode = base->nextNode;
		base->nextNode = D - dictionaryBase;
	}
    D->word = AllocateString(word,len); 
    AddProperty(D,properties);
	if (hasUpperCharacters) AddSystemFlag(D,UPPERCASE_HASH);
	D->hash = fullhash;
    return D;
}

//   insert entry into a circular list, initializing if need be
void AddCircularEntry(WORDP base, void* field,WORDP entry)
{
	if (!base) return;
	unsigned int offset = ((char*)field - (char*)base) / sizeof(WORDP);	//   spot in entry is field as word index
	WORDP* setBase = (WORDP*)base;		//   pretend to be collection of fields
	WORDP* setEntry = (WORDP*)entry;		//   pretend to be collection of fields

	//   verify item to be added not already in circular list of this kind - dont add if it is
	if (!setEntry[offset]) 
	{
		if (!setBase[offset]) setBase[offset] = base; //   if set base not initialized, make it loop to self
		setEntry[offset] = setBase[offset]; 
		setBase[offset] = entry;
	}
	else printf("%s already on circular list of %s\r\n",entry->word, base->word);
}

WORDP FindCircularEntry(WORDP base,void* field,uint64 propertyBit)
{
	if (!base) return NULL;
	unsigned int offset = ((char*)field - (char*)base) / sizeof(WORDP);	//   spot in entry is field as word index

	WORDP start = ((WORDP* ) base)[offset];
	int count = 10;
	while (start && --count)
	{
		if (start->properties & propertyBit) return start;		//   we found it
		if (start == base) break;								//   we have exhausted the circle
		start =  ((WORDP*)start)[offset];		//   next entry in list
	}
	if (!count) ReportBug("Circular entry not found for: %s",base->word);
	return NULL;	//   not found
}

WORDP FindCircularEntry(WORDP base,int offset,uint64 propertyBit)
{
	if (!base) return NULL;

	WORDP start = ((WORDP* ) base)[offset];
	int count = 10;
	while (start && --count)
	{
		if (start->properties & propertyBit) return start;		//   we found it
		if (start == base) break;								//   we have exhausted the circle
		start =  ((WORDP*)start)[offset];		//   next entry in list
	}
	if (!count) ReportBug("Circular entry not found for: %s",base->word);
	return NULL;	//   not found
}

void WalkDictionary(DICTIONARY_FUNCTION func,void* data)
{
    for (WORDP D = dictionaryBase+1; D < dictionaryFree; ++D) 
	{
		if (D->word) (func)(D,data); 
	}
}



void DeleteDictionaryEntry(WORDP D)
{
	unsigned int hash = (D->hash % MAX_HASH_BUCKETS) + 1; 
	if (D->systemFlags & UPPERCASE_HASH) ++hash;
	WORDP base = dictionaryBase + hash;
	base->nextNode = D->nextNode; //   remove entry from buckets
}

void ReturnDictionaryToFreeze() 
{ 
	while (dictionaryFree > dictionaryLocked) DeleteDictionaryEntry(--dictionaryFree); //   remove entry from buckets
    secondaryBase = stringSpaceFreeze; 

	// system doesnt track changes to properties after freeze
}

void PreBuildLockDictionary() // dictionary before build0 layer 
{
    dictionaryPreTopics = dictionaryFree;		
	stringSpacePreTopics = secondaryBase;		//   mark point for mark release
	chatbotFacts = topicFacts = build0Facts = dictionaryFacts = factFree;

	// memorize dictionary values for backup to pre build locations :build0 operations (reseting word to dictionary state)
	FILE* out1 = fopen("TMP/build0pre","wb");
	if (!out1)
	{
		ReportBug("Cant generate tmp backups of dict values");
		exit(0);
	}
	for (WORDP D = dictionaryBase+1; D < dictionaryFree; ++D) 
	{
		fwrite(&D->properties,1,8,out1); 
		fwrite(&D->systemFlags,1,8,out1); 
		fwrite(&D->multiwordHeader,1,1,out1);
	}
	fclose(out1);
}

void ReturnDictionaryToWordNet() //   drop all non-fact memeory allocated after the prefreeze
{
	while (dictionaryFree > dictionaryPreTopics) DeleteDictionaryEntry(--dictionaryFree); //   remove entry from buckets
    secondaryBase = stringSpacePreTopics;

	FILE* in1 = fopen("TMP/build0pre","rb");
	for (WORDP D = dictionaryBase+1; D < dictionaryFree; ++D) 
	{
		fread(&D->properties,1,8,in1); 
		fread(&D->systemFlags,1,8,in1); 
		int n = fread(&D->multiwordHeader,1,1,in1);
		if ( n != 1)
		{
			ReportBug("Bad return to wordnet");
			exit(0);
		}
	}
	fclose(in1);
}

void Build0LockDictionary() // dictionary after build0 and before build1 layers 
{
    dictionaryPreBuild1 = dictionaryFree;		
	stringSpacePrebuild1 = secondaryBase;	
    chatbotFacts = topicFacts = build0Facts = factFree; 
	
	FILE* out2 = fopen("TMP/build1pre","wb");
	for (WORDP D = dictionaryBase+1; D < dictionaryPreBuild1; ++D) 
	{
		fwrite(&D->properties,1,8,out2); 
		fwrite(&D->systemFlags,1,8,out2); 
		fwrite(&D->multiwordHeader,1,1,out2);
	}
	fclose(out2);
}

void ReturnDictionaryToBuild0() 
{
	while (dictionaryFree > dictionaryPreBuild1) DeleteDictionaryEntry(--dictionaryFree); //   remove entry from buckets
    secondaryBase = stringSpacePrebuild1;

	FILE* in1 = fopen("TMP/build1pre","rb");
	for (WORDP D = dictionaryBase+1; D < dictionaryFree; ++D) 
	{
		fread(&D->properties,1,8,in1); 
		fread(&D->systemFlags,1,8,in1); 
		int n = fread(&D->multiwordHeader,1,1,in1);
		if ( n != 1)
		{
			ReportBug("Bad return to build0");
			exit(0);
		}
	}
	fclose(in1);
}

void LockDictionary()
{
    dictionaryLocked = dictionaryFree;		
	stringSpaceFreeze = secondaryBase;		
    chatbotFacts = topicFacts = factFree; 
}

void CloseDictionary()
{
	free(dictionaryBase);
}

static void Write8(unsigned int val, FILE* out)
{
	unsigned char x[1];
	x[0] = val & 0x000000ff;
	fwrite(x,1,1,out);
}

static void Write16(unsigned int val, FILE* out)
{
	unsigned char x[2];
	x[0] = val & 0x000000ff;
	x[1] = (val >> 8) & 0x000000ff;
	fwrite(x,1,2,out);
}

static void Write24(unsigned int val, FILE* out)
{
	unsigned char x[3];
	x[0] = val & 0x000000ff;
	x[1] = (val >> 8) & 0x000000ff;
	x[2] = (val >> 16) & 0x000000ff;
	fwrite(x,1,3,out);
}

static void Write32(unsigned int val, FILE* out)
{
	unsigned char x[4];
	x[0] = val & 0x000000ff;
	x[1] = (val >> 8) & 0x000000ff;
	x[2] = (val >> 16) & 0x000000ff;
	x[3] = (val >> 24) & 0x000000ff;
	fwrite(x,1,4,out);
}

static void Write64(uint64 val, FILE* out)
{
	unsigned char x[8];
	x[0] = val & 0x000000ff;
	x[1] = (val >> 8) & 0x000000ff;
	x[2] = (val >> 16) & 0x000000ff;
	x[3] = (val >> 24) & 0x000000ff;
	x[4] = (val >> 32) & 0x000000ff;
	x[5] = (val >> 40) & 0x000000ff;
	x[6] = (val >> 48) & 0x000000ff;
	x[7] = (val >> 56) & 0x000000ff;
	fwrite(x,1,8,out);
}

static void WriteDWord(WORDP ptr, FILE* out)
{
	unsigned int val = Word2Index(ptr);
	unsigned char x[3];
	x[0] = val & 0x000000ff;
	x[1] = (val >> 8) & 0x000000ff;
	x[2] = (val >> 16) & 0x000000ff;
	fwrite(x,1,3,out);
}

static void WriteString(char* str, FILE* out)
{
	if (!str || !*str) Write16(0,out);
	else
	{
		unsigned int len = strlen(str);
		Write16(len,out);
		fwrite(str,1,len+1,out);
	}
}

static void WriteBinaryEntry(WORDP D, FILE* out)
{
	unsigned char c;
	if (!D->word) // empty entry
	{
		c = 0;
		WriteString((char*)&c,out);
		return;
	}

	unsigned char check = '0';
	WriteString(D->word,out);

	//   flags +  systemflags
    Write64(D->properties,out);
	Write64(D->systemFlags,out);
	Write64(D->hash,out);
	Write24(D->nextNode,out); 

	unsigned int bits = 0;
	if (D->multiwordHeader) bits |= 1 << 0;
	if (D->conjugation) bits |= 1 << 1;
	if (D->plurality) bits |= 1 << 2;
	if (D->comparison) bits |= 1 << 3;
	if (D->meaningCount) bits |= 1 << 4;
	Write8(bits,out);

	if (D->multiwordHeader)
	{
		c = (unsigned char)D->multiwordHeader;
		fwrite(&c,1,1,out); //   limit 255 no problem
	}
	if (D->conjugation) WriteDWord(D->conjugation,out);
	if (D->plurality) WriteDWord(D->plurality,out);
	if (D->comparison) WriteDWord(D->comparison,out);
	if (D->meaningCount) 
	{
		unsigned char c = (unsigned char)D->meaningCount;
		fwrite(&c,1,1,out); //   limit 255 no problem
		for (unsigned int i = 1; i <= D->meaningCount; ++i) Write32(D->meanings[i],out);
	}
	if (D->properties & WORDNET_ID)  WriteString(D->w.gloss,out);
	fwrite(&check,1,1,out);
}

void WriteBinaryDictionary()
{
	FILE* out = fopen("DICT/dict.bin","wb");
	if (!out) return;
	WORDP D = dictionaryBase;
	while (++D < dictionaryFree) WriteBinaryEntry(D,out);
	WriteString("^^^",out); //   end marker for synchronization
	fclose(out);
	printf("binary dictionary %d written\r\n",dictionaryFree - dictionaryBase);
}

static unsigned int Read8(FILE* in) 
{
	unsigned char x[1];
	if (fread(x,1,1,in) != 1) return 0; 
	return (x[0]);
}

static unsigned int Read16(FILE* in) 
{
	unsigned char x[2];
	if (fread(x,1,2,in) != 2) return 0; 
	return (x[0]) + (x[1]<<8);
}

static unsigned int Read24(FILE* in)
{
	unsigned char x[3];
	if (fread(x,1,3,in) != 3) return 0;
	return (x[0]) + (x[1]<<8) + (x[2]<<16);
}
  
static unsigned int Read32(FILE* in)
{
	unsigned char x[4];
	if (fread(x,1,4,in) != 4) return 0;
	unsigned int x3 = x[3];
	x3 <<= 24;
	return (x[0]) | (x[1]<<8) | (x[2]<<16) | x3 ;
}

static uint64 Read64(FILE* in)
{
	unsigned char x[8];
	if (fread(x,1,8,in) != 8) return 0;
	unsigned int x1,x2,x3,x4,x5,x6,x7,x8;
	x1 = x[0]; 
	x2 = x[1];
	x3 = x[2];
	x4 = x[3];
	x5 = x[4];
	x6 = x[5];
	x7 = x[6];
	x8 = x[7];
	uint64 a = x1 | (x2<<8) | (x3<<16) | (x4<<24);
	uint64 b = x8;
	b <<= 24;
	b |= x5 | (x6<<8) | (x7<<16) ;
	b <<= 32;
	a |= b;
	return a;
}

static WORDP ReadDWord(FILE* in)
{
	unsigned char x[3];
	if (fread(x,1,3,in) != 3) return 0;
	return Index2Word((x[0]) + (x[1]<<8) + (x[2]<<16));
}

static char* ReadString(FILE* in)
{
	unsigned int len = Read16(in);
	if (!len) return NULL;

	char* buffer = AllocateBuffer();
	if (fread(buffer,1,len+1,in) != len+1) return NULL;
	char* str = AllocateString(buffer,len);
	FreeBuffer();
	return str;
}

static void PostFactBinary(WORDP D,void* junk)
{
	if (D->properties & WORDNET_ID) return;
	for (unsigned int i = D->meaningCount; i >= 1 ; --i)
	{
		if (Meaning2Word(D->meanings[i]) == D) continue; // self reference
		CreateFact(MakeMeaning(D,i),Mis,D->meanings[i]);
	}
}

static WORDP ReadBinaryEntry(FILE* in)
{
	unsigned char check;
	static int count = 0;
	char* name = ReadString(in);
	if (!name)
	{
		return AllocateEntry();
	}	
	if (name[0] == '^' && name[1] == '^' && name[2] == '^') return NULL;	//   normal ending in synch
	WORDP D = AllocateEntry();

	D->word = name;
	++ndict;
	echo = true;
    D->properties = Read64(in);
	D->systemFlags |=  Read64(in); // age learned and other tidbits
	D->hash = Read64(in);
	D->nextNode = Read24(in); 
	unsigned int bits = Read8(in);

	if (bits & (1 << 0)) 
	{
		unsigned char c;
		fread(&c,1,1,in);
		D->multiwordHeader = c;
	}
	if (bits & (1 << 1)) D->conjugation = ReadDWord(in);
	if (bits & (1 << 2)) D->plurality = ReadDWord(in);
	if (bits & (1 << 3)) D->comparison = ReadDWord(in);
	if (bits & (1 << 4)) 
	{
		unsigned char c;
		fread(&c,1,1,in);
		D->meaningCount = c;
		unsigned int size = (c+1) * sizeof(MEANING);
		MEANING* meanings = (MEANING*) AllocateString(NULL,size); 
		memset(meanings,0,size); 
		D->meanings =  meanings;
		for (unsigned int i = 1; i <= c; ++i)  D->meanings[i] = Read32(in);
	}
	
	if (D->properties & WORDNET_ID) D->w.gloss = ReadString(in);
	
	fread(&check,1,1,in);
	if (check != '0')
	{
		printf("Bad Binary Dictionary entry, rebuild the binary dictionary\r\n");
		ReportBug("Bad Binary Dictionary entry, rebuild the binary dictionary\r\n");
	}
	return D;
}

bool ReadBinaryDictionary() 
{
	FILE* in = fopen("DICT/dict.bin","rb");
	if (!in) return false;
	dictionaryFree = dictionaryBase + 1;
	while (ReadBinaryEntry(in));
	fclose(in);
	//MeasureBuckets();
	return true;
}

void WriteDictionaryFlags(WORDP D, FILE* out)
{
	uint64 properties = D->properties;
	uint64 bit = 0x8000000000000000LL;		//   starting bit
	while (properties)
	{
		if (properties & bit)
		{
			properties ^= bit;
			char* label = FindNameByValue(bit);
			fprintf(out,"%s ",label);
		}
		bit >>= 1;
	}
	// missing VERB_CONJUGATE1 VERB_CONJUGATE2 VERB_CONJUGATE3
	if ( D->systemFlags & SYSTEM_MORE_MOST)  fprintf(out,"SYSTEM_MORE_MOST ");
	if ( D->systemFlags & VERBS_ACCEPTING_OF_AFTER)  fprintf(out,"VERBS_ACCEPTING_OF_AFTER ");
	if ( D->systemFlags & ALLOW_INFINITIVE)  fprintf(out,"ALLOW_INFINITIVE ");
	if ( D->systemFlags & POTENTIAL_CLAUSE_STARTER)  fprintf(out,"POTENTIAL_CLAUSE_STARTER ");
	if ( D->systemFlags & NOUN_BEING)  fprintf(out,"NOUN_BEING ");
	if ( D->systemFlags & PREP_LOCATION)  fprintf(out,"PREP_LOCATION ");
	if ( D->systemFlags & PREP_TIME)  fprintf(out,"PREP_TIME ");
	if ( D->systemFlags & PREP_RELATION)  fprintf(out,"PREP_RELATION ");
	if ( D->systemFlags & PREP_INTRODUCTION)  fprintf(out,"PREP_INTRODUCTION ");
	if ( D->systemFlags & EXISTENTIAL_BE)  fprintf(out,"EXISTENTIAL_BE ");
	if ( D->systemFlags & ADJECTIVE_OBJECT)  fprintf(out,"ADJECTIVE_OBJECT ");
	if ( D->systemFlags & ADJECTIVE_POSTPOSITIVE)  fprintf(out,"ADJECTIVE_POSTPOSITIVE ");
	if ( D->systemFlags & OMITTABLE_TIME_PREPOSITION)  fprintf(out,"OMITTABLE_TIME_PREPOSITION ");
	if ( D->systemFlags & PREDETERMINER_TARGET)  fprintf(out,"PREDETERMINER_TARGET ");
	if ( D->systemFlags & OTHER_SINGULAR)  fprintf(out,"OTHER_SINGULAR ");
	if ( D->systemFlags & OTHER_PLURAL)  fprintf(out,"OTHER_PLURAL ");
	if ( D->systemFlags & NOUN_MASS)  fprintf(out,"NOUN_MASS ");
	if ( D->systemFlags & KINDERGARTEN)  fprintf(out,"KINDERGARTEN ");
	if ( D->systemFlags & GRADE1_2)  fprintf(out,"GRADE1_2 ");
	if ( D->systemFlags & GRADE3_4)  fprintf(out,"GRADE3_4 ");
	if ( D->systemFlags & GRADE5_6)  fprintf(out,"GRADE5_6 ");
	if ( D->systemFlags & PHRASAL_VERB)  fprintf(out,"SEPARABLE_PHRASAL_VERB ");
	if ( D->systemFlags & NOUN)  fprintf(out,"posdefault:NOUN ");
	if ( D->systemFlags & VERB)  fprintf(out,"posdefault:VERB ");
	if ( D->systemFlags & ADJECTIVE)  fprintf(out,"posdefault:ADJECTIVE ");
	if ( D->systemFlags & ADVERB)  fprintf(out,"posdefault:ADVERB ");
	if ( D->systemFlags & PREPOSITION)  fprintf(out,"posdefault:PREPOSITION ");
	if (D->systemFlags & TIMEWORD) fprintf(out,"TIMEWORD ");
	if (D->systemFlags & SPACEWORD) fprintf(out,"SPACEWORD ");
	if (D->systemFlags & VERB_DIRECTOBJECT) fprintf(out,"VERB_DIRECTOBJECT ");
	if (D->systemFlags & VERB_INDIRECTOBJECT) fprintf(out,"VERB_INDIRECTOBJECT ");
	if (D->systemFlags & VERB_ADJECTIVE_OBJECT) fprintf(out,"VERB_ADJECTIVE_OBJECT ");
}

static void WriteDictionaryReference(char* label,WORDP D,FILE* out)
{
    if (!D) return; 
    fprintf(out,"%s=%s ",label,D->word);
}

void WriteDictionary(WORDP D,void* data)
{
	if (D->systemFlags & DELETED_WORD) return;
	if (D->properties & WORDNET_ID)
	{
		WORDP M = 0;
		if (!D->meaningCount) 
		{
			AddSystemFlag(D,DELETED_WORD); //   lost all data
			return;
		}
	}
	++ndict;

	//   for possible alpha sort
	FILE* x = fopen("RAWDICT/alphasortpre.txt","ab");
	fprintf(x," %s \r\n",D->word);
	fclose(x);

	//   choose appropriate subfile (to reduce file size to make it easier to human read)
	char c = toLowercaseData[(unsigned char) D->word[0]]; 
	if (IsDigit(c)) sprintf(readBuffer,"DICT/%c.txt",c); //   main real dictionary
    else if (!IsLowerCase(c))  sprintf(readBuffer,"DICT/other.txt"); //   main real dictionary
    else sprintf(readBuffer,"DICT/%c.txt",c);//   main real dictionary
    FILE* out = fopen(readBuffer,"ab");

	//   write out the basics (name meaningcount idiomcount)
	fprintf(out," %s ( ",D->word);
	if (D->meaningCount) fprintf(out,"meanings=%d ",D->meaningCount);

	//   now do the dictionary bits into english
	WriteDictionaryFlags(D,out);
 	fprintf(out,") ");

	//   these must have valuable ->properties on them
	WriteDictionaryReference("conjugate",D->conjugation,out);
	WriteDictionaryReference("plural",D->plurality,out);
	WriteDictionaryReference("comparative",D->comparison,out);

	WriteDictionaryReference("substitute",D->substitutes,out); //   should never have data since read in live at runtime
	
	//   show the meanings, with illustrative gloss
	unsigned int count = D->meaningCount;
	
	char* gloss = (D->w.gloss) ? D->w.gloss : NULL;
	if (gloss) fprintf(out,"gloss=%s\r\n",gloss); //   synset headers have actual gloss
	else fprintf(out,"\r\n");

	//   now dump the meanings
	for (unsigned int i = 1; i <= count; ++i)
	{
		MEANING M = D->meanings[i];
		fprintf(out,"  %s ", WriteMeaning(M,false));
		WORDP R = Meaning2Word(M);
		if (R->w.gloss) fprintf(out,"%s\r\n",R->w.gloss);
		else fprintf(out,"\r\n");
	}
 
    fclose(out);
}

char* ReadDictionaryFlags(WORDP D, char* ptr)
{
	char junk[MAX_WORD_SIZE];
	ptr = SkipWhitespace(ptr); //   cannot trust user editing the dictionary
	ptr = ReadCompiledWord(ptr,junk);
	uint64 properties = 0;
	while (*junk && *junk != ')' )		//   read until closing paren
	{
		if (!strncmp(junk,"meanings=",9)) D->meaningCount = atoi(junk+9);
		else if (!strncmp(junk,"#=",2));
		else if (!strcmp(junk,"TOPIC_NAME")) D->systemFlags |= TOPIC_NAME;
		else if (!strcmp(junk,"CONCEPT")) D->systemFlags |= CONCEPT; 
		else if (!strcmp(junk,"VERBS_ACCEPTING_OF_AFTER")) D->systemFlags |= VERBS_ACCEPTING_OF_AFTER;
		else if (!strcmp(junk,"SYSTEM_MORE_MOST")) D->systemFlags |= SYSTEM_MORE_MOST;
		else if (!strcmp(junk,"ALLOW_INFINITIVE")) D->systemFlags |= ALLOW_INFINITIVE;
		else if (!strcmp(junk,"POTENTIAL_CLAUSE_STARTER")) D->systemFlags |= POTENTIAL_CLAUSE_STARTER;
		else if (!strcmp(junk,"NOUN_BEING")) D->systemFlags |= NOUN_BEING;
		else if (!strcmp(junk,"EXISTENTIAL_BE")) D->systemFlags |= EXISTENTIAL_BE;
		else if (!strcmp(junk,"ADJECTIVE_OBJECT")) D->systemFlags |= ADJECTIVE_OBJECT;
		else if (!strcmp(junk,"PREP_LOCATION")) D->systemFlags |= PREP_LOCATION;
		else if (!strcmp(junk,"PREP_TIME")) D->systemFlags |= PREP_TIME;
		else if (!strcmp(junk,"PREP_RELATION")) D->systemFlags |= PREP_RELATION;
		else if (!strcmp(junk,"PREP_INTRODUCTION")) D->systemFlags |= PREP_INTRODUCTION;
		else if (!strcmp(junk,"ADJECTIVE_POSTPOSITIVE")) D->systemFlags |= ADJECTIVE_POSTPOSITIVE;
		else if (!strcmp(junk,"OMITTABLE_TIME_PREPOSITION")) D->systemFlags |= OMITTABLE_TIME_PREPOSITION;
		else if (!strcmp(junk,"PREDETERMINER_TARGET")) D->systemFlags |= PREDETERMINER_TARGET;
		else if (!strcmp(junk,"OTHER_SINGULAR")) D->systemFlags |= OTHER_SINGULAR;
		else if (!strcmp(junk,"VERB_ADJECTIVE_OBJECT")) D->systemFlags |= VERB_ADJECTIVE_OBJECT;
		else if (!strcmp(junk,"VERB_DIRECTOBJECT")) D->systemFlags |= VERB_DIRECTOBJECT;
		else if (!strcmp(junk,"VERB_INDIRECTOBJECT")) D->systemFlags |= VERB_INDIRECTOBJECT;
		else if (!strcmp(junk,"OTHER_PLURAL")) D->systemFlags |= OTHER_PLURAL;
		else if (!strcmp(junk,"NOUN_MASS")) D->systemFlags |= NOUN_MASS;
		else if (!strcmp(junk,"GRADE1_2")) D->systemFlags |= GRADE1_2;
		else if (!strcmp(junk,"GRADE3_4")) D->systemFlags |= GRADE3_4;
		else if (!strcmp(junk,"GRADE5_6")) D->systemFlags |= GRADE5_6;
		else if (!strcmp(junk,"KINDERGARTEN")) D->systemFlags |= KINDERGARTEN;
		else if (!strcmp(junk,"SEPARABLE_PHRASAL_VERB")) D->systemFlags |= PHRASAL_VERB;
		else if (!strcmp(junk,"posdefault:NOUN")) D->systemFlags |= NOUN;
		else if (!strcmp(junk,"posdefault:VERB")) D->systemFlags |= VERB;
		else if (!strcmp(junk,"posdefault:ADJECTIVE")) D->systemFlags |= ADJECTIVE;
		else if (!strcmp(junk,"posdefault:ADVERB")) D->systemFlags |= ADVERB;
		else if (!strcmp(junk,"posdefault:PREPOSITION")) D->systemFlags |= PREPOSITION;
		else if (!strcmp(junk,"TIMEWORD")) D->systemFlags |= TIMEWORD;
		else if (!strcmp(junk,"SPACEWORD")) D->systemFlags |= SPACEWORD;
		else 
		{
			uint64 val = FindValueByName(junk);
			if (val == 0)
			{
			}
			properties |= val;
		}
		ptr = SkipWhitespace(ptr); //   cannot trust user editing the dictionary
		ptr = ReadCompiledWord(ptr,junk);
	}
	AddProperty(D,properties);
	return ptr;
}

MEANING AddMeaning(WORDP D,MEANING M)
{ //   worst case wordnet meaning count = 75 (break)
	//   meaning is 1-based (0 means generic)

	//   cannot add meaning to entries before the freeze (space will free up when transient space chopped back but pointers will be bad).
	//   If some dictionary words cannot add after the fact, none should
	//   Meanings disambiguate multiple POS per word. User not likely to be able to add words that have
	//   multiple parts of speech.
	if (dictionaryLocked) return 0;

	//   no meaning given, use self with meaning one
	if (!(((ulong_t)M) & MAX_DICTIONARY)) M |= MakeMeaning(D,(1 + D->meaningCount));

	//   meanings[0] is always the count of existing meanings
	//   Actual space available is always a power of 2.
	MEANING* meanings = D->meanings;

	//   if we run out of room, reallocate meaning space double in size (ignore the hole in memory)
	unsigned int oldCount = D->meaningCount;

	//prove we dont already have this here
	for (unsigned int i = 1; i <= oldCount; ++i) if (meanings[i] == M) return M;

	unsigned int count = oldCount + 1;  
	D->meaningCount = count;
	if (!(count & oldCount)) //   new count has no bits in common with old count, is a new power of 2
	{
		unsigned int size =  (count<<1) * sizeof(MEANING);
		meanings = (MEANING*) AllocateString(NULL,size); 
		memset(meanings,0,size); //   just to be purist
		memcpy(meanings+1,D->meanings+1,oldCount * sizeof(MEANING));
		D->meanings =  meanings;
	}

	return meanings[count] = M;
}

MEANING GetMaster(MEANING T)
{ //   for a specific meaning return node that is master
	if (!T) return 0;
    WORDP D = Meaning2Word(T);
	if (!D->meaningCount) 
		return 0; // has none, all erased
    unsigned int index = Meaning2Index(T);
	if (index > D->meaningCount)
	{
		ReportBug("Bad meaning index %s %d",D->word,index);
		return T;
	}
    return (!D->meaningCount || index == 0) ? T : GetMeanings(D)[index];
}

void RemoveMeaning(MEANING M, MEANING M1)
{
	//   remove meaning and keep only valid main POS values (may not have a synset ptr when its irregular conjugation or such)
	uint64 flags = 0;
	WORDP D = Meaning2Word(M);
	uint64 removeFlag = 0;
	for (unsigned int i = 1; i <= D->meaningCount; ++i)
	{
		MEANING master = D->meanings[i];
		if (!master) continue;
		WORDP D1 = Meaning2Word(master);
		if (master == M1)
		{
			D->meanings[i] = 0;
			removeFlag = D1->properties;
		}
		else if (D1 == D);	//   self ptr
		else flags |= D1->properties & (NOUN|VERB|ADJECTIVE|ADVERB);
	}
	if (flags & removeFlag); // still have such kind left
	else RemoveProperty(D,removeFlag);
	if (!(D->properties & NOUN)) RemoveProperty(D, NOUN_PROPERTIES);
	if (!(D->properties & VERB))
	{
		RemoveSystemFlag(D, VERB_CONJUGATE1 | VERB_CONJUGATE2 | VERB_CONJUGATE3| VERB_DIRECTOBJECT | VERB_INDIRECTOBJECT | VERB_ADJECTIVE_OBJECT   );
		RemoveProperty(D, VERB_INFINITIVE | VERB_PRESENT | VERB_PRESENT_3PS );
		if (D->properties & VERB_PAST) AddProperty(D,VERB);	// felt is past tense, unrelated to verb to felt
	}
	if (!(D->properties & ADJECTIVE)) RemoveProperty(D,ADJECTIVE_BITS);
	if (!(D->properties & ADVERB)) RemoveProperty(D, ADVERB_BITS);
}

MEANING ReadMeaning(char* word,bool create,bool usemaster)
{// be wary of multiple deletes of same word in low-to-high-order
	char hold[MAX_WORD_SIZE];
	if (*word == '\\' && word[1] && !word[2]) 
	{
		strcpy(hold,word+1);	//   special single made safe, like \[  or \*
		word = hold;
	}

	unsigned int flags = 0;
	unsigned int index = 0;

	char* at = (word[0] != '~') ?  strchr(word,'~') : NULL; 
	if (at && *word != '"' ) // beware of topics or other things, dont lose them. we want xxx~n (one character) or  xxx~digits  or xxx~23n
	{
		if (IsDigit(at[1]))  // number starter
		{
			char* p = at;
			while (IsDigit(*++p)); // find end
			index = atoi(at+1);
			if (*p == 'n') flags = NOUN;
			else if (*p == 'v') flags = VERB;
			else if (*p == 'a') flags = ADJECTIVE;
			else if (*p == 'b') flags = ADVERB;
			else if (*p == 'p') flags = PREPOSITION;
			*at = 0;
		}
		if (index == 0)
		{
			if (at[2] && at[2] != ' '); // insure flag only the last character - write meaning can write multiple types, but only for internal display. externally only 1 type at a time is allowed to be input
			else if (at[1] == 'n') flags = NOUN;
			else if (at[1] == 'v') flags = VERB;
			else if (at[1] == 'a') flags = ADJECTIVE;
			else if (at[1] == 'b') flags = ADVERB;
			else if (at[1] == 'p') flags = PREPOSITION;
			if (flags) *at = 0;
		}
	}
	if (*word != '"') 
	{
		strcpy(hold,JoinWords(BurstWord(word,0)));
		word = hold;
	}
	WORDP D = (create) ? StoreWord(word,AS_IS) : FindWord(word,0,PRIMARY_CASE_ALLOWED);
    if (!D)  return 0;
	if (D->word[0] == '~'); // concept has no synset master
	else if (usemaster && index && index <= D->meaningCount && D->meanings[index]) return D->meanings[index];	// when given bird~1 internally use synset head instead of us
	return MakeMeaning(D,index) | flags;
}

bool ReadDictionary(char* file)
{
	char junk[MAX_WORD_SIZE];
	char word[MAX_WORD_SIZE];
	char* ptr;
    char* equal;
	FILE* in = fopen(file,"rb");
	if (!in) return false;
	while (ReadALine(readBuffer,in))
	{
		ptr = SkipWhitespace(readBuffer); //   cannot trust user editing the dictionary
		ptr = ReadCompiledWord(ptr,word);	//   the actual word
		if (!*word) continue;
		ptr = SkipWhitespace(ptr); //   cannot trust user editing the dictionary
		ptr = ReadCompiledWord(ptr,junk);	//   read open paren
		if (*junk != '(') ReportBug("bad dictionary alignment");
		WORDP D = StoreWord(word,AS_IS);
		ptr = ReadDictionaryFlags(D,ptr);

		++ndict; //   for validation checking
		//   precreate meanings...

		//   read cross-reference attribute ptrs
		while (*ptr)		//   read until closing paren
		{
			ptr = SkipWhitespace(ptr); //   cannot trust user editing the dictionary
			char* hold = ptr;
			ptr = ReadCompiledWord(ptr,word);
			if (!*word) break;
			equal = strchr(word,'=');
			*equal++ = 0;
			if (!strcmp(word,"conjugate")) D->conjugation = StoreWord(equal);
			else if (!strcmp(word,"plural")) D->plurality = StoreWord(equal);
			else if (!strcmp(word,"comparative")) D->comparison = StoreWord(equal);
			else if (!strcmp(word,"substitute")) D->substitutes = StoreWord(equal);
			else if (!strcmp(word,"gloss")) //   last thing (will be only on wordnet synset id)
			{
				D->w.gloss = AllocateString(hold+6); //   past gloss=
				break;
			}
		}
		//   directly create meanings, since we know the size-- no meanings may be added after this
		if (D->meaningCount)
		{
			unsigned int size = (D->meaningCount+1) * sizeof(MEANING);
			MEANING* meanings = (MEANING*) AllocateString(NULL,size); 
			memset(meanings,0,size); //   just to be purist
			D->meanings =  meanings;
			for (unsigned int i = 1; i <= D->meaningCount; ++i) //   read each meaning
			{
				ReadALine(readBuffer,in);
				ReadCompiledWord(SkipWhitespace(readBuffer),junk);	 //   cannot trust user editing the dictionary
				D->meanings[i] = ReadMeaning(junk);
			}
		}
	}
	fclose(in);
	return true;
}

MEANING MakeTypedMeaning(WORDP x, unsigned int y, unsigned int flags)
{
	if (!x) return 0;
	MEANING ans = ((MEANING)(Word2Index(x) + (((unsigned int)y) << INDEX_OFFSET))); 
    return ans | flags;
}

MEANING MakeMeaning(WORDP x, unsigned int y) //   compose a meaning
{
    if (!x) return 0;
    MEANING ans = ((MEANING)(Word2Index(x) + (((unsigned int)y) << INDEX_OFFSET))); 
    return ans;
}

WORDP Meaning2Word(MEANING x) //   convert meaning to its dictionary entry
{
    if (!x) return NULL;
    return  Index2Word((((ulong_t)x) & MAX_DICTIONARY) ); //   get the dict index part
}

WORDP Meaning2MeaningWord(MEANING T) //   get meaning as a word (label~3 for example)
{
	unsigned int index = Meaning2Index(T);
	WORDP D = Meaning2Word(T);
	if (!index) return D; //   the word of the meaning generic

	char buf[MAX_WORD_SIZE];
	strcpy(buf,D->word);
	unsigned int len = strlen(buf);
	sprintf(buf+len,"~%d",index);
	return FindWord(buf,0);
}

unsigned int GetMeaningType(MEANING T)
{
    if (T == 0) return 0;
	WORDP D = Meaning2Word(T);
    unsigned int index = Meaning2Index(T);
	if (index) T = D->meanings[index]; //   change to synset head for specific meaning
	else if (T & TYPE_RESTRICTION) return T & TYPE_RESTRICTION; //   generic word type it must be
	D = Meaning2Word(T);
	return (unsigned int) (D->properties & PART_OF_SPEECH);
}

char* GetPastTense(char* word)
{
    static char buffer[MAX_WORD_SIZE];
    WORDP D = FindWord(word);
    if (D && D->properties & VERB && D->conjugation ) //   die is both regular and irregular?
    {
        int n = 9;
        while (--n && D)
        {
            if (D->properties & VERB_PAST) return D->word; //   might be an alternate present tense moved to main
            D = GetTense(D); //   known irregular
        }
        return NULL;
    }
    
    //   check for multiword behavoir. Always change the 1st word only
    char* at =  strchr(word,'_'); 
	if (!at) at = strchr(word,'-');
    if (at)
    {
		int cnt = BurstWord(word,HYPHENS);
		char trial[MAX_WORD_SIZE];
        char words[10][MAX_WORD_SIZE];
		unsigned int lens[10];
		char separators[10];
		if (cnt > 9) return NULL;
		int len = 0;
		for (int i = 1; i <= cnt; ++i) //   get its components
		{
			strcpy(words[i],GetBurstWord(i));
			lens[i] = strlen(words[i]);
			len += lens[i];
			separators[i] = word[len++];
		}
		for (int i = 1; i <= cnt; ++i)
		{
			if (D && (D->systemFlags & (VERB_CONJUGATE1|VERB_CONJUGATE2|VERB_CONJUGATE3)))
			{
				if (D->systemFlags & VERB_CONJUGATE1 && i != 1) continue;
				if (D->systemFlags & VERB_CONJUGATE2 && i != 2) continue;
				if (D->systemFlags & VERB_CONJUGATE3 && i != 3) continue;
			}
			else if (*at == '_' && i != 1) continue;
			else if (*at == '-' && i != 2) continue;
			char* inf = GetPastTense(words[i]); //   is this word an infinitive?
			if (!inf) continue;
			*trial = 0;
			char* at = trial;
			for (int j = 1; j <= cnt; ++j) //   rebuild word
			{
				if (j == i)
				{
					strcpy(at,inf);
					at += strlen(inf);
					*at++ = separators[j];
				}
				else
				{
					strcpy(at,words[j]);
					at += lens[j];
					*at++ = separators[j];
				}
			}
			strcpy(buffer,trial);
			return buffer;
		}
	}

    strcpy(buffer,word);
    unsigned int len = strlen(buffer);
    if (buffer[len-1] == 'e') strcat(buffer,"d");
    else if (!(IsVowel(buffer[len-1]) || buffer[len-1] == 'y' ) && IsVowel(buffer[len-2]) && len > 2 && !IsVowel(buffer[len-3])) 
    {
        char lets[2];
        lets[0] = buffer[len-1];
        lets[1] = 0;
        strcat(buffer,lets); 
        strcat(buffer,"ed");
    }
    else if (buffer[len-1] == 'y' && !IsVowel(buffer[len-2])) 
    {
        buffer[len-1] = 'i';
        strcat(buffer,"ed");
    }   
    else strcat(buffer,"ed");
    return buffer; 
}

char* GetPastParticiple(char* word)
{
    static char buffer[MAX_WORD_SIZE];
    WORDP D = FindWord(word,0);
    if (D && D->properties & VERB && D->conjugation ) 
    {
        int n = 9;
        while (--n && D)
        {
            if (D->properties & VERB_PAST_PARTICIPLE) return D->word; //   might be an alternate present tense moved to main
            D = GetTense(D); //   known irregular
        }
   }
   
    //   check for multiword behavoir. Always change the 1st word only
	char* at =  strchr(word,'_'); 
	if (!at) at = strchr(word,'-');
    if (at)
    {
		int cnt = BurstWord(word,HYPHENS);
		char trial[MAX_WORD_SIZE];
        char words[10][MAX_WORD_SIZE];
		unsigned int lens[10];
		char separators[10];
		if (cnt > 9) return NULL;
		unsigned int len = 0;
		for (int i = 1; i <= cnt; ++i) //   get its components
		{
			strcpy(words[i],GetBurstWord(i));
			lens[i] = strlen(words[i]);
			len += lens[i];
			separators[i] = word[len++];
		}
		for (int i = 1; i <= cnt; ++i)
		{
			if (D && (D->systemFlags & (VERB_CONJUGATE1|VERB_CONJUGATE2|VERB_CONJUGATE3)))
			{
				if (D->systemFlags & VERB_CONJUGATE1 && i != 1) continue;
				if (D->systemFlags & VERB_CONJUGATE2 && i != 2) continue;
				if (D->systemFlags & VERB_CONJUGATE3 && i != 3) continue;
			}
			else if (*at == '_' && i != 1) continue;
			else if (*at == '-' && i != 2) continue;
			char* inf = GetPastParticiple(words[i]); //   is this word an infinitive?
			if (!inf) continue;
			*trial = 0;
			char* at = trial;
			for (int j = 1; j <= cnt; ++j) //   rebuild word
			{
				if (j == i)
				{
					strcpy(at,inf);
					at += strlen(inf);
					*at++ = separators[j];
				}
				else
				{
					strcpy(at,words[j]);
					at += lens[j];
					*at++ = separators[j];
				}
			}
			strcpy(buffer,trial);
			return buffer;
		}
	}

    strcpy(buffer,word);
	unsigned int len = strlen(buffer);
 	if ((buffer[len-1] == 's' && buffer[len-2] == 's') || 
        buffer[len-1] == 'x' || 
        (buffer[len-1] == 'h' && buffer[len-2] == 'c') || 
        (buffer[len-1] == 'h' && buffer[len-2] == 's')) strcat(buffer,"ed");
    else if (buffer[len-1] == 'e') strcat(buffer,"d");
    else if (!(IsVowel(buffer[len-1]) || buffer[len-1] == 'y' ) && IsVowel(buffer[len-2]) && len > 2 && !IsVowel(buffer[len-3])) 
    {
        char lets[2];
        lets[0] = buffer[len-1];
        lets[1] = 0;
        strcat(buffer,lets); 
        strcat(buffer,"ed");
    }
    else if (buffer[len-1] == 'y' && !IsVowel(buffer[len-2])) 
    {
        buffer[len-1] = 'i';
        strcat(buffer,"ed");
    }   
    else strcat(buffer,"ed");
    return buffer; 
}

char* GetPresentParticiple(char* word)
{
    static char buffer[MAX_WORD_SIZE];
    WORDP D = FindWord(word,0);
    if (D && D->properties & VERB && D->conjugation ) 
    {//   born (past) -> bore (a past and a present) -> bear 
        int n = 9;
        while (--n && D) //   we have to cycle all the conjugations past and present of to be
        {
            if (D->properties &  VERB_PRESENT_PARTICIPLE) return D->word; 
            D = GetTense(D); //   known irregular
        }
        //ReportBug("verb loop"); //   NOT A BUG because we allow regular forms NOT to be in dictionary, even if rest is irregular
    }
    
    strcpy(buffer,word);
    unsigned int len = strlen(buffer);
    char* inf = GetInfinitive(word);
    if (!inf) return 0;

    if (buffer[len-1] == 'g' && buffer[len-2] == 'n' && buffer[len-3] == 'i' && (!inf || stricmp(inf,word)))   //   ISNT participle though it has ing ending (unless its base is "ing", like "swing"
    {
        return word;
    }

    //   check for multiword behavoir. Always change the 1st word only
    char* at =  strchr(word,'_'); 
	if (!at) at = strchr(word,'-');
    if (at)
    {
		int cnt = BurstWord(word,HYPHENS);
		char trial[MAX_WORD_SIZE];
        char words[10][MAX_WORD_SIZE];
		int lens[10];
		char separators[10];
		if (cnt > 9) return NULL;
		int len = 0;
		for (int i = 1; i <= cnt; ++i) //   get its components
		{
			strcpy(words[i],GetBurstWord(i));
			lens[i] = strlen(words[i]);
			len += lens[i];
			separators[i] = word[len++];
		}
		for (int i = 1; i <= cnt; ++i)
		{
			if (D && (D->systemFlags & (VERB_CONJUGATE1|VERB_CONJUGATE2|VERB_CONJUGATE3)))
			{
				if (D->systemFlags & VERB_CONJUGATE1 && i != 1) continue;
				if (D->systemFlags & VERB_CONJUGATE2 && i != 2) continue;
				if (D->systemFlags & VERB_CONJUGATE3 && i != 3) continue;
			}
			else if (*at == '_' && i != 1) continue;
			else if (*at == '-' && i != 2) continue;
			char* inf = GetPresentParticiple(words[i]); //   is this word an infinitive?
			if (!inf) continue;
			*trial = 0;
			char* at = trial;
			for (int j = 1; j <= cnt; ++j) //   rebuild word
			{
				if (j == i)
				{
					strcpy(at,inf);
					at += strlen(inf);
					*at++ = separators[j];
				}
				else
				{
					strcpy(at,words[j]);
					at += lens[j];
					*at++ = separators[j];
				}
			}
			strcpy(buffer,trial);
			return buffer;
		}
	}


    strcpy(buffer,inf); //   get the real infinitive

    if (!stricmp(buffer,"be"));
    else if (buffer[len-1] == 'h' || buffer[len-1] == 'w' ||  buffer[len-1] == 'x' ||  buffer[len-1] == 'y'); //   no doubling w,x,y h, 
    else if (buffer[len-2] == 'i' && buffer[len-1] == 'e') //   ie goes to ying
    {
        buffer[len-2] = 'y';
        buffer[len-1] = 0;
    }
    else if (buffer[len-1] == 'e' && !IsVowel(buffer[len-2]) ) //   drop ending Ce  unless -nge (to keep the j sound) 
    {
        if (buffer[len-2] == 'g' && buffer[len-3] == 'n');
        else buffer[len-1] = 0; 
    }
    else if (buffer[len-1] == 'c' ) //   add k after final c
    {
        buffer[len-1] = 'k'; 
        buffer[len] = 0;
    }
   //   double consonant 
    else if (!IsVowel(buffer[len-1]) && IsVowel(buffer[len-2]) && (!IsVowel(buffer[len-3]) || (buffer[len-3] == 'u' && buffer[len-4] == 'q'))) //   qu sounds like consonant w
    {
        char* base = GetInfinitive(word);
        WORDP D = FindWord(base,0);
        if (D && D->properties & VERB && D->conjugation ) 
        {
            int n = 9;
            while (--n && D)
            {
                if (D->properties & VERB_PAST) 
                {
                    unsigned int len = strlen(D->word);
                    if (D->word[len-1] != 'd' || D->word[len-2] != 'e' || len < 5) break; 
                    if (IsVowel(D->word[len-3])) break; //   we ONLY want to see if a final consonant is doubled. other things would just confuse us
                    strcpy(buffer,D->word);
                    buffer[len-2] = 0;      //   drop ed
                    strcat(buffer,"ing");   //   add ing
                    return buffer; 
                }
                D = GetTense(D); //   known irregular
            }
            if (!n) ReportBug("verb loop"); //   complain ONLY if we didnt find a past tense
        }

        char lets[2];
        lets[0] = buffer[len-1];
        lets[1] = 0;
        strcat(buffer,lets);    //   double consonant
    }
    strcat(buffer,"ing");
    return buffer; 
}

char* GetPluralNoun(WORDP noun)
{
	if (!noun) return NULL;
    if ((noun->properties & (NOUN_SINGULAR|NOUN_PLURAL)) == (NOUN_SINGULAR|NOUN_PLURAL)) return noun->word; 
    WORDP plural = GetPlural(noun);
	if (noun->properties & NOUN_SINGULAR) 
    {
        if (plural) return plural->word;
        static char word[MAX_WORD_SIZE];
		unsigned int len = strlen(noun->word);
		char end = noun->word[len-1];
		char before = noun->word[len-2];
		if (end == 's') sprintf(word,"%ses",noun->word); // glass -> glasses
		else if (end == 'h' && (before == 'c' || before == 's')) sprintf(word,"%ses",noun->word); // witch -> witches
		else if ( end == 'o' && !IsVowel(before)) sprintf(word,"%ses",noun->word); // hero -> heroes>
		else if ( end == 'y' && !IsVowel(before)) // cherry -> cherries
		{
			if (IsUpperCase(noun->word[0])) sprintf(word,"%ss",noun->word); // Germany->Germanys
			else
			{
				strncpy(word,noun->word,len-1);
				strcpy(word+len-1,"ies"); 
			}
		}
		else sprintf(word,"%ss",noun->word);
        return word;
    }
    return noun->word;
}

char* GetAdverbBase(char* word)
{
 	adverbFormat = 0;
    if (!word) return NULL;
	if (IsUpperCase(word[0])) return NULL; // not as proper
    unsigned int len = strlen(word);
    if (len == 0) return NULL;
    char lastc = word[len-1];  
    char priorc = (len > 2) ? word[len-2] : 0; 
    char prior2c = (len > 3) ? word[len-3] : 0; 
    WORDP D = FindWord(word,0);
	adverbFormat = ADVERB_BASIC;
    if (D && D->properties & (ADVERB_BASIC | WH_ADVERB)) return D->word; //   we know it as is

	if (D && D->properties & ADVERB)
    {
        int n = 5;
		WORDP original = D;
        while (--n  && D)
        {
            D = GetCompare(D);
            if (D && !(D->properties & (ADVERB_MORE|ADVERB_MOST))) 
			{
				if (original->properties & ADVERB_MORE) adjectiveFormat = ADVERB_MOST;
				else if (original->properties & ADVERB_MORE) adjectiveFormat = ADVERB_MOST;
				return D->word;
			}
        }
    }

    if (len > 4 && priorc == 'l' && lastc == 'y')
    {
		char form[MAX_WORD_SIZE];
        D = FindWord(word,len-2); // rapidly
        if (D && D->properties & (VERB|ADJECTIVE)) return D->word;
		if (prior2c == 'i')
		{
			D = FindWord(word,len-3); // lustily
			if (D && D->properties & (VERB|ADJECTIVE)) return D->word;
			// if y changed to i, change it back
			strcpy(form,word);
			form[len-3] = 'y';
			form[len-2] = 0;
			D = FindWord(word,len-2); // happily  from happy
			if (D && D->properties & (VERB|ADJECTIVE)) return D->word;
		}
		// try terrible -> terribly
		strcpy(form,word);
		form[len-1] = 'e';
 		D = FindWord(word,len-2); // happily  from happy
		if (D && D->properties & (VERB|ADJECTIVE)) return D->word;
    }
	if (len > 5 && priorc == 'e' && lastc == 'r')
    {
        D = FindWord(word,len-2);
        if (D && D->properties & ADVERB) 
		{
			adverbFormat = ADVERB_MORE;
			return D->word;
		}
    }
	if (len > 5 && prior2c == 'e' && priorc == 's' && lastc == 't')
    {
        D = FindWord(word,len-3);
        if (D && D->properties & ADVERB) 
		{
			adverbFormat = ADVERB_MORE;
			return D->word;
		}
    }

    return NULL;
}

char* GetAdjectiveBase(char* word)
{
	adjectiveFormat = 0;
    unsigned int len = strlen(word);
    if (len == 0) return NULL;
    WORDP D = FindWord(word,0);
	char lastc = word[len-1];  
    char priorc = (len > 2) ? word[len-2] : 0;  //   Xs
    char priorc1 = (len > 3) ? word[len-3] : 0; //   Xes
    char priorc2 = (len > 4) ? word[len-4] : 0; //   Xhes
    char priorc3 = (len > 5) ? word[len-5] : 0; //   Xgest
 
	bool possibleMore = false;
	if (priorc == 'e' && lastc == 'r') possibleMore = true;//   more
	else if (priorc == 's' && lastc == 't') possibleMore = true;//   more
 
    if (D && D->properties & ADJECTIVE && D->properties & ADJECTIVE_BASIC)
	{
		adjectiveFormat = ADJECTIVE_BASIC;
		return D->word; //   already base
	}
    if (D && D->properties & ADJECTIVE &&  !possibleMore)
    {
        int n = 5;
		WORDP original = D;
        while (--n  && D)
        {
            D = GetCompare(D);
            if (D && !(D->properties & (ADJECTIVE_MORE|ADJECTIVE_MOST))) 
			{
				if (original->properties & ADJECTIVE_MORE) adjectiveFormat = ADJECTIVE_MORE;
				else if (original->properties & ADJECTIVE_MORE) adjectiveFormat = ADJECTIVE_MORE;
				return D->word;
			}
        }
    }
	   
    //   see if composite
    char composite[MAX_WORD_SIZE];
    strcpy(composite,word);
    char* hyphen = strchr(composite+1,'-');
    if (hyphen)
    {
        hyphen -= 4;
		char* althyphen = (hyphen - composite) + word;
        if (hyphen[2] == 'e' && hyphen[3] == 'r') //   lower-density?
        {
            strcpy(hyphen+2,althyphen+4); //   remove er
            char* answer = GetAdjectiveBase(composite);
            if (answer) return answer;
        }
        if (hyphen[1] == 'e' && hyphen[2] == 's' && hyphen[2] == 't' ) //   lowest-density?
        {
            strcpy(hyphen+1,althyphen+4); //   remove est
            char* answer = GetAdjectiveBase(composite);
            if (answer) return answer;
        }
    }

    if (len > 4 && priorc == 'e' && lastc == 'r') //   more
    {
		 adjectiveFormat = ADJECTIVE_MORE;
         D = FindWord(word,len-2);
         if (D && D->properties & ADJECTIVE) return D->word; //   drop er
         D = FindWord(word,len-1);
         if (D && D->properties & ADJECTIVE) return D->word; //   drop e (already ended in e)

         if (priorc1 == priorc2  )  
         {
            D = FindWord(word,len-3);
            if (D && D->properties & ADJECTIVE) return D->word; //   drop Xer
         }
         if (priorc1 == 'i') //   changed y to ier?
         {
            word[len-3] = 'y';
            D = FindWord(word,len-2);
            word[len-3] = 'i';
            if (D && D->properties & ADJECTIVE) return D->word; //   drop Xer
          }
	}  
	else if (len > 5 && priorc1 == 'e' &&  priorc == 's' && lastc == 't') //   most
    {
		adjectiveFormat = ADJECTIVE_MOST;
        D = FindWord(word,len-3);//   drop est
        if (D && D->properties & ADJECTIVE) return D->word; 
        D = FindWord(word,len-2);//   drop st (already ended in e)
        if (D && D->properties & ADJECTIVE) return D->word; 
        if (priorc2 == priorc3  )   
        {
             D = FindWord(word,len-4);//   drop Xest
             if (D && D->properties & ADJECTIVE) return D->word; 
        }
        if (priorc2 == 'i') //   changed y to iest
        {
             word[len-4] = 'y';
             D = FindWord(word,len-3); //   drop est
             word[len-4] = 'i';
             if (D && D->properties & ADJECTIVE) return D->word; 
        }   
    }

	else if (len > 4 && priorc1 == 'i' && priorc == 'n' && lastc == 'g') //   partciple verb as adjective
    {
		D = FindWord(word,len-3);
		if (D && D->properties & VERB) 
		{
			adjectiveFormat = ADJECTIVE_PARTICIPLE;
			return word;
		}
	}
	
	char form[MAX_WORD_SIZE];

	// ible and able-  visible, agreeable
	if (len < 5);
	else if (!strcmp(word+len-4,"ible") || !strcmp(word+len-4,"able"))
	{
		strcpy(form,word);
		form[len-4] = 0;
		D = FindWord(form);
		if (D) return word;
		// other 
	}

	// al and ual - sensual gradual
	if (len < 4);
	else if (!strcmp(word+len-2,"al"))
	{
		strcpy(form,word);
		form[len-2] = 0;
		if ( form[len-3] == 'u') form[len-3] = 0;  // ual
		strcat(form,"e");
		D = FindWord(form);
		if (D && D->properties & (NOUN|VERB)) return word;
		// other 
	}

	
	// an and ian - Martian 
	if (len < 4);
	else if (!strcmp(word+len-2,"an"))
	{
		strcpy(form,word);
		form[len-2] = 0;
		if ( form[len-3] == 'i') form[len-3] = 0;  // ian
		D = FindWord(form);
		if (D && D->properties & NOUN) return word;
	}

	
	// ance and ancy - assistance defiance 
	if (len < 6);
	else if (!strcmp(word+len-4,"ance") || !strcmp(word+len-4,"ancy"))
	{
		return word;
	}

	// ant - assistant 
	if (len < 3);
	else if (!strcmp(word+len-3,"ant"))
	{
		strcpy(form,word);
		form[len-3] = 0;
		D = FindWord(form);
		if (D && D->properties & VERB) return word;
	}

	// ary ery ory - dictionary bravery dormitory 
	if (len < 5);
	else if (!strcmp(word+len-3,"ary") || !strcmp(word+len-3,"ery") || !strcmp(word+len-3,"ory"))
	{
		return word;
	}

	
	// ic fic ful - 
	if (len < 5);
	else if (!strcmp(word+len-3,"fic") || !strcmp(word+len-3,"ful") || !strcmp(word+len-2,"ic")  )
	{
		return word;
	}

		
	// ish- 
	if (len < 5);
	else if (!strcmp(word+len-3,"ish")  )
	{
		return word;
	}

	// less- 
	if (len < 6);
	else if (!strcmp(word+len-4,"less")  )
	{
		return word;
	}

	//ous ious
	if (len < 5);
	else if (!strcmp(word+len-3,"ous")  )
	{
		return word;
	}

	// oid 
	if (len < 5);
	else if (!strcmp(word+len-3,"oid")  )
	{
		return word;
	}

	//escent
	if (len < 8);
	else if (!strcmp(word+len-6,"escent")  )
	{
		return word;
	}

	//iferous
	if (len < 9);
	else if (!strcmp(word+len-7,"iferous")  )
	{
		return word;
	}

    return NULL;
}

char* SingularFromSuffix(char* word,unsigned int len,WORDP D,WORDP B)
{
	
	char last = word[len-1];  
    char prior = (len > 2) ? word[len-2] : 0;  //   Xs
    char prior1 = (len > 3) ? word[len-3] : 0; //   Xes
    char prior2 = (len > 4) ? word[len-4] : 0; //   Xhes

    //   could this be a regular plural noun we dont know about?
     if (last == 's' && len > 3 && (!D || !(D->properties & PRONOUN)))//   not known plural noun - and not his hers its
     {
 		nounFormat = NOUN_PLURAL;
		if (IsUpperCase(word[0])) nounFormat = NOUN_PROPER_PLURAL;
        D = FindWord(word,len-1,PRIMARY_CASE_ALLOWED);
		if (D && D->properties & NOUN && !(D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL))) return D->word; //   dont convert rubies to Rubie

		if (prior1 == 'o' || prior1 == 's' || prior1 == 'x' ||  prior1 == 'z' || (prior1 == 'h' && (prior2 == 'c' || prior2 == 's')))  //    the "-es" ending added onto
		{
			D = FindWord(word,len-2,PRIMARY_CASE_ALLOWED);
			if (D && D->properties & NOUN) return D->word;
		}
		if (prior1 == 'i' && prior == 'e' ) //   try changed y to -ies
		{
			word[len-3] = 'y'; //   change i to y
			D = FindWord(word,len-2,PRIMARY_CASE_ALLOWED);
			word[len-3] = 'i';
			if (D && D->properties & NOUN) return D->word;
		}
     
		if (prior == 'e' ) //   try changed "is" to -es    Thesis or synopsis
		{
			word[len-2] = 'i'; //   revert e to i 
			D = FindWord(word,len,PRIMARY_CASE_ALLOWED);
			word[len-2] = 'e';
			if (D && D->properties & NOUN) return D->word;
		}
		if (prior1 == 'v' && prior == 'e' ) //    f or fe changed to to -ves
		{
			word[len-3] = 'f'; //   chanve v to f
			D = FindWord(word,len-1); //   knife->knives  --drop the s
			if (!D) D = FindWord(word,len-2,PRIMARY_CASE_ALLOWED);
			word[len-3] = 'v';
			if (D && D->properties & NOUN) return D->word;
		}
		//   plural of a gerunded like paintings -- return paint (pretend its a noun)
		if (prior2 == 'i' && prior1 == 'n' && prior == 'g' && !IsUpperCase(word[0]))
		{
			D = FindWord(word,len-4,PRIMARY_CASE_ALLOWED);
			if (D && D->properties & VERB) 
			{
				nounFormat = NOUN_GERUND;
				return D->word;
			}
		}
    }

	//   NOUNS PRONOUN_POSSESSIVE
    if (len > 3 && !stricmp(word+len-2,"'s")) //   possessive
    {
        D = FindWord(word,len-2,PRIMARY_CASE_ALLOWED);
        if (D && D->properties & NOUN) return D->word;
    }

	if (D && IsUpperCase(D->word[0]) && D->properties & NOUN) return D->word;	//   proper name 

	// upper case words are all proper names (except pronoun I)
	if (D && IsUpperCase(word[0])) 
	{
		if (D->properties & PRONOUN) return NULL;
		nounFormat = NOUN_PROPER_SINGULAR;
		return D->word;	//   proper name 
	}

	// if word is an abbreviation it is a noun (proper if uppcase)
	if (strchr(word,'.')) return word;

	// hypenated word check word at end
	char* hypen = strchr(word+1,'-');
	if ( hypen && len > 2)
	{
		D = FindWord(hypen+1);
		if (D && D->properties & NOUN) return D->word; // but might be plural?

		unsigned int len = strlen(hypen);
		if ( hypen[len-1] == 's')
		{
			D = FindWord(hypen+1,len-2);
			if (D && D->properties & NOUN) return D->word; 
		}
	}

	//suffix ade like blockade, lemonade means result of action
	if (len > 5  && !strcmp(word+len-3,"ade"))
	{
		D = FindWord(word,len-3);
		if (D && D->properties & (NOUN|VERB)) return word;
	}
	
	//suffix age like salvage storage - state of
	if (len > 5 && !strcmp(word+len-3,"age"))
	{
		char root[MAX_WORD_SIZE];
		strcpy(root,word);
		strcpy(root+len-3,"e");
		D = FindWord(root);
		if (D) return word;
	}
	
	//suffix algia like neuralgia
	if (len > 7 && !strcmp(word+len-5,"algia"))
	{
		D = FindWord(word,len-5);
		if (D && D->properties & (NOUN|VERB)) return word;
	}

		
	//suffix cian like magician
	if (len > 6 && !strcmp(word+len-4,"cian"))
	{
		return word;
	}

	//suffix cy like necromancy
	if (len > 4 && !strcmp(word+len-2,"cy"))
	{
		return word;
	}

	//suffix dom like kingdom
	if (len > 5 && !strcmp(word+len-3,"dom"))
	{
		return word;
	}

	//suffix ee like evacuee
	if (len > 4 && !B && !strcmp(word+len-2,"ee"))
	{
		return word;
	}

	//suffix ence ency  like difference
	if (len < 7);
	else  if (!strcmp(word+len-4,"ence") || !strcmp(word+len-4,"ency"))
	{
		return word;
	}

	//suffix or er  like backer
	if (len > 4 && !B && (!strcmp(word+len-2,"or") || !strcmp(word+len-2,"er")))
	{
		if (FindWord(word,len-2)) return word;
		if (FindWord(word,len-1)) return word;
	}

	//suffix esis osis  genesis hypnosis
	if (len < 7);
	else if (!strcmp(word+len-4,"esis") || !strcmp(word+len-4,"osis"))
	{
		return word;
	}

	//suffix ess - goddess --- female
	if (len > 5 && !B &&!strcmp(word+len-3,"ess") )
	{
		return word;
	}

	//suffix hood
	if (len > 6 && !strcmp(word+len-4,"hood") )
	{
		return word;
	}

	//suffix ette 
	if (len > 6 && !B &&!strcmp(word+len-4,"ette") )
	{
		return word;
	}
	
	//suffix ice 
	if (len > 5 && !B &&!strcmp(word+len-3,"ice") )
	{
		return word;
	}
	
	//suffix tude 
	if (len > 6 && !B && !strcmp(word+len-4,"tude") )
	{
		return word;
	}

	//suffix ment ness logy && ology
	if (len > 6 && !B && (!strcmp(word+len-4,"ment") || !strcmp(word+len-4,"ness") || !strcmp(word+len-4,"logy") ))
	{
		return word;
	}


	//suffix ion sion tion  
	if (len > 6 && !B && (!strcmp(word+len-3,"ion") || !strcmp(word+len-4,"sion") || !strcmp(word+len-4,"tion")) )
	{
		return word;
	}
	
	//suffix ism ist ity ty
	if (len > 5 && !B && (!strcmp(word+len-3,"ism") || !strcmp(word+len-3,"ist") || !strcmp(word+len-3,"ity")  || !strcmp(word+len-2,"ty")) )
	{
		return word;
	}

    return NULL;
}

char* GetSingularNoun(char* word)
{ 
	nounFormat = 0;
    if (!word) return NULL;
    unsigned int len = strlen(word);
    if (len == 0) return NULL;
    WORDP D = FindWord(word,0,PRIMARY_CASE_ALLOWED);
	WORDP B = D;
	nounFormat = NOUN_SINGULAR;
		 // BUG-- consider upper case BLOCKS all endings but s
	if (D && D->properties & NOUN_PROPER_SINGULAR) //   is already singular
	{
		nounFormat = NOUN_PROPER_SINGULAR;
		return D->word;
	}

    //   we know the noun and its plural, use singular
    if (D && D->properties & NOUN_SINGULAR) //   is already singular
    {
		if (word[len-1] == 's') // even if singular, if simpler exists, use that. Eg.,  military "arms"  vs "arm" the part
		{
			char* sing = SingularFromSuffix(word,len,D,B);
			if (sing) return sing;
		}

        //   HOWEVER, some singulars are plurals--- words has its own meaning as well
        unsigned int len = strlen(D->word);
        if (len > 4 && D->word[len-1] == 's')
        {
            WORDP F = FindWord(D->word,len-1,PRIMARY_CASE_ALLOWED);
            if (F && F->properties & NOUN)  return F->word;  
        }
        return D->word; 
    }
	WORDP plural = (D) ? GetPlural(D) : NULL;
    if (D  && D->properties & NOUN_PLURAL && plural) 
	{
		nounFormat = NOUN_PLURAL;
		return plural->word; //   get singular form from plural noun
	}

	if ( D && D->properties & NOUN && !(D->properties & NOUN_PLURAL) && !(D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL))) return D->word; //   unmarked as plural, it must be singular unless its a name
    //   not registered but maybe we can know it anyway....
    if (!D && IsNumber(word))  return word;
	if (D && D->properties & AUX_VERB) return NULL; // avoid "is" or "was" as plural noun

	return SingularFromSuffix(word,len,D,B);

}

char* GetInfinitive(char* word)
{
	verbFormat = 0;	//   secondary answer- std infinitive or unknown
    if (!word || !*word) return NULL;
    unsigned int len = strlen(word);
    if (len == 0) return NULL;
    WORDP D = FindWord(word,len);
    if (D && D->properties & VERB_INFINITIVE) 
	{
		verbFormat = VERB_INFINITIVE;
		return D->word; //    infinitive value
	}

    if (D && D->properties & VERB && D->conjugation ) 
    {//   born (past) -> bore (a past and a present) -> bear 
		if (D->properties & VERB_PRESENT_PARTICIPLE) verbFormat = VERB_PRESENT_PARTICIPLE;
		else if (D->properties & (VERB_PAST|VERB_PAST_PARTICIPLE)) 
		{
			verbFormat = 0;
			if (D->properties & VERB_PAST) verbFormat |= VERB_PAST;
			if (D->properties & VERB_PAST_PARTICIPLE) verbFormat |= VERB_PAST_PARTICIPLE|ADJECTIVE_PARTICIPLE;
			
		}
		else if (D->properties & (VERB_PRESENT|VERB_PRESENT_3PS)) verbFormat = VERB_PRESENT;
		unsigned int n = 10;
        while (--n && D) //   scan all the conjugations 
        {
            if (D->properties & VERB_INFINITIVE) return D->word; 
            D = GetTense(D); //   known irregular
        }
    }
	
	char last = word[len-1];  
    char prior = (len > 2) ? word[len-2] : 0;  //   Xs
    char prior1 = (len > 3) ? word[len-3] : 0; //   Xes
    char prior2 = (len > 4) ? word[len-4] : 0; //   Xhes

    //   check for multiword behavior. 
	int cnt = BurstWord(word,HYPHENS);
    if (cnt > 1)
    {
		char trial[MAX_WORD_SIZE];
        char words[10][MAX_WORD_SIZE];
		unsigned int lens[10];
		char separators[10];
		if (cnt > 9) return NULL;
		unsigned int len = 0;
		for (int i = 1; i <= cnt; ++i) //   get its components
		{
			strcpy(words[i],GetBurstWord(i));
			lens[i] = strlen(words[i]);
			len += lens[i];
			separators[i] = word[len++];
		}
		for (int i = 1; i <= cnt; ++i)
		{
			char* inf = GetInfinitive(words[i]); //   is this word an infinitive?
			if (!inf) continue;
			*trial = 0;
			char* at = trial;
			for (int j = 1; j <= cnt; ++j) //   rebuild word
			{
				if (j == i)
				{
					strcpy(at,inf);
					at += strlen(inf);
					*at++ = separators[j];
				}
				else
				{
					strcpy(at,words[j]);
					at += lens[j];
					*at++ = separators[j];
				}
			}
			WORDP E = FindWord(trial);
			if (E && E->properties & VERB_INFINITIVE) return E->word;
		}

       return NULL;  //   not a verb
    }

    //   not known verb, try to get present tense from it
    if (last == 'd' && prior == 'e' && len > 3)   //   ed ending?
    {
		verbFormat = VERB_PAST|VERB_PAST_PARTICIPLE|ADJECTIVE_PARTICIPLE;
		D = FindWord(word,len-1);	//   drop d, on scare
		if (D && D->properties & VERB) return D->word;
        D = FindWord(word,len-2);    //   drop ed
        if (D && D->properties & VERB) return D->word; //   found it
        D = FindWord(word,len-1);    //   drop d
        if (D && D->properties & VERB) return D->word; //   found it
        if (prior1 == prior2  )   //   repeated consonant at end
        {
            D = FindWord(word,len-3);    //   drop Xed
            if (D && D->properties & VERB) return D->word; //   found it
        }
        if (prior1 == 'i') //   ied came from y
        {
            word[len-3] = 'y'; //   change i to y
            D = FindWord(word,len-2);    //   y, drop ed
            word[len-3] = 'i';
            if (D && D->properties & VERB) return D->word; //   found it
        }
     }
   
    //   could this be a participle verb we dont know about?
    if (prior1 == 'i' && prior == 'n' && last == 'g' && len > 4)//   maybe verb participle
    {
        char word1[MAX_WORD_SIZE];
		verbFormat = VERB_PRESENT_PARTICIPLE;
 
        //   try removing doubled consonant
        if (len > 4 &&  word[len-4] == word[len-5])
        {
            D = FindWord(word,len-4);    //   drop Xing spot consonant repeated
            if (D && D->properties & VERB) return D->word; //   found it
        }

        //   y at end, maybe came from ie
        if (word[len-4] == 'y')
        {
            strcpy(word1,word);
            word1[len-4] = 'i';
            word1[len-3] = 'e';
            word1[len-2] = 0;
            D = FindWord(word,len-3);    //   drop ing
            if (D && D->properties & VERB) return D->word; //   found it
        }

        //   two consonants at end, see if raw word is good priority over e added form
        if (len > 4 && !IsVowel(word[len-4]) && !IsVowel(word[len-5])) 
        {
            D = FindWord(word,len-3);    //   drop ing
            if (D && D->properties & VERB) return D->word; //   found it
        }

        //   otherwise try stem with e after it
        strcpy(word1,word);
        word1[len-3] = 'e';
        word1[len-2] = 0;
        D = FindWord(word1,len-2);    //   drop ing and put back 'e'
        if (D && D->properties & VERB) return D->word; //   found it

        //   simple ing added to word
        D = FindWord(word,len-3);    //   drop ing
        if (D && D->properties & VERB) return D->word; //   found it
    }

    //   ies from y
    if (prior1 == 'i' && prior == 'e' && last == 's' && len > 4)//   maybe verb participle
    {
 		verbFormat = VERB_PRESENT_3PS;
        char word1[MAX_WORD_SIZE];
        strcpy(word1,word);
        word1[len-3] = 'y';
        word1[len-2] = 0;
        D = FindWord(word1,len-2);    //   drop ing, add e
        if (D && D->properties & VERB) return D->word; //   found it
    }

     //   unknown singular verb
    if (last == 's' && len > 3)
    {
 		verbFormat = VERB_PRESENT_3PS;
        D = FindWord(word,len-1);    //   drop s
        if (D && D->properties & VERB && D->properties & VERB_INFINITIVE) return D->word; //   found it
		if (D && D->properties & NOUN) return NULL; //   dont move bees to be
        else if (prior == 'e')
        {
            D = FindWord(word,len-2);    //   drop es
            if (D && D->properties & VERB) return D->word; //   found it
        }
   }

    if (IsHelper(word)) 
	{
		verbFormat = 0;
		return word;
	}

	char form[MAX_WORD_SIZE];

	// ate - segregate liquidate
	if (len > 4 && !strcmp(word+len-3,"ate"))
	{
		strcpy(form,word);
		form[len-3] = 0;
		D = FindWord(form);
		if (D && D->properties & (NOUN|ADJECTIVE)) return word;
	}

	// fy 
	if (len > 4 && !strcmp(word+len-2,"fy"))
	{
		return word;
	}
	// ize 
	if (len > 4 && !strcmp(word+len-3,"ize"))
	{
		return word;
	}

    return NULL;    //   signal failure
}

MEANING FindSynsetParent(MEANING T,int n) //   presume we are at the master, next wordnet
{
    WORDP D = Meaning2Word(T);
    unsigned int index = Meaning2Index(T);
    FACT* F = GetSubjectHead(D); //   facts involving word 
    while (F)
    {
        FACT* at = F;
        F = GetSubjectNext(F);
        if (!(Meaning2Word(at->verb)->systemFlags & FACT_IS)) continue; //   not IS relationship
        
		//   prove indexes mate up
		unsigned int localIndex = Meaning2Index(at->subject); //   what fact says
        if (index != localIndex) continue; //   must match generic or specific precisely
		if (n-- <= 0) return at->object; //   next set/class in line
    }
    return 0;
}

MEANING FindSetParent(MEANING T,int n) //   next set parent
{
    WORDP D = Meaning2Word(T);
    unsigned int index = Meaning2Index(T);
    FACT* F = GetSubjectHead(D); //   facts involving word 
    while (F)
    {
        FACT* at = F;
		F = GetSubjectNext(F);
        if (!(at->properties & FACT_MEMBER)) continue; //   not set or class verb
        
		//   prove indexes mate up
		unsigned int localIndex = Meaning2Index(at->subject); //   what fact says
        if (index != localIndex) continue; //   must match generic or specific precisely

        if (--n == 0)  return at->object; //   next set/class in line
    }
    return 0;
}

static void DumpSetPath(MEANING T,unsigned int depth) // once you are IN a set, the path can be this
{
	int k = 0;
	while (++k)
	{
		MEANING parent = FindSetParent(T,k); //   next set we are member of
		if (!parent)  break;

        printf("    ");
		for (unsigned int j = 0; j < depth; ++j) printf("   "); 
        printf("%s \r\n",WriteMeaning(parent)); 
		DumpSetPath(parent,depth+1); // follow up depth first
	}
}

void DumpUpHierarchy(MEANING T,int depth) //   show what matches up
{
    if (!T) return;

    WORDP E = Meaning2Word(T);
	unsigned int index = Meaning2Index(T);
    if (depth == 0)  
	{
		WORDP F = E;
		if (F->properties & WORDNET_ID) F = Meaning2Word(E->meanings[1]);
		Log(STDUSERLOG,"\r\nFor %s:\r\n",F->word); 
		Log(STDUSERLOG,"  Set hierarchy:\r\n"); 

		DumpSetPath(T,depth);	
		if (E->word[0] == '~') return;	// we are done, it is not a word

		//   then do concepts based on this word...

		unsigned int size = E->meaningCount;
		if (!size) size = 1;	//   always at least 1, itself
		Log(STDUSERLOG,"  Wordnet hierarchy:\r\n"); 

		//   draw wordnet hierarchy based on each meaning
		for  (unsigned int k = 1; k <= size; ++k)
		{
			if (index && k != index) continue; //   not all, just correct meaning

			//   get meaningptr spot facts are stored (synset head)
			if (!index ) 
			{
				if (E->meaningCount) T = E->meanings[k]; //   ptr to synset head 
				else T = MakeMeaning(E);	//   a generic since we have no meanings
			}
			else T = E->meanings[k]; 
			WORDP D1 = Meaning2Word(T);
			Log(STDUSERLOG,"  ");
			Log(STDUSERLOG,"%s~%d:",E->word,k);
			if (D1->properties & NOUN) Log(STDUSERLOG,"N   ");
			else if (D1->properties & VERB) Log(STDUSERLOG,"V   ");
			else if (D1->properties & ADJECTIVE) Log(STDUSERLOG,"Adj ");
			else if (D1->properties & ADVERB) Log(STDUSERLOG,"Adv ");
			MEANING O = Meaning2Word(T)->meanings[1];
			if (Meaning2Word(T)->w.gloss) Log(STDUSERLOG," means %s",Meaning2Word(T)->w.gloss);
			Log(STDUSERLOG,"\r\n"); 
		
			DumpSetPath(T,depth);

			int l = 0;
			MEANING parent;
			while ((parent = FindSynsetParent(T,l++)))
			{
				//   walk wordnet hierarchy
				WORDP P = Meaning2Word(parent);
				Log(STDUSERLOG,"   ");
				for (int j = 0; j < depth; ++j) Log(STDUSERLOG,"   "); 
				Log(STDUSERLOG," is %s ",WriteMeaning(parent)); //   we show the immediate parent
				if (P->w.gloss) Log(STDUSERLOG,"  %s",P->w.gloss);
				Log(STDUSERLOG,"\r\n"); 

				DumpUpHierarchy(parent,depth+1); //   we find out what sets PARENT is in (will be none- bug)
			}
		}
	}
	else //    always synset nodes
	{
		int l = 0;
		MEANING parent ;
		while ((parent = FindSynsetParent(T,l++)))
		{
			//   walk wordnet hierarchy
			WORDP P = Meaning2Word(parent);
			Log(STDUSERLOG,"   ");
			for (int j = 0; j < depth; ++j) Log(STDUSERLOG,"   "); 
			Log(STDUSERLOG," is %s\r\n",WriteMeaning(P->meanings[1])); //   we show the immediate parent
			DumpSetPath(parent,depth);
			DumpUpHierarchy(parent,depth+1); //   we find out what sets PARENT is in (will be none- bug)
		}
	}
}

char* WriteMeaning(MEANING T,bool useGeneric)
{
	if (!T) return "";
	//   base word
    WORDP D = Meaning2Word(T);

	if (D->properties & WORDNET_ID && useGeneric && D->meaningCount ) // swap generic to specific instance
	{
		T = D->meanings[1];
		D =  Meaning2Word(T);
	}

	if ((T & MEANING_BASE) == T) return D->word;  //   fast out, no modifications needed

	//   need to annotate the value
    static char mybuffer[150];
	strcpy(mybuffer,D->word); 
	char* at = mybuffer + strlen(mybuffer);
   
	//   index 
	unsigned int index = Meaning2Index(T);
	if (index > 9) 
	{
		*at++ = '~';
		*at++ = (char)((index / 10) + '0');
		*at++ = (char)((index % 10) + '0');
	}
	else if (index)
	{
		*at++ = '~';
		*at++ = (char)(index + '0');
	}

	//   POS flag
	if ((T & TYPE_RESTRICTION) && !index) *at++ = '~';	//   pos marker
    if (T & NOUN) *at++ = 'n'; 
    if (T & VERB) *at++ = 'v'; 
    if (T & ADJECTIVE) *at++ = 'a';
    if (T & ADVERB) *at++ = 'b';
	if (T & PREPOSITION) *at++ = 'p';

	*at = 0;
    return mybuffer;
}

static void SpellOne(WORDP D, void* data)
{
	char* match = (char*) data;
	int set = *match++;
	int n = (int) (long long) factSet[set][0];
	if (n > 100) return; //   limit at 100

	if (!(D->properties & (NOUN | VERB | ADJECTIVE | ADVERB | DETERMINER | PRONOUN | CONJUNCTION_BITS | PREPOSITION | AUX_VERB ))) return;
	if (!IsAlpha(D->word[0])) return;

	if (MatchesPattern(D->word,match))
	{
		WORDP E = StoreWord("1");
		factSet[set][++n] = CreateFact(MakeMeaning(E,0),MakeMeaning(FindWord("word")),MakeMeaning(D,0));
		factSet[set][0] = (FACT*) n;
	}
}

unsigned int  Spell(char* match, unsigned int set)
{
	char pattern[MAX_WORD_SIZE];
	strcpy(pattern+1,match);
	match[0] = (char)set;
	WalkDictionary(SpellOne,match);
    return (int) (long long) factSet[set][0];
}

#ifdef COPYRIGHT

Per use of the WordNet dictionary data....

 This software and database is being provided to you, the LICENSEE, by  
  2 Princeton University under the following license.  By obtaining, using  
  3 and/or copying this software and database, you agree that you have  
  4 read, understood, and will comply with these terms and conditions.:  
  5   
  6 Permission to use, copy, modify and distribute this software and  
  7 database and its documentation for any purpose and without fee or  
  8 royalty is hereby granted, provided that you agree to comply with  
  9 the following copyright notice and statements, including the disclaimer,  
  10 and that the same appear on ALL copies of the software, database and  
  11 documentation, including modifications that you make for internal  
  12 use or for distribution.  
  13   
  14 WordNet 3.0 Copyright 2006 by Princeton University.  All rights reserved.  
  15   
  16 THIS SOFTWARE AND DATABASE IS PROVIDED "AS IS" AND PRINCETON  
  17 UNIVERSITY MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR  
  18 IMPLIED.  BY WAY OF EXAMPLE, BUT NOT LIMITATION, PRINCETON  
  19 UNIVERSITY MAKES NO REPRESENTATIONS OR WARRANTIES OF MERCHANT-  
  20 ABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE  
  21 OF THE LICENSED SOFTWARE, DATABASE OR DOCUMENTATION WILL NOT  
  22 INFRINGE ANY THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR  
  23 OTHER RIGHTS.  
  24   
  25 The name of Princeton University or Princeton may not be used in  
  26 advertising or publicity pertaining to distribution of the software  
  27 and/or database.  Title to copyright in this software, database and  
  28 any associated documentation shall at all times remain with  
  29 Princeton University and LICENSEE agrees to preserve same.  
#endif

typedef std::vector<char*>::const_iterator  citer;

bool myfunction (char* i,char* j) 
{ 
	if (*i == '~' && *j != '~') return true; // all concepts come first
	if (*i != '~' && *j == '~') return false;
	return stricmp(i,j) < 0; 
}

void Sortit(char* name,int oneline)   
{
	FILE* out = fopen("cset.txt","ab");
	WORDP D = FindWord(name,0);
	if (!D) return;
	
	int cat = FindTopicIDByName(name);
	char word[MAX_WORD_SIZE];
	strcpy(word,name);
	MakeUpperCase(word);

	std::vector<char*> mylist;
	FACT* F = GetObjectHead(D);
	while (F)
	{
        if (F->properties & FACT_MEMBER)
		{
			strcpy(word,WriteMeaning(F->subject));
			if (*word == '~') MakeUpperCase(word); // cap all concepts and topics
			WORDP E = StoreWord(word);
			mylist.push_back(E->word);
		}
		F = GetObjectNext(F);
	}
	std::sort(mylist.begin(), mylist.end(),myfunction); 
	bool drop = false;
	unsigned int len = strlen(name) + 11;
	int count = 0;
	for (citer it = mylist.begin(), end = mylist.end(); it < end; ++it )   
	{	  
		if (!drop)
		{
			if (D->systemFlags & TOPIC_NAME) fprintf(out,"topic: %s ",name);
			else fprintf(out,"concept: %s ",name);
			drop = true;
			if (cat)
			{
				int flags = GetTopicFlags(cat);
                if (flags & TOPIC_SYSTEM) fprintf(out,"system ");
                if (flags & TOPIC_RANDOM) fprintf(out,"random ");
                if (flags & TOPIC_NOERASE) fprintf(out,"noerase ");
                if (flags & TOPIC_NOSTAY) fprintf(out,"nostay ");
                if (flags & TOPIC_REPEAT) fprintf(out,"repeat ");
                if (flags & TOPIC_PRIORITY) fprintf(out,"priority ");
                if (flags & TOPIC_LOWPRIORITY) fprintf(out,"deprioritize ");
			}
			fprintf(out,"(");
		}
		if (++count == 20 && !oneline)
		{
			count = 0;
			fprintf(out,"\r\n");
			for (unsigned int j = 1; j <= len; ++j) fprintf(out," ");
		}
		fprintf(out,"%s ",*it );   
	}
	if  (drop) fprintf(out,")\r\n");
	fclose(out);
}

void SortTopic(WORDP D,void* junk)
{
	if (!(D->systemFlags & (BUILD0|BUILD1))) return; // ignore internal system topics (of which there are none)
	if (D->systemFlags & TOPIC_NAME) Sortit(D->word,(int)(long long)junk);
}

void SortTopic0(WORDP D,void* junk)
{
	if (!(D->systemFlags & (BUILD0|BUILD1))) return; // ignore internal system concepts
	if (!(D->systemFlags & TOPIC_NAME)) return;
	CreateFact(MakeMeaning(D),Mmember,MakeMeaning(StoreWord("~_dummy",AS_IS)));
}

void SortConcept(WORDP D,void* junk)
{
	if (!(D->systemFlags & (BUILD0|BUILD1|TOPIC_NAME))) return; // ignore internal system concepts and topics
	if (D->word[0] != '~') return;
	Sortit(D->word,(int)(long long)junk);
}

void SortConcept0(WORDP D,void* junk)
{
	if (!(D->systemFlags & (BUILD0|BUILD1|TOPIC_NAME))) return; // ignore internal system concepts and topics
	if (D->word[0] != '~') return;
	CreateFact(MakeMeaning(D),Mmember,MakeMeaning(StoreWord("~_dummy",AS_IS)));
}

static void ReadSubstitutes(char* file)
{
    char original[MAX_WORD_SIZE];
    char replacement[MAX_WORD_SIZE];
    FILE* in = fopen(file,"rb");
    if (!in) return;
    while (ReadALine(readBuffer,in) != 0) 
    {
        if (readBuffer[0] == '#' || readBuffer[0] == 0) continue;
        char* ptr = SkipWhitespace(readBuffer);	//   cant trust user formatting
		ptr = ReadCompiledWord(ptr,original); //   original phrase

        if (*original == 0 || *original == '#') continue;
		//   replacement can be multiple words joined by + and treated as a single entry.  
		ptr = SkipWhitespace(ptr);
		ptr = ReadCompiledWord(ptr,replacement+1);    //   replacement phrase
		
		WORDP D = FindWord(original,0,PRIMARY_CASE_ALLOWED);	//   do we know original already?
		if (D && D->systemFlags & SUBSTITUTE)
		{
			//ReportBug("Already have a substitute for %s",original);
			continue;
		}
		D = StoreWord(original,AS_IS); //   original word
		AddSystemFlag(D,SUBSTITUTE); 
 
		WORDP S = NULL;
		if (replacement[1] != 0 && replacement[1] != '#') 	//   with no substitute, it will just erase itself
		{
			*replacement = '|';	//   make it so it doesnt collide with any other entry, and leave _ alone
			S = StoreWord(replacement);  //   the valid word
			D->substitutes = S;

			// now determine the multiword headerness...
			WORDP E = D;
			unsigned int n = BurstWord(E->word);
			char wd[MAX_WORD_SIZE];
			strcpy(wd,JoinWords(1));
			char* word = wd;
			if (*word == '<') 
				++word;		// do not show the < starter for lookup
			unsigned int len = strlen(word);
			if (len > 1 && word[len-1] == '>') 
				word[len-1] = 0;	// do not show the > on the starter for lookup
			E = StoreWord(word);		//   create the 1-word header
			if (n > E->multiwordHeader) E->multiwordHeader = n;	//   mark it can go this far for an idiom

			//   note for the emotions (substitutes leading to a set like ~emoyes) we 
			//   want to be able to reverse access, so need to make them a member of the set. also
			if (S->word[0] == '~') CreateFact(MakeMeaning(D),MakeMeaning(FindWord("member")),MakeMeaning(S));
		}

        //   if original has hyphens, replace as single words also. Note burst form for initial marking will be the same
        bool hadHyphen = false;
        ptr = original;
        while (*++ptr) //   replace all alphabetic hypens using _
        {
            if (*ptr == '-' && IsAlpha(ptr[1])) 
            {
                *ptr = '_';
                hadHyphen = true;
            }
        }
        if (hadHyphen) 
        {
			D = FindWord(original);	//   do we know original already?
			if (D && D->systemFlags & SUBSTITUTE)
			{
				ReportBug("Already have a substitute for %s",original);
				continue;
			}
	
			D = StoreWord(original);
			AddSystemFlag(D,SUBSTITUTE);
 			D->substitutes = S;
        }
	}
    fclose(in);
}

void ReadWordsOf(char* file,uint64 mark,bool system)
{
    char word[MAX_WORD_SIZE];
    FILE* in = fopen(file,"rb");
    if (!in) return;
    while (ReadALine(readBuffer,in) != 0) 
    {
        ReadCompiledWord(SkipWhitespace(readBuffer),word); 
        if (*word == 0 || *word == '#') continue;

		WORDP D = StoreWord(word,0); 
		if (!system) AddProperty(D,mark);
		else AddSystemFlag(D,(unsigned int) mark);
	}
    fclose(in);
}

static void ReadCanonicals(char* file)
{
    char original[MAX_WORD_SIZE];
    char replacement[MAX_WORD_SIZE];
    FILE* in = fopen(file,"rb");
    if (!in) return;
    while (ReadALine(readBuffer,in) != 0) 
    {
        if (readBuffer[0] == '#' || readBuffer[0] == 0) continue;
		char* ptr = SkipWhitespace(readBuffer);
        ptr = ReadCompiledWord(ptr,original); //   original phrase
        if (*original == 0 || *original == '#') continue;
		ptr = SkipWhitespace(ptr);
        ptr = ReadCompiledWord(ptr,replacement);    //   replacement word
		WORDP D = FindWord(original);
		if (!D) continue;	// dont know this word
		canonicalMap[D->word] = AllocateString(replacement);
		D->systemFlags |= HAS_CANONICAL;
	}
    fclose(in);
}

static void ReadQueryLabels(char* file)
{
    char word[MAX_WORD_SIZE];
    FILE* in = fopen(file,"rb");
    if (!in) return;
    while (ReadALine(readBuffer,in) != 0) 
    {
        if (readBuffer[0] == '#' ||  readBuffer[0] == 0) continue;
        char* ptr = SkipWhitespace(readBuffer);
		ptr = ReadCompiledWord(ptr,word);    //   the name
        if (*word == 0) continue;
		ptr = SkipWhitespace(ptr); // in case excess blanks before control string
        WORDP D = StoreWord(word,0);
		char* at = strchr(ptr,' '); // in case has blanks after control string
		if (at) *at = 0;
 	    D->w.userValue = AllocateString(ptr);    
		AddSystemFlag(D,QUERY_KIND);
    }
    fclose(in);
}

static void Relocate(WORDP OLD, WORDP NEW)
{
	unsigned int oldindex = OLD - dictionaryBase;
	unsigned int newindex = NEW - dictionaryBase;
	memcpy(NEW,OLD,sizeof(WORDENTRY));
	memset(OLD,0,sizeof(WORDENTRY));
	WORDP D;
	WORDP limit = dictionaryBase + MAX_HASH_BUCKETS + HASH_EXTRA;
	for ( D = dictionaryBase+1; D < limit; ++D) 
	{
		if (D->nextNode == oldindex) D->nextNode = newindex; // alter bucket link

		if (D->conjugation == OLD) D->conjugation = NEW;
		if (D->plurality == OLD) D->plurality = NEW;
		if (D->comparison == OLD) D->comparison = NEW;
	
		unsigned int meanings = D->meaningCount;
		for ( unsigned int i = 1; i <= meanings; ++i)
		{
			MEANING M = D->meanings[i];
			WORDP F = Meaning2Word(M);
			if (F == OLD) 
			{
				unsigned int index = Meaning2Index(M);
				D->meanings[i] = MakeMeaning(NEW,index);
			}
		}
	}
}

static void FillInHash()
{
	WORDP D;
	WORDP limit = dictionaryBase + MAX_HASH_BUCKETS + HASH_EXTRA;
	unsigned int n = 0;
	for ( D = dictionaryBase+1; D < limit; ++D) 
	{
		if ( D->word) continue;
		WORDP E = --dictionaryFree;
		if ( E < limit) 
		{
			printf("ran out if items to move\r\n");
			break;
		}
		Relocate(E, D);
		if (( ++n % 1000) == 0) printf(".");
	}
}

static void ReadAsciiDictionary()
{
    char buffer[50];
	unsigned int n = 0;
	for (char i = '0'; i <= '9'; ++i)
	{
		sprintf(buffer,"DICT/%c.txt",i);
		if (!ReadDictionary(buffer))  ++n;
	}
	for (char i = 'a'; i <= 'z'; ++i)
	{
		sprintf(buffer,"DICT/%c.txt",i);
		if (!ReadDictionary(buffer))  ++n;
	}
	if (!ReadDictionary("DICT/other.txt"))  ++n; //   all weirds
	if (n) printf("Missing %d word files\r\n",n);


	// move items into holes in base hash area...
	//FillInHash();
}

void LoadDictionary()
{
	ndict = 0; //   count of how many words we read, should match how many allocated
	if (!ReadBinaryDictionary()) //   if binary form not there, use text form (slower)
	{
		ReadAsciiDictionary(); // has holes in the base... not  worth it...
		WriteBinaryDictionary(); //   store the faster read form of dictionary
	}
	else printf("Read %s Dictionary entries\r\n",StdNumber(ndict));
	InitFactWords();
	WalkDictionary(PostFactBinary,0);	// adjust facts for wordnet hierarchy
}

#define BUILDCONCEPT(name,word) {name = StoreWord(word); AddSystemFlag(name,CONCEPT);}

void ExtendDictionary()
{
	Dnever = StoreWord("never");
    Dnot = StoreWord("not");
	Dchatoutput = StoreWord("chatoutput");
	Dburst = StoreWord("^burst");
	Dtopic = StoreWord("^keywordtopics");
	Dyou = StoreWord("you");
	Dslash = StoreWord("/");

	BUILDCONCEPT(Dnumber,"~number"); 
 	BUILDCONCEPT(Dmoney,"~money"); 
    BUILDCONCEPT(Dplacenumber,"~placenumber"); 
	BUILDCONCEPT(Dpropername,"~propername"); 
	BUILDCONCEPT(Dspacename,"~spacename"); 
	BUILDCONCEPT(Dmalename,"~malename"); 
	BUILDCONCEPT(Dfemalename,"~femalename"); 
	BUILDCONCEPT(Dhumanname,"~humanname"); 
	BUILDCONCEPT(Dfirstname,"~firstname"); 
	BUILDCONCEPT(Dlocation,"~locationword"); 
	BUILDCONCEPT(Dtime,"~timeword"); 
	BUILDCONCEPT(Durl,"~url"); 
	BUILDCONCEPT(Dunknown,"~unknownword"); 
	BUILDCONCEPT(Dchild,"~childword");
	BUILDCONCEPT(Dadult,"~adultword");

	DefineSystemVariables();

    ReadSubstitutes("LIVEDATA/substitutes.txt");
    ReadCanonicals("LIVEDATA/canonical.txt");
	ReadQueryLabels("LIVEDATA/queries.txt");
	ReadWordsOf("LIVEDATA/lowercaseTitles.txt",LOWERCASE_TITLE,false);
}

void RemoveSynSet(MEANING T)
{
	WORDP D = Meaning2Word(T);
	MEANING master = GetMaster(T);
	if (!master) return; //   already removed
	WORDP M = Meaning2Word(master);
	if (M == D ) 
	{
		if (D->meaningCount) printf("Cannot remove, we are master meaning %s\r\n",D->word);
		else AddSystemFlag(D,DELETED_WORD);
		return;
	}

	FACT* F = FindFact(T,Mis,master);
	if (F) F->properties |= DEADFACT;
	RemoveMeaning(T,master);
}

char* FindCanonical(char* word, unsigned int i)
{
    WORDP D = FindWord(word,0,PRIMARY_CASE_ALLOWED);
	if (i == 1)
	{
		WORDP S = FindWord(word,0,SECONDARY_CASE_ALLOWED);
		if (S && IsLowerCase(S->word[0]))  D = S;
	}
    if (D) 
	{
		char* answer = GetCanonical(D);
		if (answer) return answer; //   special canonical form (various pronouns typically)
	}

    //    numbers - use digit form
    if (IsNumber(word))
    {
        char word1[MAX_WORD_SIZE];
        if (strchr(word,'.') || strlen(word) > 9)  //   big numbers need float
        {
            float y;
			if (word[0] == '$') y = (float)atof(word+1);
			else y = (float)atof(word);
            int x = (int)y;
            if (((float) x) == y) sprintf(word1,"%d",x); //   use int where you can
            else sprintf(word1,"%.2f",atof(word)); 
        }
		else if (word[0] == '$') sprintf(word1,"%d",Convert2Integer(word+1));
        else sprintf(word1,"%d",Convert2Integer(word)); //   integer
		uint64 flags = 0;
		if (IsPlaceNumber(word)) flags = (ADJECTIVE|ADJECTIVE_ORDINAL|NOUN_ORDINAL);
		else flags = ADJECTIVE|NOUN|ADJECTIVE_CARDINAL|NOUN_CARDINAL;
        WORDP N = StoreWord(word1,flags);
		return N->word;
    }
 
    //   VERB
    char* verb = GetInfinitive(word);
    if (verb) 
    {
        WORDP V = FindWord(verb,0);
        verb = (V) ? V->word : NULL;
    }
	if (verb) return verb; //   we prefer verb base-- gerunds are nouns and verbs for which we want the verb

    //   NOUN
    char* noun = GetSingularNoun(word);
    if (noun) 
    {
        WORDP  N = FindWord(noun,0);
        noun = (N) ? N->word : NULL;
    }
	if (noun) return noun;
    
	//   ADJECTIVE
    char* adjective = GetAdjectiveBase(word);
    if (adjective) 
    {
        WORDP A = FindWord(adjective,0);
        adjective = (A) ? A->word : NULL;
    }
	if (adjective) return adjective;
 
	//   ADVERB
    char* adverb = GetAdverbBase(word);
    if (adverb) 
    {
        WORDP A = FindWord(adverb,0);
        adverb = (A) ? A->word : NULL;
    }
	if (adverb) return adverb;

	if (D && D->properties & PART_OF_SPEECH) return D->word;

	return NULL;
}

bool IsHelper(char* word)
{
    WORDP D = FindWord(word,0);
    return (D && D->properties & (AUX_VERB|AUX_VERB_TENSES) );
}

bool IsFutureHelper(char* word)
{
	WORDP D = FindWord(word,0);
    return (D &&  D->properties & AUX_VERB_FUTURE);
}    
	
bool IsPresentHelper(char* word)
{
	WORDP D = FindWord(word,0);
	return (D && D->properties & AUX_VERB_PRESENT);
}

bool IsPastHelper(char* word)
{
	WORDP D = FindWord(word,0);
    return (D &&  D->properties & AUX_VERB_PAST);
}

static void SexWordnetOut(WORDP D,FILE* out)
{
	for (unsigned int i = 1; i <= D->meaningCount; ++i)
	{
		WORDP E = Meaning2Word(D->meanings[i]);
		fprintf(out,"%s\r\n",E->word);
	}

	FACT* F = GetObjectHead(D);
	while (F)
	{
		WORDP verb = Meaning2Word(F->verb);
		if (verb->systemFlags & FACT_IS) 
		{
			D = Meaning2Word(F->subject);
			SexWordnetOut(D,out);
		}
		F = GetObjectNext(F);
	}
}

static void SexOut(WORDP D, FILE* out)
{
	if (*D->word != '~' ) 
	{
		if (D->properties & WORDNET_ID) SexWordnetOut(D,out);
		else fprintf(out,"%s\r\n",D->word);
	}
	else
	{
		FACT* F = GetObjectHead(D);
		while (F)
		{
			if (F->properties & FACT_MEMBER) 
			{
				D = Meaning2Word(F->subject);
				SexOut(D,out);
			}
			F = GetObjectNext(F);
		}
	}
}

void DumpDictionaryEntry(char* word,unsigned int limit)
{
	char name[MAX_WORD_SIZE];
	strcpy(name,word);
	MEANING M = ReadMeaning(word,false,true);
	WORDP D = Meaning2Word(M);
	unsigned int index = Meaning2Index(M);
	if (limit == 0) limit = 5; // default
	Log(STDUSERLOG,"\r\n%s: ",name);
	uint64 properties = (D) ? D->properties : 0;
	uint64 bit = 0x8000000000000000LL;		//   starting bit
	while (properties)
	{
		if (properties & bit)
		{
			properties ^= bit;
			Log(STDUSERLOG,"%s ",FindNameByValue(bit));
		}
		bit >>= 1;
	}

	WORDP entry = NULL;
	WORDP canonical = NULL;
	uint64 flags = GetPosData(name,entry,canonical); 
	bit = 0x8000000000000000LL;
	bool extended = false;
	while (flags)
	{
		if (flags & bit)
		{
			flags ^= bit;
			if (!D || !(D->properties & bit)) // bits beyond what was directly known in dictionary
			{
				if (!extended) Log(STDUSERLOG," Implied: ");
				extended = true;
				Log(STDUSERLOG,"%s ",FindNameByValue(bit));
			}
		}
		bit >>= 1;
	}
	if (!D) D = entry;
	if (!D) 
	{
		if (canonical) Log(STDUSERLOG," cannonical: %s ",canonical->word);
		Log(STDUSERLOG,"\r\n    Not in the dictionary\r\n");
		return;
	}

	if (D->systemFlags & PREP_LOCATION) Log(STDUSERLOG,"prep_location ");
	if (D->systemFlags & PREP_TIME) Log(STDUSERLOG,"prep_time ");
	if (D->systemFlags & PREP_RELATION) Log(STDUSERLOG,"prep_relation ");
	if (D->systemFlags & PREP_INTRODUCTION) Log(STDUSERLOG,"prep_introduction ");
	if (D->systemFlags & ALLOW_INFINITIVE) Log(STDUSERLOG,"allow_infinitive ");
	if (D->systemFlags & EXISTENTIAL_BE) Log(STDUSERLOG,"existential_be ");
	if (D->systemFlags & ADJECTIVE_POSTPOSITIVE) Log(STDUSERLOG,"adjective_postpostiive ");
	if (D->systemFlags & PREDETERMINER_TARGET) Log(STDUSERLOG,"predeterminer_target ");
	if (D->systemFlags & OMITTABLE_TIME_PREPOSITION) Log(STDUSERLOG,"omittable_time_preposition ");
	if (D->systemFlags & VERBS_ACCEPTING_OF_AFTER) Log(STDUSERLOG,"verbs_accepting_of_after ");
	if (D->systemFlags & SYSTEM_MORE_MOST) Log(STDUSERLOG,"system_more_most ");
	if (D->systemFlags & ADJECTIVE_OBJECT) Log(STDUSERLOG,"adjective_object ");
	if (D->systemFlags & VERB_INDIRECTOBJECT) Log(STDUSERLOG,"verb_indirectobject ");
	if (D->systemFlags & VERB_INDIRECTOBJECT) Log(STDUSERLOG,"verb_directobject ");
	if (D->systemFlags & POTENTIAL_CLAUSE_STARTER) Log(STDUSERLOG,"potential_clause_starter ");
	if (D->systemFlags & NOUN_BEING) Log(STDUSERLOG,"noun_being ");

	if (D->systemFlags & UPPERCASE_HASH) Log(STDUSERLOG,"upperhash ");
	if (D->systemFlags & TIMEWORD) Log(STDUSERLOG,"timeword ");
	if (D->systemFlags & SPACEWORD) Log(STDUSERLOG,"spaceword ");
	if (D->systemFlags & PHRASAL_VERB) Log(STDUSERLOG,"splitphrasalverb ");
	if (D->systemFlags & OTHER_SINGULAR) Log(STDUSERLOG,"other_singular ");
	if (D->systemFlags & OTHER_PLURAL) Log(STDUSERLOG,"other_plural ");
	if (D->systemFlags & NOUN_MASS) Log(STDUSERLOG,"NOUN_MASS ");
	if (D->systemFlags & VERB_DIRECTOBJECT) Log(STDUSERLOG,"VERB_DIRECTOBJECT ");
	if (D->systemFlags & VERB_INDIRECTOBJECT) Log(STDUSERLOG,"VERB_INDIRECTOBJECT ");
	if (D->systemFlags & VERB_ADJECTIVE_OBJECT) Log(STDUSERLOG,"VERB_ADJECTIVE_OBJECT ");
	
	if (canonical) Log(STDUSERLOG," cannonical: %s ",canonical->word);
	if (D->systemFlags & (NOUN|VERB|ADJECTIVE|ADVERB|PREPOSITION))  Log(STDUSERLOG," POS-tiebreak: ");
	if (D->systemFlags & NOUN) Log(STDUSERLOG,"NOUN ");
	if (D->systemFlags & VERB) Log(STDUSERLOG," VERB ");
	if (D->systemFlags & ADJECTIVE) Log(STDUSERLOG,"ADJECTIVE ");
	if (D->systemFlags & ADVERB) Log(STDUSERLOG,"ADVERB ");
	if (D->systemFlags & PREPOSITION) Log(STDUSERLOG,"PREPOSITION ");
#ifndef NOTESTING
	basestamp = inferMark;
#endif
	NextinferMark();
#ifndef NOTESTING
	if (D->properties & WORDNET_ID) Log(STDUSERLOG,"synset (%d members) ",CountDown(M,0,0));
	if (D->word[0] == '~' && !(D->systemFlags & TOPIC_NAME)) Log(STDUSERLOG,"concept (%d members) ",CountSet(D));
#endif
	if (D->systemFlags & SUBSTITUTE) 
	{
		Log(STDUSERLOG,"substitute=");
		if (D->substitutes) Log(STDUSERLOG,"%s ",D->substitutes->word+1); // skip internal | on name
		else Log(STDUSERLOG,"  ");
	}
	if (D->systemFlags & TOPIC_NAME) Log(STDUSERLOG,"topic ");
	if (D->systemFlags & QUERY_KIND) Log(STDUSERLOG,"query ");

	if (D->systemFlags & PATTERN_WORD) Log(STDUSERLOG,"pattern ");
	if (D->systemFlags & FUNCTION_NAME) 
	{
		char* kind = (D->systemFlags & IS_PATTERN_MACRO) ? (char*)"pattern" : (char*)"output";
		if (!D->x.codeIndex) Log(STDUSERLOG,"user %s function %s ",kind,D->w.fndefinition);
		else Log(STDUSERLOG,"systemfunction %d", D->x.codeIndex);
	}
	if (D->word[0] == '%') Log(STDUSERLOG,"systemvar ");
	if (D->systemFlags & BUILD0) Log(STDUSERLOG,"build0 ");
	if (D->systemFlags & BUILD1) Log(STDUSERLOG,"build1 ");
	if (D->word[0] == '$')
	{
		char* val = GetUserVariable(D->word);
		Log(STDUSERLOG,"VariableValue= \"%s\" ",val);
	}
	Log(STDUSERLOG,"\r\n");

	if (D->conjugation) 
	{
		Log(STDUSERLOG,"  conjugationLoop= ");
		WORDP E = D->conjugation;
		while (E != D)
		{
			Log(STDUSERLOG,"-> %s ",E->word);
			E = E->conjugation;
		}
		Log(STDUSERLOG,"\r\n");
	}
	if (D->plurality) 
	{
		Log(STDUSERLOG,"  pluralLoop= ");
		WORDP E = D->plurality;
		while (E != D)
		{
			Log(STDUSERLOG,"-> %s ",E->word);
			E = E->plurality;
		}
		Log(STDUSERLOG,"\r\n");
	}
	if (D->comparison) 
	{
		Log(STDUSERLOG,"  comparativeLoop= ");
		WORDP E = D->comparison;
		while (E != D)
		{
			Log(STDUSERLOG,"-> %s ",E->word);
			E = E->comparison;
		}
		Log(STDUSERLOG,"\r\n");
	}
		
	char* gloss = (D->w.gloss) ? D->w.gloss : NULL;
	if (gloss)  Log(STDUSERLOG,"gloss=%s\r\n",gloss); //   synset headers have actual gloss
	else Log(STDUSERLOG,"\r\n");

	//   now dump the meanings
	unsigned int count = D->meaningCount;
	if (D->properties & WORDNET_ID) count = 0;
	for (unsigned int i = 1; i <= count; ++i)
	{
		MEANING M = D->meanings[i];
		Log(STDUSERLOG," %d: %s ", i,WriteMeaning(M));
		WORDP R = Meaning2Word(M);
		if (R->w.gloss) 
		{
			if (R->properties & NOUN) Log(STDUSERLOG,"Noun: ");
			if (R->properties & VERB) Log(STDUSERLOG,"Verb: ");
			if (R->properties & ADJECTIVE) Log(STDUSERLOG,"Adjective: ");
			if (R->properties & ADVERB) Log(STDUSERLOG,"Adverb: ");
			Log(STDUSERLOG,"%s\r\n",R->w.gloss);
		}
		else Log(STDUSERLOG,"\r\n");
	}

	// now show all synonyms in those meanings
	Log(STDUSERLOG,"Synonyms:\r\n");
	count = D->meaningCount;
	for (unsigned int i = 1; i <= count; ++i)
	{
		MEANING M = D->meanings[i];
		Log(STDUSERLOG,"  %d:",i); //   header for this list
		WORDP D = Meaning2Word(M);
		if (D->properties & WORDNET_ID) 
		{
			for (unsigned int j  = 1; j <= D->meaningCount; ++j) 
			{
				WORDP F = Meaning2Word(D->meanings[j]);
				Log(STDUSERLOG," %s ",WriteMeaning(D->meanings[j])); 
			}
			Log(STDUSERLOG,"\r\n");
		}
		else Log(STDUSERLOG," %s ",WriteMeaning(M)); 
    }
	if (D->properties & WORDNET_ID)  Log(STDUSERLOG,"\r\n");
	if (D->v.patternStamp) Log(STDUSERLOG,"Pstamp- %d\r\n",D->v.patternStamp);
	if (D->stamp.inferMark) Log(STDUSERLOG,"Istamp- %d\r\n",D->stamp.inferMark);
	if (D->multiwordHeader) Log(STDUSERLOG,"Header- %d\r\n",D->multiwordHeader);

	// show concept/topic members
	FACT* F = GetSubjectHead(D);
	bool did = false;
	Log(STDUSERLOG,"Direct Sets: ");
	while (F)
	{
		if (F->properties & FACT_MEMBER)
		{
			did = true;
			Log(STDUSERLOG,"%s ",Meaning2Word(F->object)->word);
		}
		F = GetSubjectNext(F);
	}
	Log(STDUSERLOG,"\r\n");

	char* buffer = AllocateBuffer();
	Log(STDUSERLOG,"Facts:\r\n");

	F = GetSubjectHead(D);
	count = 0;
	while (F)
	{
		if (!(F->properties & (FACT_IS|FACT_MEMBER)))
		{
			buffer = WriteFact(F,false,buffer);
			Log(STDUSERLOG,"  %s\r\n",buffer);
			if (++count >= limit) break;
		}
		F = GetSubjectNext(F);
	}

	F = GetVerbHead(D);
	count = limit;
	while (F)
	{
		if (!(F->properties & (FACT_IS|FACT_MEMBER)))
		{
			buffer = WriteFact(F,false,buffer);
			Log(STDUSERLOG,"  %s\r\n",buffer);
			if (++count >= limit) break;
		}
		F = GetVerbNext(F);
	}
	
	F = GetObjectHead(D);
	count = 0;
	while (F)
	{
		if (!(F->properties & (FACT_IS|FACT_MEMBER)))
		{
			buffer = WriteFact(F,false,buffer);
			Log(STDUSERLOG,"  %s\r\n",buffer);
			if (++count >= limit) break;
		}
		F = GetObjectNext(F);
	}

	FreeBuffer();
}

void WriteDictionaryChange(FILE* factout, unsigned int zone)
{
	FILE* in;
	if ( zone == BUILD0) in = fopen("TMP/build0pre","rb");
	else in = fopen("TMP/build1pre","rb");
	if (!in) ReportBug("Missing zone base file");

	for (WORDP D = dictionaryBase+1; D < dictionaryFree; ++D) 
	{
		uint64 oldproperties = 0;
		uint64 oldflags = 0;
		if ( (zone == BUILD0 && D < dictionaryPreTopics ) || (zone == BUILD1 && D < dictionaryPreBuild1))
		{
			char junk[3];
			if (fread(&oldproperties,1,8,in) != 8)
			{
				printf("out of dictionary change data?\r\n");
			}
			if (fread(&oldflags,1,8,in)  != 8) // system 
			{
				printf("out of dictionary change data1?\r\n");
			}
			if (fread(junk,1,1,in)   != 1) // multiword header 
			{
				printf("out of dictionary change data2?\r\n");
			}
		}
		if (!D->word) continue;	// an empty bucket
		if (D->systemFlags & NO_EXTENDED_WRITE_FLAGS) continue; // ignre pattern words, etc
		// only write out changes in flags and properties
		uint64 prop = D->properties;
		uint64 flags = D->systemFlags;
		D->properties &= -1 ^ oldproperties; // remove the old properties
		D->systemFlags &= -1 ^ oldflags; // remove the old flags
		if (D->word[0] == '~'); // ignore any jump label pairs
		else if (D->properties || D->systemFlags)  
		{
			fprintf(factout,"+ %s ",D->word);
			WriteDictionaryFlags(D,factout); // write the new
			fprintf(factout,"\r\n");
		}
		D->properties = prop;
		D->systemFlags = flags;
	}
	fclose(in);
}
