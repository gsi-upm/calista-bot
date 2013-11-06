// spellcheck.cpp - fixes bad spelling in input


#include "common.h"
static char* SpellFix(char* word,unsigned int position,uint64 posflags);
#define LIST_BY_SIZE 1
#define LIST_BY_PREFIX 2
#define LIST_BY_SUFFIX 3

static bool BadWord(WORDP D) // dont correct these
{
	if (!(D->properties & PART_OF_SPEECH) && D->systemFlags & AGE_LEARNED) return true;//   we know when it is learned, but it has no part of speech?

	//   accept all funny punctuations as well as substitute values
	if (D->word[1] && !strchr(D->word,'+') && !strchr(D->word,'.') && D->word[0] != '~' && D->word[0] != '%' && D->word[0] != '$' && !(D->properties & PART_OF_SPEECH)  && D->multiwordHeader ) //   it has no utility?
	{
		char* verb = GetInfinitive( D->word );
		if (verb) 
		{
			WORDP E = FindWord(verb);
			if (E &&  E->properties & VERB) return true;
		}
		char* noun = GetSingularNoun( D->word );
		if (noun) 
		{
			WORDP E = FindWord(noun);
			if (E &&  E->properties & NOUN) return true;
		}
		return true;
	}
	//   dont write out regular verb parts (other than present) if that's all they are - mobypos sometimes lists pasts before presents, so gets them thru system
	if ((D->properties & PART_OF_SPEECH) == VERB  && !D->conjugation && D->multiwordHeader && !(D->properties & VERB_INFINITIVE))
	{
		char* verb = GetInfinitive( D->word );
		if (verb) 
		{
			WORDP E = FindWord(verb);
			if (E &&  E->properties & VERB) return true;
		}

		int n = BurstWord(D->word);
		if (!strchr(D->word,'_')) n = 1;	//   not really breakable into words
		if ( n > 1) //   phrase-- verb phrase?
		{
			WORDP f = FindWord(GetBurstWord(1));
			if (f && f->properties & VERB) return true;	//   accept it as it is. 
			char* verb = GetInfinitive(GetBurstWord(1));
			if (verb) 
			{
				f = FindWord(verb);
				if (f && f->properties & VERB) return true;	//   accepted as a verb tense of a composite word, with 1st as verb
			}
		}
	}
	return false;
}

#define MAX_SPELL_LENGTH 30 //   28 is largest non-coined nontechnical word
static WORDP bysizePrior[MAX_SPELL_LENGTH + 1];

static void BySize(WORDP D,void* data)
{
	if ( D->systemFlags & DELETED_WORD) return; //   cancelled word
	if ( !(D->properties & PART_OF_SPEECH) || D->properties & WORDNET_ID ) return;   //   has no POS data
	if (BadWord(D)) return;

	char* at = D->word-1;
	while (*++at && (IsAlpha(*at) || *at == '-')); //   check for non-abbr, non_ non funny stuff
	if (*at) return;  //   has funny stuff in it, not a real single word

	unsigned int len = strlen(D->word);
	if ( len > MAX_SPELL_LENGTH) return;    //   wrong size word
 	D->spellBySize = bysizePrior[len];
	bysizePrior[len] = D;
	// now do same 4 start and end lists
	if (len < 4) return;	

	char* word = (char*) data;

	//   store prefix
	strcpy(word+2,D->word);
	word[6] = 0;  
	word[1] = 'p';
	WORDP base = StoreWord(word);
	WORDP next =  base->spellByPrefix;
	D->spellByPrefix = next;
	base->spellByPrefix = D;

	if ( len == 4) return;    // no value in looking backward since it is the same

	//   store suffix
	strcpy(word+2,D->word+len-4);
	word[1] = 's';
	base = StoreWord(word,0);
	next = base->spellBySuffix;
	D->spellBySuffix = next;
	base->spellBySuffix = D;
}

