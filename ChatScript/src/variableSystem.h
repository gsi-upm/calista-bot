#ifndef _VARIABLESYSTEMH_
#define _VARIABLESYSTEMH_
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

#define WILDINDEX 1	//   _2 formations
#define MAX_WILDCARDS 9  // _0 ... _9 inclusive

#define ALREADY_HANDLED -1

extern unsigned int wildcardIndex;
extern char wildcardOriginalText[MAX_WILDCARDS+1][MAX_WORD_SIZE];  //   spot wild cards can be stored
extern char wildcardCanonicalText[MAX_WILDCARDS+1][MAX_WORD_SIZE];  //   spot wild cards can be stored
extern unsigned int wildcardPosition[MAX_WILDCARDS+1]; //   spot it started in sentence
extern int impliedSet;
extern int impliedWild;

void ClearBaseVariables();
void DumpVariables();
void ReestablishBaseVariables();
void SetBaseVariables();
void ShowChangedVariables();
void ClearVariableSetFlags();
void WriteVariables(FILE* out);
void ClearUserVariables();
char* GetwildcardOriginalText(unsigned int i, bool canon);
void SetWildCard(char* value,const char* canonicalVale,const char* index,unsigned int position);
void SetWildCard(unsigned int start, unsigned int end);
void SetWildCardStart(unsigned int);
char* GetUserVariable(const char* word);
void SetUserVariable(const char* var, char* word);
void AddVar(char* var, char* word,char minusflag);
char* PerformAssignment(char* word,char* ptr,unsigned int& result);
#endif
