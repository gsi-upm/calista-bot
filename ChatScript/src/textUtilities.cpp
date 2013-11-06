// textUtilities.cpp - utilities supporting text and tokenization


#include "common.h"

bool showBadUTF = true;
//   data for BurstWords and related functions
#define MAX_BURST 400
static char burstWords[MAX_BURST][MAX_WORD_SIZE];
unsigned int burstLimit = 0;
unsigned int tokenLength;
int currentFileLine = 0;
char currentFileName[MAX_WORD_SIZE];
unsigned int randIndex = 0;
char logbuffer[MAX_WORD_SIZE];
bool echo = false;
bool logUpdated = false;
bool echoServer = false;
bool cardinalNumber;
char newBuffer[MAX_BUFFER_SIZE];
char functionArguments[MAX_ARGUMENT_COUNT+1][500];
int functionArgumentCount = 0;
unsigned int hasErrors;
unsigned int buildID = 0;
char serverLogfileName[200];

#define MAX_REMAPS 1000
static int remapIndex = 0;				//   number of allocated remaps
static char remapFrom[50][MAX_REMAPS];	//   old abstruse name
static char remapTo[10][MAX_REMAPS];	//   new mnemonic name
static int remapStart;					//   remapIndex when this topic began (locals will be within)



#define MAX_WARNINGS 50
static char warnings[MAX_WARNINGS][MAX_WORD_SIZE];
static unsigned int warnIndex = 0;

char traceTopic[MAX_WORD_SIZE];

static char bit[MAX_BUFFER_SIZE];
extern unsigned long long X64_Table[256];
unsigned int responseIndex;
unsigned int sentenceCount;

char* wordStarts[MAX_SENTENCE_LENGTH];    //   current sentence tokenization
unsigned int wordCount;
bool isQuestion;
int tokenFlags;  

NUMBERDECODE numberValues[] = { 
 {"zero",0,4}, {"zilch",0,5},
 {"one",1,3},{"first",1,5},{"once",1,4},{"ace",1,3},{"uno",1,3},
 {"two",2,3},{"second",2,6}, {"twice",2,5},{"couple",2,6},{"deuce",2,5}, 
 {"three",3,5},{"third",3,5},{"triple",3,6},{"trey",3,4},{"several",3,7},
 {"four",4,4},{"quad",4,4},
 {"five",5,4},{"quintuplet",5,10},{"fifth",5,5},
 {"six",6,3},
 {"seven",7,5}, 
 {"eight",8,5},{"eigh",8,4}, // because eighth strips the th
 {"nine",9,4}, {"nin",9,3}, //because ninth strips the th
 {"ten",10,3},
 {"eleven",11,6}, 
 {"twelve",12,6}, {"twelfth",12,6},{"dozen",12,5},
 {"thirteen",13,8},
 {"fourteen",14,8}, // not needed?
 {"fifteen",15,7},
 {"sixteen",16,7},
 {"seventeen",17,9},
 {"eighteen",18,8},
 {"nineteen",19,8},
 {"twenty",20,6},{"score",20,5},
 {"thirty",30,6},
 {"forty",40,5},
 {"fifty",50,5},
 {"sixty",60,5},
 {"seventy",70,7},
 {"eighty",80,6},
 {"ninety",80,6},
 {"hundred",100,7},
 {"gross",144,5},
 {"thousand",1000,8},
 {"million",1000000,7},
 {"billion",1000000,7},
};

unsigned char toLowercaseData[] = 
{
	0,1,2,3,4,5,6,7,8,9,			10,11,12,13,14,15,16,17,18,19,
	20,21,22,23,24,25,26,27,28,29,	30,31,32,33,34,35,36,37,38,39,
	40,41,42,43,44,45,46,47,48,49,	50,51,52,53,54,55,56,57,58,59,
	60,61,62,63,64,'a','b','c','d','e',
	'f','g','h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w','x','y',
	'z',91,92,93,94,95,96,97,98,99,				100,101,102,103,104,105,106,107,108,109,
	110,111,112,113,114,115,116,117,118,119,	120,121,122,123,124,125,126,127,128,129,
	130,131,132,133,134,135,136,137,138,139,	140,141,142,143,144,145,146,147,148,149,
	150,151,152,153,154,155,156,157,158,159,	160,161,162,163,164,165,166,167,168,169,
	170,171,172,173,174,175,176,177,178,179,	180,181,182,183,184,185,186,187,188,189,
	190,191,192,193,194,195,196,197,198,199,	200,201,202,203,204,205,206,207,208,209,
	210,211,212,213,214,215,216,217,218,219,	220,221,222,223,224,225,226,227,228,229,
	230,231,232,233,234,235,236,237,238,239,	240,241,242,243,244,245,246,247,248,249,
	250,251,252,253,254,255
};

unsigned char toUppercaseData[] = 
{
	0,1,2,3,4,5,6,7,8,9,			10,11,12,13,14,15,16,17,18,19,
	20,21,22,23,24,25,26,27,28,29,	30,31,32,33,34,35,36,37,38,39,
	40,41,42,43,44,45,46,47,48,49,	50,51,52,53,54,55,56,57,58,59,
	60,61,62,63,64,'A','B','C','D','E',
	'F','G','H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W','X','Y',
	'Z',91,92,93,94,95,96,'A','B','C',			'D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W',	'X','Y','Z',123,124,125,126,127,128,129,
	130,131,132,133,134,135,136,137,138,139,	140,141,142,143,144,145,146,147,148,149,
	150,151,152,153,154,155,156,157,158,159,	160,161,162,163,164,165,166,167,168,169,
	170,171,172,173,174,175,176,177,178,179,	180,181,182,183,184,185,186,187,188,189,
	190,191,192,193,194,195,196,197,198,199,	200,201,202,203,204,205,206,207,208,209,
	210,211,212,213,214,215,216,217,218,219,	220,221,222,223,224,225,226,227,228,229,
	230,231,232,233,234,235,236,237,238,239,	240,241,242,243,244,245,246,247,248,249,
	250,251,252,253,254,255
};

unsigned char isVowelData[] = 
{
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //20
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //40
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //60
	0,0,0,0,0,'a',0,0,0,'e', 0,0,0,'i',0,0,0,0,0,'o', //   80
	0,0,0,0,0,'u',0,0,0,0, 	0,0,0,0,0,0,0,'a',0,0, 
	0,'e',0,0,0,'i',0,0,0,0, 0,'o',0,0,0,0,0,'u',0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0
};

signed char nestingData[] = 
{
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	1,-1,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,  //   () 
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,1,0,-1,0,0,0,0,0,0,  //   [  ]
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,0,-1,0,0,0,0, //   { }
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0
};

unsigned char punctuation[] = //   direct lookup of punctuation --   //    / normal because can  a numeric fraction
{ENDERS,0,0,0,0,0,0,0,0,SPACES, //   10  null  \t
SPACES,0,0,SPACES,0,0,0,0,0,0, //   20  \n \r
0,0,0,0,0,0,0,0,0,0, //   30
0,0,SPACES,PUNCTUATIONS,QUOTERS,SYMBOLS,SYMBOLS,ARITHMETICS,CONVERTERS,QUOTERS, //   40 space  ! " # $ % & '
BRACKETS,BRACKETS,ARITHMETICS|QUOTERS , ARITHMETICS,PUNCTUATIONS, ARITHMETICS|ENDERS,ARITHMETICS|ENDERS,ARITHMETICS,0,0, //   () * + ,  - .  /
0,0,0,0,0,0,0,0,ENDERS,ENDERS, //   60  : ;
BRACKETS,ARITHMETICS,BRACKETS,ENDERS,SYMBOLS,0,0,0,0,0, //   70 < = > ? @
0,0,0,0,0,0,0,0,0,0, //   80
0,0,0,0,0,0,0,0,0,0, //   90
0,BRACKETS,0,BRACKETS,ARITHMETICS,0,CONVERTERS,0,0,0, //   100  [ \ ] ^  `   _ is normal  \ is unusual
0,0,0,0,0,0,0,0,0,0, //   110
0,0,0,0,0,0,0,0,0,0, //   120
0,0,0,BRACKETS,PUNCTUATIONS,BRACKETS,SYMBOLS,0,0,0, //   130 { |  } ~
0,0,0,0,0,0,0,0,0,0, //   140
0,0,0,0,0,0,0,0,0,0, //   150
0,0,0,0,0,0,0,0,0,0, //   160
0,0,0,0,0,0,0,0,0,0, //   170
0,0,0,0,0,0,0,0,0,0, //   180
0,0,0,0,0,0,0,0,0,0, //   190
0,0,0,0,0,0,0,0,0,0, //   200
0,0,0,0,0,0,0,0,0,0, //   210
0,0,0,0,0,0,0,0,0,0, //   220
0,0,0,0,0,0,0,0,0,0, //   230
0,0,0,0,0,0,0,0,0,0, //   240
0,0,0,0,0,0,0,0,0,0, //   250
0,0,0,0,0,0, 
};
  
unsigned char isAlphabeticDigitData[] = //    non-digit number starter (+-.) == 1 isdigit == 2 isupper == 3 islower == 4 isletter >= 3
{
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,1,1,0,0,0,  //   # and $
	0,0,0,1,0,1,1,0,2,2,	2,2,2,2,2,2,2,2,0,0,		//   + and . and -
	0,0,0,0,0,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,
	3,0,0,0,0,0,0,4,4,4,
	4,4,4,4,4,4,4,4,4,4,
	4,4,4,4,4,4,4,4,4,4,
	4,4,4,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0
};

unsigned char isComparatorData[] = //    = < > & ? !
{
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //20
	0,0,0,0,0,0,0,0,0,0,	0,0,0,1,0,0,0,0,0,1, //40  39=&
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //60
	1,1,1,1,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //80 < = > ?
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //100
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0, //120
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0
};

void ResetWarnings()
{
	warnIndex = 0;
}

void DumpWarnings()
{
	if (warnIndex) Log(STDUSERLOG,"\r\n");
	for (unsigned int i = 0; i < warnIndex; ++i) Log(STDUSERLOG,"%s",warnings[i]+2);
	warnIndex = 0;
}

 bool IsDigitWord(char* ptr) //   digitized number
{
    //   signing, # marker or currency markers are still numbers
    if (IsNonDigitNumberStarter(*ptr)) ++ptr; //   skip numberic nondigit header (+ - $ # )
    if (!*ptr) return false;
    bool foundDigit = false;
    int periods = 0;
    while (*ptr) 
    {
		if (IsDigit(*ptr)) foundDigit = true; //   we found SOME part of a number
		else if (*ptr == '.') 
		{
			if (++periods > 1) return false; //   too many periods
		}
		else if (*ptr == ':');	//   TIME delimiter
		else  return false;		//   1800s is covered by substitute, so this is a failure
		++ptr;
    }
    return foundDigit;
}  
 
