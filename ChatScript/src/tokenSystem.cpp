// tokenSystem.cpp - convert text stream into tokens


#include "common.h"

#ifdef INFORMATION

SPACES		 \t \r \n 
PUNCTUATIONS  , | -  (see also ENDERS )
ENDERS		 . ; : ? ! -
BRACKETS 	 () [ ] { } < >
ARITHMETICS	  % * + - ^ = / .  
SYMBOLS 	  $ # @ ~  
CONVERTERS	  & `
//NORMALS 	  A-Z a-z 0-9 _  and sometimes / 

#endif
int cmt = 0;
static char* FindWordEnd(char* ptr,char* priorToken,char** words,unsigned int &count, bool &ignore)
{
	//   special behaviors at start of token

    //   quoted words?
	char c = *ptr; 
	unsigned char kind = punctuation[(unsigned char)c];
	char* end  = NULL;
	if (kind & QUOTERS) 
	{
		end = strchr(ptr+1,c); //   matching end?
		if (end && !IsAlphaOrDigit(end[1])) //   end is not in middle of a word or number
		{
			// if quote has a tailing comma or period, move it outside of the end - "Pirates of the Caribbean," 
			if (*(end-1) == ',')
			{
				*(end-1) = *end;
				*end-- = ',';
			}
			else if (*(end-1) == '.')
			{
				*(end-1) = *end;
				*end-- = '.';
			}
			char d = punctuation[(unsigned char)end[1]];
			if (d & (SPACES|PUNCTUATIONS|ENDERS)) 
			{
				if (c == '*') //   stage direction notation, erase it and return to normal processing
				{
					*end = ' ';	//   erase the closing * of a stage direction
					ignore = true;
					return ptr + 1;
				}
				else 
				{
					//   quoted words which are only alphanumeric single words (emphasis quoting)
					//   strip off the quotes
					char* at = ptr;
					while (++at < end)
					{
						if (!IsAlphaOrDigit(*at)) //   worth quoting, unless it is final char and an ender
						{
							if (at == (end-1) && punctuation[(unsigned char)*at] & ENDERS);
							else //   store string as properly tokenized, NOT as a string.
							{
								char word[MAX_WORD_SIZE];
								ReviseQuoteToUnderscore(ptr,end,word);
								words[++count] = AllocateString(word,0); 
								ignore = true;
								return end+1; //   worth quoting
							}
						}
					}
					words[++count] = AllocateString(ptr+1,end-ptr-1); 
					ignore = true;
					return end+1; //   worth quoting
				}
			}
		}
    }

	char next = ptr[1];
	if (kind & BRACKETS) //   keep all brackets separate
	{
		if (next != '=' || (c != '>' && c != '<')) return ptr+1; //   but <= and >= are operations, not brackets
	}

	if (kind & CONVERTERS) //   disallow him certain characters, rewrite them
	{
		if (c == '`') c = *ptr = '\'';	//   we use this internally as a marker, dont let him use it
		else if (c == '&') //   we need to change this to "and"
		{
			words[++count] = AllocateString("and",3); 
			ignore = true;
			return ptr + 1;
		}
	}

	//   arithmetic operator between numbers (. won't be seen because would have been swallowed already if part of a float, 
	else if (kind & ARITHMETICS && IsDigit(next) && IsDigit(*priorToken) && c != '-' && c != '+') return ptr+1;  	//   separate operators from number, except sign 
 	else if (kind & ARITHMETICS && IsDigit(next) && wordCount && IsDigit(*words[count]) && (c == '-' || c == '+')) return ptr+1;  	//   separate operators from number, except sign 
 	
	//   normal punctuation separation
	else if (c == '.' && IsDigit(ptr[1]));	//   float start like .24
	else if (kind & (ENDERS|PUNCTUATIONS) && ((unsigned char)punctuation[ptr[1]] == SPACES || ptr[1] == 0)) 
	{
		return ptr+1;  //   stand alone punctuation 
	}
 	
	//   find "normal" word end, including all touching nonwhitespace, keeping periods for now
 	end = ptr;
	while (*++end && !IsWhiteSpace(*end) && *end != '?' && *end != '!'&& *end != ',' && *end != ';'); //   these too are logical ends
	char* start = ptr;
	bool hasnumbercolon = false;
	while (*++ptr) //   now scan to see how far we really go, we go to end unless something special happens
    {
		c = *ptr;
		kind = punctuation[(unsigned char)c];

		// 5% becomes 2 tokens
		if (c == '%' && ptr != start && IsDigit(*(ptr-1)) && !IsAlpha(ptr[1]))
		{
			break;
		}

		//   Keep all brackets as separate tokens  ([ { < > } ] ) 
		if (kind & (BRACKETS|SPACES)) break;	

		if (kind & CONVERTERS) //   characters we don't allow the user to keep
		{
			if (c == '`') c = *ptr = '\'';	//   never allow user to enter this, we use it specially in topics
			else if (c == '&') break;		//   becomes its own "and" later  AT&T becomes AT and T
		}

		next = ptr[1];
		//   normal enders and special . behavior
		if (c == '.') //   is period part of word or number?
        {
			if (IsUrl(start,end)) return end; //   swallow URL as a whole
            else if (IsFloat(start,end) && IsDigit(ptr[1])) continue; //   decimal number9
	        else if (*start == '$' && IsFloat(start+1,end) && IsDigit(ptr[1])) continue; //   decimal number9 or money
			else if (IsNumericDate(start,end)) return end;	//   swallow period date as a whole - bug . after it?
			else if (!strnicmp("no.",start,3) && IsDigit(next)); //   no.8  
			else if (!strnicmp("no.",start,3)) break; //   sentence: No.
			else if (IsMadeOfInitials(start,end) == ABBREVIATION) return end; //   word of initials is ok
			else if (FindWord(start,(end-start))) continue;	// nov.  recognized by system for later use
			else if ( next == '-') continue;	// N.J.-based
			break;	//   not considered part of word
        }
		//   fails to handle 23 - 3- 4 but who cares
		else if (c == '-' && IsDigit(*start) && IsDigit(next)) break;	//   treat as minus
		else if (kind & (PUNCTUATIONS|ENDERS) && (IsWhiteSpace(next) || !next)) break; //   anything interesting at end of word will stand alone, normal stuff
		else if (c == '\'') //   do we have a possessive? if we do, we separate ' or 's into own word
		{
			//   12'6" or 12'. or 12' 
			if (IsDigit(*start) && !IsAlpha(next)) return ptr + 1;  //   12' swallow ' into number word
			if (next == 's' && !IsAlphaOrDigit(ptr[2])) 
			{
				break;	//   's becomes separate -- but MIGHT BE generator's going down.   if next word is present participle, change to is? bug
			}
			if (!IsAlphaOrDigit(next)) break;	//   ' not within a word
		}
		else if (c == '/' && IsNumericDate(start,end)) return end; //     12/20/1993
		else if (kind & ARITHMETICS && IsDigit(next)) //   numeric operations?
		{
			if (c != '/' && IsDigit(*start)) break;  //   59*5 => 59 * 5  but 59 1/2 stays
		}
		else if ( kind & ARITHMETICS && count && IsDigit(*start)) break;  // 1+1=
		else if (c == ':' && IsDigit(next) && IsDigit(*(ptr-1))) hasnumbercolon = true;  //   time 10:30 or odds 1:3
		else if (c == '\'') //   possessive ' or 's
		{
			if (punctuation[(unsigned char)next] == PUNCTUATIONS) break;	//   simple ' at end
			if (next == 's' && punctuation[(unsigned char)ptr[2]] == PUNCTUATIONS) break;	//   's
		}
    } 
	//   mashed time 8:00am? separate it
	if (hasnumbercolon)
	{
		if (!strnicmp(end-2,"am",2)) return end-2;
		else if (!strnicmp(end-2,"pm",2)) return end-2;
		else if (!strnicmp(end-3,"a.m",3)) return end-3;
		else if (!strnicmp(end-3,"p.m",3)) return end-3;
		else if (!strnicmp(end-4,"a.m.",4)) return end-4;
		else if (!strnicmp(end-4,"p.m.",4)) return end-4;
	}
    return ptr;
}

