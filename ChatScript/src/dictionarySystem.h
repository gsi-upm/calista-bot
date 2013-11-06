#ifndef _DICTIONARYSYSTEMH_
#define _DICTIONARYSYSTEMH_

#ifdef INFORMATION
Copyright (C) 2011 by Bruce Wilcox

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#endif


typedef unsigned int DICTINDEX;	//   indexed ref to a dictionary entry

//   This file has formatting restrictions because it is machine read by AcquireDefines. All macro and function calls
//   must keep their ( attached to the name and all number defines must not.
//   All tokens of a define must be separated by spaces ( TEST|TEST1  is not legal).

//   These are the properties of a dictionary entry.
//   IF YOU CHANGE THE VALUES or FIELDS of a dictionary node,
//   You need to rework ReadDictionary/WriteDictionary/ReadBinaryEntry/WriteBinaryEntry

// D->properties bits

//   BASIC_POS POS bits like these must be on the bottom of any 64-bit word, so they match MEANING values (at top) as well.
#define NOUN					0x0000000080000000ULL
#define VERB					0x0000000040000000ULL
#define ADJECTIVE				0x0000000020000000ULL	// PENNBANK: JJ when not JJR or JJS
#define ADVERB					0x0000000010000000ULL   // PENNBANK: RB when not RBR or RBS
#define BASIC_POS				( NOUN | VERB | ADJECTIVE | ADVERB )
//   the above four should be first, reflecting the Wordnet files (leaving room for wordnet offset)
#define PREPOSITION				0x0000000008000000ULL  // Pennbank: IN
#define ESSENTIAL_FLAGS			( BASIC_POS | PREPOSITION ) //  these can be type restrictions on MEANINGs
#define DETERMINER				0x0000000004000000ULL //   Articles: a and the --  demonstratives: this that these those --  Quantifiers: much many some a_little  -- Possessive pronouns: my your
// FLAGS above are shared onto systemflags for most likely pos-tag candidate type

#define PRONOUN					0x0000000002000000ULL
#define CONJUNCTION_COORDINATE	0x0000000001000000ULL // Pennbank: CC (coordinating)  IN (subordinating)  may also be interjection
#define CONJUNCTION_SUBORDINATE	0x0000000000800000ULL 

#define COMMA					0x0000000000400000ULL 

#define POSSESSIVE				0x0000000000200000ULL // is a possessive like 's or Taiwanese (but not possessive pronoun)

#define ADJECTIVE_BASIC			0x0000000000100000ULL	// "friendly"
#define ADJECTIVE_MORE			0x0000000000080000ULL	// "friendlier"
#define ADJECTIVE_MOST			0X0000000000040000ULL	// "friendliest"
#define ADJECTIVE_PARTICIPLE	0x0000000000020000ULL	// the "walking" dead  or  the "walked" dog
#define ADJECTIVE_CARDINAL		0x0000000000010000ULL	// "20" horses
#define ADJECTIVE_ORDINAL		0x0000000000008000ULL	// "to" fly

#define ADVERB_BASIC			0x0000000000004000ULL	
#define ADVERB_MORE				0x0000000000002000ULL	 
#define ADVERB_MOST				0x0000000000001000ULL	

#define THERE_EXISTENTIAL		0x0000000000000800ULL  // "There" is no future in it.

#define PRONOUN_SUBJECT			0x0000000000000400ULL	// I you we
#define PRONOUN_POSSESSIVE		0x0000000000000200ULL	// my your his
#define PRONOUN_OBJECT			0x0000000000000100ULL	// me, you, him, her, etc

#define NOUN_SINGULAR			0x0000000000000080ULL  // Pennbank: NN
#define NOUN_PLURAL				0x0000000000000040ULL  // Pennbank: NNS
#define NOUN_PROPER_SINGULAR	0x0000000000000020ULL  //   A proper noun that is NOT a noun is a TITLE like Mr.
#define PAREN					0x0000000000000010ULL  // ( or )
#define NOUN_GERUND				0x0000000000000008ULL	// "Walking" is fun
#define NOUN_CARDINAL			0x0000000000000004ULL  // I followed the "20".
#define NOUN_PROPER_PLURAL		0x0000000000000002ULL	

