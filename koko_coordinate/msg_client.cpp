#include "msg_client.h"
#include "error.h"
#include "safe_list.h"
#include "game_channel.h"
#include "player.h"
#include <unordered_map>
#include "msg_server.h"
#include "utility.h"
#include "server_config.h"
#include "log.h"
#include "Database.h"
#include "db_delay_helper.h"
#include "utf8_db.h"
#include "Query.h" 
#include "schedule_task.h"
#include "server_config.h"
#include "json_helper.hpp"
#include <deque>
#include "i_log_system.h"
#include "i_transaction.h"
#include "transaction_controller.h"
#include "transactions.h"

extern db_delay_helper	db_delay_helper_;
const int private_room_id_start = 10000000;
typedef boost::shared_ptr<msg_base<srv_socket_ptr>> vmsg_ptr;	
typedef boost::shared_ptr<msg_base<koko_socket_ptr>> msg_ptr;
extern safe_list<msg_ptr>		sub_thread_process_msg_lst_;
extern safe_list<vmsg_ptr>	sub_thread_process_vmsg_lst_;
extern std::map<int, channel_ptr>		koko_channels;

extern log_file<cout_and_file_output> glb_http_log;
extern player_ptr get_player(std::string uid);
extern service_config the_config;
extern db_delay_helper& get_delaydb();
extern boost::shared_ptr<utf8_data_base>	db_;
extern db_delay_helper	db_delay_helper_;

extern std::map<int, std::map<std::string, internal_server_inf>> registed_servers;

extern boost::shared_ptr<boost::asio::io_service> timer_srv;
extern service_config the_config;

extern std::string	md5_hash(std::string sign_pat);
extern std::map<int, present_info> glb_present_data;
extern std::map<int, item_info> glb_item_data;
extern std::map<int, activity_info>	glb_activity_data;

extern int	sync_user_item(std::string uid, int itemid);
extern safe_list<std::pair<koko_socket_ptr, smsg_ptr>> glb_thread_send_cli_msg_lst_;
extern std::map<std::string, std::map<int, system_prize>> glb_system_prize;
extern std::map<int, user_privilege>	glb_vip_privilege;
extern std::map<int, lucky_dial_info>	glb_lucky_dial_data;
extern std::map<int, std::vector<item_unit>> glb_present_bag;
extern std::map<int, user_privilege>	glb_vip_privilege;
extern std::string convert_vector_to_string(std::vector<int> vec, int ele_num, std::string big_sep = "", std::string small_sep = "");

extern int	cost_gold(std::string reason, std::string uid, __int64 count, std::vector<std::string> sync);
extern int send_mail(user_mail& mail, bool is_main_thread = true, bool is_need_conv = true);

typedef boost::shared_ptr<game_info> gamei_ptr;
std::map<int, std::map<std::string, priv_game_channel>> private_game_rooms;
std::map<int, std::map<std::string, std::deque<int>>> player_private_rooms;	//玩家好友房显示列表
extern std::map<int, gamei_ptr> all_games;

extern int	update_user_item(std::string reason,
	std::string sn,
	std::string uid, int op,
	int itemid, __int64 count, __int64& ret,
	std::vector<std::string> sync_to_server);
extern std::vector<std::string>	all_channel_servers();

int update_item(std::string reason, int update_type, int count, std::string uid, std::vector<item_unit> vitem)
{
	player_ptr pp = get_player(uid);
	koko_socket_ptr conn = nullptr;
	if (pp) {
		conn = pp->from_socket_.lock();
	}

	if (update_type == item_operator_cost) {
		int result = 0;
		int i = 0;
		for (; i < vitem.size(); i++)
		{
			if (vitem[i].cat_ == ITEM_TYPE_COMMON) {
				__int64 cost = vitem[i].item_num_ * count;
				if (vitem[i].item_id_ == item_id_gold_game) {
					int ret = cost_gold(reason, uid, cost, all_channel_servers());
					if (ret != error_success) {
						result = error_not_enough_gold;
						break;
					}
				}
				else {
					__int64 outc = 0;
					int ret = update_user_item(reason, "localcall", uid, item_operator_cost, vitem[i].item_id_, cost, outc, all_channel_servers());
					if (ret != error_success) {
						result = error_not_enough_item;
						break;
					}
				}
			}
		}

		//扣款失败则回滚之前操作
		if (result != 0) {
			for (int j = i - 1; j >= 0; j--)
			{
				if (vitem[j].cat_ == ITEM_TYPE_COMMON) {
					__int64 cost = vitem[j].item_num_ * count;
					if (vitem[j].item_id_ == item_id_gold_game) {
						__int64 outc = 0;
						int ret = update_user_item("购买失败回滚", "localcall", uid, item_operator_add, item_id_gold_game, cost, outc, all_channel_servers());
						if (ret != error_success) {
							i_log_system::get_instance()->write_log(loglv_error, "buy item fail and rollback error:%d, price_item_id:%d, price_item_num:%d", ret, vitem[j].item_id_, cost);
						}
					}
					else {
						__int64 outc = 0;
						int ret = update_user_item("购买失败回滚", "localcall", uid, item_operator_add, vitem[j].item_id_, cost, outc, all_channel_servers());
						if (ret != error_success) {
							i_log_system::get_instance()->write_log(loglv_error, "buy item fail and rollback error:%d, price_item_id:%d, price_item_num:%d", ret, vitem[j].item_id_, cost);
						}
					}
				}
			}

			return result;
		}
	}
	else if (update_type == item_operator_add) {
		//加道具
		for (int i = 0; i < vitem.size(); i++)
		{
			//普通物品
			if (vitem[i].cat_ == ITEM_TYPE_COMMON) {
				int ret = 0;
				__int64 item_num = 1;
				if (vitem[i].item_id_ <= item_id_gold_game_bank) {
					__int64 outc = 0;
					item_num = count * vitem[i].item_num_;
					ret = update_user_item(reason, "localcall", uid, item_operator_add, vitem[i].item_id_, item_num, outc, all_channel_servers());
				}
				else {
					auto itp_item = glb_item_data.find(vitem[i].item_id_);
					if (itp_item != glb_item_data.end()) {
						if (itp_item->second.type_ == 0) {
							item_num = count * vitem[i].item_num_;
						}

						__int64 outc = 0;
						ret = update_user_item(reason, "localcall", uid, item_operator_add, vitem[i].item_id_, item_num, outc, all_channel_servers());

						//时长物品加时长
						if (itp_item->second.type_ == 1) {
							__int64 sec = count * vitem[i].item_num_;
							std::string set_expire = "if(expire_time > now(), from_unixtime(unix_timestamp(expire_time) + " + lx2s(sec) + "), from_unixtime(unix_timestamp(now()) + " + lx2s(sec) + "))";

							Database& db(*db_);
							BEGIN_UPDATE_TABLE("user_item");
							SET_FIELD_VALUE("count", 1);
							SET_FINAL_VALUE_NO_COMMA("expire_time", set_expire);
							WITH_END_CONDITION("where uid = '" + uid + "' and item_id = " + lx2s(vitem[i].item_id_));
							EXECUTE_IMMEDIATE();

							extern void	send_items_data_to_player(std::string uid, boost::shared_ptr<koko_socket> psock, int item_id = -1);
							auto pconn = pp->from_socket_.lock();
							if (pconn) {
								send_items_data_to_player(pp->platform_uid_, pconn, vitem[i].item_id_);
							}
						}
					}
				}

				//如果物品没有给成功,比如网络问题,数据库问题,应该要保存下来后续再试.目前先不管了.
				if (ret != error_success) {
					return ret;
				}
			}
			//头像框
			else if (vitem[i].cat_ == ITEM_TYPE_HEADFRAME) {
				int frame_id = vitem[i].item_id_;
				{
					BEGIN_INSERT_TABLE("user_head_frame");
					SET_FIELD_VALUE("uid", uid);
					SET_FINAL_VALUE("frame_id", frame_id);
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}

				if (conn) {
					msg_user_headframe_list msg;
					msg.headframe_id_ = frame_id;
					send_protocol(conn, msg);
				}
			}
			//头像
			else if (vitem[i].cat_ == ITEM_TYPE_HEAD) {
				int head_id = vitem[0].item_id_;
				int sec = count * vitem[i].item_num_;
				{
					BEGIN_INSERT_TABLE("user_head");
					SET_FIELD_VALUE("uid", uid);
					SET_FIELD_VALUE("head_id", head_id);
					SET_FINAL_VALUE_NO_COMMA("expire_time", "from_unixtime(if(unix_timestamp(now()) + " + lx2s(sec) + " > unix_timestamp('2038-01-19 03:14:07'), unix_timestamp('2038-01-19 03:14:07'), unix_timestamp(now()) + " + lx2s(sec) + "))");
					WITH_END_CONDITION("on duplicate key update expire_time = if(expire_time > now(), from_unixtime(if(unix_timestamp(expire_time) + " + lx2s(sec) + " > unix_timestamp('2038-01-19 03:14:07'), unix_timestamp('2038-01-19 03:14:07'), unix_timestamp(expire_time) + " + lx2s(sec) + ")), from_unixtime(if(unix_timestamp(now()) + " + lx2s(sec) + " > unix_timestamp('2038-01-19 03:14:07'), unix_timestamp('2038-01-19 03:14:07'), unix_timestamp(now()) + " + lx2s(sec) + ")))");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}

				if (conn) {
					Query q(*db_);
					std::string sql = "select head_id, unix_timestamp(`expire_time`) - unix_timestamp(now()) from user_head where uid = '" + uid + "' and head_id = " + lx2s(head_id);
					if (!q.get_result(sql)) {
						return error_sql_execute_error;
					}

					if (q.fetch_row()) {
						msg_user_head_list msg;
						int head_id = q.getval();
						msg.head_ico_ = lx2s(head_id);
						msg.remain_sec_ = q.getbigint();
						send_protocol(conn, msg);
					}
				}
			}
			else {
				i_log_system::get_instance()->write_log(loglv_error, "buy item fail item cat invalid:%d", vitem[i].cat_);
			}
		}
	}

	return error_success;
}

void		remove_room(int gameid, std::string master)
{
	std::map<std::string, priv_game_channel>&mp = private_game_rooms[gameid];
	auto it = mp.begin();
	while (it != mp.end())
	{
		priv_game_channel& chn = it->second;
		auto itr = chn.rooms_.begin();
		while (itr != chn.rooms_.end())
		{
			priv_room_ptr pr = itr->second;
			if (pr->master_ == master) {
				chn.rooms_.erase(itr);
				break;
			}
			itr++;
		}
		it++;
	}
}

priv_game_channel* find_room_by_master(int gameid, std::string master)
{
	std::map<std::string, priv_game_channel>&mp = private_game_rooms[gameid];
	auto it = mp.begin();
	while (it != mp.end())
	{
		priv_game_channel& chn = it->second;
		auto itr = chn.rooms_.begin();
		while (itr != chn.rooms_.end())
		{
			priv_room_ptr pr = itr->second;
			if (pr->master_ == master) {
				return &chn;
			}
			itr++;
		}
		it++;
	}
	return nullptr;
}

priv_game_channel* find_channel_by_master(int gameid, std::string master)
{
	std::map<std::string, priv_game_channel>&mp = private_game_rooms[gameid];
	auto it = mp.begin();
	while (it != mp.end())
	{
		priv_game_channel& chn = it->second;
		auto itr = chn.rooms_.begin();
		while (itr != chn.rooms_.end())
		{
			priv_room_ptr pr = itr->second;
			if (pr->master_ == master) {
				return &chn;
			}
			itr++;
		}
		it++;
	}
	return nullptr;
}

priv_room_ptr find_room(int gameid, int roomid)
{
	std::map<std::string, priv_game_channel>&mp = private_game_rooms[gameid];
	auto it = mp.begin();
	while (it != mp.end())
	{
		priv_game_channel& chn = it->second;
		auto itr = chn.rooms_.begin();
		while (itr != chn.rooms_.end())
		{
			priv_room_ptr pr = itr->second;
			if (pr->room_id_ == roomid)	{
				return pr;
			}
			itr++;
		}
		it++;
	}
	return nullptr;
}

priv_room_ptr find_room(int gameid, std::string uid)
{
	std::map<std::string, priv_game_channel>&mp = private_game_rooms[gameid];
	auto it = mp.begin();
	while (it != mp.end())
	{
		priv_game_channel& chn = it->second;
		auto itr = chn.rooms_.begin();
		while (itr != chn.rooms_.end())
		{
			priv_room_ptr pr = itr->second;
			auto itf = std::find_if(pr->onseats_.begin(), pr->onseats_.end(), 
				[uid](const std::pair<std::string, private_room_inf::seat_ptr>& it)->bool {
				return it.first == uid;
			});
			if (itf != pr->onseats_.end()){
				return pr;
			}
			itr++;
		}
		it++;
	}
	return nullptr;
}

std::deque<int> find_room_list(int gameid, std::string uid)
{
	if (player_private_rooms.find(gameid) != player_private_rooms.end()) {
		auto mp = player_private_rooms[gameid];
		if (mp.find(uid) != mp.end()) {
			return mp[uid];
		}
	}

	return std::deque<int>();
}

internal_server_inf* find_server(srv_socket_ptr psock)
{
	auto it1 = registed_servers.find(psock->gameid_);
	if (it1 == registed_servers.end()){
		return nullptr;
	}

	auto it2 = it1->second.find(psock->instance_);
	if (it2 == it1->second.end()){
		return nullptr;
	}
	return &it2->second;
}

channel_ptr get_channel(int chnid)
{
	auto it = koko_channels.find(chnid);
	if (it == koko_channels.end()){
		channel_ptr chn = channel_ptr(new koko_channel());
		koko_channels[chnid] = chn;
		chn->channel_id_ = chnid;
		chn->start_logic();
		return chn;
	}
	else{
		return it->second;
	}
}

template<class socket_t> 
void	broadcast_room_msg(int gameid, msg_base<socket_t> &msg)
{
	auto it = private_game_rooms.find(gameid);
	if (it == private_game_rooms.end()) return;

	std::map<std::string, priv_game_channel>& mp = it->second;
	auto it1 = mp.begin();
	while (it1 != mp.end())
	{
		priv_game_channel& tv = it1->second;
		for (int i = tv.players_.size() - 1; i >= 0; i--)
		{
			if (tv.players_[i]->is_connection_lost_) {
				tv.players_.erase(tv.players_.begin() + i);
			}
			else {
				auto sock = tv.players_[i]->from_socket_.lock();
				if (sock.get()) {
					send_protocol(sock, msg);
				}
			}
		}
		it1++;
	}

}

void sync_room_info(priv_room_ptr pr)
{
	{
		msg_room_create_ret msg;
		msg.inf = *pr;
		broadcast_room_msg(pr->game_id_, msg);
	}
	{
		msg_private_room_players msg;
		msg.roomid_ = pr->room_id_;
		auto it = pr->onseats_.begin();
		int i = 0;
		while (it != pr->onseats_.end() && i < 8)
		{
			msg.players_[i] = it->second->nick_name_;
			i++;
			it++;
		}
		broadcast_room_msg(pr->game_id_, msg);
	}
}