static WORDP MergeProperNoun(int& start, int end,bool upperStart) 
{
	uint64 human_sex = 0;
	char buffer[MAX_WORD_SIZE];
	char buffer1[MAX_WORD_SIZE];
    char* ptr = buffer;
	WORDP D;
	unsigned int size = end - start;
    for (int k = start; k < end; ++k)
    {
		char* word = wordStarts[k];
        unsigned int len = strlen(word);
		D = FindWord(word,len,UPPERCASE_LOOKUP);
		if (D) human_sex |= D->properties & (NOUN_HE|NOUN_SHE|NOUN_HUMAN);
 		D = FindWord(word,len,LOWERCASE_LOOKUP);
		if (D) human_sex |= D->properties & (NOUN_HE|NOUN_SHE|NOUN_HUMAN);
        if (word[0] == ',' || word[0] == '?' || word[0] == '!' || word[0] == ':') --ptr;  //   remove the understore before it
        strcpy(ptr,word);
        ptr += len;
        if (k < (end -1)) *ptr++ = '_';
    }
    *ptr = 0;
	*buffer = toUppercaseData[(unsigned char)*buffer]; //   make it start uppercase if we can

	D = FindWord(buffer,0,UPPERCASE_LOOKUP);

	if (start > 1) //   see if determiner before is known, like The Fray or Title like Mr.
	{
		WORDP E = FindWord(wordStarts[start-1],0,UPPERCASE_LOOKUP);
		if (E && !(E->properties & NOUN_TITLE_OF_ADDRESS)) E = NULL;
		if (!E) 
		{
			E = FindWord(wordStarts[start-1],0,LOWERCASE_LOOKUP);
			if (E && !(E->properties & DETERMINER)) E = NULL;
		}
		if (E) 
		{
			strcpy(buffer1,E->word);
			strcat(buffer1,"_");
			strcat(buffer1,buffer);
			*buffer1 = toUppercaseData[(unsigned char)*buffer1]; 
			if (E->properties & DETERMINER)
			{
				WORDP F = FindWord(buffer1);
				if (F) 
				{
					--start;
					D = F; //   we know it
					++size;
				}
			}
			else //   accept title as part of unknown name
			{
				strcpy(buffer,buffer1);
				--start;
				++size;
			}
		}
	}
	if (size == 1) return NULL;	//   dont bother, we already have this word in the sentence
	if (!D && !upperStart) return NULL; //   neither know in upper case nor does he try to create it
	if (D) strcpy(buffer,D->word); //   use prior known capitalization
	
	return StoreWord(buffer,human_sex|NOUN_PROPER_SINGULAR|NOUN); 
}

static bool HasCaps(char* word)
{
    if (IsMadeOfInitials(word,word+strlen(word)) == ABBREVIATION) return true;
    if (!IsUpperCase(*word)) return false;
    if (strlen(word) == 1) return false;
    while (*++word)
    {
        if (!IsUpperCase(*word)) return true; //   do not allow all caps as such a word
    }
    return false;
}

