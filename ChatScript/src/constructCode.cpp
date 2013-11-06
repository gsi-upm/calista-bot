// constructCode.cpp - handles output scripting constructs like IF, LOOP, and comparison relations


#include "common.h"

static char* TestIf(char* ptr,unsigned int& result)
{ //   word is a stream terminated by )
	//   if (%rand == 5 ) example of a comparison
	//   if (QuerySubject()) example of a call or if (!QuerySubject())
	//   if ($var) example of existence
	//   if ('_3 == 5)  quoted matchvar
	//   if (1) what an else does
	char* word1 = AllocateBuffer();
	char* word2 = AllocateBuffer();
	char op[MAX_WORD_SIZE];
	unsigned int id;
resume:

	ptr = ReadCompiledWord(ptr,word1);	//   the var or whatever
	bool invert = false;
	if (*word1 == '!') //   inverting, go get the actual
	{
		invert = true;
		ptr = ReadCompiledWord(ptr,word1);	//   the var 
	}

	ptr = ReadCompiledWord(ptr,op);		//   the test if any, or (for call or ) for closure or AND or OR
	if (IsComparison(*op)) //   a comparison test
	{
		ptr = ReadCompiledWord(ptr,word2);	
		result = HandleRelation(word1,op,word2,true,id);
		ptr = ReadCompiledWord(ptr,op);	//   AND, OR, or ) 
	}
	else //   existence of non-failure or any content
	{
		if (*word1 == '%' || *word1 == '_' || *word1 == '$' || *word1 == '@')
		{
			char* found;
			if (*word1 == '%') found = GetSystemVariable(word1);
			else if (*word1 == '_') found = wildcardCanonicalText[word1[1]-'0'];
			else if (*word1 == '$') found = GetUserVariable(word1);
			else found = (factSet[word1[1] - '0'][0]) ? (char*) "1" : (char*) "";
			if (trace && TraceIt(OUTPUT_TRACE)) 
			{
				if (!*found) 
				{
					if (invert) id = Log(STDUSERTABLOG,"If !%s (null) ",word1);
					else id = Log(STDUSERTABLOG,"If %s (null) ",word1);
				}
				else 
				{
					if (invert) id = Log(STDUSERTABLOG,"If !%s (%s) ",word1,found);
					else id = Log(STDUSERTABLOG,"If %s (%s) ",word1,found);
				}
			}
			result = (*found) ? 0 : FAILRULE_BIT;
		}
		else  //   run it (a function/macro call) or its a constant 
		{
			if (trace && TraceIt(OUTPUT_TRACE)) 
			{
				if (result & ENDCODES) id = Log(STDUSERTABLOG,"If %c%s ",(invert) ? '!' : ' ',word1);
				else if (*word1 == '1' && word1[1] == 0) id = Log(STDUSERTABLOG,"else ");
				else id = Log(STDUSERTABLOG,"if %c%s ",(invert) ? '!' : ' ',word1);
			}
			ptr -= strlen(word1) + 3; //   back up to process the word and space--  DANGER BUG bogus?
			if (*op == '(') //   was a function call
			{
				globalDepth++;
				ptr = FreshOutput(ptr,word2,result,OUTPUT_ONCE|OUTPUT_KEEPSET); 
				globalDepth--;
				ptr = ReadCompiledWord(ptr,op); //   find out what happens next after function call
			}
			else ptr = FreshOutput(ptr,word2,result,OUTPUT_ONCE|OUTPUT_KEEPSET) + 2; //   returns on the closer and we skip to accel
		}
	}
	if (invert) result = (result & ENDCODES) ? 0 : FAILRULE_BIT; 

	if (*op == ')'); //   already past the closing )
	else if (*op == 'a') //   AND - compiler validated it
	{
		if (!(result & ENDCODES)) 
		{
			if (trace && TraceIt(OUTPUT_TRACE)) id = Log(STDUSERLOG," AND ");
			goto resume;
			//   If he fails (result is one of ENDCODES), we fail
		}
		else 
		{
			if (trace && TraceIt(OUTPUT_TRACE)) id = Log(STDUSERLOG," ... ");
			ptr = BalanceParen(ptr,true); //   find the end ) and skip over it to jump code, code is already fail
		}
	}
	else //    OR, that's all compiler would have allowed
	{
		if (!(result & ENDCODES)) 
		{
			if (trace && TraceIt(OUTPUT_TRACE)) id = Log(STDUSERLOG," ... ");
			result = 0;
			ptr = BalanceParen(ptr,true);
		}
		else 
		{
			if (trace && TraceIt(OUTPUT_TRACE)) id = Log(STDUSERLOG," OR ");
			goto resume;
		}
		//   If he fails (result is one of ENDCODES), we fail
	}

	FreeBuffer();
	FreeBuffer();
	if (trace && (result & ENDCODES) && TraceIt(OUTPUT_TRACE)) Log(id,"%s\r\n", "FAIL-if");
	return ptr;
}