void  InitSpellChecking()
{    
    char word[MAX_WORD_SIZE];
    word[0] = '$';

	// lists of words by size and with same start and end characters
	memset(bysizePrior,0,sizeof(bysizePrior));
	WalkDictionary(BySize,word);
    for (unsigned int n = 1; n <= MAX_SPELL_LENGTH; ++n)
	{
        if (bysizePrior[n]) //   we have such a list, save it on a header
        {
            char word[MAX_WORD_SIZE];
            sprintf(word,"%d",n);
            strcat(word,"%size");
            WORDP D = StoreWord(word,0);
            D->spellBySize = bysizePrior[n];
        }
	}
}

static char* SpellCheck(unsigned int i)
{
    //   on entry we will have passed over words which are KnownWord (including bases) or isInitialWord (all initials)
    //   wordstarts from 1 ... wordCount is the incoming sentence words (original). We are processing the ith word here.
    char* word = wordStarts[i];

	if (!stricmp(word,loginID) || !stricmp(word,computerID)) return word; //   dont change his/our name

    //   spell check does not handle pluralized nouns (with s)... it will remove the s.
    unsigned int len = strlen(word);

    //   test for run togetherness
    int breakAt = 0;
    unsigned int k;

    //   test for run together numbers and word (like 10mm film)
    char num[MAX_WORD_SIZE];
    WORDP D1 = NULL;
    WORDP D2 = NULL;
    if (IsDigit(word[0]))
    {
        while (IsDigit(word[++breakAt]) || word[breakAt] == '.'); //   find end of number
        if (!word[breakAt] || !FindWord(word+breakAt,0,PRIMARY_CASE_ALLOWED)) breakAt = 0; 
        else 
        {
            strncpy(num,word,breakAt);
            num[breakAt] = 0;
            D1 = StoreWord(num,ADJECTIVE|NOUN|ADJECTIVE_CARDINAL|NOUN_CARDINAL|NOUN_ORDINAL); //  store in dictionary now
        }
    }
    //   try general run together nesss- imagine all combinations of breaking the word into two
    if (!breakAt) for (k = 1; k < len-1; ++k)
    {
        if (k == 1 && word[0] != 'a' && word[0] != 'A' && word[0] != 'i' && word[0] != 'I') continue; //   only a and i are allowed single-letter words
        D1 = FindWord(word,k,PRIMARY_CASE_ALLOWED);
        if (!D1) continue;
        D2 = FindWord(word+k,len-k,PRIMARY_CASE_ALLOWED);
        if (!D2) continue;
        bool good1 = (D1->properties & PART_OF_SPEECH) != 0 || (D1->systemFlags & SUBSTITUTE) != 0; 
		if (!good1) continue;
        bool good2 = (D2->properties & PART_OF_SPEECH) != 0 || (D2->systemFlags & SUBSTITUTE) != 0;
		if (!good2) continue;

		// dont allow weird words to break into
		uint64 n1 = D1->systemFlags & AGE_LEARNED;
		uint64 n2 = D2->systemFlags & AGE_LEARNED;
		if (n1 & KINDERGARTEN || n2 & KINDERGARTEN );
		else continue;

        if (!breakAt) breakAt = k;
		else 
        {
           breakAt = -1; //   multiple interpretations, give up
           break;
		}
    }
    if (breakAt > 0)//   we found a split, insert 2nd word into word stream
    {
        ++wordCount;
        for (k = wordCount; k > i; --k) wordStarts[k] = wordStarts[k-1]; //   make room for new word
        wordStarts[i+1] = wordStarts[i]+breakAt;
        WORDP D = FindWord(wordStarts[i],breakAt,PRIMARY_CASE_ALLOWED);
        return (D) ? D->word : NULL; //   1st word gets replaced, we added another word after
    }

	char word1[MAX_WORD_SIZE];

    //   test for split words that should be joined
    //   try this word with the word before
    if (i != 1 && len > 1 && strlen(wordStarts[i-1]) > 1) // post ulate him
    {
        strcpy(word1,wordStarts[i-1]);
        strcat(word1,wordStarts[i]);
 		if (FindCanonical(word1,0))
		{
			wordStarts[i-1] = AllocateString(word1);
			memcpy(wordStarts+i,wordStarts+i+1,sizeof(char*) * (wordCount - i));
			--wordCount;
			return "";
		}
    }

    //   try this word with the word after
    if (i != wordCount && strlen(wordStarts[wordCount]) > 1) // dott ed pair
    {
        strcpy(word1,wordStarts[i]);
        strcat(word1,wordStarts[i+1]);
		if (FindCanonical(word1,0))
		{
			char* answer = AllocateString(word1);
			memcpy(wordStarts+i+1,wordStarts+i+2,sizeof(char*) * (wordCount - i -1));
			--wordCount;
			return answer;
		}
    }

    //   remove any nondigit characters repeated. Dont do this earlier, we want substitutions to have a chance at it first.  ammmmmmazing
    char* ptr = word-1; 
	char* ptr1 = word1;
    while (*++ptr)
    {
	   *ptr1 = *ptr;
	   while (ptr[1] == *ptr1 && (*ptr1 < '0' || *ptr1 > '9')) ++ptr; // skip repeats
	   ++ptr1;
    }
	*ptr1 = 0;
	if (FindCanonical(word1,0)) return AllocateString(word1);

	//   now use word spell checker 
    char* d = SpellFix(word,i,PART_OF_SPEECH); 
    if (d) 
	{
		return AllocateString(d);
	}

    return NULL;
}

