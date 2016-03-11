#ifndef _IMPACT_H
#define _IMPACT_H

struct my_rank_impact {
	double doc_length(size_t ) const {
		return 0;
	}
	double calc_doc_weight(double ) const {
		return 0;
	}
	double calculate_docscore(const double , const double f_dt, const double , const double , bool) const {
		return f_dt;
	}
};

#endif