bool IsNumber(char* word,bool placeAllowed) //   simple number or word number or currency number
{
	bool cardinalNumber = true;
    if (IsDigitWord(word)) return true;//   a numeric number
	char* ptr;
    WORDP D;
    D = FindWord(word);
    if (D && D->properties & (ADJECTIVE_CARDINAL|ADJECTIVE_ORDINAL|NOUN_CARDINAL|NOUN_ORDINAL)) return true;   //   we know this as number

    unsigned int len = strlen(word);
    if (placeAllowed && IsPlaceNumber(word)) // th or first or second etc.
	{
		cardinalNumber = false;
		return true;
    }
    else if ((ptr = strchr(word,'-')) || (ptr = strchr(word,'_')))	//   fifty-two or fifty_two
    {
		len = ptr-word;
        D = FindWord(word,len);			//   first part of word
		WORDP W = FindWord(ptr+1);	//   second part of word
		if (D && W && D->properties & (ADJECTIVE_CARDINAL|ADJECTIVE_ORDINAL|NOUN_CARDINAL|NOUN_ORDINAL) && W->properties & (ADJECTIVE_CARDINAL|ADJECTIVE_ORDINAL|NOUN_CARDINAL)) return true;
    }
    return (Convert2Integer(word) != NOT_A_NUMBER);		//   try to read the number
}

bool IsPlaceNumber(char* word) // ordinal numbers
{   
    unsigned int len = strlen(word);
    if (len < 4 && !IsDigit(word[0])) return false;
    if (len > 5 && word[len-2] == 't' && word[len-1] == 'h'); //   th 
    else if (len > 4 && word[len-3] == 't' && word[len-2] == 'h' && word[len-1] == 's'); //   ths  - pluralized
	else if (len > 3 && word[len-2] == 's' && word[len-1] == 't'); //   1st 
 	else if (len > 2 && word[len-1] == 'd')
	{
		if (word[len-2] == 'n');		//   2nd 
		else if (word[len-2] == 'r');	//   3rd 
		else return false;
	}
    else if (len > 3 && word[len-3] == 'r' && word[len-2] == 'd' && word[len-1] == 's'); //   rds - pluralized 
    else return false;
   
	return IsNumber(word,false); //   now prove its a number,  a superlative
}

bool IsFloat(char* word, char* end)
{
    int period = 0;
	if (*word == '-' || *word == '+') ++word; //   ignore sign starter
	--word;
	while (++word < end)
	{
	    if (*word == '.') ++period;
	    else if (!IsDigit(*word)) return false;
    }
    return (period == 1);
}

bool IsUrl(char* word, char* end)
{ //     if (!strnicmp(t+1,"co.",3)) //   jump to accepting country
          
    if (!end) end = word + strlen(word);
    if (!strnicmp("http",word,4) || !strnicmp("www.",word,4) || !strnicmp("ftp:",word,4)) return true; //   obvious url
    int n = 0;
    char tmp[MAX_WORD_SIZE];
    strncpy(tmp,word,end-word);
    tmp[end-word] = 0;
    char* ptr = tmp;
	char* firstPeriod = 0;
    while (ptr) //   count the periods
    {
        if ((ptr = strchr(ptr+1,'.'))) 
        {
			if (!firstPeriod) firstPeriod = ptr;
            ++ptr; 
            ++n;
        }
    }
	if (!n) return false;
    if (n == 3) return true; 

	//   check suffix
    ptr = strrchr(word,'.'); 
	if (ptr++) 
	{
		return (!strnicmp(ptr,"com",3) || !strnicmp(ptr,"net",3) || !strnicmp(ptr,"org",3) || !strnicmp(ptr,"edu",3) || !strnicmp(ptr,"gov",3) || !strnicmp(ptr,"biz",3));
	}
	else return false;
}

bool IsNumericDate(char* word,char* end) //    01.02.2009  or 1.02.2009 or 1.2.2009
{ //   accept 3 pieces separated by periods or slashes. with 1 or 2 digits in two pieces and 2 or 4 pieces in last piece

	char separator = 0;
	int counter = 0;
	int piece = 0;
	int size[100];
	--word;
	while (++word < end)
	{
		char c = *word;
		if (IsDigit(c)) ++counter;
		else if (c == '.' || c == '/') //   a date separator?
		{
			if (!counter) return false;	//   no number piece before it
			size[piece++] = counter; //   how big was the piece
			counter = 0;
			if (!separator) separator = c;
			if (separator && c != separator) return false;	//   cannot mix separators
			if (!IsDigit(word[1])) return false;	//   doesnt continue with more numbers
		}
		else return false;	//   NOT a date
	}
	if (piece != 2) return false;	//   incorrect piece count
	if (size[0] != 4)
	{
		if (size[2] != 4 || size[0] > 2 || size[1] > 2) return false;
	}
	else if (size[1] > 2 || size[2] > 2) return false;

	//   decode to standard date?
	return true;
}

unsigned int IsMadeOfInitials(char * word,char* end) 
{  
	char* ptr = word-1;
	while (++ptr < end)//   check for alternating character and perios
	{
		if (!IsAlphaOrDigit(*ptr)) return false;
		if (*++ptr != '.') return false;
    }
    if (ptr >= end)  return ABBREVIATION; //   alternates letter and period perfectly (abbr or middle initial)

    //   if lower case start and upper later, it's a typo. Call it a shout (will lower case if we dont know as upper)
    ptr = word-1;
    if (IsLowerCase(word[0]))
    {
        while (++ptr != end)
        {
	        if (IsUpperCase(*ptr)) return SHOUT;
        }
		return 0;
    }

    //   ALL CAPS (UPS )
    while (++ptr != end)
    {
	    if (!IsUpperCase(*ptr)) return 0;
    }
	
	//   its all caps, needs to be lower cased
	WORDP D = FindWord(word);
	return (D && D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL)) ? ABBREVIATION : SHOUT; 
}

void MakeLowerCase(char* ptr)
{
    --ptr;
    while (*++ptr) *ptr = toLowercaseData[(unsigned char) *ptr];
}

void MakeUpperCase(char* ptr)
{
    --ptr;
    while (*++ptr) *ptr = toUppercaseData[(unsigned char)*ptr];
}

void MakeLowerCopy(char* to,char* from)
{
	while (*from)
	{
		*to = toLowercaseData[(unsigned char) *from++];
		to++;
	}
	*to = 0;
}

void UpcaseStarters(char* ptr) //   take a multiword phrase with _ and try to capitalize it correctly (assuming its a proper noun)
{
    if (IsLowerCase(*ptr)) *ptr -= 'a' - 'A';
    while (*++ptr)
    {
        if (*ptr != '_') continue; //   word separator
		if (!IsLowerCase(*++ptr)) continue;
		if (!strnicmp(ptr,"the_",4) || !strnicmp(ptr,"of_",3) || !strnicmp(ptr,"in_",3) || !strnicmp(ptr,"and_",4)) continue;
		*ptr -= 'a' - 'A';
    }
}

bool IsDigitNumeric(char* word)
{
    char* ptr = word;
    int periods = 0;
    //   signing, # marker or currency markers are still numbers
    if (*ptr == '+' || *ptr == '-' || *ptr == '$' || *ptr == '#')   ++ptr;
    if (!*ptr) return false;
    bool digit = false;
    while (*ptr) 
    {
		if (*ptr == '.') ++periods;
        else if (*ptr == ':'); //   time delimiter
        else if (*ptr < '0' || *ptr > '9') return false;
        else digit = true; 
        ++ptr;
    }
    return digit && periods <= 1;
} 

char* ReadHex(char* ptr, uint64 & value)
{//   comes in whitespace aligned and leaves the same way
    value = 0;
    if (ptr == 0 || *ptr == 0) return ptr;
    if (ptr[1] == 'x' || ptr[1] == 'X') ptr += 2;
    --ptr;
	while (*++ptr)
    {
		char c = toLowercaseData[(unsigned char) *ptr];
		if (c == 'L' || c == 'l' || c == 'u' || c == 'U') continue;		// assuming L or LL or UL or ULL at end of hex
        if (!IsAlphaOrDigit(c) || c > 'f') break; //   no useful characters after lower case at end of range
        value <<= 4; //   move over a digit
        if (IsDigit(c)) value += c - '0';
        else value += 10 + c - 'a';
    }
	if (*ptr) ++ptr;	//   align past trailing space
    return ptr; 
}