static WORDP FindValidUppercase(char* word,unsigned int len)
{
	WORDP D = FindWord(word,len,UPPERCASE_LOOKUP);
	if (D && !(D->properties & ESSENTIAL_FLAGS)) D = NULL;	//   not real 
	return D;
}

static void ForceUpper(unsigned int i)
{
	if (IsLowerCase(wordStarts[i][0])) //   make it upper case
	{
		wordStarts[i] = AllocateString(wordStarts[i],0);
		*wordStarts[i] = toUppercaseData[(unsigned char)*wordStarts[i]]; 
	}
}

void ProcessCompositeName()
{
    int start = UNINIT;
    int end = UNINIT;
    uint64 kind = 0;
	bool upperStart = false;
    for (unsigned int i = 1; i <= wordCount; ++i) 
    {
		char* word = wordStarts[i];
        unsigned int len = strlen(word);

		//   if the word is a quoted expression, see if we KNOW it already as a noun, if so, remove parens
		if (*word == '"' && len > 2)
		{
			char buffer[MAX_WORD_SIZE];
			strcpy(buffer,word);
			char* at;
			while ((at = strchr(buffer,' '))) *at = '_';	//   convert spaces to dictionary underscore storage
			WORDP E = FindWord(buffer+1,len-2,UPPERCASE_LOOKUP);
			if (E) wordStarts[i] = E->word;
			continue;
		}

        WORDP D = FindValidUppercase(word,len);
		WORDP L = FindWord(word,len,LOWERCASE_LOOKUP);
		WORDP N;
		if (L && !IsUpperCase(word[0])) D = L;			// has lower case meaning, he didnt cap it, assume its lower case
		else if (L && i == 1 && L->properties & (PREPOSITION | PRONOUN | CONJUNCTION_BITS) ) D = L; // start of sentence, assume these are NOT in name

		if (!D) D = L; //   ever heard of this word? we ignore unreal words (like idiom headers)
		else if (i == 1 && L &&  L->properties & AUX_VERB_TENSES && (N  = FindWord(wordStarts[2])) && N->properties & PRONOUN_BITS) continue;	// obviously its not Will You but its will they
		else if (start == UNINIT && IsLowerCase(*word) && L && L->properties & (ESSENTIAL_FLAGS|QWORD|WH_ADVERB)) //   he didnt capitalize it himself and its a useful word
		{
			continue;	//   dont try to make this proper by default
		}

		//   given human first name as starter or a title
        if (D && D->properties & (NOUN_FIRSTNAME|NOUN_TITLE_OF_ADDRESS) && start == UNINIT)
        {
			upperStart = false;
			if (i != 1 && IsUpperCase(wordStarts[i][0])) upperStart = true;
			start = i; 
			kind = 0;
            end = UNINIT;    //   have no potential end yet
            if (i < wordCount) //   have a last name? or followed by a prepositon? 
            {
				unsigned int len1 = strlen(wordStarts[i+1]);
				WORDP F = FindWord(wordStarts[i+1],len1,LOWERCASE_LOOKUP);
				if (F && F->properties & (CONJUNCTION_BITS | PREPOSITION | PRONOUN)) //   dont want river in the to become River in the or Paris and Rome to become Paris_and_rome
				{
					start = UNINIT;
					++i;
					continue;
				}
				
				WORDP E  = FindWord(wordStarts[i+1],len1,UPPERCASE_LOOKUP);
				if (E && !(E->properties & ESSENTIAL_FLAGS)) E = NULL;	//   not real

                if (IsUpperCase(wordStarts[i+1][0]) || E) //   it's either capitalized or we know it is capitalizable
                {
					upperStart = true;	//   must be valid
					ForceUpper(i);		//   make sure its now upper case
                    ++i; 
					ForceUpper(i); 	//   make sure its now upper case
                    continue;
                }
           }
        }
		
		if (i == 1)
        {
			WORDP N;
			if (wordCount > 1 && *wordStarts[2] == 'I' && !wordStarts[2][1]) continue; //   2nd word is I, 1st word is not likely title but questionword or verb
			if (D && D->properties & AUX_VERB_TENSES && (N = FindWord(wordStarts[2])) && N->properties & PRONOUN_BITS) continue; // ignore 
 			if (D && D->properties & (DETERMINER|QWORD|WH_ADVERB)) continue;   //   ignore starting with a determiner or question word(but might have to back up later to accept it)
        }

        bool intended = HasCaps(word) && i != 1;
        uint64 type = (D) ? (D->systemFlags & (TIMEWORD|SPACEWORD)) : 0;
		if (!kind) kind = type;
        else if (kind && type && kind != type) intended = false;   //   cant intermix time and space

        //   Indian food is not intended
		if (intended || (D && D->properties & (NOUN_PROPER_SINGULAR|NOUN_PROPER_PLURAL|NOUN_TITLE_OF_ADDRESS))) //   cap word or proper name can start
        {
			if (D && D->properties & POSSESSIVE); //   not Taiwanese President
			else if (L && L->properties & QWORD); // ignore WHO for who
            else if (start == UNINIT) //   havent started yet, start now
            {
	 			upperStart = (intended && i != 1);  //   he started it properly or not
                start = i;  //   begin a series
				kind = (D) ? (D->systemFlags & (TIMEWORD|SPACEWORD)) : 0;
            }
            if (end != UNINIT) end = UNINIT;    //   swallow a word along the way that is allowed to be lower case
        }
        else if (start != UNINIT) //   lowercase may break the name, unless they turn out to be followed by more uppercase
        {
			// Do not allow lower case particles and connectors after a comma...  Shell Oil, the Dutch group 
            if (D && D->properties & LOWERCASE_TITLE && i > 1 && wordStarts[i-1][0] != ',' ) //   allowable particles and connecting words that can be in lower case
			{
				//   be wary of and joining two known proper names like Paris and London
				if (!strcmp(D->word,"and"))
				{
					char word[MAX_WORD_SIZE];
					char* at = word;
					for (unsigned int j = start; j < i; ++j)
					{
						strcpy(at,wordStarts[i]);
						at += strlen(at);
						if (j != (i-1)) *at++ = '_';
					}
					*at = 0;
					if (FindWord(word,at-word,UPPERCASE_LOOKUP)) goto resumeEnd;//   we know this, go no farther
				}

				//   be careful with possessives. we dont want London's Millenium Eye to match or Taiwanese President
				if (D->properties & POSSESSIVE && (i-start) == 1) start = end = UNINIT;
				else end = i;	//   potential ender
				continue;
			}
            if (*word == ',' && i < wordCount) //   comma series
            {
				end = i;
				continue;
			}
               //   Rogers, Howell, & Partner, Inc. 
               //   Portland, Oregon
			   //   unknown , Oregon
 resumeEnd:
            //   the sequence has ended
            if (end == UNINIT) end = i;
	
            //   a 1-word title gets no change. also entire short sentence gets ignored
            if (end == (int)wordCount && start == 1 && end < 5) start = end = UNINIT; 
			else if ( (end-start) == 1 && !intended) start = end = UNINIT; //   The best is yet should not trigger "the_best"
			else //   make title
			{
				WORDP E = MergeProperNoun(start,end,upperStart);
				if (E) 
				{
					AddSystemFlag(E,kind); // timeword spaceword
					//   replace now
					wordStarts[start] = E->word;
					memcpy(wordStarts+start+1,wordStarts+end,sizeof(char*) * (wordCount - end + 1));
					wordCount -= (end - start - 1);
					--end;
				}
				i = end; 
				start = end = UNINIT;
			}
       }
    }

    if (start != UNINIT ) //   proper noun is pending 
    {
        if (end == UNINIT) end = wordCount+1;
        WORDP E = NULL;
		unsigned int size = end-start;
        if (size > 1) 
		{
			E = MergeProperNoun(start,end,upperStart); 
			if (E) 
			{
				AddSystemFlag(E,kind);// timeword spaceword
				wordStarts[start] = E->word;
				memcpy(wordStarts+start+1,wordStarts+end,sizeof(char*) * (wordCount + 1 - end));
				wordCount -= (end - start - 1);
			}		
		}
    }

	//   postprocess the starting word of sentece
    WORDP D = FindWord(wordStarts[1],0,UPPERCASE_LOOKUP); //   try for the upper case of it
	WORDP E = FindWord(wordStarts[1],0,LOWERCASE_LOOKUP);
	WORDP N;
	char sing[MAX_WORD_SIZE];
	MakeLowerCopy(sing,wordStarts[1]);
	char* noun = GetSingularNoun(sing);
	if (noun && !stricmp(sing,noun)) // must preserve hypens, etc-- Flynn-Bell should not become bell
	{
		WORDP E = FindWord(noun);
		if (E && stricmp(wordStarts[1],E->word)) wordStarts[1] = E->word;
	}
	else if (E && E->properties & (CONJUNCTION_BITS|PRONOUN|PREPOSITION)) // simple word lower case it
	{
		if (stricmp(wordStarts[1],"I") && stricmp(wordStarts[1],E->word)) wordStarts[1] = E->word;
	}
	else if (E && E->properties & AUX_VERB_TENSES && (N = FindWord(wordStarts[2])) && (N->properties & (PRONOUN_BITS | NOUN_BITS) || GetSingularNoun(wordStarts[2]))) 
	{
		if (stricmp(wordStarts[1],E->word)) wordStarts[1] = E->word;
	}
	else if (D && !E) 
	{
		if (stricmp(wordStarts[1],D->word)) wordStarts[1] = D->word; //   use upper case for proper noun or I
		return;
	}

	if (wordStarts[1][0] == 'I' && wordStarts[1][1] == 0) return; // I allowed to remain uppercase as sentence start

	char* multi = strchr(wordStarts[1],'_');
	if (!multi || !IsUpperCase(multi[1])) //   remove sentence start uppercase unless its a multi-word title
	{
		char word[MAX_WORD_SIZE];
		strcpy(word,wordStarts[1]);
		MakeLowerCase(word);
		if (strcmp(wordStarts[1],word)) 
		{
			wordStarts[1] = StoreWord(word,0)->word; 
		}
	}
	else if (multi) 
	{
		wordStarts[1] = StoreWord(wordStarts[1],NOUN_PROPER_SINGULAR)->word; // implied proper noun of some kind
	}
}