#define WH_ADVERB				0x0000000000000001ULL	// adverbs- some can introduce an interrogative sentence "who is there" and others can introduce clauses "I've no idea how it works"


//////////////////////////////bits below here can not be used in values[] of tagger  
#define AS_IS					0x8000000000000000ULL   //  TRANSIENT INTERNAL dont try to reformat the word (transient flag passed to StoreWord)
#define NOUN_HUMAN				0x4000000000000000ULL  //   person or group of people that uses WHO, he, she, anyone
#define NOUN_FIRSTNAME			0x2000000000000000ULL  //   a known first name -- wiULL also be a sexed name probably
#define NOUN_SHE				0x1000000000000000ULL	//   female sexed word (used in sexed person title detection for example)
#define NOUN_HE					0x0800000000000000ULL	//   male sexed word (used in sexed person title detection for example)
#define NOUN_THEY				0x0400000000000000ULL   
#define NOUN_TITLE_OF_ADDRESS	0x0200000000000000LL	//   eg. mr, miss
#define NOUN_TITLE_OF_WORK		0x0100000000000000ULL
#define LOWERCASE_TITLE			0X0080000000000000ULL		//   lower case word may be in a title (but not a noun)
//   0x0040000000000000ULL 
#define WORDNET_ID				0x0020000000000000ULL	//   a wordnet synset header node (MASTER w gloss )
#define QWORD					0x0010000000000000ULL	// who what where why when how whose
#define NOUN_ROLE				0x0008000000000000ULL	//  human role (like doctor or pilot or ally)
//		0x0004000000000000ULL
//		0x0002000000000000ULL	
#define PUNCTUATION				0x0001000000000000ULL	
//////////////////////////////bits below here are used in values[] of tagger  0x0000 are used in tagger internals...

#define PARTICLE				0x0000800000000000ULL	//   multiword separable verb (full verb marked w systemflag PHRASAL_VERB
#define TO_INFINITIVE	 		0x0000400000000000ULL	// 20th horse
#define VERB_PRESENT			0x0000200000000000ULL	// present plural (usually infinitive)
#define VERB_PRESENT_3PS		0x0000100000000000ULL	// 3rd person singular singular
#define VERB_PRESENT_PARTICIPLE 0x0000080000000000ULL	// GERUND,  Pennbank: VBG
#define VERB_PAST				0x0000040000000000ULL	// Pennbank: VBD
#define VERB_PAST_PARTICIPLE    0x0000020000000000ULL	// Pennbank VBN
#define VERB_INFINITIVE			0x0000010000000000ULL	//   all tense forms are linked into a circular ring
#define NOUN_INFINITIVE			0x0000008000000000ULL	
//					0x0000004000000000ULL	// prep phrase
#define	PREDETERMINER			0x0000002000000000ULL	

#define AUX_VERB				0x0000001000000000ULL	//   auxilliary verb Pennbank MD
#define AUX_VERB_PRESENT		0x0000000800000000ULL
#define AUX_VERB_FUTURE			0x0000000400000000ULL
#define AUX_VERB_PAST			0x0000000200000000ULL
#define AUX_VERB_TENSES ( AUX_VERB_PRESENT | AUX_VERB_FUTURE | AUX_VERB_PAST )

#define NOUN_ORDINAL 			0x0000000100000000ULL

#define CONJUNCTION_BITS	 ( CONJUNCTION_COORDINATE | CONJUNCTION_SUBORDINATE )
#define DETERMINER_BITS		( DETERMINER | PREDETERMINER )
#define PART_OF_SPEECH			( AUX_VERB | ESSENTIAL_FLAGS | DETERMINER | PREDETERMINER | PRONOUN | CONJUNCTION_COORDINATE | CONJUNCTION_SUBORDINATE  ) 

