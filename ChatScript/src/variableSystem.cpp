// variableSystem.cpp - manage user variables ($variables)


#include "common.h"

#ifdef INFORMATION
Their are various kinds of variables.

	1. User variables beginning wih $ (regular and transient which begin with $$)
	2. Wildcard variables beginning with _
	3. Fact sets beginning with @ 
	4. Function variables beginning with ^ 
	5. System variables beginning with %  (read only)
	6. Parser variables beginning with | (read only)

#endif

#define LONG_USERVAR 2000
int impliedSet = ALREADY_HANDLED;
int impliedWild = ALREADY_HANDLED;

unsigned int wildcardIndex = 0;
char wildcardOriginalText[MAX_WILDCARDS+1][MAX_WORD_SIZE];  //   spot wild cards can be stored
char wildcardCanonicalText[MAX_WILDCARDS+1][MAX_WORD_SIZE];  //   spot wild cards can be stored
unsigned int wildcardPosition[MAX_WILDCARDS+1]; //   spot it started in sentence

//   list of active variables needing saving
#define MAX_USER_VARS 30000
static WORDP userVariableList[MAX_USER_VARS];
static WORDP baseVariableList[MAX_USER_VARS];
static char* baseVariableValues[MAX_USER_VARS];
static unsigned int userVariableIndex;
static unsigned int baseVariableIndex;

void SetWildCard(unsigned int start, unsigned int end)
{
				
    wildcardPosition[wildcardIndex] = start;
    wildcardOriginalText[wildcardIndex][0] = 0;
    wildcardCanonicalText[wildcardIndex][0] = 0;

	if (end > wordCount) end = wordCount; //   block an overrun
	if (start == 0 || wordCount == 0 || (end == 0 && start != 1) ) // null match
	{
		++wildcardIndex;
		if (wildcardIndex > MAX_WILDCARDS) wildcardIndex = 0; // rollover
		return;
	}

	if ( end < start) // EITHER the match was a null *, or we matched inside a word (so we cant store the word)
	{
		++wildcardIndex;
		if (wildcardIndex > MAX_WILDCARDS) wildcardIndex = 0; // rollover
		return;
	}

    bool started = false;
    for (unsigned int i = start; i <= end; ++i)
    {
        char* word = wordStarts[i];
        if (*word == ',') continue; //   no internal punctuation
        if (started && *word != '\'') //   possessives need no separator 
        {
            strcat(wildcardOriginalText[wildcardIndex],"_");
            strcat(wildcardCanonicalText[wildcardIndex],"_");
        }
        strcat(wildcardOriginalText[wildcardIndex],word);
        strcat(wildcardCanonicalText[wildcardIndex],wordCanonical[i]);
        started = true;
    }

    if (start >= end )//   single token
    {
        WORDP D = FindWord(wildcardOriginalText[wildcardIndex]);
        if (D && D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL))  UpcaseStarters(wildcardOriginalText[wildcardIndex]);
        D = FindWord(wildcardCanonicalText[wildcardIndex]);
        if (D && D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL)) UpcaseStarters(wildcardCanonicalText[wildcardIndex]);
     }
 	 if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG,"_%d=%s/%s ",wildcardIndex,wildcardOriginalText[wildcardIndex],wildcardCanonicalText[wildcardIndex]);

    ++wildcardIndex;
	if (wildcardIndex > MAX_WILDCARDS) wildcardIndex = 0; // rollover
}

void SetWildCardStart(unsigned int index)
{
	 wildcardIndex = index;
}

void SetWildCard(char* value, const char* canonicalVale,const char* index,unsigned int position)
{
	if (!value) value = "";
	if (!canonicalVale) canonicalVale = "";

    if (strlen(value) > 300) 
	{
		value[300] = 0;
		ReportBug("long value");
	}
    if (strlen(canonicalVale) > 300) ReportBug("long value");
    if (index ) wildcardIndex = *index - '0';
    while (value[0] == ' ') ++value;  // skip leading whitespace
    while (canonicalVale && canonicalVale[0] == ' ') ++canonicalVale;  // skip leading whitespace
    strcpy(wildcardOriginalText[wildcardIndex],value);
    strcpy(wildcardCanonicalText[wildcardIndex],(canonicalVale) ? canonicalVale : value);
    wildcardPosition[wildcardIndex] = position; 
  
    //   uppercase proper names
    WORDP D = FindWord(wildcardCanonicalText[wildcardIndex]);
    if (D && D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL)) 
    {
        UpcaseStarters(wildcardOriginalText[wildcardIndex]);
        UpcaseStarters(wildcardCanonicalText[wildcardIndex]);
    }
    ++wildcardIndex;
	if (wildcardIndex > MAX_WILDCARDS) wildcardIndex = 0;
}

