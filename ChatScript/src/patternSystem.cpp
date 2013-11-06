// patternSystem.cpp - performs pattern matching of rules


#include "common.h"
unsigned int positionStart;
unsigned int positionEnd;
unsigned int lastwordIndex;
static int defaultRange = 2;
#define MAX_PAREN_NEST 50

#define INFINITE_MATCH (unsigned int)(-(200 << 8))

static char* ptrStack[MAX_PAREN_NEST];
static int argStack[MAX_PAREN_NEST];
static int baseStack[MAX_PAREN_NEST];
static unsigned int functionNest = 0;

static bool HasProperty(char* ptr,char* compare) //   %system variable
{
	char* value = GetSystemVariable(ptr);
	if (!*value) return false;
	if (!compare) return true; //   it has a value or not
	char c = *compare;
	if (c == '=') return !stricmp(value,compare+1); //   direct uninterpreted value
	else
	{
		float v2 = (float)atof(compare+1);
		float v1 = (float)atof(value);
		if (c == '>' && v1 > v2 ) return true;
		if (c == '<' && v1 < v2 ) return true;
		if (c == '&' && atoi(compare+1) & atoi(value)) return true;
		return false;
	}
}

bool MatchTest(WORDP D, unsigned int start,char* op, char* compare,int quote)
{ //   handles ~, :, ', and regular words .  IF It fails, it should have left positionEnd untouched!!
    unsigned int id;
	while (GetNextSpot(D,start,positionStart,positionEnd))
    {
        start = positionStart; //   try again farther if this fails
        if (op)
        {
			char* word;
			word = quote ? wordStarts[positionStart] : wordCanonical[positionStart];
			if (IsAlpha(*D->word)) word = D->word; //   implicitly all normal words are relation tested as given

			if (HandleRelation(word,op,compare,false,id) & ENDCODES) continue; //   test fails. 
        }
        if (!quote) return true; //   allowed to match canonical and original

        //   we have a match, but we must prove it is a literal match, not a canonical one
		if (positionEnd < positionStart) continue;	// cant use match within (as in sets)
		if (D->word[0] == '~') return true;
        else if (positionStart >= positionEnd && !stricmp(D->word,wordStarts[positionStart])) return true;   //   literal match
		else // matching a phrase...
		{
			char full[MAX_WORD_SIZE];
			*full = 0;
			for (unsigned int i = positionStart; i <= positionEnd; ++i)
			{
				strcat(full,wordStarts[i]);
				if ( i != positionEnd) strcat(full,"_");
			}
			if (!stricmp(D->word,full)) return true;
		}
    } 
    return false;
}

bool FindSequence(char* word, unsigned int start)
{ // string contents searched for.... we dont look up the string itself because if dynamic, might not be marked...
	// outside cares where we start, we dont. but we care that our words are in order later than start
	unsigned int n = BurstWord(word);
	unsigned int actualStart = 0;
	bool matched = false;
	positionEnd = start;
	int oldend = 0; // allowed to match anywhere or only next
	for (unsigned int i = 1; i <= n; ++i)
	{
		WORDP D = FindWord(GetBurstWord(i));
		matched = MatchTest(D,positionEnd,NULL,NULL,0);
		if (matched)
		{
			if (oldend > 0 && positionStart != (oldend + 1)) // do our words match in sequence
			{
				matched = false;
				break;
			}
			if ( i == 1) actualStart = positionStart;
			oldend = positionEnd;
		}
		else break;
	}
	if (matched) positionStart = actualStart; // actual start of sequence of words
	return matched;
}

#define GAPPASSBACK  0X0000FFFF
#define NOT_BIT 0X00010000
#define FREEMODE_BIT 0X00020000
#define QUOTE_BIT 0X00080000
#define WILDGAP 0X00100000
#define WILDSPECIFIC 0X00200000