#define POSSESSIVE_BITS ( PRONOUN_POSSESSIVE | POSSESSIVE )
#define ADVERB_BITS	 ( ADVERB_BASIC | ADVERB_MORE | ADVERB_MOST | WH_ADVERB )
#define NOUN_TITLE		 (  NOUN | NOUN_TITLE_OF_WORK | NOUN_PROPER_SINGULAR )
#define NOUN_HUMANGROUP		( NOUN | NOUN_HUMAN | NOUN_TITLE_OF_WORK | NOUN_THEY )
#define NOUN_MALEHUMAN     ( NOUN | NOUN_HUMAN | NOUN_HE | NOUN_PROPER_SINGULAR )
#define NOUN_FEMALEHUMAN    ( NOUN |  NOUN_HUMAN | NOUN_SHE | NOUN_PROPER_SINGULAR )
#define NOUN_HUMANNAME ( NOUN_HUMAN | NOUN | NOUN_PROPER_SINGULAR )
#define NOUN_PROPERTIES ( NOUN_HE | NOUN_THEY | NOUN_SINGULAR | NOUN_PLURAL | NOUN_PROPER_SINGULAR | NOUN_PROPER_PLURAL | NOUN_HUMAN | NOUN_FIRSTNAME | NOUN_SHE | NOUN_TITLE_OF_ADDRESS | NOUN_TITLE_OF_WORK  )
#define PRONOUN_BITS	( PRONOUN_OBJECT | PRONOUN_SUBJECT )
#define NUMBER_BITS ( NOUN_CARDINAL | NOUN_ORDINAL | ADJECTIVE_CARDINAL | ADJECTIVE_ORDINAL )
#define VERB_TENSES (  VERB_INFINITIVE |VERB_PRESENT | VERB_PRESENT_3PS | VERB_PAST | VERB_PAST_PARTICIPLE | VERB_PRESENT_PARTICIPLE  )
#define VERB_PROPERTIES (  VERB_TENSES )
#define ADJECTIVE_BITS ( ADJECTIVE_BASIC | ADJECTIVE_MORE | ADJECTIVE_MOST | ADJECTIVE_PARTICIPLE | ADJECTIVE_CARDINAL | ADJECTIVE_ORDINAL )
#define NOUN_BITS ( NOUN_SINGULAR | NOUN_PLURAL | NOUN_PROPER_SINGULAR  | NOUN_PROPER_PLURAL | NOUN_GERUND | NOUN_CARDINAL | NOUN_ORDINAL )
#define TAG_TEST ( COMMA | PAREN | PARTICLE | VERB_TENSES | NOUN_BITS | NOUN_INFINITIVE | PREDETERMINER | ADJECTIVE_BITS | AUX_VERB_TENSES | ADVERB_BITS | PRONOUN_POSSESSIVE | PRONOUN_BITS | CONJUNCTION_BITS | POSSESSIVE | THERE_EXISTENTIAL | PREPOSITION | TO_INFINITIVE | COMMA | DETERMINER  )
#define NOUN_DESCRIPTION_BITS ( ADJECTIVE_BITS | DETERMINER  | PREDETERMINER  | ADVERB_BITS  | PRONOUN_POSSESSIVE  | POSSESSIVE | NOUN_BITS )