static void MergeNumbers(unsigned int start,unsigned int end) //   four score and twenty = four-score-twenty
{//   start thru end exclusive of end, but put in number power order if out of order (four and twenty becomes twenty-four)
    char word[MAX_WORD_SIZE];
    char* ptr = word;
    for (unsigned int i = start; i < end; ++i)
    {
		char* item = wordStarts[i];
        if (*item == ',') continue; //   ignore commas

        unsigned int len = strlen(wordStarts[i]);
		//   one thousand one hundred and twenty three
		//   OR  one and twenty 
        if (*item == 'a' || *item == 'A') //   and,  maybe flip order if first, like one and twenty, then ignore
        {
			int power1 = NumberPower(wordStarts[i-1]);
			int power2 = NumberPower(wordStarts[i+1]);
			if (power1 < power2) //   latter is bigger than former --- assume nothing before and just overwrite
			{
				strcpy(word,wordStarts[i+1]);
                ptr = word + strlen(word);
                *ptr++ = '-';
                strcpy(ptr,wordStarts[i-1]);
                ptr += strlen(ptr);
                break;
			}
            continue;
        }

        strcpy(ptr,item);
        ptr += len;
        *ptr = 0;
        if (i != start) //   prove not mixing types digits and words
        {
            if (*word == '-' && !IsDigit(*item)) return; //   - not a sign?
            else if (*word == '-'); //   leading sign
            else if (IsDigit(*word) != IsDigit(*item)) return; //   mixing words and digits
        }
        if (i < (end-1) && !IsDigit(*item) && *item != '-') *ptr++ = '-'; //   hypenate words (not digits )
        else if (i < (end-1) && strchr(wordStarts[i+1],'/')) *ptr++ = '-'; //   is a fraction? BUG
    }
    *ptr = 0;

	//   change any _ to - (substitutions or wordnet might have merged using _
	while ((ptr = strchr(word,'_'))) *ptr = '-';

	if (strchr(word,'-'))
	{
		int value = Convert2Integer(word);
		sprintf(word,"%d",value);
	}

	//   create the single word and replace all the tokens
    WORDP D = StoreWord(word,ADJECTIVE|NOUN|ADJECTIVE_CARDINAL|NOUN_CARDINAL); 
    wordStarts[start] = D->word;
	memcpy(wordStarts+start+1,wordStarts+end,sizeof(char*) * (wordCount + 1 - end ));
	wordCount -= (end - start) - 1;	//   deleting all but one of the words
}

