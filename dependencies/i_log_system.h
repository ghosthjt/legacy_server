#pragma once
#include <string>
#include "i_controller.h"
#include "boost/smart_ptr.hpp"

enum 
{
	loglv_critical_error = -1,  //½ô¼±´íÎó
	loglv_error = 0,
	loglv_warning,
	loglv_info,
	loglv_debug,
	loglv_footprint,
	loglv_all,
};

class i_log_system : public i_controller
{
public:
	static boost::shared_ptr<i_log_system> get_instance();
	i_log_system();
	virtual ~i_log_system();
	int		start();
	int		step();
	int		stop();
	int		sync();
	void	set_level(int lv) { log_level_ = lv; }
	void	write_log(int loglv, std::string fmt,...);
protected:
	int		log_level_;
};