int msg_user_login_channel::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_debug, "user login channel confirmed, %s", from_sock_->remote_ip().c_str());
	__int64 tn = 0;
	::time(&tn);
	__int64 sn = boost::lexical_cast<__int64>(sn_);
	//2小时过期
	if (tn - sn_ > 2 * 3600){
		return error_wrong_sign;
	}

	std::string sign_pattn = std::string(uid_) + boost::lexical_cast<std::string>(sn_) + the_config.token_verify_key_;
	std::string	tok = md5_hash(sign_pattn);
	if (tok != std::string(token_)){
		return error_wrong_sign;
	}

	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));

	i_log_system::get_instance()->write_log(loglv_debug, "user login channel accepted, %s", from_sock_->remote_ip().c_str());
	return error_success;
}
	
int msg_user_login_delegate::handle_this()
{
	if (sub_thread_process_msg_lst_.size() > 100){
		return error_server_busy;
	}
	
	sub_thread_process_vmsg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<srv_socket_ptr>>(shared_from_this()));
	return error_success;
}

int msg_join_channel::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_debug, "user join channel confirmed, %s", from_sock_->remote_ip().c_str());

	channel_ptr channel = get_channel(channel_);
	player_ptr pp = from_sock_->the_client_.lock();
	if (pp.get()){
		channel->player_join(pp, sn_);
	}
	i_log_system::get_instance()->write_log(loglv_debug, "user join channel complete, %s", from_sock_->remote_ip().c_str());
	return error_success;
}

int msg_chat::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()){
		return error_cant_find_player;
	}

	if (channel_ == -1){
		__int64 out_count = 0;
		int ret = cost_gold("大喇叭扣费", pp->platform_uid_, the_config.horn_cost_, all_channel_servers());
		if (ret != error_success){
			return error_not_enough_gold;
		}
		
		int ftag = 0;
		ftag = pp->isagent_;
		ftag |= pp->vip_level() << 4;
		Database& db(*db_);
		BEGIN_INSERT_TABLE("system_allserver_broadcast");
		SET_FIELD_VALUE("from_uid", pp->platform_uid_);
		SET_FIELD_VALUE("from_nickname", pp->nickname_);
		SET_FIELD_VALUE("from_tag", ftag);
		SET_FIELD_VALUE("msg_type", 1);
		SET_FINAL_VALUE("content", content_);
		EXECUTE_IMMEDIATE();
	}
	else{
		auto it = koko_channels.find(channel_);
		if (it == koko_channels.end()){
			return error_cant_find_player;
		}

		msg_chat_deliver* msg = new msg_chat_deliver();
		msg->channel_ = channel_;
		msg->from_tag_ = pp->isagent_;
		msg->from_tag_ |= pp->vip_level() << 4;
		msg->content_ = content_;
		msg->from_uid_ = pp->platform_uid_.c_str();
		msg->nickname_ = pp->nickname_.c_str();

		channel_ptr chn = it->second;
		chn->channel_id_ = channel_;
		if (std::string(to_) != ""){

			auto it = chn->players_.find(to_);
			if (it == chn->players_.end()){
				return error_cant_find_player;
			}

			msg->msg_type_ = 1;
			player_ptr to_pp = get_player(to_);
			if (!to_pp.get()){
				return error_cant_find_player;
			}

			msg->to_uid_ = to_pp->uid_;
			msg->to_nickname_ = to_pp->nickname_;
			msg->to_tag_ = to_pp->isagent_;
			msg->to_tag_ |= to_pp->vip_level() << 4;
			koko_socket_ptr topp_cnn = to_pp->from_socket_.lock();
			if (topp_cnn.get()){
				send_protocol(topp_cnn, *msg);
			}
			send_protocol(from_sock_, *msg);
		}
		else{
			msg->msg_type_ = 0;
			chn->broadcast_msg(*msg);
		}
	}
	return error_success;
}

int msg_leave_channel::handle_this()
{
	channel_ptr channel = get_channel(channel_);
	player_ptr pp = from_sock_->the_client_.lock();
	if (pp.get()){
		channel->player_leave(pp, sn_);
	}
	BEGIN_UPDATE_TABLE("setting_host_room_list");
	SET_FINAL_VALUE("is_online", "is_online - 1");
	WITH_END_CONDITION("where roomid=" + boost::lexical_cast<std::string>(channel_));
	EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	return error_success;
}

int msg_register_to_world::handle_this()
{
	if (key_ != "{3B0636F0-6831-43DF-B54D-E3700820FFC8}") {
		i_log_system::get_instance()->write_log(loglv_warning , "inlegal game server.",
			gameid_,
			instance_.c_str());
		return error_cannt_regist_more;
	}

	internal_server_inf inf;
	inf.wsock_ = from_sock_;
	inf.open_ip_ = open_ip_;
	inf.open_port_ = open_port_;
	inf.game_id_ = gameid_;
	inf.instance_ = instance_;
	inf.roomid_start_ = roomid_start_;
	std::map<std::string, internal_server_inf>& mp = registed_servers[gameid_];
	if (mp.find(instance_) == mp.end()){

		auto itf = std::find_if(mp.begin(), mp.end(),
			[this](const std::pair<std::string, internal_server_inf>& it)->bool {
			return it.second.roomid_start_ == this->roomid_start_;
		});

		//roomid start 重复
		if (itf != mp.end()){
			return error_cannt_regist_more;
		}
		
		from_sock_->set_authorized();
		from_sock_->gameid_ = gameid_;
		from_sock_->instance_ = instance_;

		mp[instance_] = inf;
		i_log_system::get_instance()->write_log(loglv_info, "game server connected:%d, instance:%s",
			gameid_, 
			instance_.c_str());

		return error_success;
	}
	else{
		if (mp[instance_].wsock_.lock()) {
			if (mp[instance_].wsock_.lock()->isworking()) {
				i_log_system::get_instance()->write_log(loglv_warning, "warning, game server has been registered,id=%d, instance:%s...",
					gameid_,
					instance_.c_str());

				msg_common_reply_internal<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_register_to_world);
				msg.err_ = -1;
				msg.SN = 0;
				send_protocol(from_sock_, msg);

				return error_cannt_regist_more;
			}
		}
		from_sock_->set_authorized();
		from_sock_->gameid_ = gameid_;
		from_sock_->instance_ = instance_;

		mp[instance_] = inf;
		i_log_system::get_instance()->write_log(loglv_info, "game server connected:%d, instance:%s",
			gameid_,
			instance_.c_str());

		return error_success;
	}
}

int msg_koko_trade_inout::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_debug, "msg_koko_trade_inout begin. [gameins = %s] dir = %d, uid = %s, sn = %s, count = %lld",
		from_sock_->instance_.c_str(),
		dir_, 
		uid_.c_str(),
		sn_.c_str(),
		count_);

	int gameid = -1;
	std::string game_name;
	auto psrv = find_server(from_sock_);
	if (psrv){
		gameid = psrv->game_id_;
		game_name = lx2s(gameid);
	}

	auto itg = all_games.find(gameid);
	if (itg != all_games.end()){
		game_name = itg->second->game_name_;
	}

	game_name_ = gbk(game_name);
	game_id_ = gameid;

	msg_koko_trade_inout_ret msg;
	msg.count_ = 0;
	msg.result_ = error_success;
	msg.sn_ = sn_;

	std::string pat = uid_;
	pat += boost::lexical_cast<std::string>(dir_);
	pat += boost::lexical_cast<std::string>(count_);
	pat += boost::lexical_cast<std::string>(time_stamp_);
	pat += sn_;
	pat += the_config.token_verify_key_;
	std::string si = md5_hash(pat);

	if (si != sign_) {
		msg.result_ = error_wrong_sign;
	}

	if (count_ < 0) {
		msg.result_ = error_invalid_data;
	}

	//12小时超时
	if ((time(nullptr) - time_stamp_) > 12 * 3600 * 1000) {
		msg.result_ = error_time_expired;
	}

	if (msg.result_ != error_success){
		int ret = send_protocol(from_sock_, msg);
		if (ret != error_success) {
			i_log_system::get_instance()->write_log(loglv_warning, "msg_koko_trade_inout finished. but reply send failed. dir = %d, uid = %s, sn = %s",
				dir_, uid_.c_str(), sn_.c_str());
		}
		else
			i_log_system::get_instance()->write_log(loglv_debug, "msg_koko_trade_inout finished with result = %d. dir = %d, uid = %s, sn = %s",
				msg.result_, dir_, uid_.c_str(), sn_.c_str());
		return ret;
	}
	else {
		extern safe_list<boost::shared_ptr<msg_koko_trade_inout>> glb_trades;
		glb_trades.push_back(boost::dynamic_pointer_cast<msg_koko_trade_inout>(shared_from_this()));
		return error_success;
	}
}

void msg_koko_trade_inout::handle_this2()
{
	static std::map<std::string, __int64> handled_trade;

	msg_koko_trade_inout_ret msg;
	msg.count_ = 0;
	msg.result_ = 0;
	msg.sn_ = sn_;

	auto it = handled_trade.find(sn_);
	//如果订单处理过
	if (it != handled_trade.end()) {
		//如果状态未确定,则查询数据库状态
		int state = it->second;
		if (state < 0) {
			do_trade(&msg, handled_trade);
		}
		else {
			msg.result_ = error_business_handled;
			msg.count_ = state;
		}
		send_protocol(from_sock_, msg);
	}
	else {
		handled_trade.insert(std::make_pair(sn_, -1));
		do_trade(&msg, handled_trade);
	}
}

void msg_koko_trade_inout::do_trade(msg_koko_trade_inout_ret* pmsg, std::map<std::string, __int64>& handled_trade)
{
	int ret = error_success;

	__int64 retv = 0;
	int cid = dir_ >> 16;
	if (cid == 0) {
		cid = item_id_gold_game;
	}
	else if (cid == 1) {
		cid = item_id_gold;
	}

	dir_ = dir_ & 0xFFFF;
	//从平台兑入游戏部分
	if (dir_ == 0) {
		//op = 0 add,  1 cost, 2 cost all
		__int64 r = 0;
		ret = update_user_item(game_name_ + ":兑入游戏-部分",
			std::string(sn_),
			uid_, item_operator_cost,
			cid, count_, retv,
			all_channel_servers());

		if (ret == error_success) {
			update_user_item(game_name_ + ":兑入游戏-部分",
				std::string(sn_),
				uid_, item_operator_add, item_id_gold_game_lock,
				retv, r, std::vector<std::string>());
		}
	}
	//兑入所有
	else if (dir_ == 1) {
		__int64 r1 = 0, r2 = 0;
		int ret1 = update_user_item(game_name_ + ":兑入游戏-所有", std::string(sn_),
			uid_, item_operator_cost_all, cid, 0, r1, std::vector<std::string>());
		ret = ret1;
		if ((r1 + r2 > 0) && cid == item_id_gold_game) {
			__int64 r = 0;
			update_user_item(game_name_ + ":自动兑入游戏-所有", std::string(sn_),
				uid_, 0, item_id_gold_game_lock, r1 + r2, r, std::vector<std::string>());
		}
		retv = r1 + r2;

		if (cid == item_id_gold_game) {
			sync_user_item(uid_, cid);
		}
	}
	//从游戏兑入平台
	else {
		if (cid == item_id_gold_game) {
			ret = update_user_item(game_name_ + ":自动兑回平台", std::string(sn_),
				uid_, item_operator_add, cid, count_, retv, std::vector<std::string>());

			sync_user_item(uid_, cid);
		}
		else {
			ret = update_user_item(game_name_ + ":自动兑回平台", std::string(sn_),
				uid_, item_operator_add, cid, count_, retv, all_channel_servers());
		}
		if (ret == error_success && cid == item_id_gold_game) {
			//平台还回来钱,锁定的钱就还原了.
			__int64 r = 0;
			update_user_item(game_name_ + ":自动兑回平台", std::string(sn_),
				uid_, item_operator_cost_all, item_id_gold_game_lock, 0, r, std::vector<std::string>());
		}
	}

	if (ret == error_success) {
		handled_trade[sn_] = retv;
		if (retv < 0) {
			pmsg->result_ = error_invalid_request;
		}
		else {
			pmsg->result_ = error_success;
			pmsg->count_ = retv;
		}
	}
	else {
		//不如不是状态未确定
		if (ret != error_mysql_execute_uncertain && ret != -100) {
			handled_trade[sn_] = 0;
		}
		pmsg->result_ = ret;
	}
	send_protocol(from_sock_, *pmsg);
}

int msg_action_record::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (pp) {
		if (op_type_ == OP_TYPE_INSERT) {
			Database& db(*db_);
			BEGIN_INSERT_TABLE("log_client_action");
			SET_FIELD_VALUE("uid", pp->platform_uid_);
			SET_FIELD_VALUE("action_id", action_id_);
			SET_FIELD_VALUE("action_data", action_data_);
			SET_FIELD_VALUE("action_data2", action_data2_);
			SET_FIELD_VALUE("action_data3", action_data3_);
			SET_FIELD_VALUE("action_data4", action_data4_);
			SET_FIELD_VALUE("action_data5", action_data5_);
			SET_FINAL_VALUE("from_ip", from_sock_->remote_ip());
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}
		else if (op_type_ == OP_TYPE_UPDATE) {
			Database& db(*db_);
			BEGIN_UPDATE_TABLE("log_client_action");
			SET_FIELD_VALUE("uid", pp->platform_uid_);
			SET_FIELD_VALUE("action_id", action_id_);
			SET_FIELD_VALUE("action_data", action_data_);
			SET_FIELD_VALUE("action_data2", action_data2_);
			SET_FIELD_VALUE("action_data3", action_data3_);
			SET_FIELD_VALUE("action_data4", action_data4_);
			SET_FIELD_VALUE("action_data5", action_data5_);
			SET_FINAL_VALUE("from_ip", from_sock_->remote_ip());
			WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "' and action_id = " + lx2s(action_id_) + " order by create_time desc limit 1");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}
	}
	
	return error_success;
}

int msg_get_token::handle_this()
{

	return error_success;
}

