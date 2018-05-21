#include "koko_socket.h"
#include "player.h"

void koko_socket::close(bool passive)
{
	remote_socket_v2::close(passive);
	player_ptr pp = the_client_.lock();
	if (pp.get()){
		pp->is_connection_lost_ = 1;
	}
}

koko_socket::koko_socket(net_server& srv):
	remote_socket_v2(srv)
{
	set_authorize_check(900000);
	is_register_ = is_login_ = false;
}
