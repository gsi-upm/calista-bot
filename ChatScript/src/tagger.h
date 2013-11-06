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

#ifndef __TAGGER__
#define __TAGGER__
extern unsigned char phrases[MAX_SENTENCE_LENGTH];
extern int bitCounts[MAX_SENTENCE_LENGTH]; 
extern unsigned char clauses[MAX_SENTENCE_LENGTH];
extern unsigned char verbals[MAX_SENTENCE_LENGTH];
extern unsigned char coordinates[MAX_SENTENCE_LENGTH];
extern uint64 values[MAX_SENTENCE_LENGTH];	
void C_Posit(char* word1);
void C_Posit1(char* word1);
void TagTest(char* file);
int BitCount (uint64 n);
void ReadPosPatterns(char* filename);
void DumpPrepositionalPhrases();
void TagIt(bool trace);
void ReadPosPatterns(const char* file);
#endif
