#pragma once
#include "net_service.h"
#include "net_socket_remote.h"
#include <map>
class game_srv_socket: public remote_socket_impl<game_srv_socket, default_socket_service>
{
public:
	int			gameid_;
	std::string instance_;
	game_srv_socket(net_server<game_srv_socket>& srv):
		remote_socket_impl(srv)
	{
		recv_helper_.reading_buffer_ = stream_buffer(boost::shared_array<char>(new char[409600]), 0, 409600);
		gameid_ = -1;
	}
	void	close(bool passive = false);

};

typedef boost::shared_ptr<game_srv_socket> srv_socket_ptr;
typedef boost::weak_ptr<game_srv_socket> srv_socket_wptr;

