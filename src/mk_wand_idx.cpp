#include <iostream>
#include "ant_param_block.h"
#include "search_engine.h"
#include "search_engine_btree_leaf.h"
#include "btree_iterator.h"
#include "memory.h"
#include "include/impact.hpp"
#include "sdsl/int_vector_buffer.hpp"
#include "include/block_postings_list.hpp"

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
	ANT_memory memory;
	ANT_search_engine search_engine(&memory);
	search_engine.open(params.index_filename);

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
	of_globalinfo << search_engine.document_count() << " "
		<< search_engine.term_count() << std::endl;

	std::cout << "Writing document lengths to " << doclen_tfile << "."
		<< std::endl;
	uint64_t uniq_terms = search_engine.get_unique_term_count();
	// Shift all IDs from ATIRE by 2 so \0 and \1 are free.
	uniq_terms += 2; 

	double mean_length;
	char *docname = new char[4096]; // 4k document names... who knows
	auto atirelengths = search_engine.get_document_lengths(&mean_length);
	if (false)
	{
		for (long long i = 0; i < search_engine.document_count(); i++)
		{
			// find document name
			search_engine.get_document_filename(docname, i);
			document_names.push_back(strdup(docname));

			// Add doclens
			doclen_out << atirelengths[i] << "\n";
			doc_lengths.push_back(atirelengths[i]);
			num_terms += atirelengths[i];
		}
	}
	// write document names
	if (false)
	{
		std::cout << "Writing document names to " << doc_names_file << "." 
			<< std::endl;
		std::ofstream of_doc_names(doc_names_file);
		for(const auto& doc_name : document_names) {
			of_doc_names << doc_name << "\n";
		}
	}
	// write the vocabulary
	if (false)
	{
		std::cout << "Writing dictionary to " << dict_file << "." << std::endl;
		std::ofstream of_dict(dict_file);

		ANT_search_engine_btree_leaf leaf;
		ANT_btree_iterator iter(&search_engine);

		size_t j = 2;
		for (char *term = iter.first(NULL); term != NULL; term = iter.next()) {
			iter.get_postings_details(&leaf);
			of_dict << term << " " << j << " "
				<< leaf.local_document_frequency << " "
				<< leaf.local_collection_frequency << " "
				<< "\n";
			map.emplace(strdup(term), j);
			j++;
		}
	}
	// write inverted files
		vector<pair<uint64_t, uint64_t>> post; 
	{
		using plist_type = block_postings_list<128>;
		vector<plist_type> m_postings_lists; 
		uint64_t a = 0, b = 0;
		uint64_t n_terms = search_engine.get_unique_term_count();

		// Open the files
		filebuf post_file;
		post_file.open(postings_file, std::ios::out);
		ostream ofs(&post_file);

		filebuf F_t_file;
		F_t_file.open(ft_file, std::ios::out);
		ostream Ft(&F_t_file);

		filebuf f_t_file;
		f_t_file.open(dft_file, std::ios::out);
		ostream ft(&f_t_file); 

		std::cerr << "Writing postings lists ..." << std::endl;

		post.reserve(INIT_SZ);

		m_postings_lists.resize(n_terms + 2);
		my_rank_impact ranker;
		sdsl::int_vector<> F_t_list(n_terms + 2); // collection frequency
		sdsl::int_vector<> f_t_list(n_terms + 2); // document frequency

		ANT_search_engine_btree_leaf leaf;
		ANT_btree_iterator iter(&search_engine);
		ANT_impact_header impact_header;
		ANT_compression_factory factory;

		ANT_compressable_integer *raw;
		long long impact_header_size = ANT_impact_header::NUM_OF_QUANTUMS * sizeof(ANT_compressable_integer) * 3;
		ANT_compressable_integer *impact_header_buffer = (ANT_compressable_integer *)malloc(impact_header_size);
		auto postings_list_size = search_engine.get_postings_buffer_length();
		auto raw_list_size = sizeof(*raw) * (search_engine.document_count() + ANT_COMPRESSION_FACTORY_END_PADDING);
		unsigned char *postings_list = (unsigned char *)malloc((size_t)postings_list_size);
		raw = (ANT_compressable_integer *)malloc((size_t)raw_list_size);

		for (char *term = iter.first(NULL); term != NULL; term = iter.next())
		{
			iter.get_postings_details(&leaf);

			postings_list = search_engine.get_postings(&leaf, postings_list);

			auto the_quantum_count = ANT_impact_header::get_quantum_count(postings_list);
			auto beginning_of_the_postings = ANT_impact_header::get_beginning_of_the_postings(postings_list);
			factory.decompress(impact_header_buffer, postings_list + ANT_impact_header::INFO_SIZE, the_quantum_count * 3);

			std::cout << term << " @ " << leaf.postings_position_on_disk << " (cf:" << leaf.local_collection_frequency << ", df:" << leaf.local_document_frequency << ", q:" << the_quantum_count << ")" << std::endl;

			long long docid, max_docid, sum;
			ANT_compressable_integer *impact_header = (ANT_compressable_integer *)impact_header_buffer;
			ANT_compressable_integer *current, *end;

			max_docid = sum = 0;
			ANT_compressable_integer *impact_value_ptr = impact_header;
			ANT_compressable_integer *doc_count_ptr = impact_header + the_quantum_count;
			ANT_compressable_integer *impact_offset_start = impact_header + the_quantum_count * 2;
			ANT_compressable_integer *impact_offset_ptr = impact_offset_start;

			post.clear();
			post.reserve(leaf.local_document_frequency);
			while (doc_count_ptr < impact_offset_start)
			{
				factory.decompress(raw, postings_list + beginning_of_the_postings + *impact_offset_ptr, *doc_count_ptr);
				docid = -1;
				current = raw;
				end = raw + *doc_count_ptr;
				while (current < end)
					{
					docid += *current++;
					post.emplace_back(docid, *impact_value_ptr);
					}
				impact_value_ptr++;
				impact_offset_ptr++;
				doc_count_ptr++;
			}

			// The above will result in sorted by impact first, so re-sort by docid
			std::sort(std::begin(post), std::end(post));

			plist_type pl(ranker, post);
			m_postings_lists[map[term]] = pl;
			F_t_list[map[term]] = leaf.local_collection_frequency;
			f_t_list[map[term]] = leaf.local_document_frequency;
		}
		size_t num_lists = m_postings_lists.size();
		std::cout << "Writing " << num_lists << " postings lists." << std::endl;
		sdsl::serialize(num_lists, ofs);
		for(const auto& pl : m_postings_lists)
		{
			sdsl::serialize(pl, ofs);
		}
		//Write F_t data to file, skip 0 and 1
		cout << "Writing F_t lists." << endl;
		F_t_list.serialize(Ft);

		//Write out document frequency (num docs that term appears in), skip 0 and 1
		cout << "Writing f_t lists." << endl;
		f_t_list.serialize(ft);

		//close output files
		post_file.close();
		F_t_file.close();
		f_t_file.close();
	}
	auto build_stop = clock::now();
	auto build_time_sec = std::chrono::duration_cast<std::chrono::seconds>(build_stop-build_start);
	std::cout << "Index built in " << build_time_sec.count() << " seconds." << std::endl;

	return EXIT_SUCCESS;
}
