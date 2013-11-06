#ifndef MAINSYSTEMH
#define MAINSYSTEMH
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

#define MAX_SENTENCE_BYTES 4000
typedef struct RESPONSE
{
	//   above can be asked about
    unsigned int responseInputIndex;                        //   which input sentence does this stem from?
    char response[MAX_SENTENCE_BYTES];                       //   answer sentences, 1 or more per input line
} RESPONSE;

#define ReportBug(...) Log(BUGLOG, __VA_ARGS__) 
#define MAX_RESPONSE_SENTENCES 50
#ifndef WIN32
#define MAX_BUFFER_SIZE 80000
#elif IOS
#define MAX_BUFFER_SIZE 80000
#else
#define MAX_BUFFER_SIZE 80000
#endif
#define SERVERTRANSERSIZE 20000
#define BIG_WORD_SIZE 10000
#define MAX_WORD_SIZE 1000       
#define MAX_MESSAGE 2000

#define SKIPWEEKDAY 4 // from gettimeinfo

#define OKTEST 1
#define WHYTEST 2
extern unsigned int repeatable;
extern unsigned int tokenControl;
extern bool moreToCome,moreToComeQuestion;
extern unsigned int trace,noreact;
extern int serverLog;
extern bool unusedRejoinder;
extern RESPONSE responseData[MAX_RESPONSE_SENTENCES+1];
extern char* currentInput;
extern int regression;
extern int errors;
extern unsigned int inputCount;
extern char readBuffer[MAX_BUFFER_SIZE];
extern bool systemReset;
extern FILE* sourceFile;
extern char revertBuffer[MAX_BUFFER_SIZE];
extern char* version;
extern int prepareMode;
extern bool sandbox;
extern unsigned int echoSource;
extern bool all;

#define MAIN_RECOVERY 4
extern jmp_buf scriptJump[5];
extern int jumpIndex;

#define MAX_USED 40
extern char chatbotSaid[MAX_USED+1][MAX_MESSAGE]; //   last n messages sent to user
extern char humanSaid[MAX_USED+1][MAX_MESSAGE]; //   last n messages read from human
extern unsigned int humanSaidIndex;
extern unsigned int chatbotSaidIndex;
extern jmp_buf scriptJump[5];
extern int userLog;
extern int jumpIndex;
extern unsigned int year;
extern unsigned int month; //   1 = jan
extern unsigned int dayOfWeek; //   1 = mon
extern unsigned int day; //   date 17th
extern unsigned int week;
extern unsigned int hour; //   24 hour time
extern unsigned int minute; 
extern unsigned int second;
extern bool leapYear;
extern unsigned int dayInYear;
extern unsigned int weekInYear; //   1st 7 days are week 1 (1 based)
extern unsigned int globalMonths;
extern unsigned int globalWeeks;
extern unsigned int globalDays; //   lie- we dont care about prior leap years for this
extern unsigned int globalYears;
extern unsigned int globalDecades;
extern unsigned int globalCenturies;
extern unsigned int globalMillenia;
extern char repeatedOutput[MAX_WORD_SIZE];
extern bool server;
extern int oktest;
extern bool debug;
extern int inputCounter,totalCounter;
extern char* nextInput;
void SaveResponse(char* msg);
char* GetTimeInfo();
char* ConcatResult();
void Reload();
int main(int argc, char * argv[]);
void myexit(int x);
void Pause();
char* AllocateBuffer();
void FreeBuffer();
bool ProcessInput(char* input);
char* DoSentence(char* input);
void PerformChat(char* user, char* usee, char* incoming,char* ip,char* output);
bool DoCommand(char* input);
void MainLoop();
void ResetSentence();
void ResetBuffers();
void CreateSystem();
int Reply();
void ResetResponses();
void OnceCode(const char* which);
void AddUsed(const char* reply);
bool HasAlreadySaid(char* msg);
bool AddResponse(char* msg);
void GetCurrentDateInfo();
char* PrepareSentence(char* input,bool mark = true,bool user=true);
void SetSource(char* name);
void InitSystem(int argc, char * argv[]);
#endif