bool Match(char* ptr, unsigned int depth, int startposition, char kind, bool wildstart,unsigned int& gap,unsigned int& wildcardSelector,unsigned int &returnstart, unsigned int& returnend )
{//   always STARTS past initial opening thing ([ {  and ends with closing matching thing
	globalDepth++;
    char word[MAX_WORD_SIZE];
	char* orig = ptr;
	int statusBits = 0; //   turns off: not, quote, startedgap, freemode, gappassback,wildselectorpassback
    if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG, "%c ",kind); //   start on new indented line

    int matched;
	unsigned int startNest = functionNest;
	unsigned int result;
    //   for user defined function calls, nested data
    unsigned int hold;
    WORDP D;
	bool success = false;
    char* compare;
	bool returned = false; //   came back from a nesting, if we dont start a new nesting, we need to restore tab on this level
    int firstMatched = -1; //   ()  should return spot it started (firstMatched) so caller has ability to bind any wild card before it
    if (wildstart)  positionStart = INFINITE_MATCH; //   INFINITE_MATCH means we are in initial startup, allows us to match ANYWHERE forward to start
    positionEnd = startposition; //   we scan starting 1 after this
 	unsigned int basicStart = startposition;	//   we must not match real stuff any earlier than here
    char* argumentText = NULL; //   pushed original text from a function arg -- function arg never decodes to name another function arg, we would have expanded it instead
    while (1) //   we have a list of things, either () or { } or [ ].  We check each item until one fails in () or one succeeds in  [ ]
    {
        unsigned int oldp = positionStart; //   in case we fail and need to restore. Also to confirm legal advance
        unsigned int olde = positionEnd;
		unsigned int id;

		//   script compiler has already insured all tokens have space separators... 
		//   and *ptr is a significant character each time thru this loop
		ptr = ReadCompiledWord(ptr,word);	//   ptr always moved to significant character after
		if (trace && TraceIt(PATTERN_TRACE))
		{
			if (*word && *word != '(' && *word != '[' && *word != '{') 
			{
				char* at = word;
				if (*word == '=') at += 2; //   skip = flag and accelerator
				if (returned) Log(STDUSERTABLOG,"%s ",at); //   restore indent level from before
				else Log(STDUSERLOG,"%s ",at);
			}
			returned = false;
		}
		char c = *word;
        switch(c) 
        {
			case '@' :
				 if (word[1] == '_') // assign position
				 {
					 wildcardIndex = word[2]-'0';
					 positionStart = positionEnd = wildcardPosition[wildcardIndex++];
					 continue;
				 }
                break;
            case '!': //   NOT condition - not a real token, always used with a following token
                statusBits |= NOT_BIT;
				if (word[1]) ptr -= strlen(word) - 1; // function argument like !~vegatable can creep thru
				continue;
			case '\'': //   single quoted item - set quote flag
                statusBits |= QUOTE_BIT;
 				if (word[1]) ptr -= strlen(word); // function argument like _~vegatable can creep thru
	            continue;
            case '_': //     memorization coming - there can be up-to-two memorizations in progress: _* and _xxx 
				
				// if we are going to memorize something AND we previously matched inside a phrase, we need to move to after...
				if ((positionStart - positionEnd) == 1)// eg. I travel here_and_there sometimes - matching "here"
				{
					positionEnd = positionStart;	// refuse to match within anymore
				}
				
				//   GAP requires no numbers for forward or - for backward
				wildcardSelector |= (*ptr == '*' && !IsDigit(ptr[1]) && ptr[1] != '-') ? WILDGAP :  WILDSPECIFIC;
				if (word[1]) ptr -= strlen(word) - 1; // function argument like _~vegatable can creep thru
				continue;
			case '<': //   sentence start marker OR << >> set
				//   if (wildcardSelector & WILDSPECIFIC) blocked during init, so ignore here
                if (gap ) //   close to end of sentence 
                {
                    positionStart = lastwordIndex + 1; //   pretend to match at end of sentence
					int start = gap & 0x000000ff;
					unsigned int limit = (gap >> 8);
                    gap = 0;   //   turn off
  					if ((positionStart - start) > limit) //   too long til end
					{
						matched = false;
 						wildcardSelector &= -1 ^ WILDGAP;
						break;
					}
                    if (wildcardSelector & WILDGAP) 
					{
						SetWildCard(start,lastwordIndex);  //   legal swallow of gap //   request memorize
 						wildcardSelector &= -1 ^ WILDGAP;
					}
                }

				if (word[1] == '<') //   << 
				{
					statusBits |= FREEMODE_BIT;
					positionStart = INFINITE_MATCH;
					positionEnd = startposition;  //   allowed to pick up after here - oldp/olde synch automatically works
				}
                else positionStart = positionEnd = 0; //   idiom < * and < _* handled under *
                continue;
            case '>': //   sentence end marker
				if (word[1] == '>') //   >> closer
				{
					statusBits &= -1 ^ FREEMODE_BIT; //   positioning left for a start of sentence
					continue;
				}

                if (statusBits & NOT_BIT) //   asks that we NOT be at end of sentence
                {
                    matched =  positionEnd != lastwordIndex; 
					statusBits &= -1 ^ NOT_BIT;
                }
                else //   declares a match at end (position ptr set there)
                {
					//   if a wildcard started, end it... eg _* > 
					if (gap)
					{
						int start = (gap & 0x000000ff);
						int limit = gap >> 8;
						int diff = (lastwordIndex + 1) - start;  //   ended - spot it started
						gap = 0;   //   turn off
						if (diff > limit) //   too long til end
						{
							matched = false;
							wildcardSelector &= -1 ^ WILDGAP;
							break;
						}

						if (wildcardSelector & WILDGAP)  
						{
							if ( diff ) SetWildCard(start,lastwordIndex);  //   legal swallow of gap
							else SetWildCard("", "",NULL,lastwordIndex);
						}
						wildcardSelector &= -1 ^ WILDGAP;
						olde = oldp = lastwordIndex;	//   insure we synch up correctly
						positionStart = positionEnd = lastwordIndex + 1; //   pretend to match a word off end of sentence
						matched = true;
					}
					else if (positionEnd == wordCount) 
					{
						positionStart = positionEnd = lastwordIndex + 1; //   pretend to match a word off end of sentence
						matched = true; 
					}
					else matched = false;
				}
                break;
             case '*': //   GAP - accept anything (perhaps with length restrictions)
				if (word[1] == '-') //   backward adjust -1 is word before us
				{
                    int count = (word[2] - '0'); //   LIMIT 9 back
					int at = positionEnd - count - 1; 
					if (at >= 0) //   no earlier than pre sentence start
					{
						olde = olde = at; //   set last match BEFORE our word
						positionStart = positionEnd = at + 1; //   cover the word now
						matched = true; 
					}
					else matched = false;
				}
				else if (IsDigit(word[1]))  //   fixed length match, treat as a matching word phrase
                {
                    unsigned int count = word[1] - '0'; //   limit 9 
					unsigned int at = positionEnd + count;
                    if (at <= lastwordIndex )
                    { //   fake match on specified word count
                        positionStart = positionEnd + 1 ; //   fake match is here -  wildcard gets the gap
                        positionEnd = at; //   resume scan will be here
                        matched = true; //   transparently moved forward 
                    }
                    else  matched = false;
                }
                else //   there MUST be something after this, safe to use continue
                {
                    if (word[1] == '~') gap = (word[2]-'0') << 8; //   LIMIT 9
                    else 
					{
						gap = 200 << 8;                         //   200 is infinitely large
						if (positionStart == 0) positionStart = INFINITE_MATCH; //   idiom < * resets to allow match anywhere
					}
                    gap |= positionEnd  + 1;
					continue;
                }
                break;
            case '$': //   user variable
				matched = (HandleRelation(word,"!=","",false,id) & ENDCODES) ? 0 : 1;
                break;
            case '^': //   function call/argument/global argument
                if  (IsDigit(word[1]) || word[1] == '$' || word[1] == '_') //   macro argument substitution or global macro var
                {
					++globalDepth;
                    argumentText = ptr; //   transient substitution of text

					if (IsDigit(word[1]))  ptr = macroArgumentList[atoi(word+1)+userCallArgumentBase];  //   LIMIT 9 macroArgumentList
					else if (word[1] == '$') ptr = GetUserVariable(word+1);
					else ptr = wildcardCanonicalText[word[2]-'0'];
					ptr = SkipWhitespace(ptr);
					continue;
                }
                
				D = FindWord(word,0); 
				if (!D) //   must be found or script compiler would be unhappy - but just in case toavoid crash
				{
					matched = false;
					break;
				} 

				if (D->x.codeIndex) //   builtin function - does not update position data
                {
					char* old = currentOutputBase;
					currentOutputBase = AllocateBuffer();
					ptr = DoFunction(word,ptr,currentOutputBase,result);
					FreeBuffer();
					currentOutputBase = old;
					matched = !(result & (ENDRULE_BIT|FAILRULE_BIT|FAILTOPIC_BIT|ENDTOPIC_BIT| ENDSENTENCE_BIT| ENDSENTENCE_BIT)); 
                    break;
                }
 
                //   user macro call
      			globalDepth++;
                if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERLOG,"("); //   function call
		        baseStack[functionNest] = userCallArgumentBase; //   old base saved
                argStack[functionNest] = userCallArgumentIndex; //   old extent saved
                ptr += 2; //   skip the ( and space
                while (*ptr != ')' && *ptr)  //   read til end of args
                {
                    char* spot = macroArgumentList[userCallArgumentIndex++];
                    ptr = ReadArgument(ptr,spot) ; //   ptr returns on next significant char  //   skip space after each arg - this does not EVAL the arg, it just gets it
                    if (*spot == '^' && spot[1] == '#' ) strcpy(spot,macroArgumentList[atoi(spot+2)+userCallArgumentBase]); //   function arg need to switch out to incoming arg NOW, because later the userCallArgumentBase will be wrong
                    if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERLOG," %s, ",spot); //   display macroArgumentList
                }
                if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERLOG,")\r\n"); //   macroArgumentList ended
                userCallArgumentBase = argStack[functionNest]; //   now change the decode base, AFTER we have decoded any macroArgumentList
                ptrStack[functionNest] = ptr+2; //   skip closing paren and space
                ++functionNest;
                ptr = D->w.fndefinition; //   substitute text in-line in execution -- stops when it runs out.
                continue;
           case 0: //   ran out of data (havent failed yet) , only happens with argument or function substitutions
                //   function arg substitution ends
                if (argumentText) // return to normal
                {
					--globalDepth;
                    ptr = argumentText;
                    argumentText = NULL;
                    continue;
                }

                //   function call ended
                else if (functionNest > startNest)
                {
 					if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,""); 
					--functionNest;
                    userCallArgumentIndex = argStack[functionNest]; //   end of argument list (for next argument set)
                    userCallArgumentBase = baseStack[functionNest]; //   base of macroArgumentList
                    ptr = ptrStack[functionNest];
					globalDepth--;
                    continue;   //   resume prior processing
                }

                else 
				{
					ReportBug("abnormal end"); 
					return false;
				}
                break;
            case '(': case '[':  case '{': //   nested condition (required or optional) (= consecutive  [ = choice   { = optional
                hold = wildcardIndex;
				{
					int oldgap = gap;
					unsigned int returnStart = positionStart,returnEnd = positionEnd;
					unsigned int oldselect = wildcardSelector;
					wildcardSelector = 0;
					// nest inherits gaps leading to it. memorization requests withheld til he returns
					matched = Match(ptr,depth+1,positionEnd,*word, positionStart == INFINITE_MATCH,gap,wildcardSelector,returnStart,returnEnd); //   subsection ok - it is allowed to set position vars, if ! get used, they dont matter because we fail
					wildcardIndex = hold; //   flushes all wildcards swallowed within
					wildcardSelector = oldselect;
					if (matched) 
					{
						positionStart = returnStart;
						positionEnd = returnEnd;
						if (positionEnd) olde = positionEnd - 1; //   nested checked continuity, so we allow match whatever it found - but not if never set it (match didnt have words)
						if (wildcardSelector)
							gap = oldgap;	// to know size of gap
					}
				}
				ptr = BalanceParen(ptr); //   ptr becomes AFTER ) } or ], closing out the list
       			returned = true;
				if (!matched) //   subsection failed, revert wildcard index - IF ! gets used, we will need this
                {
  				    if (*word == '{') 
                    {
						if (wildcardSelector & WILDSPECIFIC) //   we need to memorize failure because optional cant fail
						{
							wildcardSelector ^= WILDSPECIFIC;
							SetWildCard(0, (unsigned int)-1); //   null match
						}

                        if (gap) continue;   //   if we are waiting to close a wildcard, ignore our failed existence entirely
                        statusBits |= NOT_BIT; //   we didnt match and pretend we didnt want to
                    }
   					else wildcardSelector = 0;
                }
                else if (*word == '{') //   was optional, revert the wildcard index (keep position data)
                {
                    wildcardIndex = hold; //   drop any wildcards bound (including reserve)
                    if (!matched) continue; //   be transparent in case wildcard pending
                }
                else if (wildcardSelector > 0)  wildcardIndex = hold; //   drop back to this index so we can save on it 
                break;
            case ')': case ']': case '}' :  //   end sequence/choice/optional
				matched = (kind == '('); //   [] and {} must be failures if we are here
				if (gap) //   pending gap  -  [ foo fum * ] and { foo fot * } are pointless but [*3 *2] is not 
                {
					if (depth != 0) //   he cant end with a gap, we dont allow it for simplicity
					{
						gap = wildcardSelector = 0;
						matched = false; //   force match failure
					}
					else positionStart = lastwordIndex + 1; //   at top level a close implies > )
				}
                break; 
            case '"':  //   double quoted string - bug re 1st position? //   find LITERAL sequence (except for flipped words)
				matched = FindSequence(word,(positionEnd < basicStart && firstMatched < 0) ? basicStart: positionEnd);
				if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
				break;
            case '%': //   system variable
				if (!word[1]) //   the simple % being present
				{
					matched = MatchTest(FindWord(word),(positionEnd < basicStart && firstMatched < 0) ? basicStart: positionEnd,NULL,NULL,statusBits & QUOTE_BIT); //   possessive 's
					if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
				}
                else matched = HasProperty(word,NULL);
                break;
            case '?': //   question sentence? or var in sentence?
				if (!word[1]) matched = tokenFlags & QUESTIONMARK;
	            break;
            case '=': //   a comparison test - never quotes the left side. Right side could be quoted
				//   format is:  = 1-bytejumpcodeToComparator leftside comparator rightside
				if (!word[1]) //   the simple = being present
				{
					matched = MatchTest(FindWord(word),(positionEnd < basicStart && firstMatched < 0)  ? basicStart : positionEnd,NULL,NULL,statusBits & QUOTE_BIT); //   possessive 's
					if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
				}
				//   if left side is anything but a variable $ or _ or @, it must be found in sentence and
				//   THAT is what we compare against
				else 
				{
					compare = word + uncode[word[1]];

					char op[10];
					char* opptr = compare;
					op[0] = *opptr++;
					op[1] = 0;
					if (*opptr == '=') // was == op
					{
						op[1] = '=';
						op[2] = 0;
						++opptr;
					}
					*compare = 0;		//   temporarily separate left and right tokens

					if (*opptr == '^') // local function argument, global var and global _
					{
						char* ptr;
						if (opptr[1] == '$') ptr = GetUserVariable(opptr+1);
						else if ( opptr[1] == '_') ptr = wildcardCanonicalText[opptr[2]-'0'];
						else if (IsDigit(opptr[1])) ptr = macroArgumentList[atoi(opptr+2)+userCallArgumentBase];
						ptr = SkipWhitespace(ptr);
						strcpy(opptr,ptr);
					}

					// find $ in sentence
					if (*op == '?' && word[2] == '$'  && opptr[0] != '~')
					{
						char* val = GetUserVariable(word+2);
						matched = MatchTest(FindWord(val),(positionEnd < basicStart && firstMatched < 0) ? basicStart : positionEnd,NULL,NULL,statusBits & QUOTE_BIT); //   MUST match later 
						if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
						break;
					}
				
					// find _ in sentence
					if (*op == '?' && word[2] == '_' && opptr[0] != '~')
					{
						char* val = wildcardCanonicalText[word[3]-'0'];
						matched = MatchTest(FindWord(val),(positionEnd < basicStart && firstMatched < 0) ? basicStart: positionEnd,NULL,NULL,statusBits & QUOTE_BIT); //   MUST match later 
						if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
						break;
					}
					char* at = &word[2];
					bool quoted = false;
					if (*at == '\'') 
					{
						++at; //   there is a quoted left side
						quoted = true;
					}
					result = *at;
					if (result == '|' || result == '%' || result == '$' || result == '_' || result == '@')  //   test directly (it will decode)
					{
						result = HandleRelation(word+2,op,opptr,false,id); 
						matched = (result & ENDCODES) ? 0 : 1;
					}
					else //   FIND and then test
					{
						matched = MatchTest(FindWord(at),(positionEnd < basicStart && firstMatched < 0) ? basicStart : positionEnd,op,opptr,quoted); //   MUST match later 
						if (!matched) break;
					}
 				}
				break;
            case '\\': //   escape to allow [ ] () < > ' {  } ! as words
				if (word[1] == '!') matched =  (lastwordIndex && tokenFlags & EXCLAMATIONMARK) != 0; //   exclamatory sentence
                else 
				{
					matched =  MatchTest(FindWord(word+1),(positionEnd < basicStart && firstMatched < 0) ? basicStart : positionEnd,NULL,NULL,statusBits & QUOTE_BIT); //   MUST match later 
					if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
				}
                break;
			case '~': // current topic ~ and named topic
				if (word[1] == 0)
				{
					matched = IsCurrentTopic(topicID);
					break;
				}
			default: //   ordinary words, concept/topic, numbers, : and ~ and | and & accelerator
				matched = MatchTest(FindWord(word),(positionEnd < basicStart && firstMatched < 0) ? basicStart : positionEnd,NULL,NULL,statusBits & QUOTE_BIT); //   MUST match later 
				if (!(statusBits & NOT_BIT) && matched && firstMatched < 0) firstMatched = positionStart; //   first SOLID match
         } 
		statusBits &= -1 ^ QUOTE_BIT; //   turn off any pending quote

        if (statusBits & NOT_BIT && matched) //   flip success to failure
        {
            matched = false; 
            statusBits &= -1 ^ NOT_BIT;
            positionStart = oldp; //   restore any changed position values (if we succeed we would and if we fail it doesnt harm us)
            positionEnd = olde;
        }

		//   prove GAP was legal
 		unsigned int started = positionStart;
        if (gap && matched) 
        {
			started = (gap & 0x000000ff);
			if ((positionStart - started) <= (gap >> 8)) olde = positionStart;   //   we know this was legal, so allow advancement test not to fail- matched gap is started...olde-1
			else  
			{
				matched = false; ; //   more words than limit
				wildcardSelector &= -1 ^ WILDGAP; //   turn off any save flag
			}
        }

		if (matched)
		{
			if (wildcardSelector) //   memorize ONE or TWO things (ap will have been legal)
			{
				if (started == INFINITE_MATCH) started = 1;
				if (wildcardSelector & WILDGAP) //   would be first if both
					SetWildCard(started,positionStart-1);  //   wildcard legal swallow between elements
				if (positionStart == INFINITE_MATCH) positionStart = 1;
				if (wildcardSelector & WILDSPECIFIC)
					SetWildCard(positionStart,positionEnd);  //   specific swallow 
			}
			gap = wildcardSelector = 0; /// should NOT clear this inside a [] or a {} on failure
		}
		else //   fix side effects of anything that failed by reverting
        {
            positionStart = oldp;
            positionEnd = olde;
  			if (kind == '(') gap = wildcardSelector = 0; /// should NOT clear this inside a [] or a {} on failure since they must try again
        }

         //   end sequence/choice/optional
        if (*word == ')' || *word ==  ']' || *word == '}') 
        {
			if (matched)
			{
				if (statusBits & GAPPASSBACK ) //   passing back a gap at end of nested (... * )
				{
					gap = statusBits & GAPPASSBACK;
					wildcardSelector =  statusBits & (WILDSPECIFIC|WILDGAP);
				}
			}
            if (matched && firstMatched > 0)  matched = firstMatched; //   tell spot we matched for top level retry
			success = matched != 0; 

			if (success && argumentText) //   we are ok, but we need to resume old data
			{
				--globalDepth;
				ptr = argumentText;
				argumentText = NULL;
				continue;
			}

			break;
        }

		//   postprocess match of single word or paren expression
		if (statusBits & NOT_BIT) //   flip failure result to success now (after wildcardsetting doesnt happen because formally match failed first)
        {
            matched = true; 
			statusBits &= -1 ^ NOT_BIT;
         }

		//   word ptr may not advance more than 1 at a time (allowed to advance 0 - like a string match or test)
        //   But if initial start was INFINITE_MATCH, allowed to match anywhere to start with
        if (matched && oldp != INFINITE_MATCH && positionStart > (olde + 1) && positionStart != INFINITE_MATCH ) 
        {
			matched = false;
			if ((int)positionEnd == firstMatched) firstMatched = 0; //   cannot retry-- first match failed on position
        }

        //   now verify position of match, NEXT is default for (type, not matter for others
        if (kind == '(') //   ALL must match in sequence
        {
			//   we failed, retry shifting the start if we can
			if (!matched)
			{
				if (wildstart && firstMatched > 0) //   we are top level and have a first matcher, we can try to shift it
				{
					if (trace && TraceIt(PATTERN_TRACE)) Log(STDUSERTABLOG,"retry past %d ----------- ",firstMatched);
					//   reset to initial conditions, mostly 
					ptr = orig;
					wildcardIndex = 0; 
					basicStart = positionEnd = firstMatched;  //   THIS is different from inital conditions
					firstMatched = -1; 
					positionStart = INFINITE_MATCH; 
					gap = 0;
					wildcardSelector = 0;
					statusBits &= -1 ^ (NOT_BIT | FREEMODE_BIT);
					if (argumentText) --globalDepth;
					argumentText = NULL; 
					continue;
				}
				break; //   default fail
			}
			if (statusBits & FREEMODE_BIT) 
			{
				positionEnd = startposition;  //   allowed to pick up after here
				positionStart = INFINITE_MATCH; //   re-allow anywhere
			}
		}
        else if (matched /* && *word != '>' */ ) // was could not be END of sentence marker, why not???  
        {
			if (argumentText) //   we are ok, but we need to resume old data
			{
				ptr = argumentText;
				argumentText = NULL;
				--globalDepth;
			}
			else
			{
				success = true; //   { and [ succeed when one succeeeds 
				break;
			}
		}
    } 

	//   begin the return sequence
	
	if (functionNest > startNest)//   since we are failing, we need to close out all function calls in progress at this level
    {
		globalDepth -= (functionNest - startNest);
        userCallArgumentIndex = argStack[startNest];
        userCallArgumentBase = baseStack[startNest];
		functionNest = startNest;
    }
	
	if (success)
	{
		returnstart = (firstMatched > 0) ? firstMatched : positionStart; // if never matched a real token, report 0 as start
		returnend = positionEnd;
	}

	//   if we leave this level w/o seeing the close, show it by elipsis 
	//   can only happen on [ and { via success and on ) by failure
	if (trace && depth && TraceIt(PATTERN_TRACE))
	{
		if (*word != ')' && *word != '}' && *word !=  ']')
		{
			if (success) Log(STDUSERTABLOG,"%c ... +%d:%d ",(kind == '{') ? '}' : ']',positionStart,positionEnd);
			else Log(STDUSERTABLOG,") ... - ");
		}
		else if (*word == ')') Log(STDUSERLOG,"+%d:%d ",positionStart,positionEnd);
	}
	
	globalDepth--;
    return success; 
}

