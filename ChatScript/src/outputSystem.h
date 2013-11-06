#ifndef _OUTPUTPROCESSH_
#define _OUTPUTPROCESSH_
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

typedef enum {
    OUTPUT_ONCE = 1,
	OUTPUT_KEEPSET = 2, //   dont expand class or set
	OUTPUT_KEEPVAR = 4, //   dont expand a var
	OUTPUT_KEEPQUERYSET= 8, //   dont expand a fact var
	OUTPUT_SILENT = 16,		// dont show stuff if trace is on
	OUTPUT_EVALCODE = 32,	// process code strings
} OutputFlags;

#define CHOICE_LIMIT 100
extern char tmpword[MAX_WORD_SIZE];

extern int lastWildcardAccessed;
extern char* currentOutputBase;
extern char* currentPatternBase;
void StdNumber(char* word,char* buffer,int controls);
char* StdNumber(int n);

char* Output(char* ptr,char* buffer,unsigned int &result,int controls = 0);
char* FreshOutput(char* ptr,char* buffer,unsigned int &result,int controls = 0);
void ReformatString(char* word);
char* DoLoop(char* base,char* ptr,char* buffer);
int CountParens(char* start);
char* ReadCommandArg(char* ptr, char* answer,unsigned int &result);

#endif
