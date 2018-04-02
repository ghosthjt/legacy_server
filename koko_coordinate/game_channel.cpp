#include "game_channel.h"
#include "msg_server.h"
#include <utility.h>
#include "safe_list.h"
#include <unordered_map>
#include <boost/thread/mutex.hpp>
#include "Database.h"
#include "db_delay_helper.h"
#include "msg_client.h"
#include "net_service.h"
#include "json_helper.hpp"

extern std::unordered_map<std::string, player_ptr> online_players;
extern safe_list<smsg_ptr>		broadcast_msg_;
extern player_ptr get_player(std::string uid);
extern boost::shared_ptr<boost::asio::io_service> timer_srv;
extern std::list<player_ptr> free_bots_;
extern std::vector<std::string> auto_chats_emotion;
extern std::vector<std::string> auto_chats;
extern boost::shared_ptr<net_server<koko_socket>> the_net;
smsg_ptr koko_channel::build_player_info(player_ptr pp, bool isinit)
{
	msg_player_info* msg = new msg_player_info();
	msg->uid_ = pp->uid_;
	msg->nickname_ = pp->nickname_;
	msg->iid_ = pp->iid_;
	msg->gold_ = pp->gold_;
	msg->vip_level_ = pp->vip_level();
	msg->isagent_ = pp->isagent_;
	msg->isagent_ |= (pp->isbot_ << 16);
	msg->isagent_ |= (pp->is_newbee_ << 17);
	msg->channel_ = channel_id_;
	msg->isinit_ = isinit;
	return smsg_ptr(msg);
}

koko_channel::koko_channel()
{
	channel_id_ = 0;
	is_showing_ = false;
	bot_add_timer_.reset(new boost::asio::deadline_timer(*timer_srv));
	bot_remove_timer_.reset(new boost::asio::deadline_timer(*timer_srv));
	bot_talk_timer_.reset(new boost::asio::deadline_timer(*timer_srv));
}

koko_channel::~koko_channel()
{
	bot_add_timer_->cancel();
	bot_add_timer_.reset();

	bot_remove_timer_->cancel();
	bot_remove_timer_.reset();

	bot_talk_timer_->cancel();
	bot_talk_timer_.reset();
}

void koko_channel::player_join(player_ptr pp, unsigned int sn)
{
	if(std::find(pp->in_channels_.begin(), pp->in_channels_.end(), channel_id_)
			== pp->in_channels_.end()){
		pp->in_channels_.push_back(channel_id_);
	}

	players_[pp->uid_] = sn;
	std::vector<player_ptr> v;

	//通知玩家列表
	auto it = players_.begin(); int i = 0;
	while (it != players_.end())
	{
		player_ptr infp = get_player(it->first); it++;
		if (infp.get() && infp != pp ) {
			v.push_back(infp);
			smsg_ptr smsg = build_player_info(pp, false);
			if (smsg.get()) {
				koko_socket_ptr pp_cnn = infp->from_socket_.lock();
				if (pp_cnn.get()) {
					send_protocol(pp_cnn, *smsg);
				}
			}
		}
	}
	v.push_back(pp);

	std::sort(v.begin(), v.end(), [](player_ptr p1, player_ptr p2)->bool{
		return p1->isagent_? true : (p1->isbot_ < p2->isbot_ ? true :  p1->vip_value_ > p2->vip_value_);
	});

	for (int i = 0; i < v.size(); i++)
	{
		koko_socket_ptr pp_cnn = pp->from_socket_.lock();
		if (pp_cnn.get()){
			if (v[i] == pp)	{
				smsg_ptr smsg = build_player_info(v[i], false);
				send_protocol(pp_cnn, *smsg);
			}
			else{
				smsg_ptr smsg = build_player_info(v[i], true);
				send_protocol(pp_cnn, *smsg);
			}
		}
	}
}

extern db_delay_helper& get_delaydb();
void koko_channel::player_leave(player_ptr pp, unsigned int sn)
{
	//如果请求已无效,则不做操作
	if (sn > 0 && sn != players_[pp->uid_]) return;

	if (pp->from_devtype_ != "PC"){
		msg_player_leave* msg = new msg_player_leave();
		msg->channel_ = channel_id_;
		msg->uid_ = pp->uid_;
		broadcast_msg(*msg);
	}

	auto itf = std::find(pp->in_channels_.begin(), pp->in_channels_.end(), channel_id_);
	if (itf != pp->in_channels_.end()){
		pp->in_channels_.erase(itf);
	}
	players_.erase(pp->uid_);
}

