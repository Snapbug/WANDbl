/*
    TREC2QUERY.C
    ------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "source/channel_file.h"
#include "source/channel_trec.h"
#include "source/stem.h"
#include "source/stemmer_factory.h"
#include "atire/indexer_param_block_stem.h"

using namespace std;

/*
    MAIN()
    ------
*/
int main(int argc, char *argv[])
{
ANT_channel *inchannel, *outchannel;
ANT_indexer_param_block_stem pbs;
ANT_stem *stemmer = NULL;
char *inchannel_word;
char *term;
char stem_buffer[1024];
bool first_term;

if (argc != 3 && argc != 5)
    exit(printf("Usage:%s <trectopicfile> <tag> [-s <stemmer>]\n<tag> is any combination of t, d, n, q (title, desc, narr, query)\n-s will stem using <stemmer> (same specification as ATIRE)\n", argv[0]));

if (argc == 5)
	{
	pbs.term_expansion(argv[4], 0);
	stemmer = ANT_stemmer_factory::get_core_stemmer(pbs.stemmer);
	}

inchannel = new ANT_channel_file(argv[1]);
outchannel = new ANT_channel_file(); // Defaults to stdout

inchannel = new ANT_channel_trec(inchannel, argv[2]);

for (inchannel_word = inchannel->gets(); inchannel_word != NULL; inchannel_word = inchannel->gets())
	{
	if (stemmer != NULL)
		{
		first_term = true;
		term = strtok(inchannel_word, " ");
		while (term != NULL)
			{
			stemmer->stem(term, stem_buffer);

			if (*stem_buffer != '\0')
				{
				if (first_term)
					{
					first_term = false;
					*outchannel << stem_buffer << ";";
					}
				else
					{
					*outchannel << stem_buffer << " ";
					}
				}
			term = strtok(NULL, " ");
			}
		outchannel->puts(" ");
		}
	else
		{
		outchannel->puts(inchannel_word);
		}
	delete [] inchannel_word;
	}

if (outchannel != inchannel)
    delete outchannel;
delete inchannel;

return EXIT_SUCCESS;
}
