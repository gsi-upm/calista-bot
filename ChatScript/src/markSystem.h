#ifndef _MARKSYSTEMH_
#define _MARKSYSTEMH_
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

// phrase start marker
#define PHRASE_START 0x00003f       //   mask for start of phrase (1-31)  6 bits
#define PHRASE_MAX_START_OFFSET 63  //   6 bits for start value
#define PHRASE_END_OFFSET 6       //   start of end data past start marker
// phrase end offset
#define PHRASE_END_MAX 0x0007     //   max range of width of phrase,  at max value means -1 (a string) 

#define PHRASE_BITS 9               //   bit size of total phrase data field
#define PHRASE_FIELD 0x0001ff       //   mask for total data field (9 bits per entry) in 64 bits = 7 entries allowed
#define PHRASE_STORAGE_BITS 64
#define PHRASE_LAST_FIELD  0xffC0000000000000ULL // bits the last field would have occupied

#define MAX_TOPICS_IN_SENTENCE 1000
extern char* topicsInSentence[MAX_TOPICS_IN_SENTENCE+1];
extern unsigned int topicInSentenceIndex;
extern char respondLevel;
extern bool posTrace;
extern unsigned int matchStamp;
extern  WORDP canonicalLower[MAX_SENTENCE_LENGTH];
extern WORDP originalLower[MAX_SENTENCE_LENGTH];
extern uint64 originalFlags[MAX_SENTENCE_LENGTH];
extern unsigned int roles[MAX_SENTENCE_LENGTH];
extern char* wordCanonical[MAX_SENTENCE_LENGTH];	//   canonical form of word 
extern bool strict;
extern Bit64Map whereInSentence;
extern WORDP canonicalUpper[MAX_SENTENCE_LENGTH];
char* GetNounPhrase(unsigned int i,const char* avoid);
uint64 GetPosData(char* original,WORDP &entry,WORDP &canonical,bool firstTry = true,uint64 bits = -1) ;
unsigned int GetNextSpot(WORDP D,int start,unsigned int& positionStart,unsigned int& positionEnd);
unsigned int GetNthNextSpot(WORDP D,unsigned int start);
unsigned int GetIthSpot(WORDP D,int i);
void POSTag(bool trace);
void MarkWordHit(WORDP D, unsigned int start,unsigned int end);
void MarkFacts(MEANING M,unsigned int start,unsigned int end);
void NextMatchStamp();
void MarkAllImpliedWords();
void SetWhereInSentence(WORDP D,uint64 bits);
void ClearWhereInSentence();
void DumpPronouns();
uint64 GetWhereInSentence(WORDP D);
bool AllowedMember(FACT* F, unsigned int i,unsigned int is,unsigned int index);
char* DumpAnalysis(uint64 values[MAX_SENTENCE_LENGTH],const char* label,bool original);
#endif
