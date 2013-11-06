#ifndef _TOPICSYSTEMH
#define _TOPICSYSTEMH

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

#define NO_REJOINDER -1 //   Lastquestion is this does not exist

//   kinds of responders
#define QUESTION '?'
#define STATEMENT 's'
#define STATEMENT_QUESTION 'u'
#define RANDOM_GAMBIT   'r' 
#define GAMBIT   't'

#define ENDUNIT '`'

// jump accelerators in topics are 3 bytes, using these
//#define SMALL_JUMPS 1
#define USED_CODES 75


#ifdef SMALL_JUMPS
#define ENDUNITTEXT "`00 "
#define JUMP_OFFSET 3   //   immediately after any ` are 3 characters of jump info, then either  "space letter :" or "space null"
#define MAX_JUMP_OFFSET (USED_CODES*USED_CODES)
#else
#define ENDUNITTEXT "`000 "
#define JUMP_OFFSET 4   //   immediately after any ` are 3 characters of jump info, then either  "space letter :" or "space null"
#define MAX_JUMP_OFFSET (USED_CODES*USED_CODES*USED_CODES)
#endif
void Encode(unsigned int val,char* &ptr);
void DummyEncode(char* &data);
unsigned int Decode(char* data);
extern unsigned char uncode[];
extern unsigned char code[];

#define NEXTRESPONDER -1
#define NEXTTOPLEVEL 2

// permanent flags 
#define TOPIC_RANDOM 4      //   random access responders (not gambits)
#define TOPIC_SYSTEM 8		//   
#define TOPIC_NOSTAY 16    //   do not stay in this topic
#define TOPIC_PRIORITY 32	//   prefer this 
#define TOPIC_LOWPRIORITY 64 //   deprefer this

#define MAX_TOPIC_SIZE  1000000 // 1mb limit

//   TRANSIENT FLAGS
#define TOPIC_BLOCKED 128		//   (transient per user) disabled by some users 
#define TOPIC_USED 256			//   (transient per user) indicates need to write out the topic

#define TRANSIENT_FLAGS ( TOPIC_BLOCKED | TOPIC_USED) 

#define TOPIC_DONE 1
#define MAX_TOPIC_KEYS 5000

extern unsigned int topicNumber; //   how many topics exist 1...n
extern int rejoinder_at;
extern int rejoinder_Topic;
extern unsigned int topicID;
extern int start_Rejoinder_at;
extern int start_rejoinder_Topic;
extern unsigned int rulesTested;
extern unsigned int rulesMatched;
extern unsigned int duplicateCount;

#define MAX_TOPIC_STACK 50

extern unsigned int topicIndex;
extern unsigned int topicStack[MAX_TOPIC_STACK];
extern unsigned int* topicFlagsMap;
extern unsigned int* topicDebugMap;
extern uint64* topicChecksumMap;
extern char** topicMap; 
extern char** topicNameMap;
extern char** topicRestrictionMap;
extern unsigned char** topicUsedMap;
extern unsigned char** topicResponderDebugMap;
extern unsigned int* topicRespondersMap;
extern char globalTopicFlags;
extern char fileID[10];
extern uint64 setControl;
char* FindNextLabel(char* label, char* ptr,unsigned int &id,bool alwaysAllowed);

unsigned int GetTopicsWithGambits(char* buffer);
void GetKeywordTopic(char* keyword);
void GetActiveTopicName(char* buffer);
unsigned int FindTopicIDByName(char* request);
unsigned int KeywordTopicsCode(char* buffer);

unsigned int ProcessRuleOutput(char* rule, unsigned int id,char* buffer);
unsigned int TestTopic(unsigned int newtopicID,bool test,char* buffer);
unsigned int TestRule(unsigned int responderID,char* ptr,char* buffer);
unsigned int PerformTopic(int active,char* buffer);

bool TopLevelQuestion(char* word);
bool TopLevelStatement(char* word);
bool TopLevelGambit(char* word);
bool Continuer(char* word);
bool TopLevelRule(char* word);
bool IsRule(char* word);
bool DebuggableResponder(unsigned int n);
void DumpInterestingTopics();

char* FindNextResponder(signed char level, char* at);
unsigned int FindTypedResponse(char type,char* buffer,unsigned int& id);
bool PushTopic(unsigned int topic);
void PopTopic();
int GetTopicFlags(unsigned int id);
bool IsCurrentTopic(unsigned int topic);
char* GetTopicName(unsigned int i,bool actual = true);
char* GetTopicData(unsigned int i);
void SetTopicData(unsigned int i,char* data);
void SetResponderMark(unsigned int topic,unsigned int n);
void ClearResponderMark(unsigned int topic,unsigned int n);
void ClearResponderMarks(unsigned int topic);
bool UsableResponder(unsigned int n,unsigned int topic = topicID);
void SetRejoinder();
void SetErase();
void UndoErase(char* ptr,unsigned int topic,unsigned int id);
void AddNoErase(char* ptr);void TestTables();
void AddTopicFlag(unsigned int id, unsigned int flag);
void RemoveTopicFlag(unsigned int id, unsigned int flag);
void SetDebugMark(unsigned int n,int topic);

// bulk topic I/O
void WriteTopicData(FILE* out);
void ReadTopicData(FILE* in);

// general topic system control
void RestartTopicSystem();
void ResetTopicSentence();
void ResetTopicSystem();
void ResetTopics();
void ResetTopic(unsigned int id);
void CloseTopics();

// Interesting topics
void RemoveInterestingTopic(unsigned int topic);
unsigned int GetInterestingTopic();
void AddInterestingTopic(unsigned int topic);
char* ShowInterestingTopics();

#endif
