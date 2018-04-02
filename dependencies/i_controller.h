#pragma once
#include "boost/smart_ptr.hpp"
class remote_socket_v3;

class i_controller : public boost::enable_shared_from_this<i_controller>
{
public:
	virtual int		start() = 0;
	virtual int		step() = 0;
	virtual int		stop() = 0;
	virtual ~i_controller() {}
};