void SpellCheck()
{
	for (unsigned int i = 1; i <= wordCount; ++i)
	{
		char* word = wordStarts[i];
		WORDP D = FindWord(word);
		if (D && (D->properties & PART_OF_SPEECH || D->word[0] == '~')) continue;	// we know this word clearly or its a concept set ref emotion
		if (D && D->systemFlags & PATTERN_WORD ) continue;	// we know this word clearly or its a concept set ref emotion

		// see if hypenated word should be separate or joined
		char* hyphen = strchr(word,'-');
		if (hyphen)
		{
			char test[MAX_WORD_SIZE];
			*hyphen = 0;

			// test for split
			strcpy(test,hyphen+1);
			WORDP D = FindWord(word);
			WORDP E = FindWord(test);
			if (D && E) //   1st word gets replaced, we added another word after
			{
				++wordCount;
				for (unsigned int k = wordCount; k > i; --k) wordStarts[k] = wordStarts[k-1]; //   make room for new word
				wordStarts[i+1] = E->word;
				wordStarts[i] = D->word;
			}
			else // remove hyphen entirely?
			{
				strcpy(test,word);
				strcat(test,hyphen+1);
				D = FindWord(test);
				if (D) wordStarts[i] = D->word;
				*hyphen = '-';
			}
			continue; // IGNORE hypenated errors that we couldnt solve, because no one mistypes a hypen
		}

		if (*word != '\'' && !FindCanonical(word, i)) // dont check quoted or findable words
		{
			word = SpellCheck(i);
			if (word && !*word) // performed substitution on prior word, restart this one
			{
				--i;
				continue;
			}
			if (word) wordStarts[i] = word;
		}
    }
}

static bool SetPositions(char* word, char* set)
{
    --word;
    char c;
    int position = 0;
    while ((c = *++word) && position < 250)
    {
        if (c == '_') return false;    // its a phrase, not a word
        set[position++] = toLowercaseData[(unsigned char) c];
   }
    return true;
}