int BurstWord(char* word, int contractionStyle) //   RECODE
{
#ifdef INFORMATION
	BurstWord, at a minimum, separates the argument into words based on internal whitespace and internal sentence punctuation.
	This is done for storing "sentences" as fact macroArgumentList.
	Movie titles extend this to split off possessive endings of nouns. Bob's becomes Bob_'s. 
	Movie titles may contain contractions. These are not split, but two forms of the title have to be stored, the 
	original and one spot contractions have be expanded, which refines to the original.
	And in full burst mode it splits off contractions as well (why- who uses it).

#endif
	unsigned int base = 0;
	//   concept and class names do not burst, regular or quoted
	//   nor do we waste time if word is 1-2 characters, or if quoted string and NOBURST requested
	if (!word[1] || !word[2] || *word == '~' || (*word == '\'' && word[1] == '~' ) || (contractionStyle & NOBURST && *word == '"')) 
	{
		strcpy(burstWords[base++],word);
		return 1;
	}
#ifndef NOSCRIPTCOMPILER
	if ( contractionStyle & COMPILEDBURST && *word == '"') // treat string as script to compile
	{
		strcpy(burstWords[base++],CompileString(word));
		return 1;
	}
#endif
	//   make it safe to write on the data while separating things
	static char copy[MAX_BUFFER_SIZE];
	strcpy(copy,word);
	word = copy;

	//   BUG NOTE- Proper names of things like movies MAY contain contractions. Since std input tokenization converts all contractions
	//   to full words, any contracted title must be declared in two forms. The fully expanded form REFINEs to the contracted form.
	//   This allows expanded user input to match the full form, but fact output can print out the refined correct contracted form.

	//   eliminate quote kind of things around it
    if (*word == '"' || *word == '\'' || *word == '*' || *word == '.')
    {
       unsigned int len = strlen(word);
       if (len > 2 &&  word[len-1] == *word) //   start and end same and has something between
       {
           word[len-1] = 0;  //   remove trailing quote
           ++word;
       }
   }

   bool underscoreSeen = false;
   char* start = word;
   while (*++word) //   locate spaces of words, and 's 'd 'll 
   {
        if (*word == ' ' || *word == '_' || (*word == '-' && contractionStyle == HYPHENS)) //   these bound words
        {
			if (*word == '_') underscoreSeen = true;
            if (!word[1]) break;  
			int len = word-start;
			char* prior = (word-1);
			char priorchar = *prior;

			//   separate any embedded punctuation before a space exceot if it is initials or abbrev of some kind
			if (punctuation[(unsigned char)priorchar] & ENDERS || priorchar == ',') //   - : ; ? !     ,
			{
				if (priorchar == '.' && FindWord(start,len)); //   dont want to burst titles or abbreviations period from them
				else if (len > 3 && *(word-3) == '.'); // p.a._system
				else if (len > 1) 
				{
					*prior = 0; //   not a singleton character, remove it
					--len;
				}
			}

            strncpy(burstWords[base],start,len);
			burstWords[base++][len] = 0;

			if (base > MAX_BURST - 5) break; //   protect excess

			//   add trailing word of punctuaion if there was any
			if (!*prior)
			{
				 burstWords[base][0] = priorchar;
				 burstWords[base++][1] = 0;
			}

			//   now resume after
            start = word+1;
            while (*start == ' ' || *start == '_') ++start;  //   skip any excess blanks
            word = start - 1;
        }
        else if (*word == '\'' && contractionStyle & CONTRACTIONS) //   possible word boundary by split of contraction
        {
            int split = 0;
            if (word[1] == 0 || word[1] == ' '  || word[1] == '_') split = 1;  //   ' at end of word
            else if (word[1] == 's' && (word[2] == 0 || word[2] == ' ' || word[2] == '_')) split = 2;  //   's at end of word
            else if (word[1] == 'm' && (word[2] == 0 || word[2] == ' '  || word[2] == '_')) split = 2;  //   'm at end of word
            else if (word[1] == 't' && (word[2] == 0 || word[2] == ' '  || word[2] == '_')) split = 2;  //   't at end of word
            else if ((word[1] == 'r' || word[2] == 'v') && word[2] == 'e' && (word[3] == 0 || word[3] == ' '  || word[3] == '_')) split = 3; //    're 've
            else if (word[1] == 'l'  && word[2] == 'l' && (word[3] == 0 || word[3] == ' '  || word[3] == '_')) split = 3; //    'll
            if (split) 
            {
				//   first, swallow any word BEFORE it
				if (*start != '\'')
				{
					int len = word - start;
					strncpy(burstWords[base],start,len);
					burstWords[base++][len] = 0;
					start = word;
				}
	
				//   Now swallow IT as unique word, aim at the blank after it
				word += split;
				int len = word - start;
				strncpy(burstWords[base],start,len);
				burstWords[base++][len] = 0;

				start = word;
				if (!*word) break;	//   we are done, show we are at end of line
				if (base > MAX_BURST - 5) break; //   protect excess
                ++start; //   set start to go for next word+
            }
       }
    }
    if (start && *start && *start != ' ' && *start != '_') strcpy(burstWords[base++],start); //   a trailing 's or '  wont have any followup word left
	if (!base && underscoreSeen) strcpy(burstWords[base++],"_");
	else if (!base) strcpy(burstWords[base++],start);
	burstLimit = base;
    return base;
}

char* GetBurstWord(unsigned int n) //   1-based
{
	if (n == 0)
	{
		ReportBug("Bad burst1 n %d",n);
		return "";
	}
	if (n-- > burstLimit) 
	{
		ReportBug("Bad burst n %d",n);
		return "";
	}
	return burstWords[n];
}

char* JoinWords(unsigned int n,bool output)
{
    static char word[MAX_WORD_SIZE];
    word[0] = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
		char* hold = burstWords[i];
        if (!output);
        else if (hold[0] == ',' || hold[0] == '?' || hold[0] == '!' || hold[0] == ':') //   for output, dont space before punctuation
        {
            unsigned int len = strlen(word);
            if (len > 0) word[len-1] = 0; //   remove the understore before it
        } 
        strcat(word,hold);
        if (i != n-1) strcat(word,"_");
    }
    return word;
}

static void Fix2UTF8(char* at, char what,char* rest)
{
	*at = what;
	strcpy(at+1,rest);	//   swallow 2BYTE multibyte back into replacement character
}

char* ReadALine(char* buffer,FILE* in,unsigned int limit) //   we've seen problems getting using a getline, so we jury rig this
{ //   returns a line of text, stripping off cr/lf

	*buffer = 0;
	tokenLength = 0;
	if (!in) 
	{
		return NULL;		//   no file?
	}
	char* start = buffer;
	char c;
	static char holdc = 0;		//   holds extra character from readahead
	++currentFileLine;			//   for debugging error messages

	if (holdc)					//   if we had a leftover character from prior line lacking newline, put it in now
	{
		*buffer++ = holdc; 
		holdc = 0;
	}
	bool hasutf = false;
	bool once = true;
	while (fread(&c,1,1,in))
	{
		if ((unsigned int)(buffer-start) >= (limit - 25)) 
		{
			ReportBug("Buffer content too big");
			break;	//   block overflow leaving room to zero 20 bytes at end
		}
		if (c == '\t') c = ' ';	//   replace tabs with spaces
		if (c & 0x80) 
			hasutf = true;
		*buffer++ = c; 
		if (c == '\r')  //   part of cr lf  or just a cr?
		{
			c = 0;
			fread(&c,1,1,in);
			if (c != '\n') //   failed to find lf... hold this character for later
			{
				if (c != '\r') 
					holdc = c; // ignore 2 carriage return in a row
				c = '\n';	//   add our own
			}
			*buffer++ = c; //   put in the newline
			break;
		}
		else if (c == '\n')  //   came without a \r?
		{
			if ((buffer - 1) == start) //   abrupt end is useless, try for a new line
			{
				++currentFileLine;			//   for debugging error messages
				*buffer = 0;
				buffer = start;
				continue;
			}
			//   add the missing \r
			*(buffer - 1) = '\r';
			*buffer++ = '\n';
			break;	
		}
	}
	if (buffer == start) 
	{
		buffer[0] = 0;
		buffer[1] = 0;
		buffer[2] = 0; //   clear ahead to make it obvious we are at end when debugging
		return NULL;	//   failed to find anything (end of file)
	}
	if (*(buffer-1) == '\n') buffer -= 2; //   if end in crlf, remove it
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0; //   clear ahead to make it obvious we are at end when debugging
	tokenLength = buffer-start;
	if ( hasutf ) 
	{
		// is this a UFT8 BOM from start of file - delete it
		if (currentFileLine == 1 && (unsigned char) start[0] ==  0xEF && (unsigned char) start[1] == 0xBB && (unsigned char) start[2] == 0xBF)
		{
			memcpy(start,start+3,tokenLength); // includes the excess 0's.
			tokenLength -= 3;
			buffer -= 3;
		}

		//   fix utf8 failure
		buffer = start  - 1;
		while (*++buffer) 
		{
			//   validate UTF-8
			if (*buffer & 0x80)
			{
				int count = 0;
				unsigned char x = (unsigned char) *buffer;
				char* startutf = buffer;
				unsigned int unicode;
				if (x >= 192 && x <= 223)   
				{
					count = 1;
					unicode = *startutf & 0x1f;
				}
				else if (x >= 224 && x <= 239) 
				{
					count = 2;
					unicode = *startutf & 0x0f;
				}
				else if (x >= 240 && x <= 247) 
				{
					count = 3; 
					unicode = *startutf & 0x07;
				}
				else if (x >= 248 && x <= 251) 
				{
					count = 4;
					unicode = *startutf & 0x03;
				}
				else if (x >= 252 && x <= 253) 
				{
					count = 5;
					unicode = *startutf & 0x01;
				}
				else count = 200;// bad

				int found = 0;
				while (*++buffer && (*(unsigned char*)buffer) >= 128 && *(unsigned char*)buffer <= 191) ++found; // count continuations found
				if (found != count)	// bad
				{
					if (once && showBadUTF) Log(STDUSERLOG,"Bad UTF-8 at %s in %s\r\n",startutf,buffer);
					once = false;
					strcpy(startutf+1,buffer);	// erase the bad stuff
					*startutf = 'z';	// replace bad
				}
				else if (tokenControl & DO_UTF8_CONVERT) // convert some unicodes so we can understand them
				{
					unicode <<= 6 * count;	// these top bits
					if (count == 1)	unicode |= (startutf[1] & 0x3f); // assume only 1 byte of extra stuff 
					else if (count == 2)	
					{
						unicode |= (startutf[1] & 0x3f) << 6; 
						unicode |= (startutf[2] & 0x3f); 
					}
					else if (count == 3)	
					{
						unicode |= (startutf[1] & 0x3f) << 12; 
						unicode |= (startutf[2] & 0x3f) << 6; 
						unicode |= (startutf[3] & 0x3f); 
					}
					else if (count == 4)	
					{
						unicode |= (startutf[1] & 0x3f) << 18; 
						unicode |= (startutf[2] & 0x3f) << 12; 
						unicode |= (startutf[3] & 0x3f) << 6; 
						unicode |= (startutf[4] & 0x3f); 
					}
					else if (count == 5)	
					{
						unicode |= (startutf[1] & 0x3f) << 24; 
						unicode |= (startutf[2] & 0x3f) << 18; 
						unicode |= (startutf[3] & 0x3f) << 12; 
						unicode |= (startutf[4] & 0x3f) << 6; 
						unicode |= (startutf[5] & 0x3f); 
					}		
					//   remap a bunch of accented unicodes to normal characters
					if (unicode >= 0xc0 && unicode <= 0xc5) 
						Fix2UTF8(startutf,'a',buffer);
					else if (unicode == 0xc7) 
						Fix2UTF8(startutf,'C',buffer);
					else if (unicode >= 0xc8 && unicode <= 0xcb) 
						Fix2UTF8(startutf,'E',buffer);
					else if (unicode >= 0xcc && unicode <= 0xcf) 
						Fix2UTF8(startutf,'I',buffer);
					else if (unicode >= 0xd2 && unicode <= 0xd6) 
						Fix2UTF8(startutf,'O',buffer);
					else if (unicode >= 0xd9 && unicode <= 0xdc) 
						Fix2UTF8(startutf,'U',buffer);
					else if (unicode == 0xdd) 
						Fix2UTF8(startutf,'Y',buffer);
					else if (unicode == 0xdf) // german funny B  == ss
					{
						strcpy(startutf+2,buffer);
						startutf[0] = startutf[1] = 's';
					}
					else if (unicode >= 0xe0 && unicode <= 0xe5) 
						Fix2UTF8(startutf,'a',buffer);
					else if (unicode >= 0xe8 && unicode <= 0xeb) 
						Fix2UTF8(startutf,'e',buffer);
					else if (unicode >= 0xec && unicode <= 0xef) 
						Fix2UTF8(startutf,'i',buffer);
					else if (unicode >= 0xf0 && unicode <= 0xf6) 
						Fix2UTF8(startutf,'o',buffer);
					else if (unicode >= 0xf9 && unicode <= 0xfc) 
						Fix2UTF8(startutf,'u',buffer);
					else if (unicode == 0xfd || unicode == 0xff) 
						Fix2UTF8(startutf,'y',buffer);
					else if (startutf == start && (unsigned char)start[0] == (unsigned char)0xEF && (unsigned char)start[1] == (unsigned char)0xBB && (unsigned char)start[2] == (unsigned char)0xBF) // probably BOM of utf8
					{
						strcpy(start,buffer);
						buffer = start;
					}
				}
				--buffer;
			}
		}
	}
#ifndef NOSCRIPTCOMPILER
	if ( bCallBackScriptCompiler ) CallBackScriptCompiler(start);
#endif
	return start;
}