// SYSTEMFLAGS
#define PATTERN_WORD		0x0000000000000001ULL	//   this word is known to the topic system as a keyword (may or may not be a real word)
#define FUNCTION_NAME		0x0000000000000002ULL	//   name of a ^function  (has non-zero ->x.codeIndex if system, else is user)
#define DELETED_WORD		0x0000000000000004ULL	//   want to not use this word
#define CONCEPT				0x0000000000000008ULL	// topic or concept has been read via a definition
#define IS_PATTERN_MACRO	0x0000000000000010ULL	//   pattern vs output use
#define BUILD0				0x0000000000000020ULL// comes from build0 data (marker on functions, concepts, topics)
#define BUILD1				0x0000000000000040ULL	// comes from build1 data
#define QUERY_KIND			0x0000000000000080ULL	// is a query item
#define FACT_IS				0x0000000000000100ULL  //   is the verb is -- MATCH IN FACT by being unused
#define FACT_MEMBER			0x0000000000000200ULL  //   is the verb member  -- MATCH IN FACT by being unused
#define PATTERN_ALLOWED		0x0000000000000400ULL	// dont warn about spelling during script compile
#define SUBSTITUTE			0x0000000000000800ULL  //   word has substitute attached 
#define TOPIC_NAME			0x0000000000001000ULL	//   this is a ~xxxx topic name in the system
#define GRADE5_6            0X0000000000002000ULL //   age 10-11
#define GRADE3_4            0X0000000000004000ULL //   age 8-9
#define GRADE1_2            0x0000000000008000ULL //   age 6-7
#define KINDERGARTEN        0x0000000000010000ULL //   age 5
#define TIMEWORD			0x0000000000020000ULL
#define SPACEWORD			0x0000000000040000ULL
#define PHRASAL_VERB		0x0000000000080000ULL
#define UPPERCASE_HASH		0x0000000000100000ULL //   uses an upper case bucket  --- on MAIN WORDS
#define HAS_CANONICAL		0x0000000000200000ULL
#define VAR_CHANGED			0x0000000000400000ULL
#define IN_SENTENCE			0x0000000000800000ULL
#define OTHER_SINGULAR		0x0000000001000000ULL // for pronouns, etc
#define OTHER_PLURAL		0x0000000002000000ULL
#define NOUN_MASS			0x0000000004000000ULL

#define NO_EXTENDED_WRITE_FLAGS ( PATTERN_WORD | FUNCTION_NAME | DELETED_WORD | CONCEPT | IS_PATTERN_MACRO |  BUILD0 | BUILD1 | QUERY_KIND | PATTERN_ALLOWED | TOPIC_NAME | SUBSTITUTE )

// most likely tie-break pos-type HIGH PRIORITY
//PREPOSITION				0x0000000008000000ULL
//ADVERB					0x0000000010000000ULL
//ADJECTIVE					0x0000000020000000ULL
//VERB						0x0000000040000000ULL
//NOUN						0x0000000080000000ULL

#define VERB_CONJUGATE1				0x8008000000000000ULL	//   1st word of composite verb conjugates  (e.g. peter_out)
#define VERB_CONJUGATE2				0x4000000000000000ULL	//   2nd word of composite verb conjugates  (e.g., re-energize)
#define VERB_CONJUGATE3				0x2000000000000000ULL	//	3rd word of composite verb conjugates	

#define PREP_LOCATION				0x1000000000000000ULL
#define PREP_TIME					0x0800000000000000ULL
#define PREP_RELATION				0x0400000000000000ULL
#define PREP_INTRODUCTION			0x0200000000000000ULL

#define ALLOW_INFINITIVE			0x0000800000000000ULL	// supports infinitive must not use top byte else hasproperty messes up
#define EXISTENTIAL_BE				0x0000400000000000ULL	// "there is or there seem"
#define ADJECTIVE_OBJECT			0x0000200000000000ULL	// can take adjective as object
#define ADJECTIVE_POSTPOSITIVE		0x0000100000000000ULL	// adjective can occur AFTER the noun
#define PREDETERMINER_TARGET		0x0000080000000000ULL	// predeterminer can come before these (a an the)
#define OMITTABLE_TIME_PREPOSITION	0x0000040000000000ULL // can be used w/o preposition
#define VERBS_ACCEPTING_OF_AFTER	0x0000020000000000ULL // of can follow this verb
#define SYSTEM_MORE_MOST			0x0000010000000000ULL	// more most, less least

