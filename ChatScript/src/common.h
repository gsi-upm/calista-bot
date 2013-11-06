#ifndef _MAXH_
#define _MAXH_

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

typedef unsigned long long int  uint64;

//#define NOSERVER 1
//#define NOSCRIPTCOMPILER 1
//#define NOTESTING 1
//#define SMALL 1

#ifdef WIN32
#pragma comment(lib, "ws2_32.lib") 
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_WARNINGS 1
#define _CRT_SECURE_NO_DEPRECATE
#define  _CRT_NONSTDC_NO_DEPRECATE
#pragma warning( disable : 4996  ) 
  typedef long long_t;       
  typedef unsigned long ulong_t; 
  #pragma warning(push,1)
  #include <direct.h>
  #include <windows.h>
  #include <process.h>
	#include <fcntl.h>
	#include <io.h>
	#include <stdio.h>

#elif __MACH__
  #define stricmp strcasecmp 
  #define strnicmp strncasecmp 

  typedef long long long_t; 
  typedef unsigned long long ulong_t; 
  #include <sys/time.h> 
  #include <mach/clock.h>
  #include <mach/mach.h>
#elif IOS
  #define stricmp strcasecmp 
  #define strnicmp strncasecmp 

  typedef long long long_t; 
  typedef unsigned long long ulong_t; 
  #include <sys/time.h> 
#else
  #define stricmp strcasecmp 
  #define strnicmp strncasecmp 

  typedef long long long_t; 
  typedef unsigned long long ulong_t; 
  #include <sys/time.h> 
#endif

#ifdef __MACH__
#include <sys/malloc.h>
#else
#include <malloc.h>
#endif

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <ctype.h>
#include <iostream>
#include <map>
#include <memory.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <sys/timeb.h> 
#include <time.h>
#include <utility>
#include <vector>

#include <string>            //   For string
#include <exception>         //   For exception class
using namespace std;
typedef std::map <char*, char*> WordMap;
typedef std::map <int, uint64> Bit64Map;

#ifdef WIN32
  #pragma warning( pop )
#endif

#undef WORDP //   remove windows version (unsigned short) for ours

#define MAX_SENTENCE_LENGTH 255 //   actally max sentence size limited to this
#define MAX_ARGUMENT_COUNT 500

#include "mainSystem.h"
#include "constructCode.h"
#include "csocket.h"
#include "dictionarySystem.h"
#include "factSystem.h"
#include "functionExecute.h"
#include "infer.h"
#include "markSystem.h"
#include "outputSystem.h"
#include "patternSystem.h"
#include "readrawdata.h"
#include "scriptCompile.h"
#include "spellcheck.h"
#include "systemVariables.h"
#include "testing.h"
#include "textUtilities.h"
#include "tokenSystem.h"
#include "topicSystem.h"
#include "userSystem.h"
#include "variableSystem.h"
#include "tagger.h"

#endif
