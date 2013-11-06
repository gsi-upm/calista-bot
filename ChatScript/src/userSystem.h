#ifndef _USERSYSTEMH
#define _USERSYSTEMH
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

extern char computerID[MAX_WORD_SIZE];
extern char computerIDwSpace[MAX_WORD_SIZE];
extern char loginID[MAX_WORD_SIZE]; // lower cased
extern char loginName[MAX_WORD_SIZE]; // what user typed
extern char callerIP[100];
extern int userFirstLine;
extern char priorMessage[MAX_MESSAGE];
void WriteUserData(char* said);
void ReadUserData();
void ReadNewUser();

void Add2Blacklist(char* ip,unsigned int val);
bool Blacklisted(char* ip,char* caller);
void ReadComputerID();
void StartConversation(char* buffer);
void Login(char* ptr,char* callee,char* ip);
void PartialLogin(char* caller,char* ip);

#endif