int msg_buy_item_notice::handle_this()
{
	auto itp = glb_present_data.find(item_id_);
	if (itp == glb_present_data.end()) {
		return error_invalid_data;
	}

	if (!itp->second.price_.empty() && !itp->second.item_.empty()){
		//添加购买记录
		Database& db(*db_);
		BEGIN_INSERT_TABLE("log_buy_goods");
		SET_FIELD_VALUE("uid", uid_);
		SET_FIELD_VALUE("goods_id", item_id_);
		SET_FIELD_VALUE("goods_num", item_num_);
		SET_FIELD_VALUE("cost_item_id", itp->second.price_[0].item_id_);
		SET_FIELD_VALUE("cost_item_num", item_num_*itp->second.price_[0].item_num_);
		SET_FIELD_VALUE("item_id", itp->second.item_[0].item_id_);
		SET_FIELD_VALUE("item_num", item_num_ * itp->second.item_[0].item_num_);
		SET_FIELD_VALUE("status", itp->second.is_delay_);
		SET_FIELD_VALUE("save_sec", itp->second.buy_record_sec);
		SET_FINAL_VALUE("comment", "");
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	
	return error_business_handled;
}

int msg_use_item_notice::handle_this()
{
	auto itp = glb_item_data.find(item_id_);
	if (itp == glb_item_data.end()) {
		return error_invalid_data;
	}

	//添加使用记录
	Database& db(*db_);
	BEGIN_INSERT_TABLE("log_use_item");
	SET_FIELD_VALUE("uid", uid_);
	SET_FIELD_VALUE("item_id", item_id_);
	SET_FINAL_VALUE("count", item_num_);
	EXECUTE_NOREPLACE_DELAYED("", get_delaydb());

	return error_business_handled;
}

int msg_sync_rank_notice::handle_this()
{
	std::string sql = "select `nickname`, `headpic_name` from `user_account` where uid = '" + std::string(uid_) + "'";
	auto th = boost::dynamic_pointer_cast<msg_sync_rank_notice>(shared_from_this());
	db_delay_helper_.get_result(sql, [th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		std::string nickname;
		std::string head_ico;
		if (q.fetch_row()) {
			nickname = q.getstr();
			head_ico = q.getstr();
		}

		int rank_type = -1;
		int rank_data = th->item_num_;
		if (th->item_id_ == item_id_gold_free) {
			rank_type = 1;
			rank_data /= 100;
		}

		if (rank_type != -1) {
			Database& db(*db_);
			BEGIN_INSERT_TABLE("rank");
			SET_FIELD_VALUE("uid", th->uid_);
			SET_FIELD_VALUE("type", rank_type);
			SET_FIELD_VALUE("nickname", nickname);
			SET_FIELD_VALUE("head_ico", head_ico);
			SET_FIELD_VALUE("today", rank_data);
			SET_FINAL_VALUE("total", rank_data);
			WITH_END_CONDITION("on duplicate key update nickname = '" + nickname + "', head_ico = " + lx2s(head_ico) + ", today = if(date(now()) = date(update_time), today + " + lx2s(rank_data) + ", " + lx2s(rank_data) + "), total = total + " + lx2s(rank_data) + ", update_time = now()");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}
	});

	return error_success;
}

extern boost::thread_specific_ptr<std::vector<channel_server>> channel_servers;
int msg_server_broadcast::handle_this()
{
	if (!channel_servers.get()) return 0;

	auto& chnsrv = *channel_servers;
	for (unsigned i = 0; i < chnsrv.size(); i++)
	{
		Database& db(*db_);
		BEGIN_INSERT_TABLE("system_allserver_broadcast");
		SET_FIELD_VALUE("msg_type", 0);
		SET_FIELD_VALUE("for_server", chnsrv[i].instance_);
		SET_FINAL_VALUE("content", content_);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}

	return error_success;
}

/************************************************************************/
/* 自定义房间相关
/************************************************************************/

class confirm_enter_room_task : public task_info
{
public:
	boost::weak_ptr<private_room_inf>		wroom;
	boost::weak_ptr<private_room_inf::seat> wseat;
	confirm_enter_room_task(boost::asio::io_service& ios) :
		task_info(ios)
	{

	}
	virtual int routine()
	{
		boost::shared_ptr<private_room_inf> proom = wroom.lock();
		private_room_inf::seat_ptr pseat = wseat.lock();
		if (pseat.get() && proom.get()) {
			proom->onseats_.erase(pseat->uid_);
			sync_room_info(proom);
		}
		return task_info::routine_ret_break;
	}
};

int		msg_enter_private_room::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;

	std::map<std::string, priv_game_channel>& mp = private_game_rooms[gameid_];
	
	std::map<std::string, internal_server_inf>& mps = registed_servers[gameid_];
	if (mps.empty()){
		return error_cant_find_server;
	}

	priv_room_ptr pr = find_room(gameid_, roomid_);

	msg_enter_private_room_ret msg;
	msg.roomid_ = roomid_;
	msg.gameid_ = gameid_;
	if(!pr){
		msg.succ_ = 0;
	}
	else{
		auto itf = mps.find(pr->srv_instance_);
		if (itf == mps.end()) {
			return error_cant_find_player;
		}

		internal_server_inf& srv = itf->second;
		msg.ip_ = srv.open_ip_;
		msg.port_ = srv.open_port_;

		if (pr->need_psw_){
			if (pr->psw_ != psw_ && pr->master_ != pp->uid_){
				msg.succ_ = 3;
			}
		}

		if (msg.succ_ == -1){
			if(pr->onseats_.find(pp->uid_) == pr->onseats_.end()){
				if (pr->seats_ > pr->onseats_.size()){
					private_room_inf::seat_ptr pseat(new private_room_inf::seat());
					pseat->uid_ = pp->uid_;
					pseat->nick_name_ = pp->nickname_;
					pr->onseats_.insert(std::make_pair(pp->uid_, pseat));

					//8秒之后移除本次的预占位.
					confirm_enter_room_task* ptask = new confirm_enter_room_task(*timer_srv);
					ptask->wroom = pr;
					ptask->wseat = pseat;		
					task_ptr task(ptask);

					ptask->schedule(8000);
					msg.succ_ = 1;
				}
				else{
					msg.succ_ = 2;
				}
			}
			else{
				msg.succ_ = 1;
			}
		}
	}
	send_protocol(from_sock_, msg);

	return error_success;
}

//房间玩家同步
int msg_private_room_psync::handle_this()
{
	int gameid = -1;
	
	auto psrv = find_server(from_sock_);
	if (psrv){
		gameid = psrv->game_id_;
	}

	if (gameid == -1)
		return error_cant_find_player;

	std::map<std::string, priv_game_channel>& mp = private_game_rooms[gameid];
	if (mp.find(psrv->instance_) == mp.end()){
		return error_cant_find_player;
	}

	priv_game_channel& chn = mp[psrv->instance_];
	auto itf = chn.rooms_.find(room_id_);
	if (itf == chn.rooms_.end()){
		return error_cant_find_player;
	}

	priv_room_ptr pr = chn.rooms_[room_id_];
	if (pr == nullptr){
		return error_cant_find_player;
	}

	private_room_inf::seat_ptr pseat(new private_room_inf::seat());
	pseat->uid_ = uid_;
	pseat->nick_name_ = nick_name_;
	replace_map_v(pr->onseats_, std::make_pair(uid_, pseat));

	sync_room_info(pr);
	return error_success;
}

//房间同步
int msg_private_room_sync::handle_this()
{
	int gameid = -1;

	auto psrv = find_server(from_sock_);
	if (psrv) {
		gameid = psrv->game_id_;
	}

	if (gameid == -1)
		return error_cant_find_player;

	auto itf = private_game_rooms.find(gameid);
	if (itf == private_game_rooms.end()) {
		if (room_id_ >= private_room_id_start){
			return error_success;
		}
	}

	std::map<std::string, priv_game_channel>& mp = private_game_rooms[gameid];
	auto & room_lst = mp[psrv->instance_];

	room_lst.game_id_ = gameid;
	auto it = room_lst.rooms_.find(room_id_);
	if(it == room_lst.rooms_.end()){
		//如果同步过来的自定义房间不存在了,忽略
		if (room_id_ >= private_room_id_start){
			return error_success;
		}
		//else if(player_count_ > 0){
			priv_room_ptr pr(new private_room_inf());
			pr->config_index_ = config_index_;
			pr->room_id_ = room_id_;
			pr->srv_instance_ = psrv->instance_;
			pr->game_id_ = gameid;
			pr->player_count_ = player_count_;
			pr->seats_ = seats_;
			COPY_STR(pr->master_, master_.c_str());
			COPY_STR(pr->master_nick_, master_nick_.c_str());
			pr->total_turn_ = total_turn_;		//
			pr->params_ = params_;				//
			pr->state_ = state_;				//
			COPY_STR(pr->ip_, psrv->open_ip_.c_str());
			pr->port_ = psrv->open_port_;
			room_lst.rooms_[pr->room_id_] = pr;
			sync_room_info(pr);
		//}
	}
	else{
		priv_room_ptr pr = it->second;

		pr->config_index_ = config_index_;
		pr->player_count_ = player_count_;
		pr->seats_ = seats_;
		pr->total_turn_ = total_turn_;
		pr->params_ = params_;
		pr->state_ = state_;
		COPY_STR(pr->ip_, psrv->open_ip_.c_str());
		pr->port_ = psrv->open_port_;

		pr->onseats_.clear();
		
		/*if (pr->player_count_ == 0 && pr->master_ != "") {
			msg_private_room_remove msg;
			msg.roomid_ = pr->room_id_;
			broadcast_room_msg(pr->game_id_, msg);
			room_lst.rooms_.erase(it);
		}
		else {
			pr->onseats_.clear();
			if (pr->player_count_ == 0){
				sync_room_info(pr);
			}
		}*/
	}
	return error_success;
}

int msg_alloc_game_server::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;

	std::map<std::string, internal_server_inf>& mps = registed_servers[game_id_];

	std::pair<std::string, internal_server_inf> itsrv;
	if (!pp->last_game_srv.empty() && mps.find(pp->last_game_srv) != mps.end()) {
		itsrv = *mps.find(pp->last_game_srv);
	}
	else {
		bool is_find = false;
		auto droom = find_room_list(game_id_, pp->uid_);
		for (unsigned i = 0; i < droom.size(); i++)
		{
			priv_room_ptr pr = find_room(game_id_, droom[i]);

			if (pr) {
				auto itf = mps.find(pr->srv_instance_);
				if (itf == mps.end()) {
					continue;
				}
				else {
					itsrv = *itf;
					is_find = true;
					break;
				}
			}
		}

		if (!is_find) {
			//随机选择一个游戏服务器
			std::vector<std::pair<std::string, internal_server_inf>> v1;
			v1.insert(v1.end(), mps.begin(), mps.end());
			if (v1.empty()) {
				return error_cant_find_player;
			}
			itsrv = v1[rand_r(v1.size() - 1)];
		}
	}

	pp->last_game_id_ = itsrv.second.game_id_;
	pp->last_game_srv = itsrv.second.instance_;

	msg_switch_game_server msg_sv;
	msg_sv.ip_ = itsrv.second.open_ip_;
	msg_sv.port_ = itsrv.second.open_port_;
	send_protocol(from_sock_, msg_sv);

	return error_success;
}

int msg_get_private_room_list::handle_this()
{
	auto itf = private_game_rooms.find(game_id_);
	if (itf == private_game_rooms.end()) 
		return error_cant_find_player;

	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return error_cant_find_player;

	if (pp->last_game_srv.empty())
		return error_cant_find_player;

	std::map<std::string, priv_game_channel>& mp = private_game_rooms[game_id_];
	priv_game_channel& room_lst = mp[pp->last_game_srv];

	if (std::find(room_lst.players_.begin(), room_lst.players_.end(), pp) == room_lst.players_.end()){
		room_lst.players_.push_back(pp);
	}
	
	std::vector<std::pair<int, priv_room_ptr>> v;
	v.insert(v.end(),room_lst.rooms_.begin(), room_lst.rooms_.end());

	std::sort(v.begin(), v.end(),  [&](std::pair<int, priv_room_ptr>& p1, std::pair<int, priv_room_ptr>& p2)->bool{
		return (p1.second->master_ == pp->uid_) > (p2.second->master_ == pp->uid_);
	});

	auto it = v.begin();
	while (it != v.end())
	{
		sync_room_info(it->second);
		it++;
	}
	return error_success;
}

int msg_get_private_room_record::handle_this()
{
	if (game_id_ == -1)
		return error_cant_find_player;

	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	auto itf = player_private_rooms.find(game_id_);
	if (itf == player_private_rooms.end()) {
		return error_cant_find_room;
	}

	auto & mp = player_private_rooms[game_id_];
	auto & droom = mp[pp->uid_];
	
	for (unsigned i = 0; i < droom.size(); i++)
	{
		auto pr = find_room(game_id_, droom[i]);
		if (!pr) continue;

		msg_get_private_room_record_ret msg;
		msg.room_id_ = droom[i];
		msg.player_count_ = pr->player_count_;
		msg.seats_ = pr->seats_;
		msg.master_ = pr->master_;
		msg.total_turn_ = pr->total_turn_;
		msg.params_ = pr->params_;
		msg.state_ = pr->state_;
		
		send_protocol(from_sock_, msg);
	}

	{
		msg_sequence_send_over<none_socket> msg;
		msg.cmd_ = GET_CLSID(msg_get_private_room_record_ret);
		send_protocol(from_sock_, msg);
	}

	return error_success;
}