static bool Substitute(char* sub, unsigned int i,unsigned int erasing)
{ //   erasing is 1 less than the number of words involved
	if ( sub) ++sub;	// skip the | 
	// see if we have test condition to process (starts with !) and has [ ] with list of words to NOT match after
	if (sub && *sub== '!')
	{
		if (*++sub != '[') // not a list, a bug
		{
			ReportBug("bad substitute %s",sub);
			return false;
		}
		else
		{
			char word[MAX_WORD_SIZE];
			bool match = false;
			char* ptr = sub+1;
			while (1)
			{
				ptr = ReadSystemToken(ptr,word);
				if (*word == ']') break;
				if ( *word == '>')
				{
					if ( i == wordCount) match = true;
				}
				else if (!stricmp(wordStarts[i+1],word)) match = true;
			}
			if (match) return false;	// not to do
			sub = ptr;	// here is the thing to sub
			if (!*sub) sub = 0;
		}
	}

	int erase = 1 + (int)erasing;
	if (!sub) //   just delete the word  
	{
		if (trace & SUBSTITUTE_TRACE) 
		{
			Log(STDUSERLOG,"substitute erase:  ");
			for (unsigned int j = i; j < i+erasing+1; ++j) Log(STDUSERLOG,"%s ",wordStarts[j]);
			Log(STDUSERLOG,"\r\n");
		}
		memcpy(wordStarts+i,wordStarts+i+erasing+1,sizeof(char*) * (wordCount-i-erasing));
		wordCount -= erase;
		return true;
	}
	if (*sub == '%') //   note tokenbit and then delete
    {
		if (trace & SUBSTITUTE_TRACE) Log(STDUSERLOG,"substitute flag:  %s\r\n",sub+1);
		memcpy(wordStarts+i,wordStarts+i+erasing+1,sizeof(char*) * (wordCount-i-erasing));
		tokenFlags |= (int)FindValueByName(sub+1);
		wordCount -= erase;
		return true;
	}

	//   substitution match
	if (!strchr(sub,'+') && erasing == 0 && !strcmp(sub,wordStarts[i])) 
		return false; //   changing single word case to what it already is?
	
    char wordlist[MAX_WORD_SIZE];
    strcpy(wordlist,sub); //   bypass the initial marker used to prevent collision with other entries
    char* ptr = wordlist;
    while ((ptr= strchr(ptr,'+'))) *ptr = ' '; //   change + separators to spaces but leave _ alone

	char* tokens[1000];			//   the new tokens we will substitute
	char* backupTokens[1000];	//   place to copy the old tokens
	unsigned int count;
    Tokenize(wordlist,count,tokens); //   get the tokenization

	if (count == 1 && !erasing) //   simple replacement
	{
		if (trace & SUBSTITUTE_TRACE) Log(STDUSERLOG,"substitute replace: %s with %s\r\n",wordStarts[i],tokens[1]);
		wordStarts[i] = tokens[1];
		return true;
	}
	
	if (trace & SUBSTITUTE_TRACE) Log(STDUSERLOG,"substitute replace: \"%s\" length %d\r\n",wordlist,erase);
	memcpy(backupTokens,wordStarts + i + erasing + 1,sizeof(char*) * (wordCount - i - erasing )); //   save old tokens
	memcpy(wordStarts+i,tokens+1,sizeof(char*) * count);					//   move in new tokens
	memcpy(wordStarts+i+count,backupTokens,sizeof(char*) * (wordCount - i - erasing ));	//   restore old tokens
	wordCount += count - erase;
	return true;
}

