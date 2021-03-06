/*
 * IPrinter.cpp
 *
 *  Created on: Aug 24, 2015
 *      Author: fsedlaze
 */

#include "IPrinter.h"

bool IPrinter::to_print(Breakpoint * &SV, pair<double, double>& std, pair<double, double> & kurtosis, double & std_length) {

	std.first = 0;
	std.second = 0;

	//comp_std(SV, std_start, std_stop);
	std_length=0;
	kurtosis = comp_std_quantile(SV, std,std_length);
	bool to_print = true;

	if (((SV->get_SVtype() & INS) && SV->get_coordinates().stop.most_support - SV->get_coordinates().start.most_support == Parameter::Instance()->huge_ins) && (SV->get_types().is_ALN)) {
		return (!SV->get_types().is_SR && (std.first < 5 && std.second < 5));
	}

	if ((SV->get_SVtype() & INS) || (SV->get_SVtype() & DEL)) { //for insertions  + deletions:
		double dist = (double) (SV->get_coordinates().stop.most_support - SV->get_coordinates().start.most_support);
		//dist = (double)std::min(((int)dist * 4), Parameter::Instance()->max_dist);
		dist = dist * 4.0 * (uniform_variance / 2); //because we test against corrected value!
		//std::cout<<"DIST: "<<(SV->get_coordinates().stop.most_support - SV->get_coordinates().start.most_support)<<" "<<dist<<" STD: "<<std_start<<" "<<std_stop<<std::endl;
		return ((std.first < dist && std.second < dist)); //0.2886751
	}

	//TODO test!
	if (SV->get_SVtype() & NEST) {
		return true;
	}
	double max_allowed = 4 * Parameter::Instance()->max_dist * (uniform_variance / 2);

	if (SV->get_coordinates().support.size() < 8) { //not enough coverage
//		max_allowed = 4 * Parameter::Instance()->max_dist * (uniform_variance);
		//max_allowed = max_allowed * ((double) SV->get_coordinates().support.size()/10.0);
	}

	return (std.first < max_allowed && std.second < max_allowed);
}

std::string IPrinter::get_chr(long pos, RefVector ref) {
	//	std::cout << "pos: " << pos << std::endl;
	size_t id = 0;
	while (id < ref.size() && pos >= 0) {
		pos -= ((long) ref[id].RefLength + (long) Parameter::Instance()->max_dist);
		//	std::cout << id << std::endl;
		id++;
	}

	return ref[id - 1].RefName;
}

void IPrinter::store_readnames(std::vector<long> names, int id) {
	name_str tmp;
	tmp.svs_id = id; //stays the same
	for (size_t i = 0; i < names.size(); i++) {
		tmp.read_name = names[i];
		fwrite(&tmp, sizeof(struct name_str), 1, this->tmp_file);
	}
}

long IPrinter::calc_pos(long pos, RefVector ref, std::string &chr) {
	size_t i = 0;
	pos -= (ref[i].RefLength + Parameter::Instance()->max_dist);

	while (i+1 < ref.size() && pos >= 0) {
		i++;
	//	std::cout<<i<<" "<<ref.size()<<" "<<pos<<" "<<pos-Parameter::Instance()->max_dist<<std::endl;
		pos -= ((long) ref[i].RefLength + (long) Parameter::Instance()->max_dist);
	}
	chr = ref[i].RefName;
	return pos + ref[i].RefLength + (long) Parameter::Instance()->max_dist;
}

std::string IPrinter::get_type(char type) {
	string tmp;
	if (type & DEL) {
		tmp += "DEL";
	}
	if (type & INV) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "INV";
	}
	if (type & DUP) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "DUP";
	}
	if (type & INS) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "INS";
	}
	if (type & TRA) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		//tmp += "BND";
		tmp += "TRA";
	}
	if (type & NEST) {
		if (!tmp.empty()) {
			tmp += '/';
		}
		tmp += "INVDUP";
	}
	return tmp; // should not occur!
}
// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string IPrinter::currentDateTime() {
	time_t now = time(0);
	struct tm tstruct;
	char buf[80];
	tstruct = *localtime(&now);
	// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
	// for more information about date/time format
	strftime(buf, sizeof(buf), "%Y%m%d", &tstruct);
	return buf;
}