int msg_buy_item::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	auto itp = glb_present_data.find(item_id_);
	if (itp == glb_present_data.end()) {
		return error_invalid_data;
	}

	//判断是否禁止购买
	if (itp->second.forbid_buy_) {
		return error_invalid_request;
	}

	//判断购买数量
	if (item_count_ < itp->second.min_buy_num_ || item_count_ > itp->second.max_buy_num_) {
		return error_count_invalid;
	}

	if (pp->vip_level() < itp->second.vip_level_) {
		return error_not_enough_viplevel;
	}

	present_info pinfo = itp->second;
	auto setting_item_date = glb_item_data;

	BEGIN_TRANS(msg_buy_item);

	auto return_code_sender = [ptrans, pthis]() {
		msg_common_reply<none_socket> msg;
		msg.rp_cmd_ = pthis->head_.cmd_;
		msg.err_ = ptrans->contex_.return_code_;
		send_protocol(pthis->from_sock_, msg);
	};

	ptrans->contex_.send_return_code_ = return_code_sender;

	//判断是否超过今日购买上限
	if (pinfo.day_limit_ > 0) {
		auto sql_builder = [pp, pthis]()->std::string {
			return "set @ret = 0;select num into @ret from user_day_buy where uid = '" + pp->platform_uid_ + "' and pid = " + lx2s(pthis->item_id_) + " and date(update_time) = date(now());select @ret;";
		};

		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);

		BEGIN_QUERY_CALLBACK(error_cant_find_player, placeholder, pinfo);
		READ_ONE_ROW(error_cant_find_player);
		int today_num = q.getval();
		if (today_num + pthis->item_count_ > pinfo.day_limit_) {
			ptrans->contex_.return_code_ = error_beyond_today_limit;
			return state_failed;
		}
		ptrans->contex_.data_["today_num"] = lx2s(today_num);
		END_CALLBACK();
	}

	//购买头像框(没有才能购买)
	if (itp->second.cat_ == ITEM_TYPE_HEADFRAME) {
		for (int i = 0; i < itp->second.item_.size(); i++)
		{
			if (itp->second.item_[i].cat_ == ITEM_TYPE_HEADFRAME) {
				int frame_id = itp->second.item_[i].item_id_;
				auto sql_builder = [pp, frame_id]()->std::string {
					return "select frame_id from user_head_frame where uid = '" + pp->platform_uid_ + "' and frame_id = " + lx2s(frame_id);
				};
				ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
				CHECK_EXISTS(error_already_buy_present);
			}
		}
	}

	BEGIN_FUNCTION(pp, pinfo, setting_item_date)
	//扣款
	int ret = update_item("购买" + boost::locale::conv::between(pinfo.name_, "GB2312", "UTF-8"), item_operator_cost, pthis->item_count_, pp->platform_uid_, pinfo.price_);
	if (ret != error_success) {
		ptrans->contex_.return_code_ = ret;
		return state_failed;
	}

	//加道具
	ret = update_item("购买" + boost::locale::conv::between(pinfo.name_, "GB2312", "UTF-8"), item_operator_add, pthis->item_count_, pp->platform_uid_, pinfo.item_);
	if (ret != error_success) {
		ptrans->contex_.return_code_ = ret;
		return state_failed;
	}

	//添加购买记录
	if (pinfo.buy_record_sec > 0 && !pinfo.price_.empty() && !pinfo.item_.empty()){
		Database& db(*db_);
		BEGIN_INSERT_TABLE("log_buy_goods");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("goods_id", pthis->item_id_);
		SET_FIELD_VALUE("goods_num", pthis->item_count_);
		SET_FIELD_VALUE("cost_item_id", pinfo.price_[0].item_id_);
		SET_FIELD_VALUE("cost_item_num", pthis->item_count_ * pinfo.price_[0].item_num_);
		SET_FIELD_VALUE("item_id", pinfo.item_[0].item_id_);
		SET_FIELD_VALUE("item_num", pthis->item_count_ * pinfo.item_[0].item_num_);
		SET_FIELD_VALUE("status", pinfo.is_delay_);
		SET_FIELD_VALUE("save_sec", pinfo.buy_record_sec);
		SET_FINAL_VALUE("comment", pthis->comment_);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}

	if (pinfo.day_limit_ > 0) {
		int today_num = s2i<int>(ptrans->contex_.data_["today_num"]);
		msg_user_day_buy_data msg;
		msg.pid_ = pthis->item_id_;
		msg.num_ = today_num + pthis->item_count_;
		send_protocol(pthis->from_sock_, msg);

		Database& db = *db_;
		BEGIN_INSERT_TABLE("user_day_buy");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("pid", pthis->item_id_);
		SET_FINAL_VALUE("num", pthis->item_count_);
		WITH_END_CONDITION("on duplicate key update num = if(date(update_time) = date(now()), num + " + lx2s(pthis->item_count_) + ", " + lx2s(pthis->item_count_) + "), update_time = now()");
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}

	{
		ptrans->contex_.return_code_ = error_business_handled;
		return state_failed;
	}

	END_CALLBACK()
	END_TRANS(get_guid())

	return error_success;
}


int		get_viplv(__int64 exp)
{
	Query q(*db_);
	std::string sql = "select viplv from setting_vip where exp >= " + lx2s(exp) + " order by exp desc";
	if (!(q.get_result(sql) && q.fetch_row())) {
		return q.getval();
	}
	return 0;
}

int msg_send_present::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	BEGIN_TRANS(msg_send_present);

	do_handle_this(pp, ptrans, pthis);
	END_TRANS(get_guid())

	return error_success;
}

void msg_send_present::do_handle_this(player_ptr pp, trans_ptr ptrans, boost::shared_ptr<msg_send_present> pthis)
{
	int placeholder = 0;
	auto return_code_sender = [ptrans, pthis]() {
		msg_common_reply<none_socket> msg;
		msg.rp_cmd_ = pthis->head_.cmd_;
		msg.err_ = ptrans->contex_.return_code_;
		send_protocol(pthis->from_sock_, msg);
	};

	ptrans->contex_.send_return_code_ = return_code_sender;

	{
		auto sql_builder = [pthis]()->std::string {
			return "select uid, nickname, vip_value, is_agent from user_account where iid = '" + std::string(pthis->to_) + "'";
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		BEGIN_QUERY_CALLBACK(error_cant_find_player, placeholder);
		READ_ONE_ROW(error_cant_find_player);
		ptrans->contex_.data_["touid"] = q.getstr();
		ptrans->contex_.data_["tonickname"] = q.getstr();
		END_CALLBACK();
	}

	//查询自己是否能发送礼物
	{
		auto sql_builder = [pp]()->std::string {
			return "select uid from tbl_limit_blacklist where uid = '" + pp->platform_uid_ + "'" + " and type = 1";
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		CHECK_EXISTS(error_cannot_send_present);
	}

	//查询好友是否能接收礼物
	{
		auto sql_builder = [ptrans]()->std::string {
			return "select uid from tbl_limit_blacklist where uid = '" + ptrans->contex_.data_["touid"] + "'" + " and type = 1";
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		CHECK_EXISTS(error_cannot_recv_present);
	}

	BEGIN_FUNCTION(pp);
	auto itp = glb_item_data.find(pthis->present_id_);
	if (itp == glb_item_data.end()) {
		ptrans->contex_.return_code_ = error_invalid_data;
		return state_failed;
	}

	//物品是否可赠送
	if (itp->second.is_send_ == 0) {
		ptrans->contex_.return_code_ = error_invalid_data;
		return state_failed;
	}

	if (itp->second.to_item_.empty()) {
		ptrans->contex_.return_code_ = error_invalid_data;
		return state_failed;
	}

	//赠送数量是否合法
	if (!(pthis->count_ >= itp->second.min_send_num_ && pthis->count_ <= itp->second.max_send_num_)) {
		ptrans->contex_.return_code_ = error_invalid_data;
		return state_failed;
	}

	__int64 outc = 0;
	int ret = update_user_item("送礼物", "localcall", pp->platform_uid_,
		item_operator_cost,
		pthis->present_id_,
		pthis->count_,
		outc, all_channel_servers());

	if (ret != error_success) {
		ptrans->contex_.return_code_ = error_not_enough_gold;
		return state_failed;
	}

	__int64 pay = itp->second.to_item_[0].second * pthis->count_;
	//__int64 exc = (1 - itp->second.tax_ / 10000.0) * pay;
	__int64 exc = pay;	//TODO:暂时赋值为pay

	__int64 a1 = 0;
	ret = update_user_item("送礼物", "localcall",
		ptrans->contex_.data_["touid"],
		item_operator_add,
		pthis->present_id_,
		pthis->count_, a1, all_channel_servers());

	msg_common_broadcast* msg = new msg_common_broadcast();
	msg->type_ = msg_common_broadcast::broadcast_type_send_present;
	msg->text_ = pp->nickname_ + "|" + lx2s(pp->iid_) + "|" + lx2s(::time(nullptr)) + "|" +
		lx2s(pthis->count_) + "|" +
		lx2s(pthis->present_id_);

	//赠送时间
	boost::posix_time::ptime ptime = boost::posix_time::second_clock::local_time();
	std::string send_time(boost::posix_time::to_iso_extended_string(ptime));
	int pos = send_time.find('T');
	send_time.replace(pos, 1, std::string(" "));

	auto topp = get_player(ptrans->contex_.data_["touid"]);
	if (topp) {
		auto cnn = topp->from_socket_.lock();
		if (cnn.get()) {
			send_protocol(cnn, *msg);
		}
	}

	delete msg;
	{
		Database& db(*db_);
		BEGIN_INSERT_TABLE("log_koko_gift_send");
		SET_FIELD_VALUE("from_uid", pp->platform_uid_);
		SET_FIELD_VALUE("to_uid", ptrans->contex_.data_["touid"]);
		SET_FIELD_VALUE("present_id", pthis->present_id_);
		SET_FIELD_VALUE("count", pthis->count_);
		SET_FIELD_VALUE("pay", pay);
		SET_FIELD_VALUE("tax", pay - exc);
		SET_FIELD_VALUE("result", int(ret == 0));
		SET_FINAL_VALUE("create_time", send_time);
		EXECUTE_NOREPLACE_DELAYED("", db_delay_helper_);
	}

	if (itp->second.to_item_[0].first == item_id_gold_game) {
		int golds = pthis->count_ * itp->second.to_item_[0].second;
		Database& db = *db_;
		BEGIN_INSERT_TABLE("user_send_golds");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FINAL_VALUE("golds", golds);
		WITH_END_CONDITION("on duplicate key update golds = if(date(update_time) = date(now()), golds + " + lx2s(golds) + ", " + lx2s(golds) + "), update_time = now()");
		EXECUTE_NOREPLACE_DELAYED("", db_delay_helper_);
	}

	//保存邮件
	{
		user_mail mail;
		mail.uid_ = ptrans->contex_.data_["touid"];
		mail.mail_type_ = 1;
		mail.title_type_ = 6;
		mail.title_ = "来自玩家“" + boost::locale::conv::between(pp->nickname_, "GB2312", "UTF-8") + "”的礼物";
		mail.content_ = boost::locale::conv::between(ptrans->contex_.data_["tonickname"], "GB2312", "UTF-8") + "：\n\t\t您好！这是来自玩家“" + boost::locale::conv::between(pp->nickname_, "GB2312", "UTF-8") + "”赠送给您的礼物，请及时查收哦！";
		mail.state_ = 0;
		mail.attach_state_ = 2;
		std::vector<int> vec;
		vec.push_back(pthis->present_id_);
		vec.push_back(pthis->count_);
		mail.attach_ = combin_str(vec, ",");
		mail.save_days_ = 30;
		send_mail(mail);
	}

	{
		ptrans->contex_.return_code_ = error_business_handled;
		return state_failed;
	}

	END_CALLBACK()
}

int msg_use_present::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	auto itp = glb_item_data.find(present_id_);
	if (itp == glb_item_data.end()) {
		return error_invalid_data;
	}

	//是否能使用
	if (itp->second.is_use_ == 0) {
		return error_invalid_data;
	}

	int use_count = 1;
	//时长道具检查时效
	if (itp->second.type_ == 1) {
		std::string sql = "select unix_timestamp(`expire_time`) - unix_timestamp(now()) from `user_item` where `uid` = '" + pp->platform_uid_ + "' and item_id = " + lx2s(present_id_) +
			" and unix_timestamp(`expire_time`) - unix_timestamp(now()) > 0";
		auto th = boost::dynamic_pointer_cast<msg_use_present>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th, use_count](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}
			query_result_reader q(vret[0]);
			if (!q.fetch_row()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_use_present);
				msg.err_ = error_not_enough_item;
				send_protocol(th->from_sock_, msg);
				return;
			}

			//同步剩余时长
			{
				msg_sync_item_data msg;
				msg.item_id_ = th->present_id_;
				msg.key_ = 0;
				msg.value_ = lx2s(q.getval());
				send_protocol(th->from_sock_, msg);
			}

			//使用物品返回
			{
				msg_use_present_ret msg;
				msg.present_id_ = th->present_id_;
				msg.count_ = th->count_;
				send_protocol(th->from_sock_, msg);
			}

			//保存使用记录
			{
				Database& db(*db_);
				BEGIN_INSERT_TABLE("log_use_item");
				SET_FIELD_VALUE("uid", pp->platform_uid_);
				SET_FIELD_VALUE("item_id", th->present_id_);
				SET_FINAL_VALUE("count", use_count);
				EXECUTE_IMMEDIATE();
			}
		});
	}
	else {
		use_count = count_;
		//非时长道具
		__int64 outc = 0;
		int ret = update_user_item("使用" + boost::locale::conv::between(itp->second.name_, "GB2312", "UTF-8"), "localcall", pp->platform_uid_, item_operator_cost, present_id_, count_, outc, all_channel_servers());
		if (ret != error_success) {
			return error_not_enough_item;
		}

		//使用加其他道具的
		std::string prize_item_str;
		for (int i = 0; i < itp->second.to_item_.size(); i++)
		{
			__int64 exc = itp->second.to_item_[i].second * count_;
			if (exc != 0) {
				outc = 0;
				ret = update_user_item("使用" + boost::locale::conv::between(itp->second.name_, "GB2312", "UTF-8"), "localcall", pp->platform_uid_, item_operator_add, itp->second.to_item_[i].first, exc, outc, all_channel_servers());
				//如果钱没有给成功,比如网络问题,数据库问题,应该要保存下来后续再试.目前先不管了.
				if (ret != error_success) {
					return error_sql_execute_error;
				}

				prize_item_str = prize_item_str + lx2s(itp->second.to_item_[i].first) + "," + lx2s(exc) + ",";
			}
		}

		{
			msg_use_present_ret msg;
			msg.present_id_ = present_id_;
			msg.count_ = count_;
			msg.prize_item_ = prize_item_str;
			send_protocol(from_sock_, msg);
		}

		//保存使用记录
		{
			Database& db(*db_);
			BEGIN_INSERT_TABLE("log_use_item");
			SET_FIELD_VALUE("uid", pp->platform_uid_);
			SET_FIELD_VALUE("item_id", present_id_);
			SET_FINAL_VALUE("count", use_count);
			EXECUTE_IMMEDIATE();
		}
	}

	return error_success;
}

int msg_query_data::handle_this()
{
	if (data_id_ == 0) {
		std::string sql = "select nickname from user_account where iid = '" + params +"'";
		auto th = boost::dynamic_pointer_cast<msg_query_data>(shared_from_this());
		db_delay_helper_.get_result(sql, [th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			msg_query_data_ret msg;
			msg.data_id_ = th->data_id_;
			msg.params_ = th->params;
			query_result_reader q(vret[0]);
			if (q.fetch_row()) {
				msg.data_ = q.getstr();
			}

			send_protocol(th->from_sock_, msg);
		});
	}

	return error_success;
}

int msg_get_present_record::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}
	
	std::string select_uid;
	std::string condition_uid;

	if (1 == type_) {
		select_uid = "from_uid";
		condition_uid = "to_uid";
	}
	else {
		select_uid = "to_uid";
		condition_uid = "from_uid";
	}

	std::string sql = "SELECT l.`" + select_uid + "`, u.`nickname`, u.`iid`, " + \
		"l.`present_id`, l.`count`,	unix_timestamp(l.`create_time`) \
		FROM `log_koko_gift_send` l inner join user_account u on l.`" + select_uid + "` = u.`uid` where l.`" + condition_uid + "`" + " = '" + pp->platform_uid_ + \
		"' and unix_timestamp(now())-unix_timestamp(l.`create_time`) < 2*3600 order by l.`create_time` desc";
	auto th = boost::dynamic_pointer_cast<msg_get_present_record>(shared_from_this());
	db_delay_helper_.get_result(sql, [th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}
		int i = 0;
		msg_get_present_record_ret msg;
		msg.type_ = th->type_;
		query_result_reader q(vret[0]);
		while (q.fetch_row())
		{
			if (0 == i) {
				msg.tag_ = 0;
			}
			else {
				msg.tag_ = 1;
			}

			std::string uid = q.getstr();
			msg.nickname_ = q.getstr();
			msg.iid_ = q.getval();
			int present_id = q.getval();
			auto itp = glb_item_data.find(present_id);
			if (itp == glb_item_data.end()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_present_record);
				msg.err_ = error_invalid_data;
				send_protocol(th->from_sock_, msg);
			}
			msg.present_name_ = itp->second.name_;
			msg.count_ = q.getval();
			msg.time_stamp_ = q.getval();

			send_protocol(th->from_sock_, msg);
			i++;
		}
	});
	
	return error_success;
}