static bool ViableIdiom(WORDP word,unsigned int i,unsigned int n)
{
    if (!word) return false;

    //   dont swallow - before a number
    if (i < wordCount && IsDigit(*wordStarts[i+1]))
    {
        char* name = word->word;
        if (*name == '-' && name[1] == 0) return false;
        if (*name == '<' && name[1] == '-' && name[2] == 0) return false;
    }

	if (word->systemFlags & SUBSTITUTE) return true; // marked as a sub

    if (word->properties & (PUNCTUATION|COMMA|PREPOSITION) && n) return true; //   multiword prep is legal
    if (word->multiwordHeader) return false; //   if it is not a name or intejrection or preposition, we dont want to use the wordnet composite word list (but linkparser may have it as idiom, so we keep it around)
 
	//   exclude titles of works unless he gives them in caps
	if (word->properties & NOUN_TITLE_OF_WORK && wordStarts[i][0] >= 'a' && wordStarts[i][0] <= 'z' )
		return false;

    if (n && IsUpperCase(word->word[0]) && word->properties & PART_OF_SPEECH) 
    {
        return true;//    We will merge proper names. Means words declared ONLY as interjections wont convert in other slots
    }
	if (!n) return false;
    if (word->properties & (NOUN|ADJECTIVE|ADVERB)) return true; //   WAS FALSE BEFORE--- but now we accept words that are listed by US as mergable, not by wordnet - if its a composite
	if (word->properties & VERB) // dont merge particle verbs, let POS tagger handle them
	{
		char* part = strrchr(word->word,'_');
		if (!part) return true;	// SHOULDNT happen
		WORDP P = FindWord(part+1);
		if (P && P->properties  & PREPOSITION) return false;
		return true;
	}
	return false;
}

static bool ProcessIdiom(unsigned int i,unsigned int max,char* buffer,WORDP D, char* ptr,unsigned int &cycles)
{//   buffer is 1st word, ptr is end of it
    WORDP word;
    WORDP found = NULL;
    unsigned int idiomMatch = 0;

	unsigned int n = 0;
    for (unsigned int j = i; j <= wordCount; ++j)
    {
		if (j != i) //   add next word onto original starter
		{
			if (!stricmp(loginID,wordStarts[j])) break; //   BUG? user name cannot be swallowed. END idiom
			*ptr++ = '_'; //   separator between words
			++n; //   appended word count
			strcpy(ptr,wordStarts[j]);
			ptr += strlen(ptr);
		}
    
		//   we have to check both cases, because idiomheaders might accidently match a substitute
		WORDP localfound = found; //   we want the longest match, but do not expect multiple matches at a particular distance
		if (i == 1 && j == wordCount)  //   try for matching at end AND start
        {
			word = NULL;
			*ptr++ = '>'; //   end marker
			*ptr-- = 0;
			word = FindWord(buffer,0,PRIMARY_CASE_ALLOWED);
			if (ViableIdiom(word,1,n)) 
			{
				found = word;  
				idiomMatch = n;     //   n words ADDED to 1st word
			}
			if (found == localfound)
			{
				word = FindWord(buffer,0,SECONDARY_CASE_ALLOWED);
				if (ViableIdiom(word,1,n)) 
				{
					found = word;  
					idiomMatch = n;     //   n words ADDED to 1st word
				}			
			}
			*ptr = 0; //   remove tail end
		}
		if (found == localfound && i == 1 && (word = FindWord(buffer,0,PRIMARY_CASE_ALLOWED)) && ViableIdiom(word,1,n))
		{
			found = word;   
			idiomMatch = n;   
		}
 		if (found == localfound && i == 1 && (word = FindWord(buffer,0,SECONDARY_CASE_ALLOWED)) && ViableIdiom(word,1,n)) 
		{
			found = word;   
			idiomMatch = n;   
		}
        if (found == localfound && (word = FindWord(buffer+1,0,PRIMARY_CASE_ALLOWED)) && ViableIdiom(word,i,n))
        {
			found = word; 
			idiomMatch = n; 
		}
        if (found == localfound && (word = FindWord(buffer+1,0,SECONDARY_CASE_ALLOWED)) && ViableIdiom(word,i,n)) // used to not allow upper mapping to lower, but want it for start of sentence
        {
			if (!IsUpperCase(buffer[1]) || i == 1) // lower can always try upper, but upper can try lower ONLY at sentence start
			{
				found = word; 
				idiomMatch = n; 
			}
		}
        if (found == localfound && j != 1 && j == wordCount)  //   sentence ender
		{
			*ptr++ = '>'; //   end of sentence marker
			*ptr-- = 0;  
			word = FindWord(buffer+1,0,PRIMARY_CASE_ALLOWED);
			if (ViableIdiom(word,0,n))
            {
				found = word; 
				idiomMatch = n; 
			}
			if (found == localfound)
			{
				word = FindWord(buffer+1,0,SECONDARY_CASE_ALLOWED);
				if (ViableIdiom(word,0,n))
				{
					found = word; 
					idiomMatch = n; 
				}
			}
			*ptr= 0; //   back to normal
        }
		if (found == localfound && *(ptr-1) == 's' && j != i) // try singularlizing a noun
		{
			unsigned int len = strlen(buffer+1);
			word = FindWord(buffer+1,len-1,PRIMARY_CASE_ALLOWED|SECONDARY_CASE_ALLOWED); // remove s
			if (len > 3 && !word && *(ptr-2) == 'e') 
				word = FindWord(buffer+1,len-2,PRIMARY_CASE_ALLOWED|SECONDARY_CASE_ALLOWED); // remove es
			if (len > 3 && !word && *(ptr-2) == 'e' && *(ptr-3) == 'i') // change ies to y
			{
				char noun[MAX_WORD_SIZE];
				strcpy(noun,buffer);
				strcpy(noun+len-3,"y");
				word = FindWord(noun,0,PRIMARY_CASE_ALLOWED|SECONDARY_CASE_ALLOWED);
			}
			if (word && ViableIdiom(word,i,n)) // was composite
			{
				char* second = strchr(buffer,'_');
				if ( !IsUpperCase(word->word[0]) || (second && IsUpperCase(second[1]))) // be case sensitive in matching composites
				{
					found = StoreWord(buffer+1,NOUN_PLURAL); // generate the plural
					idiomMatch = n; 
				}
			}
		}

        if (n == max) break; //   peeked ahead to max length so we are done
	} //   end J loop

	if (!found) return false;

	D = found->substitutes;
    if (D == found)  return false;

	bool result = false;
	
	//   dictionary match to multiple word entry
	if (!(found->systemFlags & SUBSTITUTE))
	{
		if (trace & SUBSTITUTE_TRACE) 
		{
			Log(STDUSERLOG,"substitute multiword: %s for ",found->word);
			for (unsigned int j = i;  j < i + idiomMatch+1; ++j) Log(STDUSERLOG,"%s ",wordStarts[j]);
			Log(STDUSERLOG,"\r\n");
		}
		wordStarts[i] = AllocateString(found->word);
		memcpy(wordStarts+i+1,wordStarts+i+idiomMatch+1,sizeof(char*) * (wordCount - i - idiomMatch)); //   delete the rest
		wordCount -= idiomMatch;

		result =  true;
	}
	else result = Substitute(D ? D->word : NULL,i,idiomMatch);//   do substition

	if (result && cycles > 30) //   warn about overflow
	{
		ReportBug("Substitute cycle overflow on %s\r\n",found->word);
		result = false;
	}
	return result;
}