#define VERB_DIRECTOBJECT			0x0000008000000000ULL	// verb can take an object (no verb accepting an object REFUSED no object)
#define VERB_INDIRECTOBJECT			0x0000004000000000ULL	// verb can take indirect object  
#define POTENTIAL_CLAUSE_STARTER	0x0000002000000000ULL	// relative pronoun or adjective clause marker
#define VERB_ADJECTIVE_OBJECT		0x0000001000000000ULL
#define NOUN_BEING					0x0000000800000000ULL
//NOUN								0x0000000080000000ULL

// adult is all the rest
#define AGE_LEARNED ( KINDERGARTEN | GRADE1_2 | GRADE3_4 | GRADE5_6 )

#define MAX_DICTIONARY	 0x000fffff  //   1M word vocabulary limit (doubling this FAILS on amazon server)

#ifdef SMALL
#define MAX_HASH_BUCKETS 0x08000  // = 32K
#define MAX_ENTRIES      420000 
#elif  WIN32
#define MAX_HASH_BUCKETS 0x20000 
#define MAX_ENTRIES      0x000fffff 
#else
#define MAX_HASH_BUCKETS 0x20000 
#define MAX_ENTRIES      420000 
#endif


#define HASH_EXTRA		2			// +1 for being 1-based and +1 for having uppercase bump

///   DEFINITION OF A MEANING 
typedef unsigned int MEANING; //   a flagged indexed dict ptr
#define INDEX_BITS          0x07F00000  //   7 bits of ontology indexing ability  63 possible meanings allowed
#define INDEX_OFFSET        20          //   shift for ontoindex  (rang 0..63)                
#define TYPE_RESTRICTION	ESSENTIAL_FLAGS
#define MEANING_BASE		0x000fffff	//   the index of the dictionary item


// flags on facts

// USER FLAGS: 0xffffffff0000000ULL

// transient flags
#define MARKED_FACT         0x0000000000008000  //   TRANSIENT : used during inferencing sometimes to see if fact is marked
#define MARKED_FACT2        0x0000000000004000  //   TRANSIENT: used during inferencing sometimes to see if 2ndary fact is marked
#define DEADFACT			0x0000000000002000   //   has been killed off
#define TRANSIENTFACT       0x0000000000001000  //   save only with a set, not with a user or base syste,

//#define FACT_MEMBER		0x0000000000000200ULL  //   is the verb member  -- MATCH IN FACT by being unused
//#define FACT_IS			0x0000000000000100ULL  //   is the verb is -- MATCH IN FACT by being unused

#define FACTSUBJECT         0x0000000000000080  //   index is - relative number to fact 
#define FACTVERB			0x0000000080000040	//   is 1st in its bucket (transient flag for read/WriteBinary) which MIRRORS DICT BUCKETHEADER flag: 
#define FACTOBJECT		    0x0000000000000020  //   does not apply to canonical forms of words, only the original form - for classes and sets, means dont chase the set

#define ORIGINAL_ONLY       0x0000000000000002  //  dont match on canonicals

//   values of TokenMarkers (seen processing input) BAD some unused now
#define QUESTIONMARK 1         
#define EXCLAMATIONMARK 2     
#define SUPPOSEQUESTIONBIT 4   
#define PAST 8
#define FUTURE 16
#define INSULTBIT 32
#define NOTFLAG 64
#define USERINPUT 256

//   codes for BurstWord argument
#define SIMPLE 0
#define STDBURST 0		// normal burst behavior
#define POSSESSIVES 1
#define CONTRACTIONS 2
#define HYPHENS 4
#define COMPILEDBURST 8  // prepare argument as though it were output script		
#define NOBURST 16		// dont burst (like for a quoted text string)

//   values for FindWord lookup
#define PRIMARY_CASE_ALLOWED 1
#define SECONDARY_CASE_ALLOWED 2
#define STANDARD_LOOKUP (PRIMARY_CASE_ALLOWED |  SECONDARY_CASE_ALLOWED )
#define LOWERCASE_LOOKUP 4
#define UPPERCASE_LOOKUP 8