unsigned int EditDistance(char* dictword, unsigned int inputLen, char* inputSet, unsigned int min,unsigned char set1[257])
{//   dictword has no underscores, inputSet is already lower case
    char dictw[MAX_WORD_SIZE];
    MakeLowerCopy(dictw,dictword);
    char* dictinfo = dictw;
    unsigned int size = strlen(dictinfo);
    char* dictstart = dictinfo;
    unsigned int val = 0; //   a difference in length will manifest as a difference in letter count
    //   how many changes  (change a letter, transpose adj letters, insert letter, drop letter)
    if (size != inputLen) val += 3;    //  we prefer equal length
    //   first and last letter errors are rare, more likely to get them right
    else if (dictinfo[size-1] != inputSet[inputLen-1]) val += 3; // costs more to change last letter, odds are he types that right or sees its wrong
     
	unsigned char set[257];
	memset(set,0,257); // uninitialized is fine. just need them to start out the same, doesnt matter what it is. and only checking normal ascii
	for (unsigned int  i = 0; i < size; ++i) ++set[dictinfo[i]];// for length of dictionary word, not user input
	unsigned int count = 0;
	for (unsigned int  i = 0; i < 128; ++i) 
		if (set[i] && set[i] == set1[i]) 
			++count; // only need to check normal ascii
	if ((int)count < (size - (size > 7) ? 3 : 2)) 
		return 60;	// not close enough, need 2 or 3 (longer words) letters in common
	if (count == size)  // same letters (though he may have excess) --  how many transposes
	{
		unsigned int bad = 0;
		for (unsigned int i = 0; i < size; ++i) if (dictinfo[i] != inputSet[i]) ++bad;
		if (bad <= 2) return val + 3; // 1 transpose
		else if (bad <= 4) return val + 9; // 2 transpose
		else return val + 38; // many transposes
    }
	
    char* dictend = dictinfo+size;
    char* inputend = inputSet+inputLen;
	count = 0;
    while(1)
    {
		++count;
        if (*dictinfo == *inputSet) //   they match
        {
            if (inputSet == inputend && dictinfo == dictend) break;    //   they ended
            ++dictinfo;
            ++inputSet;
            continue;
        }
        if (inputSet == inputend || dictinfo == dictend) //   one ending, other has to catch up
        {
            if (inputSet == inputend) ++dictinfo;
            else ++inputSet;
            val += 9;
            continue;
        }
        //   match failed

        //   try to resynch series and reduce cost of a transposition of adj letters  
        if (*dictinfo == inputSet[1] && dictinfo[1] == *inputSet && dictinfo[2] == inputSet[2]) //   transpose after which they match
        {
            val += 4;   //   having the letters, cost is a lot less than a replace 
            dictinfo += 2;
            inputSet += 2;
			if (*dictinfo) //   not at end, skip the same letter.
			{
				dictinfo += 1;
				inputSet += 1;
			}
        }
        else if (*dictinfo == inputSet[1] && dictinfo[1] == *inputSet && !IsVowel(*inputSet )) //   simple transpose, matched next input so jump ahead
        {
            val += 8;   //   having the letters, cost is less than a replace 
            dictinfo += 2;
            inputSet += 2;
        }
        else if (*dictinfo == inputSet[1]) //   dict CURRENT input matches his next, so maybe hist input inserted a char here and  need to delete it, but dont lose an ending s
        {
            char* prior = inputSet-1;
            if (*prior == *inputSet) val += 5; //   low cost for dropping an excess repeated letter - start of word is prepadded with 0 for prior char
            else if (*inputSet == '-') val += 3; //   very low cost for removing a hypen 
            else if (inputSet+1 == inputend && *inputSet == 's') val += 30;    //   LOSING a trailing s is almost not acceptable
            else val += 9; //   high cost removing an extra letter, but not as much as having to change it
            ++inputSet;
		}
        else if (dictinfo[1] == *inputSet) //   our NEXT input matches his current, so maybe his input deleted char here and needs to insert  it
        {
            char* prior = (dictinfo == dictstart) ? (char*)" " : (dictinfo-1);
            //   low cost for adding a duplicate consonant letter to his input
            if (*dictinfo == *prior  && !IsVowel(*dictinfo )) val += 5; //   dictionary entry prior char should be safe..
            else if (IsVowel(*dictinfo ))  val += 1; //   low cost for missing a vowel ( already charged for short input), might be a texting abbreviation
            else val += 9; //   high cost for deleting a character, but not as much as changing it
            ++dictinfo;
        }
        else if (dictinfo[2] == *inputSet && dictinfo[3] == inputSet[1]) //   our 2nd next input matches his current and our 3rd next matches his next, so maybe his input deleted 2 chars and needs to insert them
        {
            //  low cost for adding a duplicate consonant letter 
            val += 18; //   high cost for deleting a character, but not as much as changing it
            dictinfo += 2; 
       }
        else if (*dictinfo == inputSet[2] && dictinfo[1] == inputSet[3]) //   his 2nd next input matches  current and his 3rd next matches his next, so maybe his input added 2 chars and needs to delete them
        {
            // low cost for adding a duplicate consonant  
            val += 18; //   high cost for deleting a character, but not as much as changing it
            inputSet += 2; 
       }
       else //   this has no valid neighbors.  alter it to be the correct, but charge for multiple occurences
       {
			if (count == 1 && *dictinfo != *inputSet) val += 30; //   costs a lot more to change the first letter, odds are he types that right or sees its wrong
			//   if two in a row are bad, check for a substituted vowel sound
			bool swap = false;
			unsigned int oldval = val;
			if (dictinfo[1] != inputSet[1]) //   2 in row are wrong.
			{
				if (!strncmp(dictinfo,"ght",3) && !strncmp(inputSet,"t",1)) 
				{
                    dictinfo += 3;
                    inputSet += 1;
                    val += 5;    //   low cost for swapping vowel or consonant sound
				}
				else if (!strncmp(dictinfo,"cki",3) && !strncmp(inputSet,"ci",2)) 
				{
                    dictinfo += 3;
                    inputSet += 2;
                    val += 5;    //   low cost for swapping vowel or consonant sound
				}
				else if (!strncmp(dictinfo,"eous",4) && !strncmp(inputSet,"ous",3)) 
				{
                    dictinfo += 4;
                    inputSet += 3;
                    val += 5;    //   low cost for swapping vowel or consonant sound
               }
              else if (!strncmp(dictinfo,"oph",3) && !strncmp(inputSet,"of",2)) 
               {
                    dictinfo += 3;
                    inputSet += 2;
                    val += 5;    //   low cost for swapping vowel or consonant sound
               }
             else if (!strncmp(dictinfo,"x",1) && !strncmp(inputSet,"cks",3)) 
               {
                    dictinfo += 1;
                    inputSet += 3;
                    val += 5;    //   low cost for swapping vowel or consonant sound
               }
               else if (!strncmp(dictinfo,"qu",2) && !strncmp(inputSet,"k",1)) 
               {
                    dictinfo += 2;
                    inputSet += 1;
                    val += 5;    //   low cost for swapping vowel or consonant sound
               }
               if (oldval != val);
               else if (!strncmp(dictinfo,"able",4) && !strncmp(inputSet,"ible",4)) swap = true;
               else if (*dictinfo == 'a' && dictinfo[1] == 'y' && *inputSet == 'e' && inputSet[1] == 'i') swap = true;
               else if (*dictinfo == 'e' && dictinfo[1] == 'a' && *inputSet == 'e' && inputSet[1] == 'e') swap = true;
               else if (*dictinfo == 'e' && dictinfo[1] == 'e' && *inputSet == 'e' && inputSet[1] == 'a') swap = true;
               else if (*dictinfo == 'e' && dictinfo[1] == 'e' && *inputSet == 'i' && inputSet[1] == 'e') swap = true;
               else if (*dictinfo == 'e' && dictinfo[1] == 'i' && *inputSet == 'a' && inputSet[1] == 'y') swap = true;
               else if (*dictinfo == 'e' && dictinfo[1] == 'u' && *inputSet == 'o' && inputSet[1] == 'o') swap = true;
               else if (*dictinfo == 'e' && dictinfo[1] == 'u' && *inputSet == 'o' && inputSet[1] == 'u') swap = true;
               else if (!strncmp(dictinfo,"ible",4) && !strncmp(inputSet,"able",4)) swap = true;
               else if (*dictinfo == 'i' && dictinfo[1] == 'e' && *inputSet == 'e' && inputSet[1] == 'e') swap = true;
               else if (*dictinfo == 'o' && dictinfo[1] == 'o' && *inputSet == 'e' && inputSet[1] == 'u') swap = true;
               else if (*dictinfo == 'o' && dictinfo[1] == 'o' && *inputSet == 'o' && inputSet[1] == 'u') swap = true;
               else if (*dictinfo == 'o' && dictinfo[1] == 'o' && *inputSet == 'u' && inputSet[1] == 'i') swap = true;
               else if (*dictinfo == 'o' && dictinfo[1] == 'u' && *inputSet == 'e' && inputSet[1] == 'u') swap = true;
               else if (*dictinfo == 'o' && dictinfo[1] == 'u' && *inputSet == 'o' && inputSet[1] == 'o') swap = true;
               else if (*dictinfo == 'u' && dictinfo[1] == 'i' && *inputSet == 'o' && inputSet[1] == 'o') swap = true;
               if (swap)
               {
                    dictinfo += 2;
                    inputSet += 2;
                    val += 5;    //   low cost for swapping vowel sound
               }
            } 

            //   no cost for letters that sound same
            if (oldval == val) 
            {
                if ((*dictinfo == 's' && *inputSet == 'z') || (*dictinfo == 'z' && *inputSet == 's')) swap = true;
                else if (*dictinfo == 'y' && *inputSet == 'i' && count > 1) swap = true; //   but not as first letter
                else if (*dictinfo == 'i' && *inputSet== 'y' && count > 1) swap = true;//   but not as first letter
                else if (*dictinfo == '/' && *inputSet == '-') swap = true;
                else if (inputSet+1 == inputend && *inputSet == 's') val += 30;    //   changing a trailing s is almost not acceptable
                if (swap) val += 5; //   low cost for exchange of similar letter, but dont do it often
                else val += 12; //   changing a letter is expensive, since it destroys the visual image
                ++dictinfo;
                ++inputSet;
            }
       } 
       if (val > min) return val; // too costly, ignore it
    }
    return val;
}

