#include "game_srv_socket.h"
//游戏服务器连接 <游戏id,服务器socket>
#include "error.h"
#include "msg_server.h"
#include "player.h"
#include "log.h"


extern std::map<int, std::map<std::string, internal_server_inf>> registed_servers; 
extern std::map<int, std::map<std::string, priv_game_channel>> private_game_rooms;
template<class socket_t> void	broadcast_room_msg(int gameid, msg_base<socket_t> &msg);
extern internal_server_inf* find_server(srv_socket_ptr psock);
void game_srv_socket::close(bool passive)
{
	remote_socket_impl::close(passive);
}