// control over tokenization
#define DO_SUBSTITUTES 1
#define DO_NUMBER_MERGE 2
#define DO_PROPERNAME_MERGE 4
#define DO_SPELLCHECK 8
#define DO_UTF8_CONVERT 16
#define DO_INTERJECTION_SPLITTING 32
#define DO_POSTAG 64
#define DO_PARSE 128


// control over topics
#define TOPIC_NOERASE 1     //   do not erase as you go
#define TOPIC_REPEAT 2      //   allow repeat output


// pos tagger result operators
#define DISCARD 1
#define KEEP 2
#define TRACE 8

// pos tagger pattern values   6 bits (0-63) + 2 flags
#define HAS 1 // any bit matching 
#define IS 2	// is exactly this
#define INCLUDE 3 // has bit AND has other bits
#define ISWORD 4 // is this word
#define POSITION 5 // sentence boundary
#define START POSITION
#define END POSITION
#define ISCANONICAL 6 // canonical word is this
#define ISMEMBER 7	// is member of this set
#define PARTICLEVERB 8	// verb before this forms a phrasal verb
#define ISADJACENT 9	// before or after is this
#define ISCLAUSE 10	//  has noun and verb freestanding
#define HASPROPERTY 11
#define ISROLE	12	
#define ENDSWITH 13			// suffix it ends in
#define LACKSROLE 14		// works when tagging has been tried
#define HASCANONICALPROPERTY 15
#define LASTCONTROL HASCANONICALPROPERTY
	
#define SKIP 1 // if it matches, move ptr along, if it doesnt DONT
#define STAY 2
#define NOTCONTROL 4 

// MARKER values
#define BE_OBJECT2 1	// noun is omitted prep phrase

// pos tagger roles and states
#define MAINSUBJECT		1			// 0x0001			// noun roles like Mainsubject etc are ordered lowest first for pronoun IT priority
#define MAINVERB	2				// 0x0002
#define MAINOBJECT		4			// 0x0004
#define MAININDIRECTOBJECT		8	// 0x0008
#define SUBJECT2	16				// 0x0010
#define VERB2		32				// 0x0020
#define OBJECT2		64				// 0x0040
#define INDIRECTOBJECT2		128		// 0x0080
#define CLAUSE 256					// 0x0100
#define PREPPHRASE 512				// 0x0200
#define INFINITIVE 1024				// 0x0400
#define CONJUNCT_WORD 2048			// 0x0800
#define CONJUNCT_PHRASE 4096		// 0x1000
#define CONJUNCT_CLAUSE 8192		// 0x2000
#define CONJUNCT_SENTENCE 16384		// 0x4000
#define COMMA_PHRASE	32768		// 0x8000
#define APPOSITIVE		65536		// 0x10000 (can lead or follow)
#define OBJECT_COMPLEMENT	0x20000	// can be noun, or adjective after object...
#define ADJECTIVE_AS_OBJECT	0x40000
#define NOT					0x80000

#define KINDS_OF_PHRASES ( CLAUSE | PREPPHRASE | INFINITIVE )

struct WORDENTRY;
typedef WORDENTRY* WORDP;

typedef void (*DICTIONARY_FUNCTION)(WORDP D, void* data);

struct FACT;

typedef unsigned int FACTOID; //   a fact index