char* HandleIf(char* ptr, char* buffer,unsigned int& result)
{
	while (1) //   do test conditions until match
	{
		//   Perform TEST condition
		ptr = TestIf(ptr+2,result); //   skip (space, returns on accelerator
						
		//   perform SUCCESS branch and then end if
		if (!(result & (FAILTOPIC_BIT|FAILRULE_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDTOPIC_BIT|ENDRULE_BIT|ENDSENTENCE_BIT))) //   IF success
		{
			globalDepth++;
			ptr = Output(ptr+5,buffer,result); //   skip accelerator and space and { and space - returns on next useful token
			globalDepth--;
			ptr += Decode(ptr);	//   offset to end of if
			break;
		}
		else result = 0;		//   test result is not the IF result

		//   On fail, move to next test
		ptr += Decode(ptr);
		if (strncmp(ptr,"else ",5))  break; //   not an ELSE, the IF is over. Will be closing accellerator
		ptr += 5; //   skip over ELSE space, aiming at the (of the condition
	}
	return ptr;
} 

char* HandleLoop(char* ptr, char* buffer, unsigned int &result)
{
	char *start = buffer;
	char* word = AllocateBuffer();
	ptr = ReadCommandArg(ptr+2,word,result)+2; //   get the loop counter value and skip closing ) space 
	unsigned int counter = atoi(word);
	FreeBuffer();
	char* endofloop = ptr + Decode(ptr);
	ptr += 5;	//   skip jump + space + { + space
	if (result & ENDCODES) 
	{
		return endofloop;
	}
	if (counter > 1000) counter = 1000; //   LIMITED
	if (counter) while (counter-- > 0)
	{
		if (trace && TraceIt(OUTPUT_TRACE)) Log(STDUSERTABLOG,"loop (%d)\r\n",counter+1);
		unsigned int result1;
		globalDepth++;
		Output(ptr,buffer,result1);
		buffer += strlen(buffer);
		globalDepth--;
		if (result1 & (FAILRULE_BIT|FAILTOPIC_BIT|ENDRULE_BIT|ENDTOPIC_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT)) 
		{
			result = result1 & (FAILTOPIC_BIT|ENDTOPIC_BIT|FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT);
			break;//   potential failure if didnt add anything to buffer
		}
	}
	char c;
	if (start != buffer) while ((c = *(buffer-1)) != 0 && (c == ' ' ||c == ',' || c == '\n'  || c == '\r'))  *--buffer = 0; //   remove trailing comma or new line
	return endofloop;
}  

bool IsWithin(MEANING M,MEANING set)
{//   within : or ~ -- we assume no loops!
	Meaning2Word(M)->stamp.inferMark = inferMark;
	FACT* F = GetSubjectHead(M);
	while (F)
	{
		if (F->verb == Mmember ) 
		{
			if (F->object == set) return true;
			if (Meaning2Word(F->object)->stamp.inferMark != inferMark && IsWithin(F->object,set)) return true;
		}
		F = GetSubjectNext(F);
	}
	return false;
}