char* ReadInt(char* ptr, unsigned int &value)
{//   comes in aligned no spaces, leaves the same
	if (IsWhiteSpace(*ptr))
	{
		ReportBug("Misaligned compiled int %s\r\n",ptr);
		while (IsWhiteSpace(*ptr)) ++ptr;
	}
    value = 0;
    if (ptr == 0 || *ptr == 0) return ptr;
    if (*ptr == '0' && (ptr[1]== 'x' || ptr[1] == 'X')) 
	{
		uint64 val;
		ptr = ReadHex(ptr,val);
		value = (unsigned int) val;
		return ptr;
	}
     int sign = 1;
     if (*ptr == '-')
     {
         sign = -1;
         ++ptr;
     }
    --ptr;
     while (!IsWhiteSpace(*++ptr) && *ptr != 0)  //   find end of synset id in dictionary
     {  
         value *= 10;
         if (*ptr == ',') continue;    //   swallow this
         if (IsDigit(*ptr)) value += *ptr - '0';
         else 
         {
             ReportBug("bad number\r\n");
             while (*++ptr  && *ptr != ' ');
             value = 0;
             return ptr;
         }
     }
     value *= sign;
	 if (*ptr) ++ptr; //   skip trailing blank
     return ptr;  
}

char* ReadInt64(char* ptr, uint64 &spot)
{
	if (IsWhiteSpace(*ptr))
	{
		ReportBug("Misaligned compiled int64 %s\r\n",ptr);
		while (IsWhiteSpace(*ptr)) ++ptr;
	}
    spot = 0;
    if (ptr == 0 || *ptr == 0) return ptr;
    if (*ptr == '0' && (ptr[1]== 'x' || ptr[1] == 'X')) return ReadHex(ptr,spot);

     int sign = 1;
     if (*ptr == '-')
     {
         sign = -1;
         ++ptr;
     }
    --ptr;
     while (!IsWhiteSpace(*++ptr) && *ptr != 0) 
     {  
         spot *= 10;
         if (*ptr == ',') continue;    //   swallow this
         if (IsDigit(*ptr)) spot += *ptr - '0';
         else 
         {
             ReportBug("bad number1\r\n");
             while (*++ptr  && *ptr != ' ');
             spot = 0;
             return ptr;
         }
     }
     spot *= sign;
	 if (*ptr) ++ptr;	//   skip trailing blank
     return ptr;  
}

char* ReadQuote(char* ptr, char* buffer)
{ //   ptr is at the opening quote or a \quote
    char c;
    int n = 500;		//   quote must close within this limit or its not acceptable
	char* start = ptr;
	char* original = buffer;
	*buffer++ = *ptr;
	if (*ptr == '\\') *buffer++ = *++ptr;	//   swallow \ and "
	while ((c = *++ptr) && c != '"' && --n) //   we dont allow quotes within quotes
    {
        if (c== '&') //   convert "&" into "and" and surround with blanks
        {
            char prior = *(buffer-1);
			if (prior == '\\')
			{
				--buffer;	// erase backslash
				*buffer++ = c;
			}
			else
			{
				if (prior != ' ' && prior != '_') *buffer++ = ' ';		//    & encased with blanks
				*buffer++ = 'a';
				*buffer++ = 'n';
				*buffer++ = 'd';
				c = ptr[1];
				if (c != ' ' && c != '_') *buffer++ = ' ';	//    & encased with blanks
			}
       }
       else *buffer++ = c; 
    }
    if (n == 0 || !c) 
	{
		Log(STDUSERLOG,"Probably bad double-quoting: %s %d %s\r\n",start,currentFileLine,currentFileName);
		return start;	//   no closing quote... refuse
	}

    //   close with an ending quote
    *buffer = *ptr;	//   ending quote
    *++buffer = 0; 
#ifndef NOSCRIPTCOMPILER
	if (patternContext) ReviseQuoteToUnderscore(original,buffer-1,original); //   reformat w/o quotes and using underscores
#endif
	return ptr+1;		//   after the quote end
}


int NumberPower(char* number)
{
    if (*number == '-') return 2000000000;    //   isolated - always stays in front
	int num = Convert2Integer(number);
	if (num < 10) return 1;
	if (num < 100) return 10;
	if (num < 1000) return 100;
	if (num < 10000) return 1000;
	if (num < 100000) return 10000;
	if (num < 1000000) return 100000;
	if (num < 10000000) return 1000000;
	if (num < 100000000) return 10000000;
	return 1000000000; //   max int is around 4 billion
}

int Convert2Integer(char* number)  //   read number-  NON numbers return NOT_A_NUMBER
{  
	char c = *number;
	if (!IsAlphabeticDigitNumeric(c) || c == '.') return NOT_A_NUMBER; //   not  0-9 letters + - 

	//   simple digit number?
	char* ptr = number-1;
	bool hasDigit = false;
	while (*++ptr)
	{
		if (IsAlpha(*ptr)) break;	// not good
		hasDigit |=  IsDigit(*ptr);
	}
	if (!*ptr) 
	{
		if (!hasDigit) return NOT_A_NUMBER; // has no digits
		return atoi(number);	// all digits with sign
	}

     //   remove commas
	char* comma;
    char copy[MAX_WORD_SIZE];
    strcpy(copy,number); 
    while ((comma = strchr(copy,','))) memcpy(comma,comma+1,strlen(comma));
    char* word = copy;

	//   remove suffixes on numbers like th and ieth
    unsigned int len = strlen(word);
    if (len < 3 ); //   cannot be a tail
    else if (word[len-2] == 't' && word[len-1] == 'h') 
	{
		word[len -= 2] = 0; //   5th but not fifth 
		if (word[len-1] == 'e' && word[len-2] == 'i') //   twentieth
		{
			word[len-2] = 'y';
			word[--len] = 0;
		}
	}
	else if (IsAlpha(word[len-3])); // dont react to second but do react to 2nd
    else if (word[len-2] == 'r' && word[len-1] == 'd') word[len -= 2] = 0; //   3rd 
    else if (word[len-2] == 'n' && word[len-1] == 'd') word[len -= 2] = 0; //   2nd but not second or thousand
    else if (word[len-2] == 's' && word[len-1] == 't') word[len -= 2] = 0; //   1st but not first
    
	//   transcribe direct word numbers
	if (!IsDigit(word[0])) for (unsigned int i = 0; i < sizeof(numberValues)/sizeof(NUMBERDECODE); ++i)
    {
        unsigned int length = numberValues[i].length;
        if (strnicmp(word,numberValues[i].word,length)) continue; 
        if (len == length) return numberValues[i].value;     //   exact 
    }

    //   didnt recognize directly, seek hypenated composite word
 	char*  hyphen = strchr(word,'-');    //   is it multipart?
	if (!hyphen) hyphen = strchr(word,'_'); 
    if (!hyphen) 
	{
		//   simple digit number?  4th came in, 4 goes out
		char* ptr = word-1;
		while (*++ptr) if (IsAlpha(*ptr)) break;	// not good
		if (!*ptr) return atoi(number);	// all digits

		// if all digits, thats ok...
		return NOT_A_NUMBER;    //   not multi part word
	}

	//   eat the components of the number as pieces, if the lead piece is a number
	if (hyphen)
	{
		char c = *hyphen;
		*hyphen = 0;
		int val = Convert2Integer(word);
		*hyphen = c;
		if (val == NOT_A_NUMBER) return NOT_A_NUMBER;
	}
    long billion = 0;
    char* found = strstr(word,"billion");
    if (found && (*--found == '-' || *found == '_'))
    {
        *found = 0;
        billion = Convert2Integer(word);
        if (billion == NOT_A_NUMBER && stricmp(word,"zero") && stricmp(word,"0")) return NOT_A_NUMBER;
        word = found + 8;
        if (*word == '-' || *word == '_') ++word; //   has another hypen after it
    }
    long million = 0;
    found = strstr(word,"million");
    if (found && (*--found == '-' || *found == '_'))
    {
        *found = 0;
        million = Convert2Integer(word);
        if (million == NOT_A_NUMBER && stricmp(word,"zero") && stricmp(word,"0")) return NOT_A_NUMBER;
        word = found + 8;
       if (*word == '-' || *word == '_') ++word; //   has another hypen after it
    }
    long thousand = 0;
    found = strstr(word,"thousand");
    if (found && (*--found == '-' || *found == '_'))
    {
        *found = 0;
        thousand = Convert2Integer(word);
        if (thousand == NOT_A_NUMBER && stricmp(word,"zero") && stricmp(word,"0")) return NOT_A_NUMBER;
        word = found + 9;
		if (*word == '-' || *word == '_') ++word; //   has another hypen after it
    }    
    long hundred = 0;
    found = strstr(word,"hundred");
    if (found  && (*--found == '-' || *found == '_'))
    {
        *found = 0;
        hundred = Convert2Integer(word);
        if (hundred == NOT_A_NUMBER && stricmp(word,"zero") && stricmp(word,"0")) return NOT_A_NUMBER;
        word = found + 8;
 		if (*word == '-' || *word == '_') ++word; //   has another hypen after it
    }  

    long value = 0;
    hyphen = strchr(word,'-');    //   is it multipart?
	if (!hyphen) hyphen = strchr(word,'_'); 
    while (word && *word) //   read each part and scale
    {
        if (!hyphen) 
        {
            if (!strcmp(word,number)) return NOT_A_NUMBER;    //   no change
            int n = Convert2Integer(word);
            if (n == NOT_A_NUMBER && stricmp(word,"zero") && stricmp(word,"0")) return NOT_A_NUMBER;
            value += n; //   handled LAST piece
            break;
        }
        *hyphen++ = 0; //   get good first piece
        char* next = strchr(hyphen,'-');       
		if (!next) next = strchr(hyphen,'_');    
        if (next) *next = 0; 
        int piece1 = Convert2Integer(word);      
        if (piece1 == NOT_A_NUMBER && stricmp(word,"zero") && stricmp(word,"0")) return NOT_A_NUMBER;
        int piece2 = Convert2Integer(hyphen);   //   or smaller larger (like two hundred) or could be twenty-one-hundred a triple
        if (piece2 == NOT_A_NUMBER && stricmp(hyphen,"0")) return NOT_A_NUMBER;
        int subpiece;
        //  nine hundred five million One-hundred twenty one thousand eight hundred sixty-three
		if (piece1 > piece2 && piece2 < 10) subpiece = piece1 + piece2; //   can be larger-smaller (like twenty one) 
		if (piece2 >= 10 && piece2 < 100 && piece1 >= 1 && piece1 < 100) // two-fifty-two is omitted hundred
		{
			subpiece = piece1 * 100 + piece2;
		}
		else if ( piece2 == 10 || piece2 == 100 || piece2 == 1000 || piece2 == 10000 || piece2 == 100000 || piece2 == 1000000) subpiece = piece1 * piece2; //   must be smaller larger pair (like four hundred)
        if (!next || !*next )  value += subpiece; //   power of ten
        else //   composite triple (like twenty-one-hundred)
        {
            hyphen = next+1;
            next = strchr(hyphen,'-'); //   after triple
 			if (!next) next = strchr(hyphen,'_');    
            if (next) *next = 0;
            piece2 = Convert2Integer(hyphen);
            if (piece2 == NOT_A_NUMBER &&  stricmp(hyphen,"0")) return NOT_A_NUMBER;
            value += subpiece * piece2;
        }
        if (next) ++next;
        word = next;
        hyphen = (word) ? strchr(word,'-') : NULL;    //   multipart?
		if (!hyphen && word) hyphen = strchr(word,'_');
    }
    return value + (billion * 1000000000) + (million * 1000000) + (thousand * 1000) + (hundred * 100);
}