static bool SpellItBetter()
{
    //   Substitutions are done before spell check so spell check doesnt alter any idioms
	bool changed = false;
    for (unsigned int i = 1; i <= wordCount; ++i)
    {
		char* word = wordStarts[i];
		unsigned int len = strlen(word); //   check out every word
        if (*word == '\'' || *word == '"') continue;

		//   do we know about this word already
		if (FindWord(wordStarts[i],len)) continue;
		char* canon = FindCanonical(wordStarts[i],i);
		if (canon && wordStarts[i] != canon) continue; //   it found a canonical way
		if (IsMadeOfInitials(word,word+len) == ABBREVIATION) 
		{
			StoreWord(word,NOUN); 
			continue; //   legal anyway
		}
		if (IsUrl(word,word+len)) 
		{
			StoreWord(word,NOUN); 
			continue; 
		}
        
		word = NULL;
		if (word) 
		{
			changed = true;
			wordStarts[i] = word;
		}
	}
	return changed;
}

void ProcessSubstitutes() //   revise contiguous words based on LIVEDATA/substitutes.txt
{
    char buffer[MAX_WORD_SIZE];
    buffer[0] = '<'; //   sentence start marker
retry:
    unsigned int cycles = 0;
    for (unsigned int i = 1; i <= wordCount; ++i)
    {
		if (!stricmp(loginID,wordStarts[i])) continue; //   BUG- example? user name cannot be swallowed. skip

        unsigned int len = strlen(wordStarts[i]);

		//   put word into buffer
		char* ptr = buffer+1;
        strcpy(ptr,wordStarts[i]);
        ptr += len;

        //   can this start a substition?  It must have an idiom count != ZERO_IDIOM_COUNT
 
        unsigned int count = 0;
		WORDP D = FindWord(buffer+1,0,PRIMARY_CASE_ALLOWED); //   main word
  		if (D) count = D->multiwordHeader;
        
		//   does secondary head longer phrases?
        WORDP E  = FindWord(buffer+1,0,SECONDARY_CASE_ALLOWED);
		if (E && E->multiwordHeader > count) count = E->multiwordHeader;

        //   now see if start-bounded word does even better
        if (i == 1) 
        {
			unsigned int startCount = 0;
			D = FindWord(buffer,0,PRIMARY_CASE_ALLOWED); //   with < header
            if (D) startCount = D->multiwordHeader;
			D = FindWord(buffer,0,SECONDARY_CASE_ALLOWED);
			if (D && D->multiwordHeader > startCount) startCount = D->multiwordHeader;
			if (startCount > count) count = startCount;
 		}

		//   now see if end-bounded word does even better
		if (i == wordCount)
        {
			unsigned int endCount = 0;
            *ptr++ = '>'; //   append boundary
            *ptr-- = 0;
            D = FindWord(buffer+1,0,PRIMARY_CASE_ALLOWED);
            if (D) endCount = D->multiwordHeader;
			D = FindWord(buffer+1,0,SECONDARY_CASE_ALLOWED);
			if (D &&  D->multiwordHeader > endCount) endCount = D->multiwordHeader;

			if (i == 1) //   can use start and end simultaneously
			{
				D = FindWord(buffer,0,PRIMARY_CASE_ALLOWED); //   with < header
				if (D) endCount = D->multiwordHeader;
				D = FindWord(buffer,0,SECONDARY_CASE_ALLOWED);
				if (D && D->multiwordHeader > endCount) endCount = D->multiwordHeader;
			}
			if (endCount > count) count = endCount;
			*ptr = 0;	//   remove tail boundary
		}
        
		//   use max count
        if (count && ProcessIdiom(i,count-1,buffer,D,ptr,cycles)) 
		{
			i = 0;  //   restart since we modified sentence
			++cycles;
		}
	}

	if (SpellItBetter()) goto retry;
}