int msg_get_buy_item_record::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	std::string sql = "select `goods_id`, `goods_num`, `status`, unix_timestamp(`create_time`), `comment` from log_buy_goods where `uid` = '" + \
		pp->platform_uid_ + "' and unix_timestamp(now()) - unix_timestamp(`create_time`) < `save_sec` order by `create_time` desc limit 999";
	auto th = boost::dynamic_pointer_cast<msg_get_buy_item_record>(shared_from_this());
	db_delay_helper_.get_result(sql, [th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}
		query_result_reader q(vret[0]);
		int i = 0;
		while (q.fetch_row())
		{
			msg_get_buy_item_record_ret msg;
			if (0 == i) {
				msg.tag_ = 0;
			}
			else {
				msg.tag_ = 1;
			}

			msg.item_id_ = q.getval();
			msg.item_num_ = q.getval();
			msg.status_ = q.getval();
			msg.time_stamp_ = q.getval();
			msg.comment_ = q.getstr();

			send_protocol(th->from_sock_, msg);
			i++;
		}
	});

	return 0;
}

int msg_verify_user::handle_this()
{
	sub_thread_process_vmsg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base>(shared_from_this()));
	return error_success;
}

int msg_transfer_gold_to_game::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_debug, "recv msg_transfer_gold_to_game!!");
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;
	__int64 outc = 0;
	int ret = update_user_item("兑换K豆", "localcall", pp->platform_uid_, item_operator_cost, item_id_gold, count_, outc, all_channel_servers());
	if (ret == error_success) {
		ret = update_user_item("兑换K豆", "localcall", pp->platform_uid_, item_operator_add, item_id_gold_game, count_, outc, all_channel_servers());
	}

	if (ret == error_success) {
		ret = error_business_handled;
	}

	return ret;
}

int msg_send_present_by_pwd::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	BEGIN_TRANS(msg_send_present_by_pwd);

	//校验保险箱密码
	{
		auto sql_builder = [pp]()->std::string {
			return "select bank_psw from user_account where uid = '" + pp->platform_uid_ + "'";
		};

		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);

		BEGIN_QUERY_CALLBACK(error_cant_find_player, placeholder);
		READ_ONE_ROW(error_cant_find_player);
		std::string pwd = q.getstr();
		if (pwd != pthis->bank_pwd_) {
			ptrans->contex_.return_code_ = error_wrong_password;
			return state_failed;
		};
		END_CALLBACK();
	}

	do_handle_this(pp, ptrans, pthis);
	END_TRANS(get_guid());

	return error_success;
}

int	msg_req_sync_item::handle_this()
{
	if (item_id_ <= 0 && item_id_ != -1) {
		return error_invalid_data;
	}

	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}
	
	extern void	send_items_to_player(std::string uid, boost::shared_ptr<koko_socket> psock, int item_id = -1);
	auto pconn = pp->from_socket_.lock();
	send_items_to_player(pp->platform_uid_, pconn, item_id_);

	return error_success;
}

int msg_private_room_remove_sync::handle_this()
{
	int gameid = -1;

	auto psrv = find_server(from_sock_);
	if (psrv) {
		gameid = psrv->game_id_;
	}

	if (gameid == -1)
		return error_cant_find_player;

	{
		auto itf = private_game_rooms.find(gameid);
		if (itf == private_game_rooms.end()) {
			if (room_id_ >= private_room_id_start) {
				return error_success;
			}
		}

		std::map<std::string, priv_game_channel>& mp = private_game_rooms[gameid];
		auto & room_lst = mp[psrv->instance_];

		auto it = room_lst.rooms_.find(room_id_);
		if (it == room_lst.rooms_.end()) {
			//如果同步过来的自定义房间不存在了,忽略
			if (room_id_ >= private_room_id_start) {
				return error_success;
			}
		}
		else {
			//由于players_里面始终为空，所以此消息改为下面发送
			/*priv_room_ptr pr = it->second;
			msg_private_room_remove msg;
			msg.roomid_ = pr->room_id_;
			broadcast_room_msg(pr->game_id_, msg);*/
			room_lst.rooms_.erase(it);
		}
	}

	{
		auto itf = player_private_rooms.find(gameid);
		if (itf == player_private_rooms.end()) {
			return error_success;
		}

		auto & mp = player_private_rooms[gameid];
		for (auto & it : mp)
		{
			auto dit = std::find(it.second.begin(), it.second.end(), room_id_);
			if (dit != it.second.end()) {
				{
					msg_private_room_remove msg;
					msg.gameid_ = gameid;
					msg.roomid_ = *dit;
					player_ptr pp = get_player(it.first);
					if (pp.get() && pp->from_socket_.lock()) {
						send_protocol(pp->from_socket_.lock(), msg);
					}
				}

				it.second.erase(dit);
				if (it.second.size() == 0) {
					mp.erase(it.first);
				}
			}
		}
	}

	return error_success;
}

//房间同步
int msg_enter_private_room_notice::handle_this()
{
	int gameid = -1;

	auto psrv = find_server(from_sock_);
	if (psrv) {
		gameid = psrv->game_id_;
	}

	if (gameid == -1)
		return error_cant_find_player;

	auto & mp = player_private_rooms[gameid];
	auto & droom = mp[uid_];

	auto it = std::find(droom.begin(), droom.end(), room_id_);
	if (it != droom.end()) {
		droom.erase(it);
	}

	droom.push_front(room_id_);

	return error_success;
}

int msg_get_login_prize_list::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	//每日签到状态
	{
		std::string sql = "select sign_days, serial_days, serial_state, date(now()) = date(last_sign_time), ifnull(date(now()) = date_add(date(last_sign_time), interval 1 day), 0), ifnull(yearweek(date_add(now(), interval -1 day)) = yearweek(date_add(`last_sign_time`, interval -1 day)), 0) from `user_sign_prize` where `uid` = '" + pp->platform_uid_ + "'";
		auto th = boost::dynamic_pointer_cast<msg_get_login_prize_list>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			int serial_days = 0;
			int serial_state = 0;
			int cur_login_day = 0;
			int cur_login_state = 0;

			int sign_days = 0;  //已签到天数
			int is_today = 0;
			int is_serial = 0;
			int is_this_week = 0;

			query_result_reader q(vret[0]);
			if (q.fetch_row()) {
				sign_days = q.getval();
				serial_days = q.getval();
				serial_state = q.getval();
				is_today = q.getval();
				is_serial = q.getval();
				is_this_week = q.getval();

				//今天已签到
				if (is_today) {
					cur_login_state = 1;
					cur_login_day = sign_days;
				}
				//今日未签到
				else {
					if (!is_this_week) {
						sign_days = 0;
					}

					cur_login_state = 0;
					cur_login_day = sign_days + 1;

					//重置连续天数和状态
					if (!is_serial || serial_state == 111) {
						serial_days = 0;
						serial_state = 0;
						{
							BEGIN_UPDATE_TABLE("user_sign_prize");
							SET_FIELD_VALUE_NO_COMMA("last_sign_time", "last_sign_time");
							SET_FIELD_VALUE("serial_days", serial_days);
							SET_FINAL_VALUE("serial_state", serial_state);
							WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "'");
							EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
						}
					}
				}
			}
			else {
				serial_days = 0;
				serial_state = 0;
				cur_login_state = 0;
				cur_login_day = sign_days + 1;

				{
					BEGIN_INSERT_TABLE("user_sign_prize");
					SET_FIELD_VALUE("uid", pp->platform_uid_);
					SET_FIELD_VALUE("sign_days", 0);
					SET_FIELD_VALUE("last_sign_time", "");
					SET_FIELD_VALUE("serial_days", 0);
					SET_FINAL_VALUE("serial_state", 0);
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}
			}

			msg_user_login_prize_list msg;
			msg.serial_days_ = serial_days;
			msg.serial_state_ = serial_state;
			msg.login_day_ = cur_login_day;
			msg.login_state_ = cur_login_state;
			send_protocol(th->from_sock_, msg);
		});
	}

	return error_success;
}

int msg_get_login_prize::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	if (!(type_ >= 0 && type_ <= 3)) {
		return error_invalid_request;
	}

	BEGIN_TRANS(msg_get_login_prize);
	auto return_code_sender = [ptrans, pthis]() {
		msg_common_reply<none_socket> msg;
		msg.rp_cmd_ = pthis->head_.cmd_;
		msg.err_ = ptrans->contex_.return_code_;
		send_protocol(pthis->from_sock_, msg);
	};

	ptrans->contex_.send_return_code_ = return_code_sender;

	{
		auto sql_builder = [pthis, pp]()->std::string {
			return "select sign_days, serial_days, serial_state, date(now()) = date(last_sign_time), ifnull(date(now()) = date_add(date(last_sign_time), interval 1 day), 0), ifnull(yearweek(date_add(now(), interval -1 day)) = yearweek(date_add(`last_sign_time`, interval -1 day)), 0) from `user_sign_prize` where `uid` = '" + pp->platform_uid_ + "'";
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		BEGIN_QUERY_CALLBACK(error_invalid_request, placeholder);
		READ_ONE_ROW(error_invalid_request);

		int sign_days = 0;			//已签到天数
		int serial_days = 0;		//连续签到天数
		int serial_state = 0;		//连续签到奖励领取状态
		int is_today = 0;
		int is_serial = 0;
		int is_this_week = 0;

		ptrans->contex_.data_["sign_days"] = lx2s(q.getval());
		ptrans->contex_.data_["serial_days"] = lx2s(q.getval());
		ptrans->contex_.data_["serial_state"] = lx2s(q.getval());
		ptrans->contex_.data_["is_today"] = lx2s(q.getval());
		ptrans->contex_.data_["is_serial"] = lx2s(q.getval());
		ptrans->contex_.data_["is_this_week"] = lx2s(q.getval());

		END_CALLBACK();
	}

	BEGIN_FUNCTION(pp);
	int sign_days = s2i<int>(ptrans->contex_.data_["sign_days"]);
	int serial_days = s2i<int>(ptrans->contex_.data_["serial_days"]);
	int serial_state = s2i<int>(ptrans->contex_.data_["serial_state"]);
	int is_today = s2i<int>(ptrans->contex_.data_["is_today"]);
	int is_serial = s2i<int>(ptrans->contex_.data_["is_serial"]);
	int is_this_week = s2i<int>(ptrans->contex_.data_["is_this_week"]);

	//领取每日登录奖励
	int day = 0;
	int item_id = 0, item_num = 0;
	std::vector<int> vitem;
	if (pthis->type_ == 0) {
		//今天已签到
		if (is_today) {
			ptrans->contex_.return_code_ = error_invalid_request;
			return state_failed;
		}

		if (!is_this_week) {
			sign_days = 0;
		}

		sign_days++;
		day = sign_days;

		//计算连续签到
		if (is_serial) {
			serial_days++;
			if (serial_days >= 9 && serial_state == 111) {
				serial_days = 1;
				serial_state = 0;
			}
		}
		else {
			serial_days = 1;
			serial_state = 0;
		}

		if (glb_system_prize.find("login") == glb_system_prize.end() || glb_system_prize["login"].find(sign_days) == glb_system_prize["login"].end()) {
			ptrans->contex_.return_code_ = error_invalid_request;
			return state_failed;
		}

		auto vprize = glb_system_prize["login"][sign_days].prize_;
		int count = glb_vip_privilege[pp->vip_level()].login_prize_times_;		//VIP加成
		//加道具
		int ret = update_item("每日签到奖励", item_operator_add, count, pp->platform_uid_, vprize);
		if (ret != error_success) {
			ptrans->contex_.return_code_ = ret;
			return state_failed;
		}

		for (int i = 0; i < vprize.size(); ++i)
		{
			vitem.push_back(vprize[i].item_id_);
			vitem.push_back(vprize[i].item_num_ * count);
		}

		{
			BEGIN_UPDATE_TABLE("user_sign_prize");
			SET_FIELD_VALUE("sign_days", sign_days);
			SET_FIELD_VALUE("serial_days", serial_days);
			SET_FIELD_VALUE("serial_state", serial_state);
			SET_FINAL_VALUE_NO_COMMA("last_sign_time", "now()");
			WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "'");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}

		{
			msg_user_login_prize_list msg;
			msg.serial_days_ = serial_days;
			msg.serial_state_ = serial_state;
			msg.login_day_ = sign_days;
			msg.login_state_ = 1;
			send_protocol(pthis->from_sock_, msg);
		}
	}
	//领取连续签到奖励
	else {
		//已经领取了
		if (((serial_state / (int)::pow(10, pthis->type_ - 1)) % 10) == 1) {
			ptrans->contex_.return_code_ = error_invalid_request;
			return state_failed;
		}

		//是否能领奖
		if (serial_days < pthis->type_ * 3) {
			ptrans->contex_.return_code_ = error_invalid_request;
			return state_failed;
		}

		if (glb_system_prize.find("serial_login") == glb_system_prize.end() || glb_system_prize["serial_login"].find(3 * pthis->type_) == glb_system_prize["serial_login"].end()) {
			ptrans->contex_.return_code_ = error_invalid_request;
			return state_failed;
		}

		auto vprize = glb_system_prize["serial_login"][3 * pthis->type_].prize_;
		int count = glb_vip_privilege[pp->vip_level()].login_prize_times_;		//VIP加成
		int ret = update_item("每日签到奖励", item_operator_add, count, pp->platform_uid_, vprize);
		if (ret != error_success) {
			ptrans->contex_.return_code_ = ret;
			return state_failed;
		}

		for (int i = 0; i < vprize.size(); ++i)
		{
			vitem.push_back(vprize[i].item_id_);
			vitem.push_back(vprize[i].item_num_ * count);
		}

		serial_state += ::pow(10, pthis->type_ - 1);
		day = 3 * pthis->type_;

		{
			BEGIN_UPDATE_TABLE("user_sign_prize");
			SET_FIELD_VALUE_NO_COMMA("last_sign_time", "last_sign_time");
			SET_FINAL_VALUE("serial_state", serial_state);
			WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "'");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}
	}

	{
		msg_user_login_prize msg;
		msg.prize_type_ = pthis->type_;
		send_protocol(pthis->from_sock_, msg);
	}

	{
		msg_common_recv_present msg;
		msg.type_ = "LoginSign|" + lx2s(pthis->type_);
		msg.item_ = convert_vector_to_string(vitem, 2, "|", ",");
		send_protocol(pthis->from_sock_, msg);
	}

	{
		BEGIN_INSERT_TABLE("log_sign_prize");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("type", pthis->type_ == 0 ? 0 : 1);
		SET_FIELD_VALUE("day", day);
		SET_FIELD_VALUE("item_id", item_id);
		SET_FINAL_VALUE("item_num", item_num);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	
	END_CALLBACK()
	END_TRANS(get_guid())

	return error_success;
}