char* GetwildcardOriginalText(unsigned int i, bool canon)
{
    return canon ? wildcardCanonicalText[i] : wildcardOriginalText[i];
}

char* GetUserVariable(const char* word)
{
	WORDP D = FindWord((char*) word);
	if (!D )  return "";	//   no such variable

	// variables defined as pattern words may exist, but value will be null

	char* item = D->w.userValue;
    if (!item)  return "";

    if (*item == '&') //   value is quoted
	{
		static char buffer[MAX_SENTENCE_BYTES];
		strcpy(buffer,item+1);
		unsigned int len = strlen(buffer);
		buffer[len-1] = 0;
		return buffer;
	}
	else return item; 
 }

void ClearVariableSetFlags()
{
	for (unsigned int i = 0; i < userVariableIndex; ++i) userVariableList[i]->systemFlags &= -1LL ^ VAR_CHANGED;
}

void ShowChangedVariables()
{
	for (unsigned int i = 0; i < userVariableIndex; ++i) 
	{
		if (userVariableList[i]->systemFlags & VAR_CHANGED)
		{
			char* value = userVariableList[i]->w.userValue;
			if (value) Log(1,"%s = %s\r\n",userVariableList[i]->word,value);
			else Log(1,"%s = null\r\n",userVariableList[i]->word);
		}
	}
}

static WORDP CreateUserVariable(const char* var)
{
    WORDP D = StoreWord((char*) var); //   find or create the var.
    if (!(D->systemFlags & VAR_CHANGED))	//   not current
    {
        userVariableList[userVariableIndex++] = D;
        if (userVariableIndex == MAX_USER_VARS) //   if too many variables, discard one (wont get written)
        {
            ReportBug("too many user vars");
            --userVariableIndex;
        }
		D->w.userValue = NULL; 
	}
	D->systemFlags |= VAR_CHANGED;
	return D;
}

void SetUserVariable(const char* var, char* word)
{
	if (word) 
	{
 		if (!stricmp(word,"null") ||!stricmp(word,"nil")) word = NULL;
		else //   some value 
		{
			if (*word == ' ') ++word; //   skip leading blank
			if (strlen(word) > LONG_USERVAR) 
			{
				ReportBug("long data %s \r\n",var);
				word = " ";
			}
			word = AllocateString(word);
		}
	}
 
	CreateUserVariable(var)->w.userValue = word; 
}

void AddVar(char* var, char* word,char minusflag)
{ //   adding onto $var or _var or ^var

	char* old;
    if (*var == '_') old = GetwildcardOriginalText(var[WILDINDEX]-'0',true); //   onto a wildcard
	else if (*var == '$') old = GetUserVariable(var); //   onto user variable
	else if (*var == '^') old = macroArgumentList[atoi(var+1)+userCallArgumentBase]; //   onto function argument
	else return; //   illegal

	bool floating = false;
	if (strchr(old,'.')) floating = true; 
	char result[MAX_WORD_SIZE];
    if (floating)
    {
        float newval = (float)atof(old);
		float more = (float)atof(word);
        if (minusflag == '-') newval -= more;
        else if (minusflag == '*') newval *= more;
        else if (minusflag == '/') newval /= more;
        else newval += more;
        sprintf(result,"%.2f",newval);
    }
    else
    {
		int newval = atoi(old);
		int more = atoi(word);
        if (minusflag == '-') newval -= more;
        else if (minusflag == '*') newval *= more;
        else if (minusflag == '/') newval /= more;
        else if (minusflag == '%') newval %= more;
        else if (minusflag == '|') newval |= more;
        else if (minusflag == '&') newval &= more;
         else if (minusflag == '^') newval ^= more;
       else newval += more;
        sprintf(result,"%d",newval);
    }

	if (*var == '_')  SetWildCard(result,result,var+WILDINDEX,0);  //   onto wildcard
	else if (*var == '$') SetUserVariable(var,result); //   onto user variable
	else if (*var == '^') strcpy(macroArgumentList[atoi(var+1)+userCallArgumentBase],result); //   onto function argument
}