unsigned int HandleRelation(char* word1,char* op, char* word2,bool output,unsigned int& id)
{ //   word1 and word2 are RAW, ready to be evaluated.
	char* val1 = AllocateBuffer();
	char* val2 = AllocateBuffer();
	unsigned int result,val1Result;
	FreshOutput(word1,val1,result,OUTPUT_ONCE|OUTPUT_KEEPSET); //   get value of 1st arg
	val1Result = result;
	if (word2 && *word2) FreshOutput(word2,val2,result,OUTPUT_ONCE|OUTPUT_KEEPSET); //   get value of 2nd arg
	result = FAILRULE_BIT;
	if (!stricmp(val1,"null")) *val1 = 0;
	if (!stricmp(val2,"null")) *val2 = 0;
	//if (!*val1) strcpy(val1,"null");
	//if (!*val2) strcpy(val2,"null");
	if (trace && TraceIt(OUTPUT_TRACE) && output) 
	{
		if (stricmp(word1,val1)) Log(STDUSERTABLOG,"if  %s (%s) %s ",word1,val1,op);
		else Log(STDUSERTABLOG,"if %s %s ",word1,op);
		if (strcmp(word2,val2))	id = Log(STDUSERLOG," %s (%s) ",word2,val2);
		else id = Log(STDUSERLOG," %s ",word2);
	}
	else if (trace && TraceIt(PATTERN_TRACE) && !output) 
	{
		if (stricmp(word1,val1)) Log(STDUSERLOG,"\"%s %s ",val1,op);
		else Log(STDUSERLOG,"\"%s %s ",word1,op);
		if (strcmp(word2,val2))	id = Log(STDUSERLOG,"(%s)\" ",val2);
		else id = Log(STDUSERLOG,"%s\" ",word2);
	}

	if (!op)
	{
		if (val1Result & ENDCODES); //   explicitly failed
		else if (*val1) result = 0;	//   has some value
	}
	else if (*op == '?') //   member of test
	{
		if (*word1 == '\'') ++word1; // ignore the quote since we are doing positional
		if (*word1 == '_') //   try for fast match - positional unit has been set up
		{
			unsigned int index = word1[1] - '0';
			index = wildcardPosition[index];
			if (index)
			{
				WORDP D = FindWord(val2);
				if (D)
				{
					unsigned int junk,junk1;
					if (GetNextSpot(D,index-1,junk,junk1) == index) result = 0; //   the set will match at this spot
					else result = FAILRULE_BIT;
				}
			}
		}
		else if (*word1 == '|')
		{
		    WORDP D = FindWord(word1);
 			unsigned int junk,junk1;
  			unsigned int start = GetNextSpot(D,0,junk,junk1); //location of 1st parser occurrence
			D = FindWord(val2);
			if (D)
			{
				if (GetNextSpot(D,start,junk,junk1) == start) result = 0; //   the set will match at this spot
			}
		}
		else
		{
			//   the slow way. For some things one could use the fast way _x? or |x?
			WORDP D1 = FindWord(val1);
			WORDP D2 = FindWord(val2);
			if (D1 && D2)
			{
				NextinferMark();
				if (IsWithin(MakeMeaning(D1),MakeMeaning(D2))) result = 0;
			}
		}
	}
	else //   boolean tests
	{
		if (!IsNumberStarter(*val1) || !IsNumberStarter(*val2) ) //   non-numeric string compare - bug, can be confused if digit starts text string
		{
			char* arg1 = val1;
			char* arg2 = val2;
			if (arg1[0] == '"')
			{
				int len = strlen(arg1);
				if (arg1[len-1] == '"') // remove STRING markers
				{
					arg1[len-1] = 0;
					++arg1;
				}
			}
			if (arg2[0] == '"')
			{
				int len = strlen(arg2);
				if (arg2[len-1] == '"') // remove STRING markers
				{
					arg2[len-1] = 0;
					++arg2;
				}
			}			if (*op == '!') result = (stricmp(arg1,arg2)) ? 0 : FAILRULE_BIT;
			else if (*op == '=') result = (!stricmp(arg1,arg2)) ? 0 : FAILRULE_BIT;
			else result = FAILRULE_BIT;
		}
		//   handle float ops
		else if ((strchr(val1,'.') && val1[1]) || (strchr(val2,'.') && val2[1])) //   float macroArgumentList
		{
			float v1 = (float) atof(val1);
			float v2 = (float) atof(val2);
			if (*op == '=') result = (v1 == v2) ? 0 : FAILRULE_BIT;
			else if (*op == '<') 
			{
				if (!op[1]) result =  (v1 < v2) ? 0 : FAILRULE_BIT;
				else result =  (v1 <= v2) ? 0 : FAILRULE_BIT;
			}
			else if (*op == '>') 
			{
				if (!op[1]) result =  (v1 > v2) ? 0 : FAILRULE_BIT;
				else result =  (v1 >= v2) ? 0 : FAILRULE_BIT;
			}
			else if (*op == '!') result = (v1 != v2) ? 0 : FAILRULE_BIT;
			else if (*op == '?') 
			{
				if (v1 > v2) result = 2;
				if (v1 < v2) result = 0;
				result = 1;
			}
			else result = FAILRULE_BIT;
		}
		else //   int compare
		{
			int v1 = atoi(val1);
			int v2 = atoi(val2);
			if (*op == '?') //   internal
			{
				if (v1 > v2) result = 2;
				else if (v1 < v2) result = 0;
				else result = 1;
			}
			else if (*op == '=') result =  (v1 == v2) ? 0 : FAILRULE_BIT;
			else if (*op == '<') 
			{
				if (!op[1]) result =  (v1 < v2) ? 0 : FAILRULE_BIT;
				else result =  (v1 <= v2) ? 0 : FAILRULE_BIT;
			}
			else if (*op == '>') 
			{
				if (!op[1]) result =  (v1 > v2) ? 0 : FAILRULE_BIT;
				else result =  (v1 >= v2) ? 0 : FAILRULE_BIT;
			}
			else if (*op == '!') result =  (v1 != v2) ? 0 : FAILRULE_BIT;
			else if (*op == '&') result =  (v1 & v2) ? 0 : FAILRULE_BIT;
			else result =  FAILRULE_BIT;
		}
	}
	FreeBuffer();
	FreeBuffer();
	return result;
}

