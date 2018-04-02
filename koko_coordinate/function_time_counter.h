#pragma once
#include <string>
#include "boost/date_time.hpp"

class function_time_counter
{
public:
	void		restart();
	int			elapse();
	void		desc(std::string s);
	function_time_counter(std::string desc, int showlog_time = 500);
	~function_time_counter();
private:
	std::string desc_;
	int			showlog_time_;
	boost::posix_time::ptime pt;
};