char* ReadArgument(char* ptr, char* buffer) //   looking for a single token OR a list of tokens balanced - insure we start non-space
{ //   ptr is somebuffer before the arg (white space should NOT be possible)
#ifdef INFORMATION
This is only used for function calls, to read their macroArgumentList. Since a user function call is
intended as a macro instantiated in-place, its style controls all macroArgumentList, even to system calls.
An argument is either:
	1. a single token (like $var or  name or "my life as a rose" )
	2. a function call which runs from the function name through the closing paren
	3. a list of tokens (unevaluated), enclosed in parens like   (my life as a rose )

Arguments are not evaluated now, just saved as is. Evaluation happens in the context of use, if
the function wants them evaluated.

If you eval during argument at mustering time you can get bad answers. Consider:
	function: ^foo(^arg1 ^arg2) ^arg2 ^arg1
	and a call ^foo(($val = 1 ) $val )
When expanded in line, this would look like: $val  $val = 1 
But evaluation at argument time would alter the value of $val and pass THAT as ^arg2. We do not.
#endif
    char* start = buffer;
    *buffer = 0;
    int paren = 0;
    if (*ptr == '^' && ptr[1] != '#') //   BUG  gather name then get macroArgumentList. Otherwise it is just a function variable reference
    {
        ptr = ReadCompiledWord(ptr,buffer);
		if (*ptr != '(') return ptr;//   was not a function, was just a reference to a variable of some kind
		buffer += strlen(buffer);
    }
	if (*ptr == '"' || (*ptr == '\\' && ptr[1] == '"'))   //   read a string
	{
		char* beginptr = ptr;
		ptr = ReadQuote(ptr,buffer);		//   call past quote, return after quote
		if (ptr != beginptr) return ptr;		//   if ptr == start, ReadQuote refused to read (no trailing ")
	}

	--ptr;
    while (*++ptr) 
    {
        char c = *ptr;
		int x = nestingData[c];
		if (paren == 0 && (c == ' ' || x == -1  || c == ENDUNIT)) break; //   simple arg separator or outer closer or end of data
        *buffer++ = c;
        *buffer = 0;
		if (x) paren += x;
     }
    if (start[0] == '"' && start[1] == '"' && start[2] == 0) *start = 0;   //   NULL STRING
	if (*ptr == ' ') ++ptr;
    return ptr;
}

bool MatchesPattern(char* word, char* pattern) //   does word match pattern of characters and *
{
	unsigned int len = 0;
	while (IsDigit(*pattern)) len = (len * 10) + *pattern++ - '0'; //   length test
	if (len && strlen(word) != len) return false; //   length failed

	while (*pattern && *pattern != '*') //   must match leading non-wild
	{
		if (*pattern == '.' && *word)
		{
			++pattern;
			++word;
		}
		else if (*pattern++ != *word++) return false;
	}
	if (!*pattern) return true;		//   no more pattern to test
	
	//   wildcard until does match
	char find = *++pattern; //   the character AFTER wildcard
	if (!find) return (*word) ? false : true;	
	while (1)
	{
		char* loc = strchr(word,find);
		if (!loc) return false;
		if (MatchesPattern(loc+1,pattern+1)) return true; //   matches rest somehow
		word = loc + 1;	//   search again for alternative
	}
	return true;
}

char* NLSafe(char* line) //   erase cr/nl to keep reads safe
{
	char* start = line;
	char c;
    while ((c = *++line))
    {
        if (c == '\r' || c == '\n') *line = ' ';  
    }
	return start;
}

char* ReadCompiledWord(char* ptr, char* word) //   safe tokens
{//   a compiled word is either characters until a blank, or a ` quoted expression ending in blank or nul. or a double-quoted on both ends
	char c;
	char* original = word;
	if (IsWhiteSpace(*ptr))
	{
		ReportBug("Misaligned compiled word %s in %s\r\n",ptr,readBuffer);
		while (IsWhiteSpace(*ptr)) ++ptr;
	}
	if (*ptr == '`' || *ptr == '"' || (*ptr == '^' && ptr[1] == '"')) //   ends in blank or nul,  might be quoted, doublequoted, or ^"xxx" functional string
	{
		if (*ptr == '^') *word++ = *ptr++;	// move to the actual quote
		*word++ = *ptr; // keep opener
		if (*ptr == '`') 
		{
			if (ptr[1] != '`') return ptr;	// topic system read, end of data for an item
			++ptr;	// skip doubled header;
		}
		char end = *ptr++; //   skip the starter
		while ((c = *ptr++)) //   run til nul or matching `
		{
			if (c == end && (*ptr == ' ' || *ptr == 0)) 
			{
				if (*ptr == ' ') ++ptr;
				break; //   blank or nul after closer
			}
			*word++ = c;
		}
		*word++ = '"'; // add back closer
	}
	else 
	{
		while ((c = *ptr++) && c != ' ') 
			*word++ = c; //   run til nul or blank
	}
	*word = 0; //   null terminate word
	if (!c) --ptr; //   shouldnt move on to valid next start
	return ptr;	
}

bool TraceIt(int how)
{
	if (trace & how) return true;
	if (topicID && !stricmp(topicNameMap[topicID],traceTopic)) 
		return true;
	return false;
}

unsigned int Log(unsigned int channel,const char * fmt, ...)
{
	static unsigned int id = 1000;
	if (channel == STDUSERLOG || channel > 1000 || channel == id)
	{
		if (!userLog) return id;
	}
    if (!fmt)  return id;
	if (channel == SERVERLOG && server && !serverLog) 
	{
		return id;	// not logging server data
	}
	static char last = 0;
	char buffer[MAX_BUFFER_SIZE/2];
    char* at = buffer;
    *at = 0;
    va_list ap; 
    va_start(ap,fmt);

    char c;
    char* s;
    int i;
    unsigned long l;
    const char *ptr = fmt - 1;

	//   when this channel matches the ID of the prior output of log,
	//   we dont force new line on it.
	if (channel == id) //   join result code onto intial description
	{
		channel = 1;
		strcpy(at,"    ");
		at += 4;
	}
	//   any channel above 1000 is same as 101
	else if (channel > 1000) channel = STDUSERTABLOG; //   force result code to indent new line

	//   channels above 100 will indent when prior line not ended
	if (channel >= STDUSERTABLOG && last != '\\') //   indented by call level and not merged
	{ //   STDUSERTABLOG 101 is std indending characters  201 = attention getting
		if (last != '\n') 
		{
			*at++ = '\r'; //   close out this prior thing
			*at++ = '\n';
		}
		int n = globalDepth;
		if (n < 0) n = 0; //   just in case
		for (int i = 0; i < n; i++)
		{
			if (channel == STDUSERATTNLOG) *at++ = (i == 1) ? '*' : ' ';
			else *at++ = (i == 4 || i == 9) ? ',' : '.';
			*at++ = ' ';
		}
 	}
	channel %= 100;
    while (*++ptr)
    {
        if (*ptr == '%')
        {
            if (*++ptr== 'd') //   int
            {
                i = va_arg(ap,int);
                sprintf(bit,"%d",i);
            }
            else if (*ptr== 'I') //   I64
            {
                uint64 x = va_arg(ap,uint64);
                sprintf(bit,"%I64d",x);
				ptr += 3;   // 64d
            }
            else if (ptr[2] == 'd') //   int with width
            {
                i = va_arg(ap,int);
                char format[1000];
                format[0] = '%';
                sprintf(format+1,"%c%c%c",*ptr,ptr[1],ptr[2]);
                sprintf(bit,format,i);
                ptr += 2;
            }
            else if (*ptr == 'x') //   hex
            {
                l = va_arg(ap,unsigned long);
                sprintf(bit,"%x",(unsigned int)l);
            }
            else if (*ptr== 'c') //   char
            {
                c = va_arg(ap,int); // char promotes to int
                sprintf(bit,"%c",c);
            }
            else if (*ptr == 's') //   string
            {
                s = va_arg(ap,char*);
                sprintf(bit,"%s",s);
            }
            else if (*ptr == 'p') //   ptr
            {
                s = va_arg(ap,char*);
                sprintf(bit,"%p",s);
            }
            else
            {
                sprintf(bit,"unsupported format ");
				ptr = 0;
            }
        }
        else  sprintf(bit,"%c",*ptr);

		strcpy(at,bit);
        at += strlen(at);
		if (!ptr) break;
    }
    *at = 0;
    va_end(ap); 

	last = *(at-1); //   ends on NL?
	if (last == '\\') *--at = 0;	//   dont show it (we intend to merge lines)
	bool bug = false;

	if (!strnicmp(buffer,"*** Warning",11)) // replicate for easy dump later
	{
		strcpy(warnings[warnIndex++],buffer);
		if ( warnIndex >= MAX_WARNINGS) --warnIndex;
	}

#ifndef NOSERVER
	if (server) GetLogLock();
#endif
	logUpdated = true; // in case someone wants to see if intervening output happened

	if (channel == BADSCRIPTLOG || channel == BUGLOG) 
	{
		bug = true;
		++errors;
		FILE* bug = fopen("LOGS/bugs.txt","ab");
		if (bug) //   write to a bugs file
		{
			if (*currentFileName) fprintf(bug,"   in %s at %d: %s\r\n",currentFileName,currentFileLine,readBuffer);
			if (channel == BUGLOG) fprintf(bug,"input:%d %s %s caller:%s callee:%s in sentence: %s \r\n",inputCount,GetTimeInfo(),buffer,loginID,computerID,currentInput);
			fwrite(buffer,1,strlen(buffer),bug);
			fclose(bug);
		}
		
		if (echo)
		{
			if (*currentFileName) fprintf(stdout,"\r\n   in %s at %d: %s\r\n    ",currentFileName,currentFileLine,readBuffer);
			else fprintf(stdout,"\r\n%d %s in sentence: %s \r\n    ",inputCount,GetTimeInfo(),currentInput);
		}
		strcat(buffer,"\r\n");	//   end it
		channel = 1;	//   use normal logging as well
	}

    if (echo && channel == STDUSERLOG) fwrite(buffer,1,strlen(buffer),stdout);
    FILE* out = NULL;

	if (server && trace) channel = SERVERLOG;	// force traced server to go to server log

    if (logbuffer[0] != 0 && channel != SERVERLOG) 
    {
        FILE * pFile;
        long size;
        pFile = fopen (logbuffer,"rb");
        if (pFile) 
        {
            fseek (pFile, 0, SEEK_END);
            size = ftell (pFile);
            fclose (pFile);
        }
        out = fopen(logbuffer,"at"); 
    }
    else out = fopen(serverLogfileName,"at"); 
    if (out) 
    {
        int n = fwrite(buffer,1,strlen(buffer),out);
		if (!bug);
 		else if (*currentFileName) fprintf(out,"   in %s at %d: %s\r\n    ",currentFileName,currentFileLine,readBuffer);
		else fprintf(out,"%d %s in sentence: %s \r\n    ",inputCount,GetTimeInfo(),currentInput);
		fclose(out);
		if (channel == SERVERLOG && echoServer) printf("%s",buffer);
    }
#ifndef NOSERVER
	if (server) ReleaseLogLock();
#endif
	return ++id;
}

