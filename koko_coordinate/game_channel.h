#pragma once
#include <set>
#include "player.h"
#include "koko_socket.h"
#include "boost/asio/deadline_timer.hpp"
#include "boost/smart_ptr.hpp"

template<class remote_t> class msg_base;
struct none_socket;
typedef boost::shared_ptr<msg_base<none_socket>> smsg_ptr;

class koko_channel : public boost::enable_shared_from_this<koko_channel>
{
public:
	int					channel_id_;
	std::string			avip_;
	int					avport_;
	bool				is_showing_;
	std::string			host_uid_;
	std::map<std::string, unsigned int> players_;

	void			player_join(player_ptr pp, unsigned int sn);
	void			player_leave(player_ptr pp, unsigned int sn);
	void			broadcast_msg(msg_base<none_socket>& msg);
	void			start_logic();

	koko_channel();
	~koko_channel();

protected:
	boost::shared_ptr<boost::asio::deadline_timer> bot_add_timer_, bot_remove_timer_, bot_talk_timer_;
	void			add_one_bot(boost::system::error_code ec, bool init);
	void			remove_one_bot(boost::system::error_code ec, bool init);
	void			auto_talk(boost::system::error_code ec);
	smsg_ptr		build_player_info(player_ptr pp, bool isinit = false);
};

typedef boost::shared_ptr<koko_channel> channel_ptr;