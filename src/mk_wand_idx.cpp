#include <iostream>

/* #include "indri/Repository.hpp" */
/* #include "indri/CompressedCollection.hpp" */
#include "sdsl/int_vector_buffer.hpp"
#include "include/block_postings_list.hpp"
#include "include/bm25.hpp"


#define INIT_SZ 4096 

bool
directory_exists(std::string dir)
{
	struct stat sb;
	const char* pathname = dir.c_str();
	if (stat(pathname, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		return true;
	}
	return false;
}

void
create_directory(std::string dir)
{
	if (!directory_exists(dir)) {
		if (mkdir(dir.c_str(),0777) == -1) {
			perror("could not create directory");
			exit(EXIT_FAILURE);
		}
	}
}

/*   // write inverted files */
/*   { */
/*     using plist_type = block_postings_list<128>; */
/*     vector<plist_type> m_postings_lists; */ 
/*     uint64_t a = 0, b = 0; */
/*     uint64_t n_terms = index->uniqueTermCount(); */

/*     vector<pair<uint64_t, uint64_t>> post; */ 
/*     post.reserve(INIT_SZ); */

/*     // Open the files */
/*     filebuf post_file; */
/*     post_file.open(postings_file, std::ios::out); */
/*     ostream ofs(&post_file); */

/*     filebuf F_t_file; */
/*     F_t_file.open(ft_file, std::ios::out); */
/*     ostream Ft(&F_t_file); */

/*     filebuf f_t_file; */
/*     f_t_file.open(dft_file, std::ios::out); */
/*     ostream ft(&f_t_file); */ 

/*     std::cerr << "Writing postings lists ..." << std::endl; */

/*     m_postings_lists.resize(n_terms + 2); */
/*     my_rank_bm25<90,40> ranker(doc_lengths, num_terms); */
/*     sdsl::int_vector<> F_t_list(n_terms + 2); */
/*     sdsl::int_vector<> f_t_list(n_terms + 2); */

/*     const auto& index = (*state)[0]; */
/*     indri::index::DocListFileIterator* iter = index->docListFileIterator(); */
/*     iter->startIteration(); */

/*     while( !iter->finished() ) { */
/*       indri::index::DocListFileIterator::DocListData* entry = */ 
/*         iter->currentEntry(); */
/*       indri::index::TermData* termData = entry->termData; */

/*       entry->iterator->startIteration(); */
/*       post.clear(); */

/*       F_t_list[map[termData->term]] = termData->corpus.totalCount; */
/*       f_t_list[map[termData->term]] = termData->corpus.documentCount; */
/*       while( !entry->iterator->finished() ) { */
/*         indri::index::DocListIterator::DocumentData* doc = */ 
/*           entry->iterator->currentEntry(); */

/*         a = doc->document - 1; */
/*         b = doc->positions.size(); */
/*         post.emplace_back(a,b); */
/*         entry->iterator->nextEntry(); */
/*       } */
/*       plist_type pl(ranker, post); */
/*       m_postings_lists[map[termData->term]] = pl; */
/*       iter->nextEntry(); */
/*     } */
/*     delete iter; */

/*     size_t num_lists = m_postings_lists.size(); */
/*     cout << "Writing " << num_lists << " postings lists." << endl; */
/*     sdsl::serialize(num_lists, ofs); */
/*     for(const auto& pl : m_postings_lists) { */
/*       sdsl::serialize(pl, ofs); */
/*     } */

/*     //Write F_t data to file, skip 0 and 1 */
/*     cout << "Writing F_t lists." << endl; */
/*     F_t_list.serialize(Ft); */

/*     //Write out document frequency (num docs that term appears in), skip 0 and 1 */
/*     cout << "Writing f_t lists." << endl; */
/*     f_t_list.serialize(ft); */

/*     //close output files */
/*     post_file.close(); */
/*     F_t_file.close(); */
/*     f_t_file.close(); */
/*   } */

/*   auto build_stop = clock::now(); */
/*   auto build_time_sec = std::chrono::duration_cast<std::chrono::seconds>(build_stop-build_start); */
/*   std::cout << "Index built in " << build_time_sec.count() << " seconds." << std::endl; */

/*   return (EXIT_SUCCESS); */
/* } */

#include "atire_api.h"
#include "ant_param_block.h"
#include "search_engine.h"
#include "search_engine_btree_leaf.h"
#include "btree_iterator.h"

int main(int argc, char **argv)
{
	ANT_ANT_param_block params(argc, argv);
	long last_param = params.parse();

	if (last_param == argc)
	{
		std::cout << "USAGE: " << argv[0];
		std::cout << " [ATIRE options] <collection folder>" << std::endl;
		return EXIT_FAILURE;
	}
	using clock = std::chrono::high_resolution_clock;

	std::string collection_folder = argv[last_param];
	create_directory(collection_folder);
	std::string dict_file = collection_folder + "/dict.txt";
	std::string doc_names_file = collection_folder + "/doc_names.txt";
	std::string postings_file = collection_folder + "/WANDbl_postings.idx";
	std::string ft_file = collection_folder + "/WANDbl_F_t.idx";
	std::string dft_file = collection_folder + "/WANDbl_df_t.idx";
	std::string global_info_file = collection_folder + "/global.txt";
	std::string doclen_tfile = collection_folder + "/doc_lens.txt";

	std::ofstream doclen_out(doclen_tfile);

	auto build_start = clock::now();

	// load the ATIRE index
	ATIRE_API *atire = new ATIRE_API();
	long fail = atire->open(params.file_or_memory, params.index_filename, params.doclist_filename, params.quantization, params.quantization_bits);

	if (fail)
	{
		std::cout << "Failed to load index!" << std::endl;
		return EXIT_FAILURE;
	}

  // Keep track of term ordering
  unordered_map<string, uint64_t> map;

  //Vector of doc lengths
  vector<uint64_t> doc_lengths;
  uint64_t num_terms = 0;

  std::cout << "Writing global info to " << global_info_file << "."
            << std::endl;
  std::vector<std::string> document_names;

  // dump global info; num documents in collection, num of all terms
  std::ofstream of_globalinfo(global_info_file);
  of_globalinfo << atire->get_document_count() << " "
                << atire->get_term_count() << std::endl;

  std::cout << "Writing document lengths to " << doclen_tfile << "."
            << std::endl;
  uint64_t uniq_terms = atire->get_unique_term_count();
  // Shift all IDs from Indri by 2 so \0 and \1 are free.
  uniq_terms += 2; 

  double mean_length;
  char *docname = new char[4096]; // 4k document names... who knows
  auto atirelengths = atire->get_search_engine()->get_document_lengths(&mean_length);
  for (long long i = 0; i < atire->get_document_count(); i++)
  {
	  // find document name
	  atire->get_document_filename(docname, i);
	  document_names.push_back(strdup(docname));

	  // Add doclens
	  doclen_out << atirelengths[i] << "\n";
	  doc_lengths.push_back(atirelengths[i]);
	  num_terms += atirelengths[i];
  }
  // write document names
  {
	  std::cout << "Writing document names to " << doc_names_file << "." 
		  << std::endl;
	  std::ofstream of_doc_names(doc_names_file);
	  for(const auto& doc_name : document_names) {
		  of_doc_names << doc_name << "\n";
	  }
  }
  {
	  std::cout << "Writing dictionary to " << dict_file << "." << std::endl;
	  std::ofstream of_dict(dict_file);

	  ANT_search_engine_btree_leaf leaf;
	  ANT_btree_iterator iter(atire->get_search_engine());

	  size_t j = 2;
	  for (char *term = iter.first(NULL); term != NULL; term = iter.next()) {
		  map.emplace(strdup(term), j);
		  atire->get_search_engine()->get_postings_details(term, &leaf);
		  of_dict << term << " " << j << " "
			  << leaf.global_document_frequency << " "
			  << leaf.global_collection_frequency << " "
			  << "\n";

		  j++;
	  }
  }
}
