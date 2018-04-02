#include "i_log_system.h"
#include "log.h"

boost::shared_ptr<log_file<cout_and_file_output>> gfile;

#if defined(WIN32)
log_file<debug_output> outp;
#endif

static boost::shared_ptr<i_log_system> gins;
std::map<int, std::string> vmap;
boost::shared_ptr<i_log_system> i_log_system::get_instance()
{
	if (!gins){
		gins = boost::shared_ptr<i_log_system>(new i_log_system());
	}
	return gins;
}

i_log_system::i_log_system()
{
	log_level_ = loglv_all;
	vmap[loglv_critical_error] = "!!!!!!=====>[critical error]<====!!!!!!!!!!!!!!!!";
	vmap[loglv_error]		= "[error]<====!!!!!!!!!!!!!!!!";
	vmap[loglv_warning]		= "[warning]<====!!!!!!!!!!!!!!!!";
	vmap[loglv_debug]		= "[debug]";
	vmap[loglv_info]		= "[info]";
	vmap[loglv_footprint]	= "[repeat]";
	gfile.reset(new log_file<cout_and_file_output>());
}

i_log_system::~i_log_system()
{

}

int i_log_system::start()
{
	auto pt = boost::posix_time::second_clock::local_time();
	pt = boost::posix_time::ptime(pt.date(), boost::posix_time::seconds(0));
	std::string fname = boost::posix_time::to_iso_string(pt);
	if (!gfile){
		gfile.reset(new log_file<cout_and_file_output>());
	}
	gfile->output_.fname_ = "runlog_" + fname +".log";
	return 0;
}

int i_log_system::step()
{
	return 0;
}

int i_log_system::stop()
{
	if (gfile){
		gfile->stop_log();
		gfile->sync();
#if defined(WIN32)	
		outp.stop_log();
		outp.sync();
#endif
		gfile.reset();
	}
	return 0;
}

int i_log_system::sync()
{
	if (gfile){
		gfile->sync();
	}
#if defined(WIN32)
	outp.sync();
#endif
	return 0;
}

void i_log_system::write_log(int loglv, std::string fmt,...)
{
	if (!gins) return;
	if (!gfile) return;

	auto pt = boost::posix_time::second_clock::local_time();
	pt = boost::posix_time::ptime(pt.date(), boost::posix_time::seconds(0));
	std::string fname = boost::posix_time::to_iso_string(pt);
	fname = "runlog_" + fname + ".log";
	//如果名称不一样了,则重来
	if (fname != gfile->output_.fname_ && gfile->output_.fname_ != ""){
		stop();
		start();
	}

	if (loglv > log_level_) return;

	fmt += vmap[loglv];
	va_list ap;
	va_start(ap, fmt);
	bool ret = gfile->write_log(true, fmt.c_str(), ap);
#if defined(WIN32)
	outp.write_log(true, fmt.c_str(), ap);
#endif
	va_end(ap);
}