char* PerformAssignment(char* word,char* ptr,unsigned int &result)
{//   we can assign to and from  $var, _var, ^var, @set, and %sysvar
    char op[MAX_WORD_SIZE];
	char* word1 = AllocateBuffer();
	FACT* F = currentFact;
	unsigned int id;
	int oldImpliedSet = impliedSet;		//   in case nested calls happen
	int oldImpliedWild = impliedWild;	//   in case nested calls happen
	result = 0;

	impliedSet = (*word == '@') ? (word[1] - '0') : ALREADY_HANDLED; //   what a set will use
	impliedWild = (*word == '_') ? (word[1]-'0') : ALREADY_HANDLED; //   what wildcard to start with
	int setImply = impliedSet;
	int setWild = impliedWild;

    ptr = ReadCompiledWord(ptr,op);//   assignment operator = += -= /= *= %=
	int assignFromWild =  (*ptr == '_' && IsDigit(ptr[1])) ? (ptr[1] - '0') : -1;
	if (trace && TraceIt(OUTPUT_TRACE)) id = Log(STDUSERTABLOG,"%s %s ",word,op);
 
	globalDepth++;
	if (assignFromWild >= 0 && *word == '_') ptr = ReadCompiledWord(ptr,word1); // dual wild assign, dont need to evaluate it, but need to swallow it
	else
	{
		ptr = ReadCommandArg(ptr,word1,result); //   assign value
		if (result & ENDCODES) goto exit;
	}
 
	//   a fact was created and not pre-used, so we must convert it to a reference to fact for assignment
	if (currentFact != F && setImply == impliedSet && setWild == impliedWild) sprintf(word1,"%d",currentFactIndex() );
	
   	if (!stricmp(word1,"null") || !stricmp(word1,"nil")) *word1 = 0;		//   assign null

	//   now sort out who we are assigning into and if its arithmetic or simple assign

	if (*op == '+' || *op == '-' || *op == '*' || *op == '/' || *op == '%' || *op == '|'  || *op == '&'  || *op == '^') AddVar(word,word1,op[0]);
	else if (*word == '_') //   assign to wild card
	{
		if (impliedWild != ALREADY_HANDLED) //   ALREADY_HANDLED means someone already did the assignment
		{
			if (assignFromWild >= 0) // full tranfer of data
			{
				wildcardPosition[wildcardIndex] =  wildcardPosition[assignFromWild];
				SetWildCard(wildcardOriginalText[assignFromWild],wildcardCanonicalText[assignFromWild],word+WILDINDEX,0); 
			}
			else
			{
				SetWildCard(word1,word1,word+WILDINDEX,0); 
				wildcardPosition[wildcardIndex] = 0;
			}
		}
	}
	else if (*word == '^') strcpy(macroArgumentList[atoi(word+1)+userCallArgumentBase],word1); //   assign to macro argument
	else if (*word == '@')
	{
		if (impliedSet == ALREADY_HANDLED);	//   got assigned by someone already
		else if (!*word1) factSet[impliedSet][0] = 0;
		else if (IsDigit(word1[0]))
		{
			char* p;
			while ( (p = strchr(word1,','))) strcpy(p,p+1);	// eliminate commas in number
			int n = atoi(word1);
			if (n < 0 || n > Fact2Index(factFree)) ReportBug("Value not a fact id - %s",word1);
			else 
			{
				FACT* F = GetFactPtr(n);
				unsigned int count = (ulong_t)factSet[impliedSet][0] + 1;
				if ( count >= MAX_FIND) --count;
				factSet[impliedSet][count] = F;
				factSet[impliedSet][0] = (FACT*) count;
			}
		}
		else // add in a fact
		{
			ReportBug("Bad assign to fact set %s %s",word,word1);
		}
	}
	else if (*word == '%') OverrideSystemVariable(word,word); // assign to system var
	else if (*word == '$') SetUserVariable(word,word1);//   assign to variable 898

	//   more arithmetic to follow up?  dont confuse ^ operator with ^function()
	while (ptr && ptr[1] == ' ' && (*ptr == '+' || *ptr == '-' || *ptr == '*' || *ptr == '/' || *ptr == '%' || *ptr == '|' || *ptr == '&' || *ptr == '^'))
	{
		ptr = ReadCompiledWord(ptr,op);
		ptr = ReadCommandArg(ptr,word1,result); //   next value
		if (result & ENDCODES) 
		{
			FreeBuffer();
			return ptr;
		}
		AddVar(word,word1,op[0]);
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERLOG," %s %s",op,word1);
	}
	if (trace && TraceIt(OUTPUT_TRACE))
	{
		char* answer = AllocateBuffer();
		unsigned int result;
		logUpdated = false;
		if (*word == '$') strcpy(answer,GetUserVariable(word));
		else if (*word == '_') 
		{
			int id = word[1]-'0';
			strcpy(answer,wildcardOriginalText[id]);
		}
		else FreshOutput(word,answer,result,OUTPUT_SILENT);
		if (*word == '@') //   how many stored in set
		{
			unsigned int x = (ulong_t) factSet[word[1]-'0'][0];
			if ( logUpdated ) Log(STDUSERTABLOG,"=> [%d] end-assign\r\n",x);
			else Log(1," [%d] \r\n",x);
		}
		else 
		{
			if (!*answer) 
			{
				if ( logUpdated ) Log(STDUSERTABLOG,"=> null  end-assign\r\n");
				else Log(1,"null \r\n");
			}
			else if ( logUpdated ) Log(STDUSERTABLOG,"=> %s  end-assign\r\n",answer);
			else Log(1," %s  \r\n",answer);
		}
		FreeBuffer();
	}