void IPrinter::sort_insert(int pos, std::vector<int> & positions) {

	size_t i = 0;
	while (i < positions.size() && positions[i] < pos) {
		i++;
	}
	positions.insert(positions.begin() + i, pos);

}
void IPrinter::comp_std_med(Breakpoint * &SV, double & std_start, double & std_stop) {
	std::vector<int> std_start_dists;
	std::vector<int> std_stop_dists;

	std::map<std::string, read_str> support = SV->get_coordinates().support;
	for (std::map<std::string, read_str>::iterator i = support.begin(); i != support.end(); i++) {
		if ((*i).second.SV & SV->get_SVtype()) {
			if ((*i).second.coordinates.first != -1) {
				long diff = (SV->get_coordinates().start.most_support - (*i).second.coordinates.first);
				//	std::cout << "DIFF Start: " << diff << std::endl;
				sort_insert(std::pow((double) diff, 2.0), std_start_dists);
				//std_start += std::pow((double) diff, 2.0);
			}
			if ((*i).second.coordinates.second != -1) {
				long diff = (SV->get_coordinates().stop.most_support - (*i).second.coordinates.second);
				//	std::cout << "DIFF Stop: " << diff << std::endl;
				sort_insert(std::pow((double) diff, 2.0), std_stop_dists);
				//std_stop += std::pow((double) diff, 2.0);
			}
		}
	}
	int median = std_stop_dists.size() / 2;
	std_start = std::sqrt(std_start_dists[median]);
	std_stop = std::sqrt(std_stop_dists[median]);
}

pair<double, double> IPrinter::comp_std_quantile(Breakpoint * &SV, pair<double, double> & std, double & std_length) {
	double count = 0;
	std::vector<int> std_start_dists;
	std::vector<int> std_stop_dists;
	std::vector<int> std_length_dists;

	//std::stringstream ss;
	double s4_start = 0;
	double s4_stop = 0;
	double s2_start = 0;
	double s2_stop = 0;
	std_length=0;
	std::map<std::string, read_str> support = SV->get_coordinates().support;
	for (std::map<std::string, read_str>::iterator i = support.begin(); i != support.end(); i++) {
		if ((*i).second.SV & SV->get_SVtype()) {
				long diff=SV->get_length() -((*i).second.coordinates.second-(*i).second.coordinates.first);
			//	sort_insert(std::pow((double) diff, 2.0), std_length_dists); //TODO think about that!!
				std_length+=std::pow((double) diff, 2.0);
			if ((*i).second.coordinates.first != -1) {
				diff = (SV->get_coordinates().start.most_support - (*i).second.coordinates.first);
				//ss << '\t';
				//ss << diff;
				sort_insert(std::pow((double) diff, 2.0), std_start_dists);
				s4_start += std::pow((double) diff, 4.0);
				s2_start += std::pow((double)diff, 2.0);;
			}
			if ((*i).second.coordinates.second != -1) {
				diff = (SV->get_coordinates().stop.most_support - (*i).second.coordinates.second);
				//ss << '\t';
				//ss << diff;

				sort_insert(std::pow((double) diff, 2.0), std_stop_dists);
				s4_stop += std::pow((double) diff, 4.0);
				s2_stop += std::pow((double)diff, 2.0);
			}
		}
		count++;
	}
	std_length=std::sqrt(std_length/count);

	s4_start = s4_start / count;
	s4_stop = s4_stop / count;
	s2_start = s2_start / count;
	s2_stop = s2_stop / count;

	count = 0;
	for (int i = 0; i < std::max((int) std_stop_dists.size() / 2, 10) && i<std_start_dists.size(); i++) {
		std.first += std_start_dists[i];
		std.second += std_stop_dists[i];
		count++;

		/*s4_start += std::pow((double) std_start_dists[i], 2.0);
		 s4_stop += std::pow((double) std_stop_dists[i], 2.0);
		 s2_start += std_start_dists[i];
		 s2_stop += std_stop_dists[i];*/
	}

	pair<double, double> kurtosis;
	kurtosis.first = (s4_start / std::pow(s2_start, 2.0)) - 3;
	kurtosis.second = (s4_stop / std::pow(s2_stop, 2.0)) - 3;

	std.first = std::sqrt(std.first / count);
	std.second = std::sqrt(std.second / count);

	for(size_t i=0;i<std_length_dists.size();i++){

	}
	return kurtosis;
}

void IPrinter::comp_std(Breakpoint * &SV, double & std_start, double & std_stop) {
	double count = 0;
	std_start = 0;
	std_stop = 0;
	std::map<std::string, read_str> support = SV->get_coordinates().support;
	for (std::map<std::string, read_str>::iterator i = support.begin(); i != support.end(); i++) {
		if ((*i).second.SV & SV->get_SVtype()) {
			count++;
			if ((*i).second.coordinates.first != -1) {
				long diff = (SV->get_coordinates().start.most_support - (*i).second.coordinates.first);
				//	std::cout << "DIFF Start: " << diff << std::endl;
				std_start += std::pow((double) diff, 2.0);
			}
			if ((*i).second.coordinates.second != -1) {
				long diff = (SV->get_coordinates().stop.most_support - (*i).second.coordinates.second);
				//	std::cout << "DIFF Stop: " << diff << std::endl;
				std_stop += std::pow((double) diff, 2.0);
			}

		}
	}
	std_start = std::sqrt(std_start / count);
	std_stop = std::sqrt(std_stop / count);
}
