#ifndef _EXECUTE
#define _EXECUTE


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

#include "common.h"

typedef unsigned int (*EXECUTEPTR)(char* buffer);

#define OWNTRACE 1
#define NEWLINE_RESULT 2

typedef struct SystemFunctionInfo 
{
	const char* word;					//   dictionary word entry
	EXECUTEPTR fn;				//   function to use to get it
	int argumentCount;			//   how many macroArgumentList it takes
	int	properties;				//   non-zero means does its own argument tracing
	const char* comment;
} SystemFunctionInfo;

//   special argeval codes
#define VARIABLE_ARGS -1
#define STREAM_ARG -2

//   DoFunction results
#define ENDRULE_BIT 1		//   stop executing rule
#define FAILRULE_BIT 2		//   fail rule
#define ENDTOPIC_BIT 4		//   stop executing topic
#define FAILTOPIC_BIT 8		//   fail topic
#define ENDSENTENCE_BIT 16	//   stop executing sentence
#define FAILSENTENCE_BIT 32	//   fail sentence 
#define ENDINPUT_BIT 64		//   stop executing input 
#define RETRY_BIT 128           //   do rule again
#define SUCCESSFULE_BIT 256		//   claim an answer has been given, even if buffer empty - internal, not for user to issue
#define UNDEFINED_FUNCTION 512	//   potential function call has no definition so isnt
#define ENDCODES (FAILTOPIC_BIT|FAILRULE_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDTOPIC_BIT|ENDRULE_BIT|ENDSENTENCE_BIT)

//   argument data for system calls
extern int systemCallArgumentIndex;
extern int systemCallArgumentBase;

#define MAX_ARG_LIST 100
extern char systemArgumentList[MAX_ARG_LIST][MAX_WORD_SIZE];
extern char lastInputSubstitution[MAX_BUFFER_SIZE];
extern int globalDepth;

//   argument data for user calls
extern char macroArgumentList[MAX_ARGUMENT_COUNT+1][MAX_WORD_SIZE];    //   function macroArgumentList
extern unsigned int userCallArgumentIndex;
extern unsigned int userCallArgumentBase;

#define ARGUMENT(n) systemArgumentList[systemCallArgumentBase+n]
extern int executingID;
extern char* executingRule;		//   rule we are processing
extern SystemFunctionInfo systemFunctionSet[];
void DumpFunctions();
char* DoFunction(char* name, char* ptr, char* buffer,unsigned int &result);
void InitFunctionSystem(); 
WORDP FindCommand(char* word);
void AddInput(char* buffer);
#endif