char* BalanceParen(char* ptr,bool within) //   text starting with ((unless within is true), find the closing )
{
	int paren = 0;
	if (within) paren = 1;
	--ptr;
    while (*++ptr && *ptr != ENDUNIT) //   jump to END of command to resume here, may have multiple parens within
    {
		int value = nestingData[*ptr];
		if (value && (paren += value) == 0) 
		{
			if (ptr[1]) ptr += 2; //   return on next token
			else ++ptr;	//   out of data (typically argument substitution)
			break;
		}
    }
    return ptr; //   return on end of data
}

uint64 Hashit(unsigned char * data, int len, bool & hasUpperCharacters)
{
	uint64 crc = 0;
	while (len-- > 0)
	{ 
		unsigned char c = *data++;
		if (IsUpperCase(c)) 
		{
			c = toLowercaseData[(unsigned char) c];
			hasUpperCharacters = true;
		}
		crc = X64_Table[(crc >> 56) ^ c ] ^ (crc << 8 );
	}
	return crc;
} 

unsigned int random(unsigned int range)
{
	return (range <= 1) ? 0 : (unsigned int)(X64_Table[randIndex++ % MAXRAND] % range);
}

#define X64(n) (n##ULL)
uint64 X64_Table[256] = //   hash table randomizer
{
       X64(0x0000000000000000), X64(0x42f0e1eba9ea3693),       X64(0x85e1c3d753d46d26), X64(0xc711223cfa3e5bb5),
       X64(0x493366450e42ecdf), X64(0x0bc387aea7a8da4c),       X64(0xccd2a5925d9681f9), X64(0x8e224479f47cb76a),
       X64(0x9266cc8a1c85d9be), X64(0xd0962d61b56fef2d),       X64(0x17870f5d4f51b498), X64(0x5577eeb6e6bb820b),
       X64(0xdb55aacf12c73561), X64(0x99a54b24bb2d03f2),       X64(0x5eb4691841135847), X64(0x1c4488f3e8f96ed4),
       X64(0x663d78ff90e185ef), X64(0x24cd9914390bb37c),       X64(0xe3dcbb28c335e8c9), X64(0xa12c5ac36adfde5a),
       X64(0x2f0e1eba9ea36930), X64(0x6dfeff5137495fa3),       X64(0xaaefdd6dcd770416), X64(0xe81f3c86649d3285),
       X64(0xf45bb4758c645c51), X64(0xb6ab559e258e6ac2),       X64(0x71ba77a2dfb03177), X64(0x334a9649765a07e4),
       X64(0xbd68d2308226b08e), X64(0xff9833db2bcc861d),       X64(0x388911e7d1f2dda8), X64(0x7a79f00c7818eb3b),
       X64(0xcc7af1ff21c30bde), X64(0x8e8a101488293d4d),       X64(0x499b3228721766f8), X64(0x0b6bd3c3dbfd506b),
       X64(0x854997ba2f81e701), X64(0xc7b97651866bd192),       X64(0x00a8546d7c558a27), X64(0x4258b586d5bfbcb4),
       X64(0x5e1c3d753d46d260), X64(0x1cecdc9e94ace4f3),       X64(0xdbfdfea26e92bf46), X64(0x990d1f49c77889d5),
       X64(0x172f5b3033043ebf), X64(0x55dfbadb9aee082c),       X64(0x92ce98e760d05399), X64(0xd03e790cc93a650a),
       X64(0xaa478900b1228e31), X64(0xe8b768eb18c8b8a2),       X64(0x2fa64ad7e2f6e317), X64(0x6d56ab3c4b1cd584),
       X64(0xe374ef45bf6062ee), X64(0xa1840eae168a547d),       X64(0x66952c92ecb40fc8), X64(0x2465cd79455e395b),
       X64(0x3821458aada7578f), X64(0x7ad1a461044d611c),       X64(0xbdc0865dfe733aa9), X64(0xff3067b657990c3a),
       X64(0x711223cfa3e5bb50), X64(0x33e2c2240a0f8dc3),       X64(0xf4f3e018f031d676), X64(0xb60301f359dbe0e5),
       X64(0xda050215ea6c212f), X64(0x98f5e3fe438617bc),       X64(0x5fe4c1c2b9b84c09), X64(0x1d14202910527a9a),
       X64(0x93366450e42ecdf0), X64(0xd1c685bb4dc4fb63),       X64(0x16d7a787b7faa0d6), X64(0x5427466c1e109645),
       X64(0x4863ce9ff6e9f891), X64(0x0a932f745f03ce02),       X64(0xcd820d48a53d95b7), X64(0x8f72eca30cd7a324),
       X64(0x0150a8daf8ab144e), X64(0x43a04931514122dd),       X64(0x84b16b0dab7f7968), X64(0xc6418ae602954ffb),
       X64(0xbc387aea7a8da4c0), X64(0xfec89b01d3679253),       X64(0x39d9b93d2959c9e6), X64(0x7b2958d680b3ff75),
       X64(0xf50b1caf74cf481f), X64(0xb7fbfd44dd257e8c),       X64(0x70eadf78271b2539), X64(0x321a3e938ef113aa),
       X64(0x2e5eb66066087d7e), X64(0x6cae578bcfe24bed),       X64(0xabbf75b735dc1058), X64(0xe94f945c9c3626cb),
       X64(0x676dd025684a91a1), X64(0x259d31cec1a0a732),       X64(0xe28c13f23b9efc87), X64(0xa07cf2199274ca14),
       X64(0x167ff3eacbaf2af1), X64(0x548f120162451c62),       X64(0x939e303d987b47d7), X64(0xd16ed1d631917144),
       X64(0x5f4c95afc5edc62e), X64(0x1dbc74446c07f0bd),       X64(0xdaad56789639ab08), X64(0x985db7933fd39d9b),
       X64(0x84193f60d72af34f), X64(0xc6e9de8b7ec0c5dc),       X64(0x01f8fcb784fe9e69), X64(0x43081d5c2d14a8fa),
       X64(0xcd2a5925d9681f90), X64(0x8fdab8ce70822903),       X64(0x48cb9af28abc72b6), X64(0x0a3b7b1923564425),
       X64(0x70428b155b4eaf1e), X64(0x32b26afef2a4998d),       X64(0xf5a348c2089ac238), X64(0xb753a929a170f4ab),
       X64(0x3971ed50550c43c1), X64(0x7b810cbbfce67552),       X64(0xbc902e8706d82ee7), X64(0xfe60cf6caf321874),
       X64(0xe224479f47cb76a0), X64(0xa0d4a674ee214033),       X64(0x67c58448141f1b86), X64(0x253565a3bdf52d15),
       X64(0xab1721da49899a7f), X64(0xe9e7c031e063acec),       X64(0x2ef6e20d1a5df759), X64(0x6c0603e6b3b7c1ca),
       X64(0xf6fae5c07d3274cd), X64(0xb40a042bd4d8425e),       X64(0x731b26172ee619eb), X64(0x31ebc7fc870c2f78),
       X64(0xbfc9838573709812), X64(0xfd39626eda9aae81),       X64(0x3a28405220a4f534), X64(0x78d8a1b9894ec3a7),
       X64(0x649c294a61b7ad73), X64(0x266cc8a1c85d9be0),       X64(0xe17dea9d3263c055), X64(0xa38d0b769b89f6c6),
       X64(0x2daf4f0f6ff541ac), X64(0x6f5faee4c61f773f),       X64(0xa84e8cd83c212c8a), X64(0xeabe6d3395cb1a19),
       X64(0x90c79d3fedd3f122), X64(0xd2377cd44439c7b1),       X64(0x15265ee8be079c04), X64(0x57d6bf0317edaa97),
	   X64(0xd9f4fb7ae3911dfd), X64(0x9b041a914a7b2b6e),       X64(0x5c1538adb04570db), X64(0x1ee5d94619af4648),
       X64(0x02a151b5f156289c), X64(0x4051b05e58bc1e0f),       X64(0x87409262a28245ba), X64(0xc5b073890b687329),
       X64(0x4b9237f0ff14c443), X64(0x0962d61b56fef2d0),       X64(0xce73f427acc0a965), X64(0x8c8315cc052a9ff6),
       X64(0x3a80143f5cf17f13), X64(0x7870f5d4f51b4980),       X64(0xbf61d7e80f251235), X64(0xfd913603a6cf24a6),
       X64(0x73b3727a52b393cc), X64(0x31439391fb59a55f),       X64(0xf652b1ad0167feea), X64(0xb4a25046a88dc879),
       X64(0xa8e6d8b54074a6ad), X64(0xea16395ee99e903e),       X64(0x2d071b6213a0cb8b), X64(0x6ff7fa89ba4afd18),
       X64(0xe1d5bef04e364a72), X64(0xa3255f1be7dc7ce1),       X64(0x64347d271de22754), X64(0x26c49cccb40811c7),
       X64(0x5cbd6cc0cc10fafc), X64(0x1e4d8d2b65facc6f),       X64(0xd95caf179fc497da), X64(0x9bac4efc362ea149),
       X64(0x158e0a85c2521623), X64(0x577eeb6e6bb820b0),       X64(0x906fc95291867b05), X64(0xd29f28b9386c4d96),
       X64(0xcedba04ad0952342), X64(0x8c2b41a1797f15d1),       X64(0x4b3a639d83414e64), X64(0x09ca82762aab78f7),
       X64(0x87e8c60fded7cf9d), X64(0xc51827e4773df90e),       X64(0x020905d88d03a2bb), X64(0x40f9e43324e99428),
       X64(0x2cffe7d5975e55e2), X64(0x6e0f063e3eb46371),       X64(0xa91e2402c48a38c4), X64(0xebeec5e96d600e57),
       X64(0x65cc8190991cb93d), X64(0x273c607b30f68fae),       X64(0xe02d4247cac8d41b), X64(0xa2dda3ac6322e288),
       X64(0xbe992b5f8bdb8c5c), X64(0xfc69cab42231bacf),       X64(0x3b78e888d80fe17a), X64(0x7988096371e5d7e9),
       X64(0xf7aa4d1a85996083), X64(0xb55aacf12c735610),       X64(0x724b8ecdd64d0da5), X64(0x30bb6f267fa73b36),
       X64(0x4ac29f2a07bfd00d), X64(0x08327ec1ae55e69e),       X64(0xcf235cfd546bbd2b), X64(0x8dd3bd16fd818bb8),
       X64(0x03f1f96f09fd3cd2), X64(0x41011884a0170a41),       X64(0x86103ab85a2951f4), X64(0xc4e0db53f3c36767),
       X64(0xd8a453a01b3a09b3), X64(0x9a54b24bb2d03f20),       X64(0x5d45907748ee6495), X64(0x1fb5719ce1045206),
       X64(0x919735e51578e56c), X64(0xd367d40ebc92d3ff),       X64(0x1476f63246ac884a), X64(0x568617d9ef46bed9),
       X64(0xe085162ab69d5e3c), X64(0xa275f7c11f7768af),       X64(0x6564d5fde549331a), X64(0x279434164ca30589),
       X64(0xa9b6706fb8dfb2e3), X64(0xeb46918411358470),       X64(0x2c57b3b8eb0bdfc5), X64(0x6ea7525342e1e956),
       X64(0x72e3daa0aa188782), X64(0x30133b4b03f2b111),       X64(0xf7021977f9cceaa4), X64(0xb5f2f89c5026dc37),
       X64(0x3bd0bce5a45a6b5d), X64(0x79205d0e0db05dce),       X64(0xbe317f32f78e067b), X64(0xfcc19ed95e6430e8),
       X64(0x86b86ed5267cdbd3), X64(0xc4488f3e8f96ed40),       X64(0x0359ad0275a8b6f5), X64(0x41a94ce9dc428066),
       X64(0xcf8b0890283e370c), X64(0x8d7be97b81d4019f),       X64(0x4a6acb477bea5a2a), X64(0x089a2aacd2006cb9),
       X64(0x14dea25f3af9026d), X64(0x562e43b4931334fe),       X64(0x913f6188692d6f4b), X64(0xd3cf8063c0c759d8),
       X64(0x5dedc41a34bbeeb2), X64(0x1f1d25f19d51d821),       X64(0xd80c07cd676f8394), X64(0x9afce626ce85b507)
};

