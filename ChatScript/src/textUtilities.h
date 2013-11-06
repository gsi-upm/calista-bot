#ifndef _TEXTUTILITIESH_
#define _TEXTUTILITIESH_

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


		//   Level 1:  show input/ key analysis / output  1
		//   Level 2:  show pattern match 
		//   Level 3:  show output trace
		//   Level 4:  show pattern trace
		//   level 5   show infer trace
#define BASIC_TRACE 1	
#define PREPARE_TRACE 2
#define MATCH_TRACE 4
#define OUTPUT_TRACE 8
#define PATTERN_TRACE 16
#define INFER_TRACE 32
#define SUBSTITUTE_TRACE 64
#define FACTCREATE_TRACE 128
#define VARIABLE_TRACE 256
#define HIERARCHY_TRACE 512

#define NO_TRACE 256

#define SERVERLOG 0
#define STDUSERLOG 1
#define BADSCRIPTLOG 9
#define BUGLOG 10
#define STDUSERTABLOG 101
#define STDUSERATTNLOG 201


#define SPACES 1			//   \t \r \n 
#define PUNCTUATIONS 2      //    , | -  (see also ENDERS )
#define ENDERS	4			//   . ; : ? ! -
#define BRACKETS 8			//   () [ ] { } < >
#define ARITHMETICS 16		//    % * + - ^ = / .  (but / can be part of a word)
#define SYMBOLS 32			//    $ # @ ~  ($ and # can preceed, & is stand alone)
#define CONVERTERS 64		//   & `
#define QUOTERS 128			//   " ' *
//NORMALS 			//   A-Z a-z 0-9 _ 

#define UNINIT -1
typedef struct NUMBERDECODE
{
    const char* word;				//   word of a number
    int value;				//   value of word
	unsigned int length;	//   length of word
} NUMBERDECODE;

#define SHOUT 1
#define ABBREVIATION 2
#define BADSCRIPT(...) {++hasErrors; Log(STDUSERLOG,"\r\n *** Error- "); Log(BADSCRIPTLOG, __VA_ARGS__); JumpBack();}

#define NOT_A_NUMBER 2147483646
extern bool cardinalNumber;
extern bool echoServer;

// accesses to these arrays MUST use unsigned char in the face of UTF8 strings
extern unsigned char punctuation[];
extern unsigned char toLowercaseData[];
extern unsigned char toUppercaseData[];
extern unsigned char isVowelData[];
extern unsigned char isAlphabeticDigitData[];
extern unsigned char isComparatorData[];
#define IsWhiteSpace(c) (punctuation[(unsigned char)c] == SPACES)
#define IsVowel(c) (isVowelData[(unsigned char)c] != 0)
#define IsAlphabeticDigitNumeric(c) (isAlphabeticDigitData[(unsigned char)c] != 0)
#define IsUpperCase(c) (isAlphabeticDigitData[(unsigned char)c] == 3)
#define IsLowerCase(c) (isAlphabeticDigitData[(unsigned char)c] == 4)
#define IsAlpha(c) (isAlphabeticDigitData[(unsigned char)c] >= 3)
#define IsDigit(c) (isAlphabeticDigitData[(unsigned char)c] == 2)
#define IsAlphaOrDigit(c) (isAlphabeticDigitData[(unsigned char)c] >= 2)
#define IsNonDigitNumberStarter(c) (isAlphabeticDigitData[(unsigned char)c] == 1)
#define IsNumberStarter(c) (isAlphabeticDigitData[(unsigned char)c] && isAlphabeticDigitData[(unsigned char)c] <= 2)
#define IsComparison(c) (isComparatorData[(unsigned char)c])

extern signed char nestingData[];
extern unsigned int burstLimit;
extern unsigned int tokenLength; //   size ReadALine returns
extern unsigned int randIndex;
extern bool echo;
extern unsigned int hasErrors;
extern bool showBadUTF;
extern char functionArguments[MAX_ARGUMENT_COUNT+1][500];
extern int functionArgumentCount;
extern char serverLogfileName[200];
#define MAXRAND 256

extern bool logUpdated;
extern unsigned int buildID; // build 0 or build 1

extern int currentFileLine;
extern char currentFileName[MAX_WORD_SIZE];
extern  int tokenFlags;  
extern char logbuffer[MAX_WORD_SIZE];

extern char* wordStarts[MAX_SENTENCE_LENGTH];		//   current sentence tokenization
extern unsigned int wordCount;			//   words in current tokenization
extern bool isQuestion;
extern unsigned int responseIndex;
extern unsigned int sentenceCount;  
extern char traceTopic[MAX_WORD_SIZE];
extern char newBuffer[MAX_BUFFER_SIZE];
void ResetWarnings();
void DumpWarnings();
char* ReadNextSystemToken(FILE* in,char* ptr, char* word, bool separateUnderscore=true,bool peek=false);
char* ReadSystemToken(char* ptr, char* word, bool separateUnderscore=true);
char* FindComparison(char* word);

//   basic word tests
void ClearRemaps();
void StartRemaps();
bool IsDigitNumeric(char* word);
bool IsNumber(char* word,bool placeAllowed = true);
bool IsPlaceNumber(char* word);
bool IsDigitWord(char* word);
bool IsUrl(char* word, char* end);
unsigned int IsMadeOfInitials(char * word,char* end);
bool IsNumericDate(char* word,char* end);
bool IsFloat(char* word, char* end);
bool MatchesPattern(char* word, char* pattern);
void ReviseQuoteToUnderscore(char* start, char* end, char* buffer);
void MakeLowerCase(char* ptr);
void MakeUpperCase(char* ptr);
void MakeLowerCopy(char* to,char* from);
void UpcaseStarters(char* ptr);
void ConvertUnderscores(char* buffer,bool upcase,bool removeClasses=true,bool removeBlanks=false);
char* GetText(char* start, int offset, int length, char* spot,bool balance);
char* NLSafe(char* line);
void JumpBack();

char* ReadHex(char* ptr, uint64 & value);
char* ReadInt(char* ptr, unsigned int & value);
char* ReadInt64(char* ptr, uint64 & w);
char* ReadQuote(char* ptr, char* buffer);
char* ReadArgument(char* ptr, char* buffer);
char* ReadCompiledWord(char* ptr, char* word);
char* ReadALine(char* buf,FILE* file,unsigned int limit = MAX_BUFFER_SIZE);
char* SkipWhitespace(char* ptr);

char* JoinWords(unsigned int n,bool output = false);
int BurstWord(char* word, int contractionStyle = 0);
char* GetBurstWord(unsigned int n);
char* BalanceParen(char* ptr,bool within=true);
unsigned int random(unsigned int range);
void CopyFile2File(const char* newname,const char* oldname,bool autoNumber);
int NumberPower(char* number);
int Convert2Integer(char* word);
unsigned int Log(unsigned int spot,const char * fmt, ...);
bool TraceIt(int how);

uint64 Hashit(unsigned char * data, int len, bool & hasUpperCharacters);

#endif