static char* StemSpell(char* word,unsigned int i)
{
    static char word1[MAX_WORD_SIZE];
    strcpy(word1,word);
    unsigned int len = strlen(word);
	char* ending = NULL;
    char* best;
    //   no prefixes for now. (un and de would be possible)

    //   suffixes
    if (!strnicmp(word+len-3,"ing",3))
    {
        word1[len-3] = 0;
        best = SpellFix(word1,0,VERB); 
        if (best)
		{
			WORDP F = FindWord(best);
			if (F) return GetPresentParticiple(best);
		}
	}
    else if (!strnicmp(word+len-2,"ed",2))
    {
        word1[len-2] = 0;
        best = SpellFix(word1,0,VERB); 
        if (best)
        {
			char* past = GetPastTense(best);
			unsigned int len = strlen(past);
			if (past[len-1] == 'd') return past;
			ending = "ed";
        }
    }
    else if (!strnicmp(word+len-4,"less",4))
    {
        word1[len-4] = 0;
        best = SpellFix(word1,0,NOUN); 
        if (best) ending = "less";
	}
    else if (!strnicmp(word+len-4,"ness",4))
    {
        word1[len-4] = 0;
        best = SpellFix(word1,0,ADJECTIVE|NOUN); 
        if (best) ending = "ness";
    }
	else if (!strnicmp(word+len-2,"en",2))
    {
        word1[len-2] = 0;
        best = SpellFix(word1,0,ADJECTIVE); 
        if (best) ending = "en";
	}
	else if (!strnicmp(word+len-2,"ly",2))
    {
        word1[len-2] = 0;
        best = SpellFix(word1,0,ADJECTIVE); 
        if (best) ending = "ly";
	}
	else  if (!stricmp(word+len-3,"est"))
	{
        word1[len-3] = 0;
        best = SpellFix(word1,0,ADJECTIVE); 
        if (best) ending = "est";
	}
	else  if (!stricmp(word+len-2,"er"))
    {
        word1[len-2] = 0;
        best = SpellFix(word1,0,ADJECTIVE); 
        if (best) ending = "er";
    }
    else if (word[len-1] == 's')
    {
        word1[len-1] = 0;
        best = SpellFix(word1,0,VERB|NOUN); 
        if (best)
        {
			WORDP F = FindWord(best);
			if (F && F->properties & NOUN) return GetPluralNoun(F);
			ending = "s";
        }
   }
   if (ending)
   {
		strcpy(word1,best);
		strcat(word1,ending);
		return word1;
   }
   return NULL;
}