typedef struct WORDENTRY //   a dictionary entry  - starred items are written to the dictionary
{
	//   if you edit this, you may need to change ReadBinaryEntry and WriteBinaryEntry

	//   Basic Node Information
	FACTOID subjectHead;		//   start threads for facts run thru here (parallels FACT ones)
	FACTOID verbHead;			//   start threads for facts run thru here (parallels FACT ones)
	FACTOID  objectHead;		//   start threads for facts run thru here (parallels FACT ones)

	//   related members of this word (circular lists) 
	WORDP			conjugation;	//   conjugation list for irregular verbs -- (padding matchs FACT)
	WORDP			plurality;		//   plurality list for irregular nouns	-- (padding matchs FACT)
	WORDP			comparison;		//   comparison list for irregular adjective/adverb -- (padding matchs FACT)
	
	uint64  properties;				//   description of this node -- (parallels FACT ones)
	// above mirrors FACT definition

	uint64	hash;					//   we presume 2 hashs never collide, so we dont check the string for matching
	uint64  tried;					//   bits of which meaning the inferMark applies to
	uint64 systemFlags;	//   non dictionary properties of the entry	- can vary based on :build state

	char*     word;				//   entry name
  	MEANING*  meanings;			//   list of meanings (synsets) of this word - WiULL be wordnet synset id OR self ptr -- 1-based since 0th meaning means all meanings
 
	//   spelling lists
	WORDP			spellBySize;	//   spell check list indexed by size
	WORDP			spellByPrefix;	//   spell check list indexed by first 4 letters
	WORDP			spellBySuffix;	//   spell check list indexed by last 4 letters
	WORDP			substitutes;	//   words that should be adjusted to during tokenization
	
	union {
		char* gloss;				//   for ordinary word- dictionary definition (synset)
		char* topicRestriction;		//   for topic name- bot topic applies to
	    char * fndefinition;		//   for macro name- if systemflag is on, is user script
	    char * userValue;			//   if a $uservar OR if a search label uservar 
	}w;
	
    union {//   also useful for marking on the fly
          FACT* F;						//   In infer this is a breadcrump trail
          unsigned int topicIndex;		//   for a ~topic or %systemVariable, this is its id
		  unsigned int codeIndex;		//   for a function, its the table index for it
    }x;

	// basic dictionary representation
  	unsigned int nextNode;		//   bucket-link for dictionary hash 
	
	unsigned char meaningCount;	//   how many meanings		-- max 63
	unsigned char multiwordHeader; // can this word lead a phrase to be joined - can vary based on :build state -- really only needs 4 bits
	unsigned char xx;			// alignment chars unused
	unsigned char yy;			// alignment chars unused

    union { //   general marking on the fly during inferencing
        unsigned int    inferMark;     //   been here marker during marking facts, inferencing (for propogation) and during scriptcompile 
   }stamp;

    //   these two fields used in pattern matching set up once
    union {
        unsigned int  patternStamp;  //   is this found in sentence (current timestamp, in which case whereInSentence is locations)
        unsigned int  argumentCount; //   for function defintions - if systemflag is on
    }v;


} WORDENTRY;


extern WordMap canonicalMap;
extern uint64 verbFormat;
extern uint64 nounFormat;
extern uint64 adjectiveFormat;
extern uint64 adverbFormat;
extern int ndict;
extern WORDP Dnot;
extern WORDP posWords[64];
extern WORDP Dnever;
extern WORDP Dnumber;
extern WORDP Dplacenumber;
extern WORDP Dmoney;
extern WORDP Dpropername;
extern WORDP Dspacename;
extern WORDP dictionaryFree;
extern WORDP Dmalename,Dfemalename,Dhumanname,Dfirstname;
extern WORDP Dlocation,Dtime;
extern WORDP Durl;
extern WORDP Dunknown;
extern WORDP Dchild,Dadult;
extern WORDP Dyou;
extern WORDP Dslash;
extern WORDP Dchatoutput;
extern WORDP Dburst;
extern WORDP Dtopic;

extern WORDP dictionaryBase;

char* AllocateString(char* word,unsigned int len = 0,bool align64=false);
WORDP StoreWord(char* word, uint64 properties = 0);
WORDP FindWord(const char* word, int len = 0,unsigned int caseAllowed = STANDARD_LOOKUP);

void AcquireDefines(char* fileName);
char* FindNameByValue(uint64 val);
uint64 FindValueByName(char* name);

