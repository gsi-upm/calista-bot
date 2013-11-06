#ifndef _TESTINGH
#define _TESTINGH
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

#ifndef NOTESTING

typedef void (*COMMANDPTR)(char* input);

typedef struct CommandInfo 
{
	const char* word;					//   dictionary word entry
	COMMANDPTR fn;				//   function to use to get it
	bool resetEcho;	
	const char* comment;
} CommandInfo;

extern CommandInfo commandSet[];

extern unsigned int basestamp;
extern bool wasCommand;

void InitCommandSystem();
void DrawDownHierarchy(MEANING T,unsigned int depth,unsigned int limit,bool kid,bool sets);
MEANING FindWordNetParent(MEANING who,int n);
int CountSet(WORDP D);
void RegressSubstitutes();
void VerifyAccess(char* topic,char kind);
void VerifyAllTopics(char kind);
void DisplayTables();
int CountDown(MEANING T,int all,int depth);
void DisplayTopic(char* name);
void DumpSetHierarchy(WORDP D,unsigned int depth,int all);
void ValidateTopicVocabulary(char* name);
void TestPattern(char* input);
bool Command(char* input);

#endif
#endif