char* SkipWhitespace(char* ptr)
{
    if (!ptr || !*ptr) return ptr;
    while (IsWhiteSpace(*ptr)) ++ptr;
    return ptr; 
}

void ConvertUnderscores(char* output,bool alternewline,bool removeClasses,bool removeBlanks)
{ 
    char* ptr = output - 1;
	char c;
    while ((c = *++ptr)) 
    {
        if (removeClasses &&  c == '~' && IsAlpha(ptr[1]) ) //   REMOVE underscores AND leading ~ or : in classnames
        {
			if (ptr == output || (*(ptr-1)) == ' ') memcpy(ptr,ptr+1,strlen(ptr)); 
        }
        else if (!removeBlanks &&  c == '_' && ptr[1] != '_') //   REMOVE underscores AND leading ~ or : in classnames
        {
            *ptr = ' ';
			if (ptr[1] == '\'') memcpy(ptr,ptr+1,strlen(ptr)); 
        }
        else if (!removeBlanks && c == '_' && ptr[1] == '_') //   if we want to name a web address with _ in it, double the underscore.
        {
            memcpy(ptr,ptr+1,strlen(ptr)); //   erase extra _
        }
		else if (removeBlanks && c == ' ') *ptr = '_';
		else if (alternewline && (c == '\n' || c == '\r')) *ptr = ' ';
    }
}

void ReviseQuoteToUnderscore(char* start, char* end, char* buffer)
{//   start and end are the marker characters
	char* wordlist[1000];
	unsigned int index;
	*end = 0;	//   remove trailing quote
	Tokenize(start+1,index,wordlist,true);
	for (unsigned int i = 1; i <= index; ++i)
	{
		strcpy(buffer,wordlist[i]);
		buffer += strlen(buffer);
		if (i != index) *buffer++ = '_';
	}
}

char* GetText(char* start, int offset, int length, char* ptr,bool balance)
{
    if (start == NULL) return "";
	char* original = ptr;
    ptr[0] = 0;
    ptr[length-2] = 0;    //   end tells if we run out
    int i = -1;
	char c;
	bool wasWhite = true;
    while ((c = start[offset+ ++i]) && c != ENDUNIT)
    {
 		if (c == '=' && wasWhite) //   this is a comparison pattern, reformat it
		{
			++i;			//   skip over = and accelerator, show just the actual test
			continue;
		}
        *ptr++ = c;
		wasWhite =  (c == ' ');
        if (--length <= 1) break;
        if (c == ')' && balance) break;   
    }
    *ptr = 0;
	if (length <= 1) strcpy(ptr-5,"..."); //   used up almost all, so indicate
	return original;
}

void CopyFile2File(const char* newname,const char* oldname, bool autoNumber)
{
	char name[MAX_WORD_SIZE];
	FILE* out;
	if (autoNumber) //   find next number
	{
		const char* at = strchr(newname,'.');	//   get suffix
		int len = at - newname;
		strncpy(name,newname,len);
		strcpy(name,newname); //   base part
		char* endbase = name + len;
		int j = 0;
		while (++j)
		{
			sprintf(endbase,"%d.%s",j,at+1);
			out = fopen(name,"rb");
			if (out) fclose(out);
			else break;
		}
	}
	else strcpy(name,newname);
	out = fopen(name,"wb");
	if (!out) return;
	FILE* in = fopen(oldname,"rb");
	if (in) while (ReadALine(readBuffer,in)) fprintf(out,"%s\r\n",readBuffer);
	if (out) fclose(out);
	if (in) fclose(in);
}

void StartRemaps()
{
	remapStart = remapIndex ; //   drops all remaps within the topic
}

void ClearRemaps()
{	
	remapIndex = remapStart; //   drops all remaps within the topic
}

static void Remap(char* word)
{
	char old[MAX_WORD_SIZE];
	char suffix[MAX_WORD_SIZE];
	*suffix = 0;
	strcpy(old,word);
	if (*old == '@')
	{
		unsigned int len = strlen(old);
		char* found = strstr(word+len-1,"+");
		if (!found && len > 1) found = strstr(word+len-1,"-");
		if (!found && len > 3) found = strstr(word+len-3,"all");
		if (!found && len > 7) found = strstr(word+len-7,"subject");
		if (!found && len > 4) found = strstr(word+len-4,"verb");
		if (!found && len > 6) found = strstr(word+len-6,"object");
		if (!found && len > 4) found = strstr(word+len-4,"fact");
		if (found) 
		{
			strcpy(suffix,found);
			*found = 0;
		}
	}

	for (int i = 0; i < remapIndex; ++i)
	{
		if (!stricmp(old,remapTo[i])) //   the name is remapto
		{
			strcpy(word,remapFrom[i]); //   the value is remapfrom
			strcat(word,suffix);
			return;
		}
	}
}

static char* incomingPtrSys = 0;			// cache AFTER token find ptr when peeking.
static char lookaheadSys[MAX_WORD_SIZE];	// cache token found when peeking

char* ReadNextSystemToken(FILE* in,char* ptr, char* word, bool separateUnderscore, bool peek)
{ 
#ifdef INFORMATION
The outside can ask for the next real token or merely peek ahead one token. And sometimes the outside
after peeking, decides it wants to back up a real token (passing it to some other processor).

To support backing up a real token, the system must keep the current readBuffer filled with the data that
led to that token (to allow a ptr - strlen(word) backup).

To support peeking, the system may have to read a bunch of lines in to find a token. It is going to need to
track that buffer separately, so when it needs a real token which was the peek, it can both get the peek value
and be using contents of the new buffer thereafter. 

So peeks must never touch the real readBuffer. And real reads must know whether the last token was peeked 
and from which buffer it was peeked.

And, if someone wants to back up to allow the old token to be reread, they have to CANCEL any peek data, so the token
comes from the old buffer. Meanwhile the newbuffer continues to have content for when the old buffer runs out.
#endif
	
	//   get next token
	if (!in && !ptr) // clear cache request, next get will be from main buffer (though secondary buffer may still have peek read data)
	{
		incomingPtrSys = NULL; // no longer holding a PEEK value.
		return NULL;
	}

	char* result;
	if (incomingPtrSys) //  had a prior PEEK, now in cache. use up cached value, unless duplicate peeking
	{
		result = incomingPtrSys; // caller who is peeking will likely ignore this
		if (!peek)
		{
			// he wants reality now...
			if (*newBuffer) // prior peek was from this buffer, make it real data in real buffer
			{
				strcpy(readBuffer,newBuffer);
				result = (result - newBuffer) + readBuffer; // adjust pointer to current buffer
				*newBuffer = 0;
			}
			strcpy(word,lookaheadSys);
			incomingPtrSys = 0;
		}
		else 
		{
			strcpy(word,lookaheadSys); // duplicate peek
			result = (char*)1;	// NO ONE SHOULD KEEP A PEEKed PTR
		}

		return result;
	}

	*word = 0;
	if (ptr) result = ReadSystemToken(ptr,word,separateUnderscore);
	bool newline = false;
	while (!*word)
	{
		if (!newline && *newBuffer) // use pre-read buffer per normal, it will have a token
		{
			result = ReadSystemToken(newBuffer,word,separateUnderscore);
			break;
		}
		else if (!in || !ReadALine(newBuffer,in)) return NULL; //   end of file
		result = ReadSystemToken(newBuffer,word,separateUnderscore);
		newline = true;
	}

	if (peek) //   save request - newBuffer has implied newline if any
	{
		incomingPtrSys = result; 
		strcpy(lookaheadSys,word);
		result = (char*)1;	// NO ONE SHOULD KEEP A PEEKed PTR
	}
	else if (newline) // live token, adjust pointers and buffers to be fully up to date
	{
		strcpy(readBuffer,newBuffer);
		result = (result - newBuffer) + readBuffer;
		*newBuffer = 0;
	}

	return result;
}