// adjust data on a dictionary entry
void AddProperty(WORDP D, uint64 flag);
void RemoveProperty(WORDP D, uint64 flag);
void RemoveSystemFlag(WORDP D, uint64 flag);
void AddSystemFlag(WORDP D, uint64 flag);
void AddCircularEntry(WORDP base, void* field,WORDP entry);
WORDP FindCircularEntry(WORDP base,void* field,uint64 propertyBit);
WORDP FindCircularEntry(WORDP base,int field,uint64 propertyBit);
void RemoveSynSet(MEANING T);

void WriteDictionaryChange(FILE* out, unsigned int zone);

char* GetCanonical(char* word);
char* GetCanonical(WORDP D);

// startup and shutdown routines
void InitDictionary();
void ReturnDictionaryToFreeze();
void CloseDictionary();
void LoadDictionary();
void ExtendDictionary();
void LoadRawDictionary();
void PreBuildLockDictionary();
void Build0LockDictionary();

void ReturnDictionaryToWordNet();
void ReturnDictionaryToBuild0();
void LockDictionary();
unsigned int TotalFreeSpace();
void DeleteDictionaryEntry(WORDP D);

// read and write dictionary or its entries
void WriteDictionary(WORDP D,void* data);
void DumpDictionaryEntry(char* word,unsigned int limit);
void MeasureBuckets();
bool ReadDictionary(char* file);
char* ReadDictionaryFlags(WORDP D, char* ptr);
void WriteDictionaryFlags(WORDP D, FILE* out);
void WriteBinaryDictionary();
bool ReadBinaryDictionary();
void DumpUpHierarchy(MEANING T,int depth); 

// utilities
void ReadWordsOf(char* file,uint64 mark,bool system);
void WalkDictionary(DICTIONARY_FUNCTION func,void* data = NULL);

void Sortit(char* name,int oneline);
void SortTopic(WORDP D,void* junk);
void SortConcept(WORDP D,void* junk);
void SortConcept0(WORDP D,void* junk);
void SortTopic0(WORDP D,void* junk);

#define Index2Word(n) (dictionaryBase+n)
#define Word2Index(D) ((unsigned int) (D-dictionaryBase))

//   routines to manipulate various forms of words

#define GetPlural(D) (D->plurality)
char* GetSingularNoun(char* word);
char* GetPluralNoun(WORDP noun);

char* GetAdjectiveBase(char* word);
char* GetAdverbBase(char* word);
#define GetCompare(D) (D->comparison)

#define GetTense(D) (D->conjugation)
char* GetPastTense(char* word);
char* GetPastParticiple(char* word);
char* GetPresentParticiple(char* word);
char* GetInfinitive(char* word);

bool IsHelper(char* word);
bool IsFutureHelper(char* word);
bool IsPresentHelper(char* word);
bool IsPastHelper(char* word);

char* FindCanonical(char* word, unsigned int i);

unsigned int Spell(char* match, unsigned int set);

///   code to manipulate MEANINGs
MEANING MakeTypedMeaning(WORDP x, unsigned int y, unsigned int flags);
MEANING MakeMeaning(WORDP x, unsigned int y = 0);
#define Meaning2Index(x) ((unsigned int)((x & INDEX_BITS) >> INDEX_OFFSET)) //   which dict entry meaning
WORDP Meaning2Word(MEANING x);
MEANING AddMeaning(WORDP D,MEANING M);
void RemoveMeaning(MEANING M, MEANING M1);

MEANING ReadMeaning(char* word,bool create=true,bool usemaster = true);
char* WriteMeaning(MEANING T,bool useGeneric = true);

#define GetMeanings(D) (D->meanings)
#define GetMeaningsFromMeaning(T) (Meaning2Word(T)->meanings)
MEANING GetMaster(MEANING T);
unsigned int GetMeaningType(MEANING T);
WORDP Meaning2MeaningWord(MEANING T);
MEANING FindSynsetParent(MEANING T,int n);
MEANING FindSetParent(MEANING T,int n);


#endif
