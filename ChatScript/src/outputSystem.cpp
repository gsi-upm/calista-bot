// outputSystem.cpp - executes output script


#include "common.h"
//   BUG - confirm ALL routines return ptr on good spot, removing if (*ptr == ' ') checks
int lastWildcardAccessed;

char* currentOutputBase;	//   the current buffer into which output is written (may be real or scratch output)
char* currentPatternBase;	//   the buffer being used by REAL output
int executingID = -1;	//   current rule id
char* executingRule = 0;		//   current rule being procesed
char tmpword[MAX_WORD_SIZE];

static char* ProcessChoice(char* ptr,char* buffer,unsigned int &result,int controls) //   given block of  [xx] [yy] [zz]  randomly pick one
{
	char* choice[CHOICE_LIMIT];
	char** choiceset = choice;
	int count = 0;
    char* endptr; //   will be end of [] [] sequence

	//   random choice can be [xxx]  - simple
	//   or []	-- intentionally empty
	//   or [b: xxx] -- has designated rejoinder
	//   or [$var xxx] -- only valid if var is defined
	//   or [$var b: xxx ] -- valid test and designated rejoinder
           
	//   gather the choices
	while (*ptr == '[') //   any non-choice breaks up choice block
	{
		//   find the closing ]
		endptr = ptr-1;
		while (1) 
		{
			endptr = strchr(endptr+1,']'); //   find closing ] - we MUST find it (check in initsql)
			if (*(endptr-2) != '\\') break; //   found if not a literal \[
		}
            
		//   a choice unit can start with variable test conditions:  [ $var1 xxx]
		//   but its not a test condition if its a variable assignment [ $var1 = 5 xxx ]
		//   if the variable is not defined, the case is ignored
		//   There is no need for testing var against a value, you can do that elsewhere before
		//   creating a new variable.
		bool valid = true;
		char word[MAX_WORD_SIZE];
		char* base = ptr+2;
		char* at = ReadCompiledWord(base,word); //   check for var flag enable

		//   We allow a choice to be simple: [this is output]
		//   We allow a choice to be controlled by a variable: [$ready  this is output]
		//   This is still simple: [$ready = true this is output]
		//   And a choice can designate which continuer goes with it: [d: this is output]
		if (*word == '$' && IsAlpha(word[1])) //   its a variable, not a money amount
		{
			ReadCompiledWord(at,tmpword);	//what happens after this? 
			if (*tmpword == '=' || tmpword[1] == '=') at = base; //   assignment = or *= kind, leave as full
			else if (!*GetUserVariable(word)) valid = false;	//   failed the test
		}
		else if (*word == '!' && !word[1] && *at == '$') // !$value
		{
			at = ReadCompiledWord(at,word);
			if (*GetUserVariable(word)) valid = false;	//   failed the inverse test
		}
		else at = base; 

		if (valid) choiceset[count++] = at;          
		ptr = endptr + 2;   //   jump past ] space
	}

	//   pick a choice randomly
	while (count > 0)
	{
		int r = random(count);
		char* ptr = choiceset[r];
		if (*ptr == ']') break;//   intentionally EMPTY choice
		if (ptr[1] == ':' && ptr[2] == ' ') //   requesting special rejoinder code if used
		{
			respondLevel = ptr[0];
			ptr += 3; //   skip d: space 
		}
		else respondLevel = 0;
		Output(ptr,buffer,result,controls);
		if (result & (FAILRULE_BIT | FAILTOPIC_BIT | FAILSENTENCE_BIT|ENDINPUT_BIT|ENDSENTENCE_BIT))
		{
			respondLevel = 0;
			break; //   they request failure
		}
		if (*buffer && !repeatable && HasAlreadySaid(buffer)) 
		{
			if (trace) Log(STDUSERLOG,"Choice Rejected: %s already said\r\n",buffer);
			if (!*repeatedOutput) strcpy(repeatedOutput,buffer);
			buffer[0] = 0;
		}
		else break;
		choiceset[r] = choiceset[--count];
	}
	if (!count) respondLevel = 0; // failed to find one
	return endptr+2; //   skip to end of rand past the ] and space
}