int msg_get_headframe_list::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	std::string sql = "select `frame_id` from `user_head_frame` where `uid` = '" + pp->platform_uid_ + "'";
	auto th = boost::dynamic_pointer_cast<msg_get_headframe_list>(shared_from_this());
	db_delay_helper_.get_result(sql, [th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_headframe_list);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}

		query_result_reader q(vret[0]);
		while (q.fetch_row())
		{
			msg_user_headframe_list msg;
			msg.headframe_id_ = q.getval();
			send_protocol(th->from_sock_, msg);
		}
	});

	return error_success;
}

int msg_set_head_and_headframe::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	int head_id = 0;
	try {
		head_id = boost::lexical_cast<int>(head_ico_);
	}
	catch (const boost::bad_lexical_cast& e) {
		return error_invalid_data;
	}

	if (head_id < 0) {
		return error_invalid_data;
	}

	//非默认头像(0~5为免费头像)
	//非默认头像框(1为免费头像框)
	std::string sql = "select `head_id` from `user_head` where `uid` = '" + pp->platform_uid_ + "'" +
		" and head_id = " + lx2s(head_id) + " and expire_time > now();" +
		"select `frame_id` from `user_head_frame` where `uid` = '" + pp->platform_uid_ + "'" +
		" and frame_id = " + lx2s(headframe_id_);
	auto th = boost::dynamic_pointer_cast<msg_set_head_and_headframe>(shared_from_this());
	db_delay_helper_.get_result(sql, [pp, th, head_id](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_headframe_list);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}

		query_result_reader q1(vret[0]);
		query_result_reader q2(vret[1]);
		if ((head_id > 5 && !q1.fetch_row()) || (th->headframe_id_ != 1 && !q2.fetch_row())) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_headframe_list);
			msg.err_ = error_invalid_data;
			send_protocol(th->from_sock_, msg);
			return;
		}

		{
			Database& db = *db_;
			BEGIN_UPDATE_TABLE("user_account");
			SET_FIELD_VALUE("headpic_name", th->head_ico_);
			SET_FINAL_VALUE("headframe_id", th->headframe_id_);
			WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "'");
			EXECUTE_IMMEDIATE();
		}

		pp->head_ico_ = th->head_ico_;
		pp->headframe_id_ = th->headframe_id_;
		{
			msg_user_head_and_headframe msg;
			msg.head_ico_ = pp->head_ico_;
			msg.headframe_id_ = pp->headframe_id_;
			send_protocol(th->from_sock_, msg);
		}
	});

	return error_success;
}

int msg_buy_item_by_activity::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	auto ita = glb_activity_data.find(activity_id_);
	if (ita == glb_activity_data.end()) {
		return error_invalid_request;
	}

	__int64 now = ::time(nullptr);
	if (now < ita->second.begin_time_ || now > ita->second.end_time_) {
		return error_activity_is_invalid;
	}

	auto pinfo = ita->second;

	BEGIN_TRANS(msg_buy_item_by_activity);
	auto return_code_sender = [ptrans, pthis]() {
		msg_common_reply<none_socket> msg;
		msg.rp_cmd_ = pthis->head_.cmd_;
		msg.err_ = ptrans->contex_.return_code_;
		send_protocol(pthis->from_sock_, msg);
	};

	ptrans->contex_.send_return_code_ = return_code_sender;

	//购买头像框(没有才能购买)
	{
		auto sql_builder = [pp, pthis]()->std::string {
			return "select `rowid` from `log_buy_goods_by_activity` where `uid` = '" + pp->platform_uid_ + "'" +
				" and activity_id = " + lx2s(pthis->activity_id_);
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		CHECK_EXISTS(error_already_buy_present);
	}

	//头像框
	if (pinfo.cat_ == ITEM_TYPE_HEADFRAME) {
		if (pinfo.item_.size() == 1) {
			int frame_id = pinfo.item_[0].item_id_;
			auto sql_builder = [pp, frame_id]()->std::string {
				return "select frame_id from user_head_frame where uid = '" + pp->platform_uid_ + "' and frame_id = " + lx2s(frame_id);
			};
			ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
			CHECK_EXISTS(error_already_buy_present);
		}
		else {
			i_log_system::get_instance()->write_log(loglv_error, "buy item fail item size:%d != 1", pinfo.item_.size());
			{
				ptrans->contex_.return_code_ = error_invalid_request;
				return state_failed;
			}
		}
	}

	BEGIN_FUNCTION(pp, pinfo)

	//判断购买数量
	if (pthis->num_ < pinfo.min_buy_num_ || pthis->num_ > pinfo.max_buy_num_) {
		{
			ptrans->contex_.return_code_ = error_invalid_request;
			return state_failed;
		}
	}

	//扣款
	int ret = update_item("活动购买" + boost::locale::conv::between(pinfo.name_, "GB2312", "UTF-8"), item_operator_cost, pthis->num_, pp->platform_uid_, pinfo.price_);
	if (ret != error_success) {
		ptrans->contex_.return_code_ = ret;
		return state_failed;
	}

	//加道具
	ret = update_item("活动购买" + boost::locale::conv::between(pinfo.name_, "GB2312", "UTF-8"), item_operator_add, pthis->num_, pp->platform_uid_, pinfo.item_);
	if (ret != error_success) {
		ptrans->contex_.return_code_ = ret;
		return state_failed;
	}

	if (!pinfo.price_.empty() && !pinfo.item_.empty()) {
		Database& db(*db_);
		BEGIN_INSERT_TABLE("log_buy_goods_by_activity");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("activity_id", pthis->activity_id_);
		SET_FIELD_VALUE("buy_num", pthis->num_);
		SET_FIELD_VALUE("cost_item_id", pinfo.price_[0].item_id_);
		SET_FIELD_VALUE("cost_item_num", pthis->num_ * pinfo.price_[0].item_num_);
		SET_FIELD_VALUE("item_id", pinfo.item_[0].item_id_);
		SET_FINAL_VALUE("item_num", pthis->num_ * pinfo.item_[0].item_num_);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	{
		ptrans->contex_.return_code_ = error_business_handled;
		return state_failed;
	}

	END_CALLBACK()
	END_TRANS(get_guid())

	return error_success;
}

int msg_get_activity_list::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	for (auto ita = glb_activity_data.begin(); ita != glb_activity_data.end(); ita++)
	{
		std::string sql = "select `rowid` from `log_buy_goods_by_activity` where `uid` = '" + pp->platform_uid_ + "'" +
			" and activity_id = " + lx2s(ita->second.activity_id_);
		auto th = boost::dynamic_pointer_cast<msg_get_activity_list>(shared_from_this());
		auto info = ita->second;
		db_delay_helper_.get_result(sql, [pp, th, info](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_headframe_list);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			msg_user_activity_list msg;
			query_result_reader q(vret[0]);
			if (q.fetch_row()) {
				msg.state_ = msg_get_activity_list::STATE_INVALID;
			}
			else {
				msg.state_ = msg_get_activity_list::STATE_ACTIVATE;
			}

			msg.activity_id_ = info.activity_id_;
			send_protocol(th->from_sock_, msg);
		});
	}

	return error_success;
}

int msg_get_recharge_record::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	//充值钻石
	{
		std::string sql = "select `amount`, `status`, unix_timestamp(`recharge_date`) from tbl_recharge_record where `account_id` = '" + \
			pp->platform_uid_ + "' and unix_timestamp(now()) - unix_timestamp(`recharge_date`) < 15*24*3600 order by `recharge_date` desc limit 999";
		auto th = boost::dynamic_pointer_cast<msg_get_recharge_record>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_headframe_list);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			query_result_reader q(vret[0]);
			int count = 0;
			while (q.fetch_row())
			{
				msg_user_recharge_record msg;
				if (0 == count) {
					msg.tag_ = 0;
				}
				else {
					msg.tag_ = 1;
				}
				msg.recharge_type_ = msg_get_recharge_record::RECHARGE_DIAMOND;
				msg.recharge_num_ = q.getval();
				msg.status_ = q.getval();
				msg.time_stamp_ = q.getval();
				send_protocol(th->from_sock_, msg);
				++count;
			}
		});
	}

	//充值K豆
	{
		string reason = boost::locale::conv::between("兑换K豆", "UTF-8", "GB2312");
		std::string sql = "select `count`, unix_timestamp(`create_time`) from log_player_gold_changes where `uid` = '" + \
			pp->platform_uid_ + "' and `op` = 0 and `reason` = '" + reason + "' and unix_timestamp(now()) - unix_timestamp(`create_time`) < 15*24*3600 order by `create_time` desc limit 999";
		auto th = boost::dynamic_pointer_cast<msg_get_recharge_record>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_headframe_list);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			query_result_reader q(vret[0]);
			int count = 0;
			while (q.fetch_row())
			{
				msg_user_recharge_record msg;
				if (0 == count) {
					msg.tag_ = 0;
				}
				else {
					msg.tag_ = 1;
				}
				msg.recharge_type_ = msg_get_recharge_record::RECHARGE_GOLDS;
				msg.recharge_num_ = q.getval();
				msg.status_ = 4;
				msg.time_stamp_ = q.getval();
				send_protocol(th->from_sock_, msg);
				++count;
			}
		});
	}

	return error_success;
}

int msg_play_lucky_dial::one_lucky_dial(player_ptr pp, std::map<int, lucky_dial_info>& dial_conf, int& lucky_id_, int& serial_times_, int& lucky_value_)
{
	int is_have_record = 1;
	int lucky_value = 0;
	int serial_times = 0;
	int is_same_week = 0;
	{
		std::string sql = "select lucky_value, serial_times, yearweek(date_add(now(), interval -1 day)) = yearweek(date_add(`update_time`, interval -1 day)) from `user_serial_lucky` where `uid` = '" + lx2s(pp->platform_uid_) + "'";
		Database& db = *db_;
		Query q(db);

		q.get_result(sql);
		if (q.fetch_row()) {
			lucky_value = q.getval();
			serial_times = q.getval();
			is_same_week = q.getval();
			q.free_result();

			if (!is_same_week) {
				serial_times = 0;
			}
		}
		else {
			is_have_record = 0;
		}
	}

	//最大奖
	auto max_itor = dial_conf.begin();
	for (; max_itor != dial_conf.end(); max_itor++)
	{
		if (max_itor->second.max_lucky_value_ > 0) {
			break;
		}
	}

	if (max_itor == dial_conf.end()) {
		return error_no_record;
	}

	int ran = 0;
	auto lucky_itor = dial_conf.begin();
	do {
		//幸运值满直接中最大奖
		if (lucky_value >= max_itor->second.max_lucky_value_) {
			lucky_itor = max_itor;
			if (lucky_itor != dial_conf.end()) {
				break;
			}
		}

		ran = rand_r(0, 10000 * 10000) % 10000;
		int sum = 0;
		for (lucky_itor = dial_conf.begin(); lucky_itor != dial_conf.end(); lucky_itor++)
		{
			if (ran > sum && ran <= sum + lucky_itor->second.probability_) {
				break;
			}

			sum += lucky_itor->second.probability_;
		}

		//判断系统库存
		if (lucky_itor->second.limit_sys_num_ > 0) {
			std::string sql = "select count(rowid) from `log_lucky_dial` where `lucky_id` = '" + lx2s(lucky_itor->second.lucky_id_) + "' and date(`create_time`) = date(now())";
			Database& db = *db_;
			Query q(db);

			q.get_result(sql);
			if (!q.fetch_row()) {
				return error_sql_execute_error;
			}

			int sys_num = q.getval();
			q.free_result();
			if (sys_num >= lucky_itor->second.limit_sys_num_) {
				continue;
			}
		}

		//判断个人库存
		if (lucky_itor->second.limit_user_num_ > 0) {
			std::string sql = "select count(rowid) from `log_lucky_dial` where `lucky_id` = '" + lx2s(lucky_itor->second.lucky_id_) + "' and uid = '" +
				pp->platform_uid_ + "' and date(`create_time`) = date(now())";
			Database& db = *db_;
			Query q(db);

			q.get_result(sql);
			if (!q.fetch_row()) {
				return error_sql_execute_error;
			}

			int user_num = q.getval();
			q.free_result();
			if (user_num >= lucky_itor->second.limit_user_num_) {
				continue;
			}
		}

		//判断中奖间隔
		if (lucky_itor->second.limit_second_ > 0) {
			std::string sql = "select count(rowid) from `log_lucky_dial` where `lucky_id` = '" + lx2s(lucky_itor->second.lucky_id_) + "' and uid = '" +
				pp->platform_uid_ + "' and unix_timestamp(now()) - unix_timestamp(`create_time`) < " + lx2s(lucky_itor->second.limit_second_);
			Database& db = *db_;
			Query q(db);

			q.get_result(sql);
			if (!q.fetch_row()) {
				return error_sql_execute_error;
			}

			int limit_second_num = q.getval();
			q.free_result();
			if (limit_second_num > 0) {
				continue;
			}
		}

		if (lucky_itor != dial_conf.end()) {
			break;
		}
	} while (1);

	int item_id = lucky_itor->second.prize_id_;
	int item_num = lucky_itor->second.prize_num_;
	{
		__int64 outc = 0;
		int ret = update_user_item("幸运转盘", "localcall", pp->platform_uid_, item_operator_add, item_id, item_num, outc, all_channel_servers());

		//如果物品没有给成功,比如网络问题,数据库问题,应该要保存下来后续再试.目前先不管了.
		if (ret != error_success) {
			i_log_system::get_instance()->write_log(loglv_error, "do lucky dial update user item error:%d, lucky_id:%d", ret, lucky_itor->first);
		}
	}

	lucky_id_ = lucky_itor->first;

	{
		Database& db = *db_;
		BEGIN_INSERT_TABLE("log_lucky_dial");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("lucky_id", lucky_itor->first);
		SET_FIELD_VALUE("rand", ran);
		SET_FIELD_VALUE("prize_id", item_id);
		SET_FINAL_VALUE("prize_num", item_num);
		EXECUTE_IMMEDIATE();
	}

	serial_times++;
	if (lucky_value >= max_itor->second.max_lucky_value_) {
		lucky_value = 0;
	}
	else {
		lucky_value++;
	}

	//更新连续次数
	if (is_have_record) {
		Database& db = *db_;
		BEGIN_UPDATE_TABLE("user_serial_lucky");
		SET_FIELD_VALUE("serial_times", serial_times);
		SET_FIELD_VALUE("lucky_value", lucky_value);
		SET_FINAL_VALUE_NO_COMMA("update_time", "now()");
		WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "'");
		EXECUTE_IMMEDIATE();
	}
	else {
		Database& db = *db_;
		BEGIN_INSERT_TABLE("user_serial_lucky");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("serial_times", serial_times);
		SET_FINAL_VALUE("lucky_value", lucky_value);
		EXECUTE_IMMEDIATE();
	}

	//是否触发礼物
	{
		if (glb_system_prize.find("serial_lucky_dial") == glb_system_prize.end()) {
			return error_invalid_data;
		}

		if (glb_system_prize["serial_lucky_dial"].find(serial_times) != glb_system_prize["serial_lucky_dial"].end()) {
			int pid = glb_system_prize["serial_lucky_dial"][serial_times].pid_;

			//新增礼物(失效时间为下周一00:00:00)
			{
				Database& db = *db_;
				BEGIN_INSERT_TABLE("user_present_bag");
				SET_FIELD_VALUE("uid", pp->platform_uid_);
				SET_FIELD_VALUE("pid", pid);
				SET_FIELD_VALUE("state", 1);
				SET_FINAL_VALUE_NO_COMMA("expire_time", "date_add(date(now()), interval (7 - weekday(date(now()))) day)");
				WITH_END_CONDITION("on duplicate key update state = 1, expire_time = date_add(date(now()), interval (7 - weekday(date(now()))) day)");
				EXECUTE_IMMEDIATE();
			}

			boost::shared_ptr<msg_common_present_state_change> psend(new msg_common_present_state_change());
			psend->present_id_ = pid;
			psend->state_ = 1;
			glb_thread_send_cli_msg_lst_.push_back(std::make_pair(from_sock_, psend));
		}
	}

	serial_times_ = serial_times;
	lucky_value_ = lucky_value;
	return error_success;
}

