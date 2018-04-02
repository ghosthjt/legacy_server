#include "function_time_counter.h"
#include "i_log_system.h"

void function_time_counter::restart()
{
	pt = boost::posix_time::microsec_clock::local_time();
}

int function_time_counter::elapse()
{
	return (int)(boost::posix_time::microsec_clock::local_time() - pt).total_milliseconds();
}

void function_time_counter::desc(std::string s)
{
	desc_ = s;
}

function_time_counter::function_time_counter(std::string desc, int showlog_time /*= 500*/)
{
	desc_ = desc;
	showlog_time_ = showlog_time;
	restart();
}

function_time_counter::~function_time_counter()
{
	int el = elapse();
	if (el > showlog_time_) {
		i_log_system::get_instance()->write_log(loglv_warning, "watching for module [%s] cost %d ms.", desc_.c_str(), elapse());
	}
}