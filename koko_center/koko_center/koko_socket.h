#pragma once
#include "net_service.h"
#include "error.h"
#include "boost/date_time.hpp"
#include "boost/atomic.hpp"

class koko_player;
class koko_socket;
typedef boost::shared_ptr<koko_socket> koko_socket_ptr;
typedef boost::weak_ptr<koko_socket> koko_socket_wptr;

class koko_socket : public remote_socket_v2
{
public:
	koko_socket(net_server& srv);
	atomic_value<std::string>		verify_code_, verify_code_backup_;		//ÑéÖ¤Âë±£´æ¡£
	std::string			mverify_code_, mverify_error_;
	boost::atomic_bool			is_login_, is_register_;
	boost::posix_time::ptime		mverify_time_;

	boost::weak_ptr<koko_player>	the_client_;
	virtual void close(bool passive = false);
};