int msg_play_lucky_dial::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	if (!((times_ == 1 && (item_id_ == item_id_lucky_ticket || item_id_ == item_id_gold_game)) || (times_ == 5 && item_id_ == item_id_gold_game))) {
		return error_invalid_request;
	}

	auto dial_conf = glb_lucky_dial_data;
	if (dial_conf.size() == 0) {
		return error_no_record;
	}

	//扣除手续费
	int cost_num = 0;
	{
		if (times_ == 1 && item_id_ == item_id_lucky_ticket) {
			cost_num = 1;
		}
		else if (times_ == 1 && item_id_ == item_id_gold_game) {
			cost_num = 10000;
		}
		else if (times_ == 5 && item_id_ == item_id_gold_game) {
			cost_num = 45000;
		}
		else {
			return error_invalid_request;
		}

		__int64 outc = 0;
		int ret = update_user_item("幸运转盘", "localcall", pp->platform_uid_, item_operator_cost, item_id_, cost_num, outc, all_channel_servers());
		if (ret != error_success) {
			return error_not_enough_item;
		}
	}

	std::vector<int> vid;
	int index = 0;
	int ret = error_success;
	int serial_times = 0, lucky_value = 0;
	for (; index < times_; index++)
	{
		int lucky_id = -1;
		ret = one_lucky_dial(pp, dial_conf, lucky_id, serial_times, lucky_value);
		if (ret != error_success) {
			if (lucky_id == -1) {
				index--;
			}
			break;
		}

		vid.push_back(lucky_id);
	}

	//没有转完则退费
	if (ret != error_success) {
		//扣奖励
		for (int i = index; i > 0; i--)
		{
			int item_id = dial_conf[vid[i]].prize_id_;
			int item_num = dial_conf[vid[i]].prize_num_;

			__int64 outc = 0;
			int ret = update_user_item("幸运转盘", "localcall", pp->platform_uid_, item_operator_cost, item_id, item_num, outc, all_channel_servers());
			if (ret != error_success) {
				return error_not_enough_item;
			}
		}

		//返手续费
		__int64 outc = 0;
		int ret = update_user_item("幸运转盘", "localcall", pp->platform_uid_, item_operator_add, item_id_, cost_num, outc, all_channel_servers());
		if (ret != error_success) {
			return error_not_enough_item;
		}

		return ret;
	}

	{
		boost::shared_ptr<msg_lucky_dial_prize> psend(new msg_lucky_dial_prize());
		psend->lucky_id_ = convert_vector_to_string(vid, 1, "|", ",");
		glb_thread_send_cli_msg_lst_.push_back(std::make_pair(from_sock_, psend));
	}

	{
		__int64 reset_timestamp = 0;
		Database& db2 = *db_;
		Query q2(db2);
		std::string sql2 = "select unix_timestamp(date_add(date(now()), interval (7 - weekday(date(now()))) day))";
		q2.get_result(sql2);
		if (q2.fetch_row()) {
			reset_timestamp = q2.getbigint();
			q2.free_result();
		}

		boost::shared_ptr<msg_user_serial_lucky_state> psend(new msg_user_serial_lucky_state());
		psend->serial_times_ = serial_times;
		psend->luck_value_ = lucky_value;
		psend->reset_timestamp_ = reset_timestamp;
		glb_thread_send_cli_msg_lst_.push_back(std::make_pair(from_sock_, psend));
	}

	return error_success;
}

int msg_get_serial_lucky_state::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}

	std::string sql = "select `serial_times`, `lucky_value`, yearweek(date_add(now(), interval -1 day)) = yearweek(date_add(`update_time`, interval -1 day)), unix_timestamp(date_add(date(now()), interval (7 - weekday(date(now()))) day)) from `user_serial_lucky` where `uid` = '" + pp->platform_uid_ + "'";
	auto th = boost::dynamic_pointer_cast<msg_get_serial_lucky_state>(shared_from_this());
	db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_serial_lucky_state);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}

		query_result_reader q(vret[0]);
		if (q.fetch_row()) {
			int serial_times = 0;
			int lucky_value = 0;
			int is_same_week = 0;
			int reset_timestamp = 0;

			serial_times = q.getval();
			lucky_value = q.getval();
			is_same_week = q.getval();
			reset_timestamp = q.getbigint();

			if (!is_same_week) {
				serial_times = 0;
			}

			msg_user_serial_lucky_state msg;
			msg.serial_times_ = serial_times;
			msg.luck_value_ = lucky_value;
			msg.reset_timestamp_ = reset_timestamp;
			send_protocol(th->from_sock_, msg);
		}
		else {
			std::string sql = "select unix_timestamp(date_add(date(now()), interval (7 - weekday(date(now()))) day))";
			db_delay_helper_.get_result(sql, [th](bool result, const std::vector<result_set_ptr>& vret) {
				if (!result || vret.empty()) {
					msg_common_reply<none_socket> msg;
					msg.rp_cmd_ = GET_CLSID(msg_get_serial_lucky_state);
					msg.err_ = error_sql_execute_error;
					send_protocol(th->from_sock_, msg);
					return;
				}

				int serial_times = 0;
				int lucky_value = 0;
				int is_same_week = 0;
				int reset_timestamp = 0;
				query_result_reader q(vret[0]);

				if (q.fetch_row()) {
					reset_timestamp = q.getbigint();
				}

				msg_user_serial_lucky_state msg;
				msg.serial_times_ = serial_times;
				msg.luck_value_ = lucky_value;
				msg.reset_timestamp_ = reset_timestamp;
				send_protocol(th->from_sock_, msg);
			});
		}
	});

	return error_success;
}

string&   replace_mail_special_string(string& str, player_ptr pp)
{
	std::string nickname = "$(nickname)";
	for (string::size_type pos(0); pos != string::npos; pos += pp->nickname_.length()) {
		if ((pos = str.find(nickname, pos)) != string::npos)
			str.replace(pos, nickname.length(), pp->nickname_);
		else   break;
	}

	return   str;
}

int msg_get_mail_detail::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}
	
	{
		std::string sql = "select u.uid, u.content, u.hyperlink, if(u.uid = 'system', ifnull(s.state, 0), u.state), if(u.uid = 'system', ifnull(s.attach_state, u.attach_state), u.attach_state), u.attach1_id, u.attach1_num, u.attach2_id, u.attach2_num, u.attach3_id, u.attach3_num, u.attach4_id, u.attach4_num, u.attach5_id, u.attach5_num, u.attach6_id, u.attach6_num from (select * from user_mail where id = " + lx2s(id_) + ") u left join user_mail_slave s on (s.id = u.id and s.uid = '" + pp->platform_uid_ + "')";
		auto th = boost::dynamic_pointer_cast<msg_get_mail_detail>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_mail_detail);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			query_result_reader q(vret[0]);
			if (!q.fetch_row()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_mail_detail);
				msg.err_ = error_no_record;
				send_protocol(th->from_sock_, msg);
				return;
			}

			int state = -1;
			int attach_state = -1;
			std::string uid;
			{
				uid = q.getstr();
				if (uid != pp->platform_uid_ && uid != "system") {
					msg_common_reply<none_socket> msg;
					msg.rp_cmd_ = GET_CLSID(msg_get_mail_detail);
					msg.err_ = error_invalid_request;
					send_protocol(th->from_sock_, msg);
					return;
				}

				msg_user_mail_detail msg;
				msg.id_ = th->id_;
				std::string content = q.getstr();
				msg.content_ = replace_mail_special_string(content, pp);
				msg.hyperlink_ = q.getstr();
				state = q.getval();
				if (state == 2) {
					msg_common_reply<none_socket> msg;
					msg.rp_cmd_ = GET_CLSID(msg_get_mail_detail);
					msg.err_ = error_no_record;
					send_protocol(th->from_sock_, msg);
					return;
				}

				attach_state = q.getval();
				int attach[6][2] = { { 0,0 } };

				std::vector<int> vprize;
				for (int i = 0; i < 6; i++)
				{
					int item_id = q.getval();
					int item_num = q.getval();

					if (item_id >= 0 && item_num > 0) {
						vprize.push_back(item_id);
						vprize.push_back(item_num);
					}
				}

				msg.attach_ = convert_vector_to_string(vprize, 2, "|", ",");
				send_protocol(th->from_sock_, msg);

				//设置为已读
				if (state == 0) {
					if (uid != "system") {
						Database& db = *db_;
						BEGIN_UPDATE_TABLE("user_mail");
						SET_FINAL_VALUE("state", 1);
						WITH_END_CONDITION("where id = '" + lx2s(th->id_) + "'");
						EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
					}
					else {
						Database& db = *db_;
						BEGIN_INSERT_TABLE("user_mail_slave");
						SET_FIELD_VALUE("id", th->id_);
						SET_FIELD_VALUE("uid", pp->platform_uid_);
						SET_FIELD_VALUE("state", 1);
						SET_FIELD_VALUE("attach_state", attach_state);
						SET_FINAL_VALUE_NO_COMMA("create_time", "now()");
						WITH_END_CONDITION("on duplicate key update state = 1, create_time = create_time");
						EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
					}

					{
						msg_user_mail_op msg2;
						msg2.op_type_ = msg_user_mail_op::MAIL_OP_SET_READ;
						msg2.id_ = th->id_;
						send_protocol(th->from_sock_, msg2);
					}
				}
			}
		});
	}

	return error_success;
}

int msg_del_mail::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}

	{
		std::string sql = "select uid, mail_type, state, attach_state from user_mail where id = " + lx2s(id_);
		auto th = boost::dynamic_pointer_cast<msg_del_mail>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_del_mail);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			query_result_reader q(vret[0]);
			std::string uid;
			int mail_type = 0, state = 0, attach_state = 0;
			if (!q.fetch_row()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_del_mail);
				msg.err_ = error_no_record;
				send_protocol(th->from_sock_, msg);
				return;
			}

			uid = q.getstr();
			mail_type = q.getval();
			state = q.getval();
			attach_state = q.getval();

			if (uid != pp->platform_uid_ && uid != "system") {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_del_mail);
				msg.err_ = error_invalid_request;
				send_protocol(th->from_sock_, msg);
				return;
			}

			if (uid != "system") {
				if (state != 1) {
					msg_common_reply<none_socket> msg;
					msg.rp_cmd_ = GET_CLSID(msg_del_mail);
					msg.err_ = error_invalid_request;
					send_protocol(th->from_sock_, msg);
					return;
				}

				if (attach_state == 1) {
					msg_common_reply<none_socket> msg;
					msg.rp_cmd_ = GET_CLSID(msg_del_mail);
					msg.err_ = error_cannt_del_attach_mail;
					send_protocol(th->from_sock_, msg);
					return;
				}

				{
					Database& db = *db_;
					BEGIN_DELETE_TABLE("user_mail");
					WITH_END_CONDITION("where id = " + lx2s(th->id_));
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}

				{
					msg_user_mail_op msg;
					msg.op_type_ = msg_user_mail_op::MAIL_OP_DEL;
					msg.id_ = th->id_;
					send_protocol(th->from_sock_, msg);
				}
			}
			else {
				std::string sql = "select state, attach_state from user_mail_slave where id = " + lx2s(th->id_) + " and uid = '" + pp->platform_uid_ + "'";
				db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
					if (!result || vret.empty()) {
						msg_common_reply<none_socket> msg;
						msg.rp_cmd_ = GET_CLSID(msg_del_mail);
						msg.err_ = error_sql_execute_error;
						send_protocol(th->from_sock_, msg);
						return;
					}

					query_result_reader q(vret[0]);
					if (!q.fetch_row()) {
						msg_common_reply<none_socket> msg;
						msg.rp_cmd_ = GET_CLSID(msg_del_mail);
						msg.err_ = error_no_record;
						send_protocol(th->from_sock_, msg);
						return;
					}

					int sys_state = q.getval();
					int sys_attach_state = q.getval();

					if (sys_state != 1) {
						msg_common_reply<none_socket> msg;
						msg.rp_cmd_ = GET_CLSID(msg_del_mail);
						msg.err_ = error_invalid_request;
						send_protocol(th->from_sock_, msg);
						return;
					}

					if (sys_attach_state == 1) {
						msg_common_reply<none_socket> msg;
						msg.rp_cmd_ = GET_CLSID(msg_del_mail);
						msg.err_ = error_cannt_del_attach_mail;
						send_protocol(th->from_sock_, msg);
						return;
					}

					{
						Database& db = *db_;
						BEGIN_UPDATE_TABLE("user_mail_slave");
						SET_FINAL_VALUE("state", 2);
						WITH_END_CONDITION("where id = " + lx2s(th->id_) + " and uid = '" + pp->platform_uid_ + "'");
						EXECUTE_IMMEDIATE();
					}

					{
						msg_user_mail_op msg;
						msg.op_type_ = msg_user_mail_op::MAIL_OP_DEL;
						msg.id_ = th->id_;
						send_protocol(th->from_sock_, msg);
					}
				});
			}
		});
	}

	return error_success;
}