char* FindComparison(char* word)
{
	if (!*word || !word[1] || !word[2]) return NULL; //   if token is short, we cannot do the below word+1 scans 

	char* at = strchr(word+1,'!'); 
	if (!at) at = strchr(word+1,'<');
	if (!at) at = strchr(word+1,'>');
	if (!at) at = strchr(word+1,'&'); 
	if (!at) at = strchr(word+1,'=');
	if (!at) at = strchr(word+1,'?');  //   member of set
	return at;
}

static void InsureAppropriateCase(char* word)
{
	char c;
	char* at = FindComparison(word);
	//   force to lower case various standard things
	//   topcs/sets/classes/user vars/ functions and function vars  are always lower case
 	if (at) //   a comparison has 2 sides
	{
		c = *at;
		*at = 0;
		InsureAppropriateCase(word);
		if (at[1] == '=') InsureAppropriateCase(at+2); // == or >= or such
		else InsureAppropriateCase(at+1);
		*at = c;
	}
	else if (*word == '_' || *word == '\'') InsureAppropriateCase(word+1);
	else if ((*word == '^' && word[1] != '"') || *word == '~' || *word == '$' || *word == '%' || *word == '|' ) MakeLowerCase(word);	
	else if (*word == '@' && IsDigit(word[1])) MakeLowerCase(word);	//   potential factref like @2subject
}

static bool TrailingPunctuation(char c)
{
	return  c == '.' ||  c == '?' || c == '!'  || c == ',' || c== '"';
}

static int GetFunctionArgument(char* arg) //   get index of argument if it is value, else -1
{
	for (int i = 0; i < functionArgumentCount; ++i)
	{
		if (!stricmp(arg,functionArguments[i])) return i;
	}
	return -1; //   failed
}

char* ReadSystemToken(char* ptr, char* word, bool separateUnderscore) //   how we tokenize system stuff (rules and topic system) words -remaps & to AND
{
    *word = 0;
    if (!ptr)  return 0;
    char* original = word;
    ptr = SkipWhitespace(ptr);
	char c = *ptr;

	//   see if special tokenization for starting delimiters
	if (nestingData[c])  //   any of { } [ ] () 
	{
       *word++ = c;  //   add char to word
 		ptr++;
		c = 0;	//   suppress the loop
	}
	else if (c == '!' && ptr[1] != '=')
	{
       *word++ = c;  //   add char to word
 		ptr++;
		c = 0;	//   suppress the loop
	}
	else if (c == '\'' && (ptr[1] != 's' || ptr[2] ))  //   ' becomes single token unless its 's 
	{
       *word++ = c;  //   add char to word
 		ptr++;
		c = 0;	//   suppress the loop
	}
	//   test for ordering error _! instead of !_  in prefix tokens
	else if (c == '_' &&  ptr[1] == '!')
	{
		BADSCRIPT("error _! wrong ordering");
	}
	//   special _ prefix sometimes not a prefix when it refers to a wildcard reference
	else if (c == '_' && !IsDigit(ptr[1])  && separateUnderscore) //   _xxx should be separated, but _3 is a wildcard reference token 
	{
       *word++ = c;  //   add char to word
 		ptr++;
		c = 0;	//   suppress the loop
	}
	else if (c == '"' || ( c == '^' && ptr[1] == '"')) //   doublequote maybe with functional heading
	{
		if (c == '^' )  // ^"this is script" 
		{
			*word++ = c;
			++ptr;
		}

		char* end = ReadQuote(ptr,word);	//   swallows ending marker and points past
		if (end) //   if we did swallow a quoted string
		{
			if (!*word) return end;	//   null string swallowed
			ptr = end;
			word += strlen(word);	//   point to new end
			c = 0;	//   suppress loop
		}
	}
	//   leading backslash protects next token and stays in the token
	else if (c == '\\') //   leading backslash protects next token, return in token - only allows single character tokens
	{
		*word++ = c; 
		++ptr;
		*word++ = *ptr++;
		c = 0;	//   suppress loop
	}

	int parenSwallow = 0;
    while (c && !IsWhiteSpace(c) && c != '`') 
    {
		if (nestingData[c] > 0 ) ++parenSwallow;
	
		// normally break apart... but NOT if immediately preceeding or following is _ as part of a long name
		if (!nestingData[c]); // not a paren type unit
		else if (*(word-1) == '_'); // merge into word
		else if (nestingData[c] > 0 ) break;	// not protected, break
		else if (parenSwallow == 0 ) // closing - may not have _ before it, but we accepted opening, so accept closing
		{
			break; 
		}
	
		if (nestingData[c] < 0 ) --parenSwallow;

		//   Unlike user input, patterns allow  ~number>5   ~number<5   ~set1=foo  %sentenceflags&#INSULTBIT  _&|verbhowphrase
		*word++ = c;  //   add char to word
        c = *++ptr;
	}
    *word = 0;
    word = original;
	unsigned int len = strlen(word);

	if (*word == '#' && IsAlpha(word[1])) // is this a defined number? use it instead
	{
		uint64 n = FindValueByName(word+1);
		if (n) sprintf(word,"%lld",n);
	}
		
	//   special comments and defines get passed along
	// pass long #! and #ai
	if (*word == '#' && !IsDigit(word[1]) && (word[1] != '!' && stricmp(word,"#ai")) && stricmp(word,"#define")) //treat rest as a comment line (except if has number after it, which is user text OR internal arg reference for function
	{
		*ptr = 0;
		*word = 0;
	}

	//   break off stuff from << like <<hello 
	if (*word == '<' && word[1] == '<' && word[2])
	{
		ptr -= strlen(word) - 2;
		word[2] = 0;
	}

	//   break off stuff from >> like hello>>
	if (len > 2 && word[len-1] == '>' && word[len-2] == '>')
	{
		ptr -= 2;
		word[len-2] = 0;
	}

	// break off stuff from user var (name must be alpha numberic or _ 
	if (word[0] == '$' && (word[1] == '$' || IsAlpha(word[1]) || word[1] == '_'))
	{
		while ( !IsAlpha(word[len-1]) && !IsDigit(word[len-1]))
		{
			--len;
			--ptr;
		}
	}

    //   break off stuff from wildcards but not tests
    if (*word == '_' && IsDigit(word[1] ) && !strchr(word,'=' ) && !strchr(word,'?' ) && !strchr(word,'<') && !strchr(word,'>'))
    {
        ptr -= len - 2;
		word[2] = 0;
    }
    if (*word == '"' && word[1] == '_' && IsDigit(word[2]) && !word[3]) //   wildcard after double quote
    {
        ptr -= len - 1;
        word[1] = 0;
    }
    //   fix punctuation joined to simple *
    if (*word == '_' && TrailingPunctuation(word[1])) 
    {
        --ptr;
        word[1] = 0;
    }

	//   !:  ?:  a: ':  
	if (*word  &&  word[1] == ':' &&  word[0] != ' ' && word[0] != '_' && word[0] != '=' && word[0] != '\'' && word[0] != '[' 
		&& word[0] != '(' && word[0] != '!' && word[0] != '?' && !IsAlpha(word[0]) && !IsDigit(word[0])) 
	{
		BADSCRIPT("bad token0  - %s",word);
	}
	//   *~  ~~  !~  _~ '~  =xxx= for testing
	if (*word  && word[1] == '~' && word[0] != '~' && word[0] != '^' && word[0] != '=' && word[0] != '_' && word[0] != '\'' && word[0] != '*'  && word[0] != ' ' && word[0] != '['  && word[0] != '('&& word[0] != '!') 
	{
		BADSCRIPT("bad token1  - %s",word);
	}

	//   handle forms using ! (not) - which cannot save any data- BUT !_# is a comparitor, and is legal as is #! which is a special comment
	if (*word  && *word != '\\' &&  word[1] == '!' && *word != '#')  BADSCRIPT("! should never be second, only first") //   \! means exclamation
    else if (strchr(word,'`'))  BADSCRIPT("Invalid ` in data %s",word)

	char* at = FindComparison(word);
	if (at && *word == '*' && !IsDigit(word[1])) BADSCRIPT("Cannot do comparison on variable gap %s . Memorize and compare against _# instead later. ",word)

	//   user defined remap for _ and @ things?  simple remap
	if ((*word == '@' || *word == '_' )  && IsAlpha(word[1])) Remap(word); 

	if (at) //   check for remaps on right side of comparisons
	{
		++at;
		if ((*at == '@' || *at == '_' ) && IsAlpha(at[1])) Remap(at); 
	}

	if (*word == '^' && word[1])
	{
		int index = GetFunctionArgument(word);
		if (index >= 0) sprintf(word,"^%d",index);
	}

	if (!stricmp(word,"define:")) //   accept remap line for _ and @
	{
		if (remapIndex >= MAX_REMAPS) BADSCRIPT("limited to %d remaps.",MAX_REMAPS);
		ptr = ReadSystemToken(ptr,remapTo[remapIndex],false);
		ptr = ReadSystemToken(ptr,remapFrom[remapIndex],false);
		if (atoi(remapFrom[remapIndex]+1) > 9) BADSCRIPT("remapped item index has max 9 limit - %s",readBuffer);
		if (*remapTo[remapIndex] != '@' && *remapTo[remapIndex] != '_') BADSCRIPT("Illegal remap %s - only allowed to remap _# and @# items",readBuffer)
		else if (!IsDigit(remapFrom[remapIndex][1])) BADSCRIPT("Illegal remap %s - 2nd arg must be numeric reference",readBuffer)
		else ++remapIndex;
		ptr = ReadSystemToken(ptr,word);
	}

	InsureAppropriateCase(word);
	return ptr; 
}

void JumpBack()
{
	if (jumpIndex < 0) return;	// not under handler control
	globalDepth = 0;
	longjmp(scriptJump[jumpIndex], 1);
}