//make multiword numbers goto digit form instead of word form... but single word numbers remain. 
//	allowing us to manage seeing 'one'  'first' etc.  (hell with movie and book titles)  7 of 9 
//   555 222 566  is a number in international
void ProcessCompositeNumber()
{
    //   convert a series of numbers into one hypenated one or remove commas from a comma-digited string
    unsigned int start = (unsigned int)UNINIT;
    unsigned int end = (unsigned int)UNINIT;
    for (unsigned int i = 1; i <= wordCount; ++i) 
    {
        bool isnum = IsNumber(wordStarts[i]) && !IsPlaceNumber(wordStarts[i]) && wordStarts[i][0] != '$'; // not money either
		unsigned int len = strlen(wordStarts[i]);
        //   tokenizer swallows plus signs directly attach
        //   is a number OR is a starting minus in front of a number
        if (isnum || (start == UNINIT && *wordStarts[i] == '-' && i < wordCount && IsDigit(*wordStarts[i+1]))) //   number word
        {
            if (start == UNINIT)  start = i;  //   begin a series
            if (end != UNINIT) end = (unsigned int)UNINIT;    //   swallow a word along the way that is allowed to be there  == and  , 
        }
        else
        {
            if (start == UNINIT) continue; //   there is no sequence started

            //   keep , if followed and preceeded by digits (tokenizer separated numbers into separate words
            if (wordStarts[i][0] == ',' ) 
            {
                if (IsDigit(wordStarts[i-1][0]) && i < wordCount && IsDigit(wordStarts[i+1][0]))
                {
                    if (strlen(wordStarts[i+1]) == 3) //   trailing must be 3 digits
                    {
                        end = i; //   potential stopper
                        continue;
                    }
                }
            }
            //   keep AND if followed and preceeded by non-digits -- also need to keep and if same scale of number
            else if (!strnicmp("and",wordStarts[i],len)) 
            {
				end = i;
				if (IsDigit(wordStarts[i-1][0]) || i == wordCount || IsDigit(wordStarts[i+1][0]));
                else continue;
            }

            //   this definitely breaks the sequence
            if (end  == UNINIT) end = i;
            if ((end-start) == 1) 
            {
                start = end = (unsigned int)UNINIT;
                continue; //   no change if its a 1-length item
            }

			//  and cannot merge numbers like 1 2 3  instead numbers after the 1st digit number must be triples (international)

			if (IsDigit(wordStarts[start][0]))
			{
				for (unsigned int i = start + 1; i <= end; ++i) 
				{
					if (strlen(wordStarts[i]) != 3) 
					{
						end = UNINIT;
						break;
					}
				}
			}

            if (end != UNINIT) MergeNumbers(start,end);
            i = start + 1;
            end = start = (unsigned int)UNINIT;
        }
    }

    if (start != UNINIT) //   merge is pending
    {
        if (end  == UNINIT) end = wordCount+1; //   drops off the end
		int count = end-start;
        if (count > 1) 
		{
			//   dont merge a date-- number followed by comma 4 digit number - January 1, 1940
			//   and 3 , 3455 or   3 , 12   makes no sense either. Must be 3 digits past the comma
			//  and cannot merge numbers like 1 2 3  instead numbers after the 1st digit number must be triples (international)

			if (IsDigit(wordStarts[start][0]))
			{
				for (unsigned int i = start + 1; i <= end; ++i) 
				{
					if (strlen(wordStarts[i]) != 3) return;
				}
			}

			unsigned nextLen = strlen(wordStarts[start+1]);
			if (count != 2 || !IsDigit(*wordStarts[start+1]) || nextLen == 3) MergeNumbers(start,end); 
		}
    }
}

char* Tokenize(char* input,unsigned int &count,char** words,bool all) //   return ptr to stuff to continue analyzing later
{ 
    char* ptr = input;
	count = 0;
	char* priorToken = "";
	while (ptr) //   find all tokens til end of sentence or end of tokens
	{
		ptr = SkipWhitespace(ptr);
		if (!*ptr) break; 
		while (*ptr == ptr[1] && !IsAlphaOrDigit(*ptr)) ++ptr; //   skip repeated non-alpha non-digit characters (... becomes .)

		//   find end of word
		bool ignore = false;
		char* end = FindWordEnd(ptr,priorToken,words,count,ignore); 
		if (ignore) //   FindWordEnd already stored something
		{
			ptr = end;
			continue;
		}
		int len = end - ptr;
		
		//   reserve next word, unless we have too many
		if (++count >= (MAX_SENTENCE_LENGTH-1)) return ptr;  //   discard excess words - -1 allows the "feet" extra word to be safe

		//   handle symbols for feet and inches by expanding them
		if (*(end-1) == '\'' && IsDigit(*ptr)) //   PRESUME this is a number with ' at end
		{
			words[count] = AllocateString(ptr,len-1);  //   the number w/o the '
			words[++count] = AllocateString("feet",4); //   replace the ' with spelled out
			ptr = end;
			continue;
		}
		else if (*(end-1) == '"' && IsDigit(*ptr)) //   PRESUME this is a number with " at end
		{
			words[count] = AllocateString(ptr,len-1);  //   the number w/o the "
			words[++count] = AllocateString("inches",4); //   replace the " with spelled out
			ptr = end;
			continue;
		}

 		char* token = words[count] = priorToken = AllocateString(ptr,len);   

		if (len == 1 && *token == 'i') *token = 'I'; //   force uppser case on I

		//   set up for next token
		ptr = end;
		char c = *token;

		//   end sentence
		if (all || c == ',');
		else if (c == ':' && (strchr(ptr,',') || strstr(ptr,"and")));	//   assume its a colon for a list, not a sentence join ad assume we are not grabbing the comma from an unrelated sentence
		else if (punctuation[(unsigned char)c] & ENDERS) //   done a sentence or fragment
		{
			ptr = SkipWhitespace(ptr);
			if (c == '-')
			{
				if (IsDigit(*ptr)); //   dont confuse minus with hyphen here.
				else
				{
					*token = '.';
					break;
				}
			}
			else break;	
		}
	}
	return ptr;
}