int msg_del_all_mail::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}

	BEGIN_TRANS(msg_del_all_mail);
	auto return_code_sender = [ptrans, pthis]() {
		msg_common_reply<none_socket> msg;
		msg.rp_cmd_ = pthis->head_.cmd_;
		msg.err_ = ptrans->contex_.return_code_;
		send_protocol(pthis->from_sock_, msg);
	};

	ptrans->contex_.send_return_code_ = return_code_sender;

	//删除个人邮件
	{
		auto sql_builder = [pp, pthis]()->std::string {
			return "select id from user_mail where mail_type = " + lx2s(pthis->mail_type_) + " and uid = '" + pp->platform_uid_ + "' and state = 1 and attach_state != 1";
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		BEGIN_QUERY_CALLBACK(0, placeholder);
		std::vector<int> v;
		READ_ALL_ROW()
			int id = q.getval();
			v.push_back(id);
		END_READ_ALL_ROW()

		if (v.size() > 0) {
			std::string ids = combin_str(v, ",", false);
			{
				BEGIN_DELETE_TABLE("user_mail");
				WITH_END_CONDITION("where id in(" + ids + ")");
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}

			for (int i = 0; i < v.size(); i++)
			{
				msg_user_mail_op msg;
				msg.op_type_ = msg_user_mail_op::MAIL_OP_DEL;
				msg.id_ = v[i];
				send_protocol(pthis->from_sock_, msg);
			}

			v.clear();
		}
		
		END_CALLBACK();
	}

	//删除系统邮件
	{
		auto sql_builder = [pp, pthis]()->std::string {
			return "select u.id from (select * from user_mail where uid = 'system' and mail_type = " + lx2s(pthis->mail_type_) + ") u left join user_mail_slave s on(s.id = u.id and s.uid = '" + pp->platform_uid_ + "') where s.state = 1 and s.attach_state != 1";
		};
		ADD_QUERY_TRANS(sql_builder, db_delay_helper_);
		BEGIN_QUERY_CALLBACK(0, placeholder, pp);
		std::vector<int> v;
		READ_ALL_ROW()
			int id = q.getval();
		v.push_back(id);
		END_READ_ALL_ROW()

			if (v.size() > 0) {
				std::string ids = combin_str(v, ",", false);
				{
					BEGIN_UPDATE_TABLE("user_mail_slave");
					SET_FINAL_VALUE("state", 2);
					WITH_END_CONDITION("where id in (" + ids + ") and uid = '" + pp->platform_uid_ + "' and state = 1 and attach_state != 1");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}

				for (int i = 0; i < v.size(); i++)
				{
					msg_user_mail_op msg;
					msg.op_type_ = msg_user_mail_op::MAIL_OP_DEL;
					msg.id_ = v[i];
					send_protocol(pthis->from_sock_, msg);
				}
			}

		END_CALLBACK();
	}
	END_TRANS(get_guid())

	return error_success;
}

int msg_get_mail_attach::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}
	
	{
		std::string sql = "select u.uid, u.mail_type, if(u.uid = 'system', ifnull(s.attach_state, u.attach_state), u.attach_state), u.attach1_id, u.attach1_num, u.attach2_id, u.attach2_num, u.attach3_id, u.attach3_num, u.attach4_id, u.attach4_num, u.attach5_id, u.attach5_num, u.attach6_id, u.attach6_num from (select * from user_mail where id = " + lx2s(id_) + ") u left join user_mail_slave s on (s.id = u.id and s.uid = '" + pp->platform_uid_ + "')";
		auto th = boost::dynamic_pointer_cast<msg_get_mail_attach>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_mail_attach);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			query_result_reader q(vret[0]);
			if (!q.fetch_row()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_mail_attach);
				msg.err_ = error_no_record;
				send_protocol(th->from_sock_, msg);
				return;
			}

			std::string uid;
			int mail_type = 0;
			std::vector<int> vprize;
			{
				uid = q.getstr();
				mail_type = q.getval();
				int attach_state = q.getval();
				for (int i = 0; i < 6; i++) {
					int item_id = q.getval();
					int item_num = q.getval();

					if (item_id >= 0 && item_num > 0) {
						vprize.push_back(item_id);
						vprize.push_back(item_num);
					}
				}

				if (uid != "system" && (uid != pp->platform_uid_ || attach_state != 1)) {
					msg_common_reply<none_socket> msg;
					msg.rp_cmd_ = GET_CLSID(msg_get_mail_attach);
					msg.err_ = error_invalid_request;
					send_protocol(th->from_sock_, msg);
					return;
				}
			}

			std::string reason;
			if (mail_type == 0) {
				reason = "系统邮件";
			}
			else if (mail_type == 1) {
				reason = "礼物邮件";
			}

			for (int i = 0; i < vprize.size(); i += 2)
			{
				//普通物品
				if (vprize[i] >= 0 && vprize[i + 1] > 0) {
					int ret = 0;
					__int64 item_num = 1;
					if (vprize[i] <= item_id_gold_game_bank) {
						__int64 outc = 0;
						item_num = vprize[i + 1];
						ret = update_user_item(reason, "localcall", uid, item_operator_add, vprize[i], item_num, outc, all_channel_servers());
					}
					else {
						auto itp_item = glb_item_data.find(vprize[i]);
						if (itp_item != glb_item_data.end()) {
							if (itp_item->second.type_ == 0) {
								item_num = vprize[i + 1];
							}

							__int64 outc = 0;
							ret = update_user_item(reason, "localcall", uid, item_operator_add, vprize[i], item_num, outc, all_channel_servers());

							//时长物品加时长
							if (itp_item->second.type_ == 1) {
								__int64 sec = vprize[i + 1];
								std::string set_expire = "if(expire_time > now(), from_unixtime(unix_timestamp(expire_time) + " + lx2s(sec) + "), from_unixtime(unix_timestamp(now()) + " + lx2s(sec) + "))";

								Database& db(*db_);
								BEGIN_UPDATE_TABLE("user_item");
								SET_FIELD_VALUE("count", 1);
								SET_FINAL_VALUE_NO_COMMA("expire_time", set_expire);
								WITH_END_CONDITION("where uid = '" + uid + "' and item_id = " + lx2s(vprize[i]));
								EXECUTE_IMMEDIATE();

								extern void	send_items_data_to_player(std::string uid, boost::shared_ptr<koko_socket> psock, int item_id = -1);
								auto pconn = pp->from_socket_.lock();
								if (pconn) {
									send_items_data_to_player(pp->platform_uid_, pconn, vprize[i]);
								}
							}
						}
					}

					//如果物品没有给成功,比如网络问题,数据库问题,应该要保存下来后续再试.目前先不管了.
					if (ret == error_success) {
						Database& db = *db_;
						std::string table_name;
						if (vprize[i] < 100) {
							table_name = "user_curre";
						}
						else {
							table_name = "user_props";
						}

						BEGIN_INSERT_TABLE(table_name);
						SET_FIELD_VALUE("uid", pp->platform_uid_);
						SET_FIELD_VALUE("STATUS", 8);
						SET_FIELD_VALUE("item_id", vprize[i]);
						SET_FINAL_VALUE("count", vprize[i + 1]);
						EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
					}
					else {
						i_log_system::get_instance()->write_log(loglv_error, "get mail attach update user item error:%d, mail_id:%d", ret, th->id_);
					}
				}
			}

			//删除邮件
			if (uid == "system") {
				Database& db = *db_;
				BEGIN_INSERT_TABLE("user_mail_slave");
				SET_FIELD_VALUE("id", th->id_);
				SET_FIELD_VALUE("uid", pp->platform_uid_);
				SET_FIELD_VALUE("state", 2);
				SET_FINAL_VALUE("attach_state", 2);
				WITH_END_CONDITION("on duplicate key update state = 2, attach_state = 2");
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
			else {
				Database& db = *db_;
				BEGIN_DELETE_TABLE("user_mail");
				WITH_END_CONDITION("where id = " + lx2s(th->id_));
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}

			{
				msg_common_recv_present msg;
				msg.type_ = "Mail|" + lx2s(th->id_);
				msg.item_ = convert_vector_to_string(vprize, 2, "|", ",");
				send_protocol(th->from_sock_, msg);
			}

			{
				msg_user_mail_op msg;
				msg.op_type_ = msg_user_mail_op::MAIL_OP_DEL;
				msg.id_ = th->id_;
				send_protocol(th->from_sock_, msg);
			}

			{
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_get_mail_attach);
				msg.err_ = error_business_handled;
				send_protocol(th->from_sock_, msg);
			}
		});
	}

	return error_success;
}

int msg_client_get_data::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}

	std::string sql = "select value from user_client_data where `uid` = '" + pp->platform_uid_ + "' and `key` = '" + key_ + "'";
	auto th = boost::dynamic_pointer_cast<msg_client_get_data>(shared_from_this());
	db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_client_get_data);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}

		std::string value;
		query_result_reader q(vret[0]);
		if (q.fetch_row()) {
			value = q.getstr();
		}

		msg_client_data msg;
		msg.key_ = th->key_;
		msg.value_ = value;
		send_protocol(th->from_sock_, msg);
	});

	return error_success;
}

int msg_client_set_data::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}

	char key[256] = { 0 };
	char value[256] = { 0 };
	int len = mysql_real_escape_string(&db_->grabdb()->mysql, key, key_.c_str(), strlen(key_.c_str()));
	len = mysql_real_escape_string(&db_->grabdb()->mysql, value, value_.c_str(), strlen(value_.c_str()));

	{
		Database& db = *db_;
		BEGIN_INSERT_TABLE("user_client_data");
		SET_FIELD_VALUE("uid", pp->platform_uid_);
		SET_FIELD_VALUE("key", key);
		SET_FINAL_VALUE("value", value);
		WITH_END_CONDITION("on duplicate key update value = '" + value_ + "'");
		EXECUTE_IMMEDIATE();
	}

	return error_business_handled;
}

int msg_common_get_present::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp) {
		return error_cant_find_player;
	}

	if (glb_present_bag.find(present_id_) == glb_present_bag.end()) {
		return error_invalid_request;
	}

	//是否有该礼包
	{
		std::string sql = "select pid from user_present_bag where uid = '" + pp->platform_uid_ + "' and pid = " + lx2s(present_id_) + " and state = 1 and expire_time > now()";
		auto th = boost::dynamic_pointer_cast<msg_common_get_present>(shared_from_this());
		db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_common_get_present);
				msg.err_ = error_sql_execute_error;
				send_protocol(th->from_sock_, msg);
				return;
			}

			query_result_reader q(vret[0]);
			if (!q.fetch_row()) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_common_get_present);
				msg.err_ = error_no_record;
				send_protocol(th->from_sock_, msg);
				return;
			}

			auto vpresent = glb_present_bag[th->present_id_];
			int ret = update_item("每日签到奖励", item_operator_add, 1, pp->platform_uid_, vpresent);
			if (ret != error_success) {
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_common_get_present);
				msg.err_ = ret;
				send_protocol(th->from_sock_, msg);
				return;
			}

			std::vector<int> vprize;
			for (int i = 0; i < vpresent.size(); i++)
			{
				vprize.push_back(vpresent[i].item_id_);
				vprize.push_back(vpresent[i].item_num_);
			}

			//更新礼包状态
			{
				Database& db = *db_;
				BEGIN_UPDATE_TABLE("user_present_bag");
				SET_FIELD_VALUE("state", 2);
				SET_FINAL_VALUE_NO_COMMA("expire_time", "expire_time");
				WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "' and pid = " + lx2s(th->present_id_));
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}

			{
				msg_common_recv_present msg;
				msg.type_ = "Present|" + lx2s(th->present_id_);
				msg.item_ = convert_vector_to_string(vprize, 2, "|", ",");
				send_protocol(th->from_sock_, msg);
			}

			{
				msg_common_present_state_change msg;
				msg.present_id_ = th->present_id_;
				msg.state_ = 2;
				send_protocol(th->from_sock_, msg);
			}

			{
				msg_common_reply<none_socket> msg;
				msg.rp_cmd_ = GET_CLSID(msg_common_get_present);
				msg.err_ = error_business_handled;
				send_protocol(th->from_sock_, msg);
				return;
			}
		});
	}

	return error_success;
}

int msg_send_mail_to_player::handle_this()
{
	user_mail mail;
	mail.uid_ = uid_;
	mail.mail_type_ = mail_type_;
	mail.title_type_ = title_type_;
	mail.title_ = title_;
	mail.content_ = content_;
	mail.state_ = 0;
	mail.attach_state_ = attach_state_;
	mail.attach_ = attach_;
	mail.save_days_ = save_days_;

	send_mail(mail);
	return error_success;
}

int msg_get_rank_data::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	std::string check_str;
	//今日榜
	if (flag_ == 0) {
		check_str = "today";
	}
	else {
		check_str = "total";
	}

	std::string sql = "select `uid`, `nickname`, `head_ico`, `" + check_str + "` from rank where `type` = " + lx2s(type_) + " order by `" + check_str + "` desc limit 100";
	auto th = boost::dynamic_pointer_cast<msg_get_rank_data>(shared_from_this());
	db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}

		query_result_reader q(vret[0]);
		int rank = 1;
		while (q.fetch_row())
		{
			msg_user_rank_data msg;
			msg.rank_ = rank;
			msg.uid_ = q.getstr();
			msg.nickname_ = q.getstr();
			msg.head_ico_ = q.getstr();
			msg.type_ = th->type_;
			msg.flag_ = th->flag_;
			msg.data_ = q.getbigint();

			send_protocol(th->from_sock_, msg);
			rank++;
		}

		{
			msg_sequence_send_over<none_socket> msg;
			msg.cmd_ = GET_CLSID(msg_user_rank_data);
			send_protocol(th->from_sock_, msg);
		}
	});
	
	return error_success;
}

int msg_get_head_list::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) {
		return error_cant_find_player;
	}

	std::string sql = "select `head_id`, unix_timestamp(`expire_time`) - unix_timestamp(now()) from `user_head` where `uid` = '" + pp->platform_uid_ + "' and expire_time > now()";
	auto th = boost::dynamic_pointer_cast<msg_get_head_list>(shared_from_this());
	db_delay_helper_.get_result(sql, [pp, th](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = GET_CLSID(msg_get_rank_data);
			msg.err_ = error_sql_execute_error;
			send_protocol(th->from_sock_, msg);
			return;
		}

		query_result_reader q(vret[0]);
		while (q.fetch_row())
		{
			msg_user_head_list msg;
			msg.head_ico_ = lx2s(q.getval());
			msg.remain_sec_ = q.getbigint();
			send_protocol(th->from_sock_, msg);
		}
	});
	
	return error_success;
}

int msg_request_client_item::handle_this()
{
	player_ptr pp = get_player(uid_);

	msg_request_client_item_ret msg;
	msg.uid_ = uid_;
	msg.item_id_ = item_id_;

	if (pp) {
		if (item_id_ == item_id_head) {
			msg.item_data_ = pp->head_ico_;
		}
		else if (item_id_ == item_id_headframe) {
			msg.item_data_ = lx2s(pp->headframe_id_);
		}
	}
	else {
		if (item_id_ == item_id_head) {
			std::string sql = "select headpic_name from `user_account` where uid = '" + uid_ + "'";
			Query q(*db_);
			if (q.get_result(sql)) {
				int rank = 1;
				if (q.fetch_row()) {
					std::string head_id = q.getstr();
					q.free_result();

					try {
						msg.item_data_ = boost::lexical_cast<std::string>(head_id);
					}
					catch (const boost::bad_lexical_cast& e) {
						msg.item_data_ = lx2s(0);
					}
				}
			}
		}
		else if (item_id_ == item_id_headframe) {
			std::string sql = "select headframe_id from `user_account` where uid = '" + uid_ + "'";
			Query q(*db_);
			if (q.get_result(sql)) {
				int rank = 1;
				if (q.fetch_row()) {
					int headframe_id = q.getval();
					q.free_result();

					msg.item_data_ = lx2s(headframe_id);
				}
			}
		}
	}
	
	send_protocol(from_sock_, msg);
	return error_success;
}