int CountParens(char* start) 
{
	int paren = 0;
	start--;		//   back up so we can start at beginning
	while (*++start) if (*start == '"') ++paren; 
	return paren;
}

char* FreshOutput(char* ptr,char* buffer,unsigned int &result,int controls)
{//   sometimes we need to change buffers-- this tracks the new base and restores the old when done
	//   we need to know the base to detect overflow along the way
	char* oldbase = currentOutputBase;
	currentOutputBase = buffer;
	ptr = Output(ptr,buffer,result,controls);
	currentOutputBase = oldbase;
	return ptr;
}

static void TraceOutput(char* word)
{
	Log(STDUSERLOG,"&%s ",word);
}

#ifdef INFORMATION
There are two kinds of output streams. The ONCE only stream expects to read an item and return.
If a fail code is hit when processing an item, then if the stream is ONCE only, it will be done
and return a ptr to the rest. If a general stream hits an error, it has to flush all the remaining
tokens and return a ptr to the end.
#endif

#define CONDITIONAL_SPACE() 	if (space) {*buffer++ = ' '; *buffer = 0;}

char* Output(char* ptr,char* buffer,unsigned int &result,int controls)
{   //   Generate output. buffer is ALWAYS based on currentOutputBase 
    //   Output tokens are separated by blanks, except for:
    //	 1: at start 
    //	 2: before  puncutation
    //	 3: after start doublequote and before end doublequote

	// TRY not to alter values passed in (might be variables)
	if (*ptr == ' ') //   BUG-- we dont want to have to do this.
	{
		ptr = SkipWhitespace(ptr);
	}
	//   an output stream consists of words, special words, [] random zones, commands, C-style script
	char* original = buffer;
	*buffer = 0; //   always starts out blank
	if (!*ptr) return NULL;	//   out of data

    int len;
    
    //   see if some words need translating
    char word[MAX_WORD_SIZE];
    result = 0;
	char* at;
	char* hold;
	int paren = 0;

    while (ptr)
    {
		if (!*ptr || *ptr == ENDUNIT) break;	//   cannot go on
        ptr = ReadCompiledWord(ptr,word); //   returns pointing at next significant char, protecting dq words
 		if ((*word == ']'  || *word == ')'  || *word == '}')  && !paren  ) break; //   end of data, end of random unit, end of body of if or loop
	
		//   determine whether to space before item or not
		bool space = false;
		if (!(controls & OUTPUT_ONCE))
		{
			char before = *(buffer-1);
			space = before && before != '$' && before != '[' && before != '(' && before != '"' && before != '/' && before != '\n';
			if (before == '"') space = !(CountParens(currentOutputBase) & 1); //   if parens not balanced, add space before opening doublequote
		}


		switch (*word)
        {
		case ')':  case '}':  
			if (--paren < 0) return ptr + 2; //   skip over blank after item
			CONDITIONAL_SPACE();
			*buffer++ = *word;
			*buffer = 0;
	        if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(word);
            break;
		case ']':  //   close of stream or macroArgumentList to function or whatever
			return ptr + 2; //   skip over blank after item
		case '(': case '{':
			CONDITIONAL_SPACE();
			if (word[1]) // its not really one of these, its a full token like (ice)_ is_what
			{
				StdNumber(word,buffer,controls);
				break;
			}
			++paren;
			*buffer++ = *word;
			*buffer = 0;
	        if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(word);
            break;
         case '%':  //   system variable
		 {
			if (word[1])  //assignment into a system variable
            {
				//   is this assignment:  $variable = 23 
				char* at = ptr; //   dont disburb leaving off at the blank, so others can skip over 1 if they want
				if ((*at == '=' && at[1] != '=') || (at[1] == '=' && *at != '='  && *at != '!'))	
				{
					ptr = PerformAssignment(word,at,result); //   =  or *= kind of construction
					break;
				}
			}
				
			at = (IsAlpha(word[1]) ) ? GetSystemVariable(word) :  word; //   literal %
			if (*at)
			{
 				CONDITIONAL_SPACE();
				strcpy(buffer,at); 
				if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(at);
			}
			break;
		 }
        case '~':	//concept set 
			CONDITIONAL_SPACE();
			strcpy(buffer,word);
			if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(word);
            break;
        case '\\':  //   backslash needed for new line  () [ ]   
            if (word[1] == 'n' && !word[2])  //   \n
            {
				CONDITIONAL_SPACE();
#ifdef WIN32
                strcpy(buffer,"\r\n"); // WINDOWS
#elif IOS
                strcpy(buffer,"\n"); // IOS
#else
                strcpy(buffer,"\n"); // LINUX
#endif
			}
            else //   some other random backslashed content
            {
 				CONDITIONAL_SPACE();
				strcpy(buffer,word+1); 
  				if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(word+1);
           }
            break;
        case '$': //   user variable or money
            if (word[1] && !IsDigit(word[1])) //   user variable
            {
				//   is this assignment:  $variable = 23 
				char* at = ptr; //   dont disburb leaving off at the blank, so others can skip over 1 if they want
				if ((*at == '=' && at[1] != '=') || (*at && at[1] == '=' && *at != '='  && *at != '!'))	ptr = PerformAssignment(word,at,result); //   =  or *= kind of construction
				else
				{
					char* value = GetUserVariable(word);
					if (trace & OUTPUT_TRACE) Log(STDUSERLOG," %s=%s ",word,value);
					//if (value && *value) { CONDITIONAL_SPACE();}
					Output(value,buffer,result,controls);//   this value needs to be evaled, in case it is _0 or something like that
					//StdNumber(value,buffer,controls);
				}
			}
            else //   must be money
            {
				CONDITIONAL_SPACE();
				strcpy(buffer,word); 
            }
            break;
 		  case '_': //   wildcard or standalone _ OR just an ordinary token
		  {
			//   see if assignment statement  =   +=  -=  *=  /=  %=
			char* at = ptr;
			bool assign = false;
			if (*at == '=' && at[1] == ' ') assign = true;
			else if (at[1] == '=' && (*at == '+'  || *at == '-' || *at == '*' || *at == '/' || *at == '%') ) assign = true;
			if (assign) ptr = PerformAssignment(word,at,result); 
			else if (!IsDigit(word[1])) //   stand-alone _ or some non wildcard
			{
				CONDITIONAL_SPACE();
				strcpy(buffer,word);
				break;
			}
			else //   simple wildcard
			{
				lastWildcardAccessed = atoi(word+WILDINDEX); //   which wildcard
				hold = wildcardCanonicalText[lastWildcardAccessed];
				//if (*hold) { CONDITIONAL_SPACE();}

				Output(hold,buffer,result,controls);//   this value needs to be evaled, in case it is _0 or something like that

				//   remove double quotes if it has them 
				//if (*hold == '"') ++hold;

				//StdNumber(hold,buffer,controls);

				//   remove trailing double quote if it has it
				//len = strlen(buffer) - 1;
				//if (buffer[len] == '"') buffer[len] = 0;
			}
		  }
            break;
        case '"': //
			len = strlen(word);
			CONDITIONAL_SPACE();

			if (word[1] == '`' && controls & OUTPUT_EVALCODE)// execute code within the string
			{
				word[len-1] = 0;	// remove closing "
				Output(word+2,buffer,result,controls);
			}
			else strcpy(buffer,word); //   either a simple quote (open or close) or a start of quote expression
            break;
		case '\'': //   quoted item
			CONDITIONAL_SPACE();
			if (word[1] == 's' && !word[2]) 
			{
				if (space) --buffer;
				strcpy(buffer,"'s"); //   possessive 's
			}
			else if (word[1] == '_') 
			{
				int index = word[WILDINDEX+1]-'0'; //   which one
				at = wildcardOriginalText[index];
				if (!*at && space)  *--buffer = 0; //   remove space we assumed
			    //   remove double quotes if it has them 
				if (*at == '"') ++at;
				if (trace & OUTPUT_TRACE) Log(STDUSERLOG," %s=%s ",word,at);
				StdNumber(at,buffer,controls);
				buffer += strlen(buffer);
				//   remove trailing double quotes if it has them
				if (*(buffer-1) == '"') *--buffer = 0;
			}
			else  strcpy(buffer,word); 
			break;
		case '^': //   function call or function variable or FORMAT string
			if (IsDigit(word[1]))  //   replace the variable with its content
			{
				char* at = SkipWhitespace(ptr); //   dont disburb leaving off at the blank, so others can skip over 1 if they want
				if ((*at == '=' && at[1] != '=') || (at[1] == '=' && *at != '='  && *at != '!')) ptr = PerformAssignment(word,at,result); //   =  or *= kind of construction
				else 
				{
					char* value = macroArgumentList[atoi(word+1)+userCallArgumentBase];
					if (trace & OUTPUT_TRACE) Log(STDUSERLOG," %s=%s ",word,value);
					Output(value,buffer,result,controls);//   this value needs to be evaled, in case it is _0 or something like that
				}
				break;
			}
			if (word[1] == '"') // format string
			{
				ReformatString(word+1);//   balanced closing quote - no space needed before it.
				CONDITIONAL_SPACE();
				strcpy(buffer,word+1); //   either a simple quote (open or close) or a start of quote expression
				break;
			}
			
			//   execute function call
			//   if the command doesnt put data in, remove space after if we put one in
			CONDITIONAL_SPACE();
			if (!word[1]) strcpy(buffer,word);
			else ptr =  DoFunction(word,ptr,buffer,result);  //   false return means DO NOT PROCESS further
			if (space && buffer[0] == 0)  *--buffer = 0;
			break;
		case '[':  //   random choice. process the choice area, then resume in this loop after the choice is made
			if (word[1]) // its not really one of these, its a full token like ]ice)_ is_what
			{
				StdNumber(word,buffer,controls);
				break;
			}
			ptr = ProcessChoice(ptr-2,buffer,result,controls);  //   let ProcessChoice see the [        
			break;
        case '@': //   a fact set reference like @9 @1subject @2object or something else
		{
			char* at = ptr; 

			//   you can clear a set by @9 = null or maybe put in set like @9 = burst(...)
			//   clear fact set (assign to null)
			if ((*at == '=' && at[1] != '=') || (at[1] == '=' && *at != '='  && *at != '!'))	
			{
				ptr = PerformAssignment(word,ptr,result);
				break;
			}
			else if (impliedSet != ALREADY_HANDLED) //   set copy like @2 = @1
			{
				unsigned int set = word[1]-'0';
				unsigned int count = (unsigned int) ((ulong_t)factSet[set][0]);
				factSet[impliedSet][0] = (FACT*) count;
				while (count)
				{
					factSet[impliedSet][count] = factSet[set][count];
					--count;
				}
				break;
			}

            if (IsDigit(word[1]) && IsAlpha(word[2]) && !(controls & OUTPUT_KEEPQUERYSET)) //   fact set subject/object reference
            {
                unsigned int store = word[1] - '0';
                unsigned int count = (ulong_t)factSet[store][0];
                if (!count || count >= MAX_FIND) break;
                FACT* F = factSet[store][1]; //   use FIRST fact (most recent) - like for sentence outputs
				if (!F) break;
				MEANING T;
				uint64 flags;
				if (word[2] == 's' ) 
				{
					T = F->subject;
					flags = F->properties & FACTSUBJECT;
				}
				else if (word[2] == 'v')
				{
					T = F->verb;
					flags = F->properties & FACTVERB;
				}
				else 
				{
					T = F->object;
					flags = F->properties & FACTOBJECT;
				}
				char* answer;
				if (flags) answer = "?";
				else answer = Meaning2Word(T)->word;
                if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(answer);
 				Output(answer,buffer,result,controls);//   this value needs to be evaled, in case it is _0 or something like that
				//strcpy(buffer,answer);
            }
			else 
			{
				CONDITIONAL_SPACE();
				strcpy(buffer,word);
			}
		}
            break;
        case '?': case '!': case '.': case ',': case ';': //   various punctuation wont want a space before
            if (*word == '.') //   is it a number period?
            {
                char* at = buffer-1;
                if (IsDigit(*at)) *buffer++ = ' '; //   space before period after number
            }
            if (trace && TraceIt(OUTPUT_TRACE) && !(controls & (OUTPUT_ONCE|OUTPUT_SILENT))) TraceOutput(word);
			strcpy(buffer,word); 
            break;
		case ':': // a debug command?
		{
#ifndef NOTESTING
			WORDP D = FindWord(word);
			if (D && D->x.codeIndex) 
			{
				ptr -= strlen(word) + 1;	// back to the start of the command
				Command(ptr);
				return NULL; // we cant know where it ends... so claim it is over
			}
#endif
		}
		// DROP THRU TO DEFAULT WHEN NOT A DEBUG COMMAND
        default: //   either normal text or C-Script
			if (*ptr != '(') //   SIMPLE word
			{
				CONDITIONAL_SPACE();
				StdNumber(word,buffer,controls);
			}
			else  //   loop,if,function call W/O ^
			{
				if (!strncmp(word,"loop",4)) ptr = HandleLoop(ptr,buffer,result); //   LOOP - always does at least once
				else if (!strcmp(word,"if")) ptr = HandleIf(ptr,buffer,result); //   IF clause
				else  //   function call w/o ^ in front of name
				{
					char name[MAX_WORD_SIZE];
					name[0] = '^';
					strcpy(name+1,word);
					ptr = DoFunction(name,ptr,buffer,result);  //   false return means DO NOT PROCESS further
					if (result & UNDEFINED_FUNCTION ) //   not a function call, redo as simple word
					{
						result = 0;
						CONDITIONAL_SPACE();
						StdNumber(word,buffer,controls);
					}
				}
	        }
        }

		//   update location and check for overflow
		buffer += strlen(buffer);
        if ((buffer - currentOutputBase) >= (MAX_BUFFER_SIZE-1000)) result = FAILRULE_BIT; //   buffer approaching overflow.

		if (result & (RETRY_BIT|ENDCODES))
		{
			if (result & (FAILRULE_BIT|FAILSENTENCE_BIT|ENDSENTENCE_BIT)) *original = 0; //   cancel previous entries into buffer 
			if (!(controls & OUTPUT_ONCE)) ptr = BalanceParen(ptr,true); //   swallow excess input BUG - does anyone actually care
			break;	//   abort
		}
		if (controls & OUTPUT_ONCE) break;    
	}
    return ptr;  //   returns pointing at next significant char
}