exit:
	FreeBuffer();
	impliedSet = oldImpliedSet;
	impliedWild = oldImpliedWild;
	globalDepth--;
	return ptr;
}

void ReestablishBaseVariables()
{
	for (unsigned int i = 0; i < baseVariableIndex; ++i)
	{
		baseVariableList[i]->w.userValue = baseVariableValues[i];
	}
}

void ClearBaseVariables()
{
	baseVariableIndex = 0;
}

void SetBaseVariables()
{
	if (userVariableIndex) printf("Read %s Variables\r\n",StdNumber(userVariableIndex));
	baseVariableIndex = userVariableIndex;
	for (unsigned int i = 0; i < baseVariableIndex; ++i)
	{
		baseVariableList[i] = userVariableList[i];
		baseVariableValues[i] = baseVariableList[i]->w.userValue;
	}
	ClearUserVariables();
}

void ClearUserVariables()
{
	while (userVariableIndex)
	{
		WORDP D = userVariableList[--userVariableIndex];
		D->w.userValue = NULL;
		D->systemFlags &= -1LL ^ VAR_CHANGED;
 	}
}

void WriteVariables(FILE* out)
{
    while (userVariableIndex)
    {
        WORDP D = userVariableList[--userVariableIndex];
        if (!(D->systemFlags & VAR_CHANGED) ) continue; //   gone
        if (D->word[1] !=  '$' && D->w.userValue) //   transients not dumped, nor are NULL values
		{
			char* val = D->w.userValue;
			while ((val = strchr(val,'\n'))) *val = ' '; //   clean out newlines
			fprintf(out,"%s=%s\n",D->word,NLSafe(D->w.userValue));
		}
        D->w.userValue = NULL;
		D->systemFlags &= -1LL ^ VAR_CHANGED;
    }
}

void DumpVariables()
{
	for (unsigned int i = 0; i < userVariableIndex; ++i)
	{
		WORDP D = userVariableList[i];
		char* value = D->w.userValue;
		if (!value) continue;
		if (value && !*value) continue;
		if (value)  Log(STDUSERLOG,"  variable: %s = %s\r\n",D->word,value);
	}
}