static char* SpellFix(char* word,unsigned int position,uint64 posflags)
{
   if (IsDigit(*word)) return NULL; // number-based words and numbers must be treated elsewhere

	//   Priority is to a word that looks like what the user typed, because the user probably would have noticed if it didnt and changed it.
    //   Ergo add/delete  has priority over tranform
    unsigned int len = strlen(word);
    unsigned int min = (len > 7) ? 38 : 28; //   longer words take more value to kill
    WORDP choices[4000];
    WORDP bestGuess[4000];
    unsigned int index = 0;
    unsigned int bestGuessindex = 0;
    char base[257];
    memset(base,0,257);
    SetPositions(word,base+1); // mark positions of the letters and make lower case

	unsigned char set[257];
	memset(set,0,257); // uninitialized is fine. just need them to start out the same, doesnt matter what it is. and only checking normal ascii
	for (unsigned int  i = 1; i <= len; ++i) ++set[base[i]];// for length of dictionary word, not user input

	int n = BurstWord(word);
	if (n > 1) //   phrase-- verb phrase?
	{
		char word1[MAX_WORD_SIZE];
		char word2[MAX_WORD_SIZE];
		strcpy(word1,GetBurstWord(1));
		strcpy(word1,GetBurstWord(2));
		char* verb = GetInfinitive(word1); // balance_out (except its one we dont know)
		if (verb) 
		{
			WORDP D = FindWord(verb);
			if (D && D->properties & VERB) return word;	//   accepted as a verb tense of a composite word, with 1st as verb
		}
		verb = GetInfinitive(word2); // guest-star (except its one we dont know)
		if (verb) 
		{
			WORDP D = FindWord(verb);
			if (D && D->properties & VERB) return word;	//   accepted as a verb tense of a composite word, with 2nd as verb
		}
		return NULL; //   phrase, not a word
	}

	
	static int range[] = {0,-1,1,-2,2}; // size of word matching range
    int rangeIndex = 0;
	uint64  pos = PART_OF_SPEECH;  //   all pos allowed
    char name[MAX_WORD_SIZE];
    unsigned int listlen = len;
    unsigned int slot;
    WORDP D;
    NextMatchStamp();
    if (posflags == PART_OF_SPEECH && position < wordCount) // see if we can restrict word based on next word
    {
        D = FindWord(wordStarts[position+1],0,PRIMARY_CASE_ALLOWED);
        uint64 flags = (D) ? D->properties : (-1); //   if we dont know the word, it could be anything
        if (flags & PREPOSITION) pos &= -1 ^ (PREPOSITION|NOUN);   //   prep cannot be preceeded by noun or prep
        if (!(flags & (PREPOSITION|VERB|CONJUNCTION_BITS|ADVERB)) && flags & DETERMINER) pos &= -1 ^ (DETERMINER|ADJECTIVE|NOUN|ADJECTIVE_CARDINAL|ADJECTIVE_ORDINAL|NOUN_CARDINAL|NOUN_ORDINAL); //   determiner cannot be preceeded by noun determiner adjective
        if (!(flags & (PREPOSITION|VERB|CONJUNCTION_BITS|DETERMINER|ADVERB)) && flags & ADJECTIVE) pos &= -1 ^ (NOUN); 
        if (!(flags & (PREPOSITION|NOUN|CONJUNCTION_BITS|DETERMINER|ADVERB|ADJECTIVE)) && flags & VERB) pos &= -1 ^ (VERB); //   we know all helper verbs we might be
        if (D && D->word[0] == '\'' && D->word[1] == 's' ) pos &= NOUN;    //   we can only be a noun if possive - contracted 's should already be removed by now
    }
    if (posflags == PART_OF_SPEECH && position > 1)
    {
        D = FindWord(wordStarts[position-1],0,PRIMARY_CASE_ALLOWED);
        uint64 flags = (D) ? D->properties : (-1); //   if we dont know the word, it could be anything
        if (flags & DETERMINER) pos &= -1 ^ (VERB|CONJUNCTION_BITS|PREPOSITION|DETERMINER);  
    }
    posflags &= pos; //   if pos types are known and restricted and dont match

    if (len < 8 || len > 12) //   be thorough for short and long words. too expensive for intermediate words 
    {
        sprintf(name,"%d",listlen);
        strcat(name,"%size");
        slot = LIST_BY_SIZE;
    }
    else //   for intermediate words, try assuming 1st four or last 4 letters are correct before using the length lists
    {
        strcpy(name+2,word);
        name[6] = 0;
        name[0] = '$';
        name[1] = 'p';
        slot = LIST_BY_PREFIX;
    }
    D = FindWord(name,0,PRIMARY_CASE_ALLOWED);
    if (D) 
	{
		if (slot == LIST_BY_SIZE) D = D->spellBySize;
		else if (slot == LIST_BY_PREFIX) D = D->spellByPrefix;
		else if (slot == LIST_BY_SUFFIX) D = D->spellBySuffix;
	}

    while (1)
    {
        if (D && D->properties & posflags)  //   initial request word type
        {
            unsigned int val = EditDistance(D->word, len, base+1,min,set);
            if (val <= min) //   as good as or better
            {
                if (val < min)
                {
                    index = 0;
                    min = val;
                }
                if (D->v.patternStamp != matchStamp) 
                {
                    choices[index++] = D;
                    if (index > 3998) break; 
                    D->v.patternStamp = matchStamp;
                }
            }
        }

        //   now find next one
		if (D) 
		{
			if (slot == LIST_BY_SIZE) D = D->spellBySize;
			else if (slot == LIST_BY_PREFIX) D = D->spellByPrefix;
			else if (slot == LIST_BY_SUFFIX) D = D->spellBySuffix;
		}

        if (!D) //   list empty, get new list
        {
			if (index) break;	// plausible match, dont bother with longer or shorter
            if (slot == LIST_BY_PREFIX) 
            {
                strcpy(name+2,word+len-4);
                name[6] = 0;
                name[0] = '$';
                name[1] = 's';
                slot = LIST_BY_SUFFIX;
            }
            else if (slot == LIST_BY_SUFFIX)
            {
                if (index) break;
                sprintf(name,"%d%%size",listlen);
                slot = LIST_BY_SIZE;
                sprintf(name,"%d%%size",len+range[rangeIndex]);
            }
            else if (rangeIndex < 2)  //   right now only use len +- 1
            {
                ++rangeIndex;
                sprintf(name,"%d%%size",len+range[rangeIndex]);
            }
            else break;
            D = FindWord(name);
			if (D) 
			{
				if (slot == LIST_BY_SIZE) D = D->spellBySize;
				else if (slot == LIST_BY_PREFIX) D = D->spellByPrefix;
				else if (slot == LIST_BY_SUFFIX) D = D->spellBySuffix;
			}
        }
    }

	// try endings ing, s, etc
	if (position) // no stem spell if COMING from a stem spell attempt (position == 0)
	{
		char* stem = StemSpell(word,position);
		if (stem) 
		{
			WORDP D = FindWord(stem);
			for (unsigned int j = 0; j < index; ++j) if (choices[j] == D) D = NULL; // already in our list
			if (D) choices[index++] = D;
		}
	}

    if (!index) // no whole found
	{
		return NULL;
	}

	// take our guesses, and pick the most common (earliest learned) word
    uint64 agemin = 0;
    bestGuess[0] = NULL;
    for (unsigned int j = 0; j < index; ++j) 
    {
        uint64 val = choices[j]->systemFlags & AGE_LEARNED;
        if (val < agemin) continue;
        if (val > agemin)
        {
            agemin = val;
            bestGuessindex = 0;
        }
        bestGuess[bestGuessindex++] = choices[j];
    }
   return (bestGuessindex == 1) ? bestGuess[0]->word : NULL; //   dont accept multiple choices at same grade level
}