char* ReadCommandArg(char* ptr, char* buffer,unsigned int& result)
{ 
	return FreshOutput(ptr,buffer,result,OUTPUT_ONCE|OUTPUT_KEEPSET);
}

void ReformatString(char* word)
{
	char revised[MAX_BUFFER_SIZE];
	strcpy(revised,word);
	char* at = revised+1;
	char* to = word;

	//   ai to rewrite parts of it- any var before a space or at start.
	while(*at)
	{
		if (*at == '$' && (at[1] == '$' || IsAlpha(at[1]))) //   treat as $var and substitute it
		{
			char* base = at;
			while(IsAlpha(*++at) || IsDigit(*at) || at[0] == '$');
			char hold = *at;
			*at = 0;
			char* value = GetUserVariable(base);
			*at = hold;
			*to = 0;
			if (value && *value) 
			{
				strcpy(to,value);
				to += strlen(to);
			}
			continue;
		}
		else if (*at == '@' && IsDigit(at[1])) //   set access.
		{
			int len = 0 ;
			at += 2;
			if (!strnicmp(at,"subject",7)) len = 7;
			else if (!strnicmp(at,"object",6)) len = 6;
			char c = at[len];
			at[len] = 0;
			char* junk = AllocateBuffer();
			unsigned int result;
			ReadCommandArg(at-2,junk,result);
			at[len] = c;
			strcpy(to,junk);
			FreeBuffer();
			to += strlen(to);
			at += len;
			if (result & ENDCODES) 
			{
				*word = 0;
				return;
			}
			continue;
		}
		else if (*at == '^' && IsDigit(at[1])) //   treat as ^var but cant substitute it, string can be passed along to deeper and deeper calls before reformat.
		{
			ReportBug("Illegal use of function var in string reformat: %s",word);
			return;
		}
		else if (*at == '_' && IsDigit(at[1])) //   treat as _var and substitute it (canonical)
		{
			char* base = at+WILDINDEX-1;
			while(IsDigit(*++at));
			char hold = *at;
			*at = 0;
			char* value = GetwildcardOriginalText(base[WILDINDEX]-'0',true); //   get canon word
			*at = hold;
			*to = 0;
			strcpy(to,value);
			to += strlen(to);
			continue;
		}
		else if (*at == '\'' && at[1] == '_' && IsDigit(at[2])) //   treat as _var and substitute it (original)
		{
			char* base = at+WILDINDEX-1;
			at += 2;
			while(IsDigit(*++at));
			char hold = *at;
			*at = 0;
			char* value = GetwildcardOriginalText(base[WILDINDEX]-'0',false); //   get original word
			*at = hold;
			*to = 0;
			strcpy(to,value);
			to += strlen(to);
			continue;
		}

		*to++ = *at++;
	}
	*--to = 0; //   remove trailing quote
}

