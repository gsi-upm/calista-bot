#ifndef _INFERH_
#define _INFERH_
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

#define MAX_FIND 5000
#define MAX_FIND_SETS 10

extern unsigned int foundIndex;
extern FACT* factSet[MAX_FIND_SETS][MAX_FIND+1];
extern unsigned int inferMark;
unsigned int NextinferMark();
unsigned int Query(char* search, char* subject, char* verbword, char* object, unsigned int count, char* from, char* toset, char* up, char* match);
void Sort(char* set);
unsigned int QueryTopicsOf(char* word,unsigned int store,char* kind);
unsigned int HierarchyIntersect(char* store,char* from, char* toset);
unsigned int TopicIntersect(char* from, char* to);
void ScanHierarchy(MEANING T,int saveMark,unsigned int flowmark,bool up,int flag,unsigned int type);
#endif
