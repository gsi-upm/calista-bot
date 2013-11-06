#ifndef _SCRIPTCOMPILEH_
#define _SCRIPTCOMPILEH_
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


#ifndef NOSCRIPTCOMPILER

extern bool patternContext;
extern bool bCallBackScriptCompiler;
extern WORDP currentFunctionDefinition;
void CallBackScriptCompiler(char* buffer);
  
void SetJumpOffsets(char* data);
void AIGenerate(char* filename);

FILE* UTF8Open(char* filename,char* mode);

char* ReadOutput(char* ptr, FILE* in,char* &data,char* rejoinders);
bool ReadTopicFiles(char* name,unsigned int zone, FACT* base,int spell);
char* ReadPattern(char* ptr, FILE* in, char* &data,bool macro);
char* CompileString(char* ptr);
#endif


#endif
