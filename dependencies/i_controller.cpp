#include "i_controller.h"
#include "i_global_define.h"
boost::shared_ptr<msg_base> i_controller::create_msg(int cmd)
{
	return boost::shared_ptr<msg_base>();
}

int i_controller::on_client_msg(boost::shared_ptr<msg_base> msg,
	boost::shared_ptr<remote_socket_v3> psock,
	int& result)
{
	return error_msg_has_no_handler;
}