char* StdNumber(int n)
{
	char buffer[50];
	static char answer[50];
	*answer = 0;
	sprintf(buffer,"%d",n);
	StdNumber(buffer,answer,0);
	return answer;
}

void StdNumber(char* word,char* buffer,int controls)
{
    if (!IsDigitWord(word) || strchr(word,':')) //   not a number / or its a time
    {
        strcpy(buffer,word);  
 		if (*word && trace && TraceIt(OUTPUT_TRACE) && !(controls & OUTPUT_ONCE)) TraceOutput(word);
        return;
    }
    unsigned int len = strlen(word); //   sign & decimal perhaps too
    char* dot = strchr(word,'.'); //   has decimal point
    if (dot) 
    {
        *dot = 0; //   break off integer part
        len = dot-word;
    }

    if (len < 5) //   no commas on 4 digit or less, protects year numbers
    {
        if (dot) *dot = '.'; 
        strcpy(buffer,word);  
        return;
    }
    char* ptr = word;
    unsigned int offset = len % 3;  //   3 digits per zone
    len = (len + 2 - offset) / 3;  //   how many triples
    strncpy(buffer,ptr,offset);  //   copy the header
    buffer += offset;
    ptr += offset;
    if (offset && len) *buffer++ = ','; //   , separate each zone
    while (len--)
    {
        *buffer++ = *ptr++; //   move tuple
        *buffer++ = *ptr++;
        *buffer++ = *ptr++;
        if (len) *buffer++ = ',';
    }
	*buffer = 0;
	if (dot) 
	{
		*buffer++ = '.';
		strcpy(buffer,dot+1);
	}
}