void koko_channel::broadcast_msg(msg_base<none_socket>& msg)
{
	auto it = players_.begin();
	while (it != players_.end())
	{	
		auto itp = online_players.find(it->first); it++;
		if (itp == online_players.end()) continue;

		player_ptr pp = itp->second;
		koko_socket_ptr conn = pp->from_socket_.lock();
		if (conn.get()){
			send_protocol(conn, msg);
		}
	}
}

void koko_channel::add_one_bot(boost::system::error_code ec, bool init)
{
	if(!free_bots_.empty()){
		player_ptr pp = free_bots_.front();
		free_bots_.pop_front();
		player_join(pp, 0);
		online_players.insert(std::make_pair(pp->uid_, pp));
	}
	if (!init){
		bot_add_timer_->expires_from_now(boost::posix_time::seconds(rand_r(3000, 9000)));
		bot_add_timer_->async_wait(boost::bind(&koko_channel::add_one_bot, 
			shared_from_this(),
			boost::asio::placeholders::error,
			false));
	}
}

void koko_channel::remove_one_bot(boost::system::error_code ec, bool init)
{
	if (!init){
		auto it = players_.begin();
		while (it != players_.end())
		{	
			auto itp = online_players.find(it->first); it++;
			if (itp == online_players.end()) continue;

			player_ptr pp = itp->second;
			if (pp->isbot_){
				player_leave(pp, 0);
				online_players.erase(itp);
				free_bots_.push_back(pp);
				break;
			}
		}
	}
	bot_remove_timer_->expires_from_now(boost::posix_time::seconds(6000));
	bot_remove_timer_->async_wait(boost::bind(&koko_channel::remove_one_bot,
		shared_from_this(),
		boost::asio::placeholders::error, 
		false));
}

void koko_channel::start_logic()
{
	int sz = rand_r(10, 20);
	for (int i = 0; i < sz; i++)
	{
		add_one_bot(boost::system::error_code(), true);
	}
	add_one_bot(boost::system::error_code(), false);
	remove_one_bot(boost::system::error_code(), false);

	bot_talk_timer_->expires_from_now(boost::posix_time::seconds(rand_r(60, 600)));
	bot_talk_timer_->async_wait(boost::bind(&koko_channel::auto_talk, 
		shared_from_this(),
		boost::asio::placeholders::error));
}

void koko_channel::auto_talk(boost::system::error_code ec)
{
	return;
	do 
	{
		if (auto_chats_emotion.empty() || auto_chats.empty() || players_.empty()){
			break;
		}
		std::vector<std::pair<std::string, int>> v;
		v.insert(v.end(), players_.begin(), players_.end());
		player_ptr pp; int cnt = 0;
		while (!pp.get() && cnt < 1000)
		{
			pp = get_player(v[rand_r(v.size() - 1)].first);
			if (pp.get() && pp->isbot_){
				break;
			}
			cnt++;
		}
		
		if (!pp.get()) break;
		if (!pp->isbot_) break;

		msg_chat msg;
		msg.channel_ = channel_id_;
		msg.to_[0] = 0;
		msg.from_sock_ = koko_socket_ptr(new koko_socket(*the_net));
		msg.from_sock_->the_client_ = pp;

		std::string em_prefix, em_suffix;
		int c = rand_r(3);
		for(int i = 0; i < c; i++)
		{
			em_prefix += auto_chats_emotion[rand_r(auto_chats_emotion.size() - 1)];
		}

		c = rand_r(3);
		for(int i = 0; i < c; i++)
		{
			em_suffix += auto_chats_emotion[rand_r(auto_chats_emotion.size() - 1)];
		}

		if (rand_r(100) < 40 || em_prefix + em_suffix == ""){
			int rr = rand_r(100);
			if (rr < 30){
				msg.content_ = auto_chats[rand_r(auto_chats.size() - 1)];
			}
			else if (rr < 60){
				msg.content_ = em_prefix + auto_chats[rand_r(auto_chats.size() - 1)];
			}
			else if (rr < 80 )	{
				msg.content_ = auto_chats[rand_r(auto_chats.size() - 1)] + em_suffix;
			}
			else
				msg.content_ = em_prefix + auto_chats[rand_r(auto_chats.size() - 1)] + em_suffix;
		}
		else{
			msg.content_ = em_prefix + em_suffix;
		}

		msg.handle_this();
	} while (0);


	bot_talk_timer_->expires_from_now(boost::posix_time::seconds(rand_r(5, 45)));
	bot_talk_timer_->async_wait(boost::bind(&koko_channel::auto_talk, 
		shared_from_this(),
		boost::asio::placeholders::error));
}
