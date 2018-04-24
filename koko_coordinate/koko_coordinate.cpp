#include "boost/asio.hpp"
#include "boost/date_time.hpp"
#include "boost/atomic.hpp"
#include "boost/regex.hpp"
#include <unordered_map>
#include "memcache_defines.h"
#include "schedule_task.h"
#include <set>
#include <math.h>
#include "net_service.h"
#include "utf8_db.h"
#include "Query.h"
#include "db_delay_helper.h"
#include "utility.h"
#include "error.h"
#include "md5.h"
#include "player.h"
#include "msg_server.h"
#include "game_channel.h"
#include "msg_client.h"
#include "log.h"
#include "server_config.h"
#include "simple_xml_parse.cpp"
#include "json_helper.cpp"
#include <DbgHelp.h>
#include "json_helper.hpp"
#include "boost/atomic.hpp"
#include "db_delay_helper.cpp"
#include "i_log_system.cpp"
#include "i_transaction.cpp"
#include "transactions.cpp"
#include "transaction_controller.cpp"
#include "function_time_counter.h"
#include "net_socket_basic.cpp"
#include "server_config.h"

#pragma auto_inline (off)
#pragma comment(lib, "dbghelp.lib")
log_file<cout_and_file_output> glb_http_log;
boost::shared_ptr<utf8_data_base>	db_;
db_delay_helper	db_delay_helper_;
boost::shared_ptr<boost::asio::io_service> timer_srv;
extern boost::thread_specific_ptr<std::vector<channel_server>> channel_servers;
std::unordered_map<unsigned int, task_wptr> task_info::glb_running_task_;
boost::shared_ptr<net_server<koko_socket>> the_net;
boost::shared_ptr<net_server<game_srv_socket>> the_internal_net;
std::map<int, present_info>		glb_present_data;
std::map<int, item_info>		glb_item_data;
std::map<int, activity_info>	glb_activity_data;
std::map<int, lucky_dial_info>	glb_lucky_dial_data;
std::map<std::string, std::map<int, system_prize>> glb_system_prize;
std::map<int, std::vector<item_unit>> glb_present_bag;

boost::atomic_uint	msg_recved(0);
boost::atomic_uint	msg_handled(0);
boost::atomic_uint	unsended_data(0);

typedef boost::shared_ptr<msg_base<koko_socket_ptr>> msg_ptr;		//从客户端发过来的消息类型
typedef boost::shared_ptr<msg_base<none_socket>> smsg_ptr;					//发送给客户端的消息类型
typedef boost::shared_ptr<msg_base<srv_socket_ptr>> vmsg_ptr;		//从游戏服务器发过来的消息类型

//std::list<msg_ptr>	glb_msg_list_;
std::map<boost::shared_ptr<koko_socket>, std::list<msg_ptr>> glb_msg_list_;
std::list<vmsg_ptr>	glb_vmsg_list_;

std::map<int, channel_ptr>		koko_channels;
//游戏服务器连接 <游戏id,服务器socket>
std::map<int, std::map<std::string, internal_server_inf>> registed_servers;

std::unordered_map<std::string, player_ptr> online_players;
//机器人
std::list<player_ptr> free_bots_;
std::vector<std::string> auto_chats;
std::vector<std::string> auto_chats_emotion;
safe_list<player_ptr> pending_login_users_;

safe_list<msg_ptr>		sub_thread_process_msg_lst_;
safe_list<vmsg_ptr>		sub_thread_process_vmsg_lst_;

safe_list<smsg_ptr>		broadcast_msg_;

std::unordered_map<std::string, boost::shared_ptr<trade_inout_orders>> glb_trades;

service_config the_config;
typedef boost::shared_ptr<host_info> hosti_ptr;
typedef boost::shared_ptr<game_info> gamei_ptr;
std::map<int, gamei_ptr> all_games;
task_ptr task;

boost::atomic_int	performance_counter[12];
boost::shared_ptr<boost::thread> monitor_thread, dbthread_;

extern int	sync_user_item(std::string uid, int itemid);
extern int	update_user_item(std::string reason,
	std::string sn,
	std::string uid, int op, 
	int itemid, __int64 count, __int64& ret, 
	std::vector<std::string> sync_to_server);
extern int	do_multi_costs(std::string reason, std::string uid, std::map<int, int> cost, std::vector<std::string> sync);
extern std::string			md5_hash(std::string sign_pat);
extern std::vector<std::string>	all_channel_servers();
void		broadcast_msg(smsg_ptr msg);
extern void	load_vip_privilege();
extern int		run_dbthread();
extern LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo);
extern std::map<int, user_privilege>	glb_vip_privilege;

safe_list<std::pair<koko_socket_ptr, smsg_ptr>> glb_thread_send_cli_msg_lst_;
safe_list<std::pair<srv_socket_ptr, smsg_ptr>> glb_thread_send_srv_msg_lst_;

#define HANDLER_MSG(msg_cls, msg)\
	case GET_CLSID(msg_cls):\
		{\
		ret = handle_##msg_cls((msg_cls*)msg.get());\
		}\
		break

db_delay_helper& get_delaydb()
{
	return db_delay_helper_;
}

player_ptr get_player(std::string uid)
{
	auto itf = online_players.find(uid);
	if (itf != online_players.end()){
		return itf->second;
	}

	itf = online_players.find("PC" + uid);
	if (itf != online_players.end()) {
		return itf->second;
	}
	
	return player_ptr();
}

std::vector<player_ptr> get_all_players(std::string uid)
{
	std::vector<player_ptr> pps;
	auto itf = online_players.find(uid);
	if (itf != online_players.end()) {
		pps.push_back(itf->second);
	}

	itf = online_players.find("PC" + uid);
	if (itf != online_players.end()) {
		pps.push_back(itf->second);
	}

	return pps;
}

void send_msg_to_uid(std::string uid, msg_base<none_socket>& msg)
{
	auto pps = get_all_players(uid);
	for (unsigned i = 0; i < pps.size(); ++i)
	{
		auto pp = pps[i];
		if (pp.get()) {
			auto pcnn = pp->from_socket_.lock();
			if (pcnn.get()) {
				send_protocol(pcnn, msg);
			}
		}
	}
}

player_ptr get_player_by_iid(__int64 iid)
{
	auto it = std::_Find_pr(online_players.begin(),
		online_players.end(),
		iid, [](std::pair<std::string, player_ptr> itp, __int64 iid)->bool{
			return itp.second->iid_ == iid;
	});
	if (it == online_players.end()){
		return player_ptr();
	}
	else{
		return it->second;
	}
}

void sql_trim(std::string& str)
{
	for (unsigned int i = 0; i < str.size(); i++)
	{
		if (str[i] == '\''){
			str[i] = ' ';
		}
	}
}

std::string convert_vector_to_string(std::vector<int> vec, int ele_num, std::string big_sep = "", std::string small_sep = "")
{
	std::string result;
	if (vec.size() % ele_num != 0) {
		return result;
	}

	for (int i = 0; i < vec.size() / ele_num; i++)
	{
		if (0 != i) {
			result += big_sep;
		}

		for (int j = 0; j < ele_num; j++)
		{
			if (0 != j) {
				result += small_sep;
			}

			result += lx2s(vec[i*ele_num + j]);
		}
	}

	return result;
}

void send_system_mail(std::string uid, boost::shared_ptr<koko_socket> psock)
{
	std::string sql = "select u.id, u.attach_state from user_mail u left join user_mail_slave s on (s.id = u.id and s.uid = '" + uid + "') where u.uid = 'system' and u.mail_type = -1 and s.id is null";
	db_delay_helper_.get_result(sql, [uid, psock](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		while (q.fetch_row())
		{
			int id = q.getval();
			int attach_state = q.getval();

			Database& db = *db_;
			BEGIN_INSERT_TABLE("user_mail_slave");
			SET_FIELD_VALUE("id", id);
			SET_FIELD_VALUE("uid", uid);
			SET_FIELD_VALUE("state", 0);
			SET_FIELD_VALUE("attach_state", attach_state);
			SET_FINAL_VALUE_NO_COMMA("create_time", "now()");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}

		std::string sql = "select u.id, u.title_type, u.title, if(u.uid = 'system', ifnull(s.state, 0), u.state), if(u.uid = 'system', ifnull(s.attach_state, u.attach_state), u.attach_state), unix_timestamp(if(mail_type = -1, ifnull(s.create_time, u.create_time), u.create_time))"\
			" from user_mail u left join user_mail_slave s on (u.id = s.id and s.uid = '" + uid + "') where u.uid = 'system' and u.mail_type = -1 and s.state != 2";
		db_delay_helper_.get_result(sql, [uid, psock](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row()) 
			{
				msg_user_mail_list msg;
				msg.id_ = q.getval();
				msg.mail_type_ = 0;
				msg.title_type_ = q.getval();
				msg.title_ = q.getstr();
				msg.state_ = q.getval();
				msg.attach_state_ = q.getval();
				msg.timestamp_ = q.getbigint();
				send_protocol(psock, msg);
			}
		});
	});
}

//非统一邮件：30日内的无附件或附件已领取的未删除邮件 + 所有附件未领取邮件
//统一邮件：30日内且未过期、未删除邮件
int send_all_mail_to_player(std::string uid, boost::shared_ptr<koko_socket> psock)
{
	for (int mail_type = 0; mail_type < 3; mail_type++)
	{
		int count = 50;
		//附件未领取
		std::string sql = "select u.id, u.mail_type, u.title_type, u.title, if(u.uid = 'system', ifnull(s.state, 0), u.state), if(u.uid = 'system', ifnull(s.attach_state, u.attach_state), u.attach_state), unix_timestamp(u.create_time) from user_mail u left join user_mail_slave s on (u.id = s.id and s.uid = '" + uid + "') where "\
			"(u.uid = '" + uid + "' and u.mail_type = " + lx2s(mail_type) + " and u.attach_state = 1)"\
			" or (u.uid = 'system' and u.mail_type = " + lx2s(mail_type) + " and u.attach_state = 1 and (s.attach_state is null or (s.uid = '" + uid + "' and s.attach_state = 1))) order by u.create_time desc limit " + lx2s(count);
		db_delay_helper_.get_result(sql, [uid, mail_type, psock, count](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			int ncount = count;
			while (q.fetch_row())
			{
				msg_user_mail_list msg;
				msg.id_ = q.getval();
				msg.mail_type_ = q.getval();
				msg.title_type_ = q.getval();
				msg.title_ = q.getstr();
				msg.state_ = q.getval();
				msg.attach_state_ = q.getval();
				msg.timestamp_ = q.getbigint();
				send_protocol(psock, msg);

				ncount--;
				if (ncount <= 0) {
					msg_sequence_send_over<none_socket> msg;
					msg.cmd_ = GET_CLSID(msg_user_mail_list);
					send_protocol(psock, msg);
					return;
				}
			}

			//未读(没有附件或附件已领取(好友赠送礼物邮件))
			if (ncount > 0)
			{
				std::string sql = "select u.id, u.mail_type, u.title_type, u.title, if(u.uid = 'system', ifnull(s.state, 0), u.state), if(u.uid = 'system', ifnull(s.attach_state, u.attach_state), u.attach_state), unix_timestamp(u.create_time) from user_mail u left join user_mail_slave s on (u.id = s.id and s.uid = '" + uid + "') where "\
					"(u.uid = '" + uid + "' and u.mail_type = " + lx2s(mail_type) + " and u.state = 0 and (u.attach_state = 0 or u.attach_state = 2))"\
					" or (u.uid = 'system' and u.mail_type = " + lx2s(mail_type) + " and u.attach_state = 0 and s.uid is null) order by u.create_time desc limit " + lx2s(count);
				db_delay_helper_.get_result(sql, [uid, mail_type, psock, ncount](bool result, const std::vector<result_set_ptr>& vret) {
					if (!result || vret.empty()) return;
					int count = ncount;
					query_result_reader q(vret[0]);
					while (q.fetch_row())
					{
						msg_user_mail_list msg;
						msg.id_ = q.getval();
						msg.mail_type_ = q.getval();
						msg.title_type_ = q.getval();
						msg.title_ = q.getstr();
						msg.state_ = q.getval();
						msg.attach_state_ = q.getval();
						msg.timestamp_ = q.getbigint();
						send_protocol(psock, msg);

						count--;
						if (count <= 0) {
							msg_sequence_send_over<none_socket> msg;
							msg.cmd_ = GET_CLSID(msg_user_mail_list);
							send_protocol(psock, msg);
							return;
						}
					}

					//已读
					if (count > 0)
					{
						std::string sql = "select u.id, u.mail_type, u.title_type, u.title, if(u.uid = 'system', ifnull(s.state, 0), u.state), if(u.uid = 'system', ifnull(s.attach_state, u.attach_state), u.attach_state), unix_timestamp(u.create_time) from user_mail u left join user_mail_slave s on (u.id = s.id and s.uid = '" + uid + "') where "\
							"(u.uid = '" + uid + "' and u.mail_type = " + lx2s(mail_type) + " and u.state = 1)"\
							" or (u.uid = 'system' and u.mail_type = " + lx2s(mail_type) + " and s.uid = '" + uid + "' and s.state = 1) order by u.create_time desc limit " + lx2s(count);
						db_delay_helper_.get_result(sql, [uid, mail_type, psock, count](bool result, const std::vector<result_set_ptr>& vret) {
							if (!result || vret.empty()) return;
							int ncount = count;
							query_result_reader q(vret[0]);
							while (q.fetch_row())
							{
								msg_user_mail_list msg;
								msg.id_ = q.getval();
								msg.mail_type_ = q.getval();
								msg.title_type_ = q.getval();
								msg.title_ = q.getstr();
								msg.state_ = q.getval();
								msg.attach_state_ = q.getval();
								msg.timestamp_ = q.getbigint();
								send_protocol(psock, msg);

								ncount--;
								if (ncount <= 0) {
									break;
								}
							}

							{
								msg_sequence_send_over<none_socket> msg;
								msg.cmd_ = GET_CLSID(msg_user_mail_list);
								send_protocol(psock, msg);
							}
						});
					}
				});
			}
		});
	}

	return error_success;
}

int send_present_state_to_player(std::string uid, boost::shared_ptr<koko_socket> psock)
{
	std::string condition;
	int max_pid = 0;
	for (auto it = glb_present_bag.begin(); it != glb_present_bag.end(); it++)
	{
		if (it->first > max_pid) {
			max_pid = it->first;
		}
	}

	std::map<int, int> bmap;
	for (int i = 1; i <= max_pid; i++)
	{
		bmap.insert(std::make_pair(i, 0));
	}

	for (auto it = glb_present_bag.begin(); it != glb_present_bag.end(); it++)
	{
		if (it != glb_present_bag.begin()) {
			condition += ",";
		}

		condition += lx2s(it->first);
	}

	std::string sql = "select pid, state from user_present_bag where uid = '" + uid + "' and pid in(" + condition + ") and expire_time > now()";
	db_delay_helper_.get_result(sql, [psock, bmap](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		std::map<int, int> bbmap(bmap);
		while (q.fetch_row())
		{
			int pid = q.getval();
			int state = q.getval();

			bbmap[pid] = state;
		}

		std::vector<int> vec;
		for (auto it = bbmap.begin(); it != bbmap.end(); it++)
		{
			vec.push_back(it->second);
		}

		msg_common_present_state msg;
		msg.present_state_ = convert_vector_to_string(vec, 1);
		send_protocol(psock, msg);
	});

	return error_success;
}

int send_head_to_player(player_ptr pp, boost::shared_ptr<koko_socket> psock)
{
	int head_id = 0;
	try {
		head_id = boost::lexical_cast<int>(pp->head_ico_);
		if (head_id > 5) {
			//判断头像是否有效
			std::string sql = "select `head_id` from `user_head` where `uid` = '" + pp->platform_uid_ + "' and `head_id` = " + lx2s(head_id) + " and expire_time > now()";
			db_delay_helper_.get_result(sql, [pp, psock, head_id](bool result, const std::vector<result_set_ptr>& vret) {
				if (!result || vret.empty()) return;
				query_result_reader q(vret[0]);
				int h_id = 0;	//默认头像
				if (q.fetch_row()) {
					h_id = q.getval();
				}

				{
					msg_user_head_and_headframe msg;
					msg.head_ico_ = lx2s(h_id);
					msg.headframe_id_ = pp->headframe_id_;
					send_protocol(psock, msg);
				}

				if (h_id != head_id) {
					pp->head_ico_ = lx2s(h_id);

					BEGIN_UPDATE_TABLE("user_account");
					SET_FIELD_VALUE("headpic_name", pp->head_ico_);
					SET_FINAL_VALUE("headframe_id", pp->headframe_id_);
					WITH_END_CONDITION("where uid = '" + pp->platform_uid_ + "'");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}
			});
		}
	}
	catch (const boost::bad_lexical_cast& e) {

	}

	msg_user_head_and_headframe msg;
	msg.head_ico_ = pp->head_ico_;
	msg.headframe_id_ = pp->headframe_id_;
	send_protocol(psock, msg);

	return error_success;
}

int send_server_time_to_player(player_ptr pp, boost::shared_ptr<koko_socket> psock)
{
	std::string sql = "select unix_timestamp(now())";
	db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		if (q.fetch_row()) {
			msg_server_time<none_socket> msg;
			msg.timestamp_ = q.getbigint();
			send_protocol(psock, msg);
		}
	});

	return error_success;
}

int send_day_buy_data(std::string uid, boost::shared_ptr<koko_socket> psock)
{
	std::string sql = "select pid, num from user_day_buy where uid = '" + uid + "' and date(update_time) = date(now())";
	db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		while (q.fetch_row())
		{
			msg_user_day_buy_data msg;
			msg.pid_ = q.getval();
			msg.num_ = q.getval();
			send_protocol(psock, msg);
		}

		{
			msg_sequence_send_over<none_socket> msg;
			msg.cmd_ = GET_CLSID(msg_user_day_buy_data);
			send_protocol(psock, msg);
		}
	});
	
	return error_success;
}

//发邮件
int send_mail(user_mail& mail, bool is_main_thread/*是否是主线程调用*/ = true, bool is_need_conv/*是否需要转码*/ = true)
{
	//验证邮件的正确性
	std::vector<std::pair<int, int>> att;
	{
		//系统邮件必须由主线程发送
		if (mail.uid_ == "system" && !is_main_thread) {
			i_log_system::get_instance()->write_log(loglv_error, "send mail err[uid_: %s, is_main_thread: %d]", mail.uid_.c_str(), (int)is_main_thread);
			return error_invalid_data;
		}

		if (!(mail.mail_type_ >= 0 && mail.mail_type_ <= 2)) {
			i_log_system::get_instance()->write_log(loglv_error, "send mail err[mail_type_: %d]", mail.mail_type_);
			return error_invalid_data;
		}

		if (!(mail.title_type_ >= 0 && mail.title_type_ <= 7)) {
			i_log_system::get_instance()->write_log(loglv_error, "send mail err[title_type_: %d]", mail.title_type_);
			return error_invalid_data;
		}

		if (mail.state_ != 0) {
			i_log_system::get_instance()->write_log(loglv_error, "send mail err[state: %d != 0]", mail.state_);
			return error_invalid_data;
		}

		//附件
		std::vector<int> item;
		split_str(mail.attach_, ",", item, true);
		if (item.size() % 2 != 0) {
			i_log_system::get_instance()->write_log(loglv_error, "send mail err[attach_size: %d %% 2 != 0]", item.size());
			return error_invalid_data;
		}

		for (int i = 0; i<int(item.size() - 1); i += 2)
		{
			att.push_back(std::make_pair(item[i], item[i + 1]));
		}

		if (att.size() > 6) {
			i_log_system::get_instance()->write_log(loglv_error, "send mail err[attach_total: %d]", att.size());
			return error_invalid_data;
		}
	}

	//插入邮件
	std::string title;
	std::string content;
	int id = -1;
	{
		if (is_need_conv) {
			title = boost::locale::conv::between(mail.title_, "UTF-8", "GB2312");
			content = boost::locale::conv::between(mail.content_, "UTF-8", "GB2312");
		}
		else {
			title = mail.title_;
			content = mail.content_;
		}
		
		Database& db = *db_;
		BEGIN_INSERT_TABLE("user_mail");
		SET_FIELD_VALUE("uid", mail.uid_);
		SET_FIELD_VALUE("mail_type", mail.mail_type_);
		SET_FIELD_VALUE("title_type", mail.title_type_);
		SET_FIELD_VALUE("title", title);
		SET_FIELD_VALUE("content", content);
		SET_FIELD_VALUE("hyperlink", mail.hyperlink_);
		SET_FIELD_VALUE("state", mail.state_);
		SET_FIELD_VALUE("attach_state", mail.attach_state_);
		for (int i = 0; i < (int)att.size(); i++)
		{
			std::string attach_id = "attach" + lx2s(i + 1) + "_id";
			std::string attach_num = "attach" + lx2s(i + 1) + "_num";
			SET_FIELD_VALUE(attach_id, lx2s(att[i].first));
			SET_FIELD_VALUE(attach_num, lx2s(att[i].second));
		}
		SET_FINAL_VALUE_NO_COMMA("expire_time", "date_add(now(), interval " + lx2s(mail.save_days_) + " day)");
		EXECUTE_IMMEDIATE();
		id = q.insert_id();
	}

	//新邮件提醒
	if (id != -1) {
		__int64 timestamp = 0;
		{
			Query q(*db_);
			std::string sql = "select unix_timestamp(create_time) from user_mail where id = " + lx2s(id);
			if (!q.get_result(sql))
				return error_sql_execute_error;

			if (!q.fetch_row()) {
				return error_success;
			}
			else {
				timestamp = q.getbigint();
				q.free_result();
			}
		}

		boost::shared_ptr<msg_user_mail_list> pmsg(new msg_user_mail_list());
		pmsg->id_ = id;
		pmsg->mail_type_ = mail.mail_type_;
		pmsg->title_type_ = mail.title_type_;
		pmsg->title_ = title;
		pmsg->state_ = mail.state_;
		pmsg->attach_state_ = mail.attach_state_;
		pmsg->timestamp_ = timestamp;

		if (mail.uid_ == "system") {
			broadcast_msg(pmsg);
		}
		else {
			auto pp = get_player(mail.uid_);
			if (pp && pp->from_socket_.lock().get()) {
				if (is_main_thread) {
					send_protocol(pp->from_socket_.lock(), *pmsg);
				}
				else {
					glb_thread_send_cli_msg_lst_.push_back(std::make_pair(pp->from_socket_.lock(), pmsg));
				}
			}
		}
	}

	return error_success;
}

//VIP每日奖励
int send_daily_present_to_vip(player_ptr pp, boost::shared_ptr<koko_socket> psock)
{
	//是否今日第一次登录
	{
		std::string sql = "select uid from user_last_login_time where uid = '" + pp->platform_uid_ + "' and date(`last_login_time`) = date(now())";
		db_delay_helper_.get_result(sql, [pp](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			if (q.fetch_row()) {
				return;
			}

			//发放回馈券
			int lucky_ticket_num = glb_vip_privilege[pp->vip_level()].lucky_dial_times_;
			if (lucky_ticket_num > 0) {
				user_mail mail;
				mail.uid_ = pp->platform_uid_;
				mail.mail_type_ = 0;
				mail.title_type_ = 5;
				mail.title_ = "这里有一份VIP奖励需求您领取哦！";
				mail.content_ = "尊敬的VIP" + lx2s(pp->vip_level()) + "级用户“" + boost::locale::conv::between(pp->nickname_, "GB2312", "UTF-8") + "”：\n\t\t您好！小k为您送上您今日的VIP登录奖励，请及时查收哦！";
				mail.state_ = 0;
				mail.attach_state_ = 1;
				std::vector<int> vec;
				vec.push_back(item_id_lucky_ticket);
				vec.push_back(lucky_ticket_num);
				mail.attach_ = combin_str(vec, ",");
				mail.save_days_ = 30;
				send_mail(mail);
			}

			//更新状态
			{
				Database& db = *db_;
				BEGIN_INSERT_TABLE("user_last_login_time");
				SET_FIELD_VALUE("uid", pp->platform_uid_);
				SET_FINAL_VALUE("lucky_ticket", lucky_ticket_num);
				WITH_END_CONDITION("on duplicate key update last_login_time = now(), lucky_ticket = " + lx2s(lucky_ticket_num));
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
		});
	}

	return error_success;
}

template<class remote_t>
void	response_user_login(player_ptr pp, remote_t pconn, boost::shared_ptr<msg_user_login_ret> pmsg)
{	
	pmsg->gold_ = pp->gold_;
	pmsg->game_gold_ = pp->gold_game_;
	pmsg->game_gold_free_ = pp->gold_free_;
	pmsg->iid_ = pp->iid_;
	pmsg->vip_level_ = pp->vip_level();
	pmsg->sequence_ = pp->seq_;
	pmsg->idcard_ = pp->idnumber_;

	pmsg->phone = pp->mobile_;
	pmsg->phone_verified_ = pp->mobile_v_;

	pmsg->email_ = pp->email_;
	pmsg->email_verified_ = pp->email_v_;
	pmsg->age_ = pp->age_;
	pmsg->gender_ = pp->gender_;
	pmsg->level_ = pp->level_;
	pmsg->byear_ = pp->byear_;
	pmsg->bmonth_ = pp->bmonth_;
	pmsg->region1_ = pp->region1_;
	pmsg->region2_ = pp->region2_;
	pmsg->region3_ = pp->region3_;
	pmsg->bday_ = pp->bday_;

	pmsg->address_ = pp->address_;
	pmsg->nickname_ = pp->nickname_;
	pmsg->uid_ = pp->uid_;
	pmsg->token_ = pp->token_;
}

msg_ptr create_msg_from_client(unsigned short cmd)
{
	DECLARE_MSG_CREATE()
	REGISTER_CLS_CREATE(msg_join_channel);
	REGISTER_CLS_CREATE(msg_chat);
	REGISTER_CLS_CREATE(msg_leave_channel);
	REGISTER_CLS_CREATE(msg_user_login_channel);
	REGISTER_CLS_CREATE(msg_action_record);
	REGISTER_CLS_CREATE(msg_enter_private_room);
	REGISTER_CLS_CREATE(msg_get_private_room_list);
	REGISTER_CLS_CREATE(msg_buy_item);
	REGISTER_CLS_CREATE(msg_send_present);
	REGISTER_CLS_CREATE(msg_alloc_game_server);
	REGISTER_CLS_CREATE(msg_use_present);
	REGISTER_CLS_CREATE(msg_query_data);
	REGISTER_CLS_CREATE(msg_get_present_record);
	REGISTER_CLS_CREATE(msg_get_buy_item_record);
	REGISTER_CLS_CREATE(msg_transfer_gold_to_game);
	REGISTER_CLS_CREATE(msg_send_present_by_pwd);
	REGISTER_CLS_CREATE(msg_get_private_room_record);
	REGISTER_CLS_CREATE(msg_get_login_prize_list);
	REGISTER_CLS_CREATE(msg_get_login_prize);
	REGISTER_CLS_CREATE(msg_get_headframe_list);
	REGISTER_CLS_CREATE(msg_set_head_and_headframe);
	REGISTER_CLS_CREATE(msg_buy_item_by_activity);
	REGISTER_CLS_CREATE(msg_get_activity_list);
	REGISTER_CLS_CREATE(msg_get_recharge_record);
	REGISTER_CLS_CREATE(msg_play_lucky_dial);
	REGISTER_CLS_CREATE(msg_get_serial_lucky_state);
	REGISTER_CLS_CREATE(msg_get_mail_detail);
	REGISTER_CLS_CREATE(msg_del_mail);
	REGISTER_CLS_CREATE(msg_del_all_mail);
	REGISTER_CLS_CREATE(msg_get_mail_attach);
	REGISTER_CLS_CREATE(msg_client_get_data);
	REGISTER_CLS_CREATE(msg_client_set_data);
	REGISTER_CLS_CREATE(msg_common_get_present);
	REGISTER_CLS_CREATE(msg_get_rank_data);
	REGISTER_CLS_CREATE(msg_get_head_list);
	END_MSG_CREATE()
	return  ret_ptr;
}


vmsg_ptr create_msg_from_internal(unsigned short cmd)
{
	vmsg_ptr ret_ptr; 
	switch (cmd)
	{
		REGISTER_CLS_CREATE(msg_register_to_world);
		REGISTER_CLS_CREATE(msg_user_login_delegate); 
		REGISTER_CLS_CREATE(msg_koko_trade_inout); 
		REGISTER_CLS_CREATE(msg_server_broadcast); 
		REGISTER_CLS_CREATE(msg_private_room_sync); 
		REGISTER_CLS_CREATE(msg_private_room_psync);
		REGISTER_CLS_CREATE(msg_enter_private_room_notice);
		REGISTER_CLS_CREATE(msg_buy_item_notice);
		REGISTER_CLS_CREATE(msg_use_item_notice);
		REGISTER_CLS_CREATE(msg_sync_rank_notice);
		REGISTER_CLS_CREATE(msg_verify_user);
		REGISTER_CLS_CREATE(msg_private_room_remove_sync);
		REGISTER_CLS_CREATE(msg_send_mail_to_player);
	default:
		break;
	}
	return  ret_ptr;
}

boost::atomic_bool glb_exit;
bool glb_exited = false;
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	switch(CEvent)
	{
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		glb_exit = true;
	}
	while (!glb_exited)
	{

	}
	return TRUE;
}

class task_on_5sec : public task_info
{
public:
	task_on_5sec(boost::asio::io_service& ios):task_info(ios)
	{

	}
	int		routine();
};

void	pickup_player_msgs(bool& busy)
{
	auto  remotes = the_net->get_remotes();
	for (unsigned int i = 0 ;i < remotes.size(); i++)
	{
		koko_socket_ptr psock = remotes[i];
		bool got_msg = false;
		unsigned int pick_count = 0;
		do 	{
			got_msg = false;
			auto	pmsg =  pickup_msg_from_socket(psock, create_msg_from_client, got_msg);
			if (pmsg.get()){
				busy = true;
				pick_count++;
				pmsg->from_sock_ = psock;
				glb_msg_list_[psock].push_back(pmsg);
				msg_recved++;

				if (glb_msg_list_[psock].size() > 50) {
					psock->close();
					glb_msg_list_.erase(psock);
				}
			}
		}while(got_msg && pick_count < 200);
	}
}

void	pickup_internal_msgs(bool& busy)
{
	auto  remotes = the_internal_net->get_remotes();
	for (unsigned int i = 0 ;i < remotes.size(); i++)
	{
		auto psock = remotes[i];
		bool got_msg = false;
		unsigned int pick_count = 0;
		do{
			got_msg = false;
			auto pmsg = pickup_msg_from_socket(psock, create_msg_from_internal, got_msg);
			if (pmsg.get()){
				busy = true;
				pick_count++;
				pmsg->from_sock_ = psock;
				glb_vmsg_list_.push_back(pmsg);
				msg_recved++;
			}
		}while(got_msg && pick_count < 200);
	}
}

template<class remote_t>
int		verify_user_info(msg_user_login_t<remote_t>* plogin, bool check_sign = true)
{
	if (check_sign){
		std::string	sign_pat = plogin->acc_name_ + std::string(plogin->pwd_hash_) + plogin->machine_mark_ + the_config.signkey_;
		std::string sign_key = md5_hash(sign_pat);
		std::string sign_remote;
		sign_remote.insert(sign_remote.begin(), plogin->sign_, plogin->sign_ + 32);
		if (sign_key != sign_remote){
			return error_wrong_sign;
		}
	}
	//防止sql注入攻击
	sql_trim(plogin->acc_name_);
	sql_trim(plogin->machine_mark_);
	
	if (isbaned(plogin->acc_name_)) {
		return error_user_banned;
	}
	return error_success;
}

int			do_load_user(std::string sql, player_ptr& pp, bool check_psw, std::string pwd_check)
{
	Database& db = *db_;
	Query q(db);

	if (!q.get_result(sql))
		return error_sql_execute_error;

	if (!q.fetch_row()){
		return error_no_record;
	}

	std::string uid = q.getstr();
	std::string	pwd = q.getstr();
	std::string nickname = q.getstr();
	__int64 iid = q.getbigint();
	__int64 gold = q.getbigint();
	__int64 gold_game = q.getbigint();
	__int64 gold_free = q.getbigint();
	int			vip_value = q.getval();
	int			vip_limit = q.getval();
	std::string	head_ico = q.getstr();
	int			headframe_id = q.getval();
	std::string idnumber = q.getstr();
	std::string mobile = q.getstr();
	int mobile_verified = q.getval();
	std::string mail = q.getstr();
	int mail_verified = q.getval();
	int lv = q.getval();
	int r1 = q.getval();
	int r2 = q.getval();
	int r3 = q.getval();
	int gender = q.getval();
	int	byear = q.getval();
	int bmonth = q.getval();
	int bday = q.getval();
	std::string addr = q.getstr();
	int age = q.getval();
	int isagent = q.getval();
	__int64	create_tm = q.getval();

	std::vector<char> headpic;
	q.getblob("headpic", headpic);

	if (check_psw && md5_hash(pwd) != pwd_check){
		return error_wrong_password;
	}

	if (nickname == ""){
		nickname = "newplayer";
	}

	q.free_result();

	__int64 seq = 0;	
	::time(&seq);

	std::string token = md5_hash(uid + boost::lexical_cast<std::string>(seq) + the_config.token_verify_key_);

	pp->uid_ = uid;
	pp->platform_uid_ = uid;
	pp->connect_lost_tick_ = boost::posix_time::microsec_clock::local_time();
	pp->is_connection_lost_ = 0;
	pp->iid_ = iid;
	pp->nickname_ = nickname;
	pp->headpic_ = headpic;
	pp->gold_ = gold;
	pp->vip_value_ = vip_value;
	pp->vip_limit_ = vip_limit;
	pp->head_ico_ = head_ico;
	pp->headframe_id_ = headframe_id;
	pp->token_ = token;
	pp->seq_ = seq;
	pp->idnumber_ = idnumber;
	pp->age_ = age;
	pp->mobile_ = mobile;
	pp->email_ = mail;
	pp->region1_ = r1;
	pp->region2_ = r2;
	pp->region3_ = r3;
	pp->isagent_ = isagent;
	pp->is_newbee_ = (::time(nullptr) - create_tm) < boost::posix_time::hours(48).total_seconds();
	pp->gold_game_ = gold_game;
	pp->mobile_v_ = mobile_verified;
	pp->email_v_ = mail_verified;
	pp->byear_ = byear;
	pp->bmonth_ = bmonth;
	pp->bday_ = bday;
	pp->address_ = addr;
	pp->gender_ = gender;
	pp->gold_free_ = gold_free;

	return error_success;
}

template<class remote_t>
int load_user_by_account(msg_user_login_t<remote_t>* plogin, player_ptr& pp, bool check_sign)
{
	int verify = verify_user_info(plogin, check_sign);
	if (verify != error_success){
		return verify;
	}
	char saccname[256] = {0};
	int len =	mysql_real_escape_string(&db_->grabdb()->mysql,saccname, plogin->acc_name_.c_str(), strlen(plogin->acc_name_.c_str()));
	std::string sql = "select uid, pwd, nickname, iid, gold, gold_game, gold_free, vip_value, vip_limit, headpic_name, headframe_id,"
		" idnumber, mobile_number, mobile_verified, mail, mail_verified, level, addr_province, addr_city, addr_region,"
		" gender, byear, bmonth, bday, address, age, is_agent,"
		" unix_timestamp(create_time), headpic from user_account where uname = '" + std::string(saccname) + "'"
		" or (mobile_number = '" + std::string(saccname) + "' and mobile_verified = 1)"
		" or (mail = '" + std::string(saccname) + "' and mail_verified = 1)";

	return do_load_user(sql, pp, true, plogin->pwd_hash_);
}

int load_user_by_uid(msg_user_login_channel* plogin, player_ptr& pp, bool check_sign)
{
	std::string sql = "select uid, pwd, nickname, iid, gold, gold_game, gold_free, vip_value, vip_limit, headpic_name, headframe_id,"
		" idnumber, mobile_number, mobile_verified, mail, mail_verified, level, addr_province, addr_city, addr_region,"
		" gender, byear, bmonth, bday, address, age, is_agent, "
		" unix_timestamp(create_time), headpic from user_account where uid = '" + std::string(plogin->uid_) + "'";

	return do_load_user(sql, pp, false, "");
}

int		handle_user_login(msg_user_login_channel* plogin)
{
	player_ptr pp(new koko_player());
	int ret = load_user_by_uid(plogin, pp, true);
	if (ret != error_success){
		i_log_system::get_instance()->write_log(loglv_debug, "user data load fail by msg_user_login_channel request.");
		return ret;
	}
	i_log_system::get_instance()->write_log(loglv_debug, "user data loaded by msg_user_login_channel request.");
	plogin->from_sock_->set_authorized();
	pp->from_socket_ = plogin->from_sock_;
	plogin->from_sock_->the_client_ = pp;

	pp->from_devtype_ = plogin->device_;
	if (pp->from_devtype_ == "PC"){
		pp->uid_ = "PC" + pp->uid_;
	}

	pending_login_users_.push_back(pp);
	return error_success;
}

int		handle_user_login(msg_user_login_delegate* plogin)
{
	player_ptr pp(new koko_player());
	int ret = load_user_by_account(plogin, pp, false);
	if (ret != error_success){
		boost::shared_ptr<msg_common_reply_internal<none_socket>> psend(new msg_common_reply_internal<none_socket>);
		psend->rp_cmd_ = GET_CLSID(msg_user_login_delegate);
		psend->err_ = ret;
		psend->SN = plogin->sn_;
		glb_thread_send_srv_msg_lst_.push_back(std::make_pair(plogin->from_sock_, psend));
		return ret;
	}

	boost::shared_ptr<msg_user_login_ret_delegate> psend(new msg_user_login_ret_delegate());
	psend->sn_ = plogin->sn_;
	response_user_login(pp, plogin->from_sock_, psend);
	glb_thread_send_srv_msg_lst_.push_back(std::make_pair(plogin->from_sock_, psend));
	return error_success;
}

std::string pickup_value(std::string params, std::string field)
{
	std::string pat = std::string(field + "=");
	char* vstart = strstr((char*)params.c_str(), pat.c_str());
	if (!vstart) return "";
	char* vend = strstr(vstart + pat.length(), "&");
	if (!vend){
		return vstart + pat.length();
	}
	else {
		std::string ret;
		ret.insert(ret.begin(), vstart + pat.length(), vend);
		return ret;
	}
}

bool	isbaned(std::string uid);
void	verify_user(msg_verify_user* pmsg)
{
	boost::shared_ptr<msg_verify_user_ret> psend(new msg_verify_user_ret());
	psend->sn_ = pmsg->sn_;
	psend->result_ = isbaned(pmsg->uid) ? 1 : 0;
	glb_thread_send_srv_msg_lst_.push_back(std::make_pair(pmsg->from_sock_, psend));
}

bool	db_thread_func()
{
	unsigned int idle = 0;

	bool busy = false;

	msg_ptr pmsg;
	if (sub_thread_process_msg_lst_.pop_front(pmsg)) {
		int ret = error_success;
		if (pmsg->head_.cmd_ == GET_CLSID(msg_user_login_channel)) {
			function_time_counter ftc("handle_user_login((msg_user_login_channel*)pmsg.get())", 100);
			ret = handle_user_login((msg_user_login_channel*)pmsg.get());
			if (ret != error_success) {
				pmsg->from_sock_->is_login_ = false;
			}
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_query_info)) {
			ret = error_success;
		}

		if (ret != error_success) {
			boost::shared_ptr<msg_common_reply<none_socket>> psend(new msg_common_reply<none_socket>);
			psend->rp_cmd_ = pmsg->head_.cmd_;
			psend->err_ = ret;
			glb_thread_send_cli_msg_lst_.push_back(std::make_pair(pmsg->from_sock_, psend));
		}
		busy = true;
	}

	vmsg_ptr vmsg;
	if (sub_thread_process_vmsg_lst_.pop_front(vmsg)) {
		if (vmsg->head_.cmd_ == GET_CLSID(msg_user_login_delegate)) {
			function_time_counter ftc("handle_user_login((msg_user_login_delegate*)vmsg.get())", 100);
			int ret = handle_user_login((msg_user_login_delegate*)vmsg.get());
		}
		else if (vmsg->head_.cmd_ == GET_CLSID(msg_verify_user)){
			verify_user((msg_verify_user*)vmsg.get());
		}
		busy = true;
	}

	return busy;
}

int	handle_internal_msgs(std::map<int,int>& handled)
{
	auto it = glb_vmsg_list_.begin();
	while (it != glb_vmsg_list_.end())
	{
		auto cmd = *it;	it++;
		int ret = cmd->handle_this();
		handled[cmd->head_.cmd_]++;
	}
	glb_vmsg_list_.clear();
	return error_success;
}

bool func_2(channel_server& a1, channel_server& a2)
{
	return a1.online_ < a2.online_;
}

int	handle_pending_logins()
{
	while (pending_login_users_.size() > 0)
	{
		player_ptr pp;
		if(!pending_login_users_.pop_front(pp))
			break;

		//在账号服务器和频道服务器配成一个的情况下
		//用户的登录频道服务器所加载的player不做处理。
		//如果用户用频道服务器方式登录

		auto itp = online_players.find(pp->uid_);
		if (itp == online_players.end() ){
			online_players.insert(std::make_pair(pp->uid_, pp));
		}
		else{
			auto conn = itp->second->from_socket_.lock();
			//重复发送登录，不作处理
			if (conn == pp->from_socket_.lock()){
				return error_success;
			}

			if(conn.get()){
				msg_same_account_login msg;
				send_protocol(conn, msg, true);
			}
			//用户重新登录上来了
			std::map<int, channel_ptr> cpl = koko_channels;
			std::list<int> cpll = itp->second->in_channels_;
			std::for_each(cpll.begin(), cpll.end(), [&cpl, pp](int chn) {
				auto it = cpl.find(chn);
				it->second->player_join(pp, 0);
			});
			replace_map_v(online_players, std::make_pair(pp->uid_, pp));
		}

		auto pconn = pp->from_socket_.lock();
		if (pconn.get()){
			pconn->is_login_ = false;

			msg_common_reply<koko_socket_ptr> msg;
			msg.rp_cmd_ = GET_CLSID(msg_user_login_channel);
			msg.err_ = error_success;
			i_log_system::get_instance()->write_log(loglv_debug, "response to client on user login success.");
			send_protocol(pconn, msg);

			send_head_to_player(pp, pconn);
			send_server_time_to_player(pp, pconn);

			if (pp->from_devtype_ != "PC") {
				send_daily_present_to_vip(pp, pconn);
			}

			send_present_state_to_player(pp->platform_uid_, pconn);
			send_day_buy_data(pp->platform_uid_, pconn);

			extern void	send_all_items_to_player(std::string uid, boost::shared_ptr<koko_socket> psock);
			extern void	send_items_data_to_player(std::string uid, boost::shared_ptr<koko_socket> psock, int item_id = -1);
			send_all_items_to_player(pp->platform_uid_, pconn);
			send_items_data_to_player(pp->platform_uid_, pconn);
			send_system_mail(pp->platform_uid_, pconn);
			send_all_mail_to_player(pp->platform_uid_, pconn);
		}
	}
	return 0;
}

int	handle_thread_send_cli_msg()
{
	while (glb_thread_send_cli_msg_lst_.size() > 0)
	{
		std::pair<koko_socket_ptr, smsg_ptr> pmsg;
		glb_thread_send_cli_msg_lst_.pop_front(pmsg);
		if (pmsg.first) {
			send_protocol(pmsg.first, *(pmsg.second));
		}
	}

	return 0;
}

int	handle_thread_send_srv_msg()
{
	while (glb_thread_send_srv_msg_lst_.size() > 0)
	{
		std::pair<srv_socket_ptr, smsg_ptr> pmsg;
		glb_thread_send_srv_msg_lst_.pop_front(pmsg);
		if (pmsg.first) {
			send_protocol(pmsg.first, *(pmsg.second));
		}
	}

	return 0;
}

int handle_player_msgs()
{
	auto it_cache = glb_msg_list_.begin();
	while (it_cache != glb_msg_list_.end())
	{
		int count = 0;
		for (auto it_list = it_cache->second.begin(); it_list != it_cache->second.end() && count < 20; it_list++, count++)
		{
			auto cmd = *it_list;
			auto pauth_msg = boost::dynamic_pointer_cast<msg_authrized<koko_socket_ptr>>(cmd);
			//如果需要授权的消息，没有得到授权，则退出
			if (pauth_msg.get() &&
				!pauth_msg->from_sock_->is_authorized()) {
				pauth_msg->from_sock_->close(false);
			}
			else {
				int ret = cmd->handle_this();
				if (ret != 0) {
					msg_common_reply<koko_socket_ptr> msg;
					msg.err_ = ret;
					msg.rp_cmd_ = cmd->head_.cmd_;
					send_protocol(cmd->from_sock_, msg);
				}
			}
		}
		
		it_cache++;
		msg_handled++;
	}
	glb_msg_list_.clear();
	return 0;
}

int		start_network()
{
	the_net->add_acceptor(the_config.client_port_);

	if(the_net->run()){
		i_log_system::get_instance()->write_log(loglv_info, "net service start success!");
	}
	else{
		i_log_system::get_instance()->write_log(loglv_info, "!!!!!net service start failed!!!!");
		return -1;
	}

	the_internal_net->add_acceptor(the_config.interal_port_);
	if(the_internal_net->run()){
		i_log_system::get_instance()->write_log(loglv_info, "interal net service start success!");
	}
	else{
		i_log_system::get_instance()->write_log(loglv_info, "!!!!!interal net service start failed!!!!");
		return -1;
	}

	return 0;
}

void		broadcast_msg(smsg_ptr msg)
{
	auto itp = online_players.begin();
	while (itp != online_players.end()){
		koko_socket_ptr cnn = itp->second->from_socket_.lock();	itp++;
		if (cnn.get()){
			send_protocol(cnn, *msg);
		}
	}
}

void	update_players()
{
	auto it = online_players.begin();
	while (it != online_players.end())
	{
		player_ptr pp = it->second;
		if (pp->is_connection_lost_ == 1 && 
			(boost::posix_time::microsec_clock::local_time() - pp->connect_lost_tick_).total_seconds() > 5){
				pp->connection_lost();

				std::map<int, channel_ptr> cpl = koko_channels;
				std::list<int> cpll = pp->in_channels_;
				std::for_each(cpll.begin(), cpll.end(), [&cpl, pp](int chn) {
					auto it = cpl.find(chn);
					it->second->player_leave(pp, 0);
				});

				it = online_players.erase(it);
		}
		else{
			it++;
		}
	}
}

int		load_all_games()
{
	std::string sql = "SELECT `id`,`tech_type`,\
		`dir`,	`exe`,	`update_url`,	`help_url`,\
		`game_name`,	`thumbnail`,	`solution`, `no_embed`, `catalog`, `seats` \
		FROM `setting_game_list`";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		all_games.clear();
		while (q.fetch_row())
		{
			gamei_ptr pgame(new game_info);
			pgame->id_ = q.getval();
			pgame->type_ = q.getval();
			COPY_STR(pgame->dir_, q.getstr());
			COPY_STR(pgame->exe_, q.getstr());
			COPY_STR(pgame->update_url_, q.getstr());
			COPY_STR(pgame->help_url_, q.getstr());
			COPY_STR(pgame->game_name_, q.getstr());
			COPY_STR(pgame->thumbnail_, q.getstr());
			COPY_STR(pgame->solution_, q.getstr());
			pgame->no_embed_ = q.getval();
			COPY_STR(pgame->catalog_, q.getstr());
			pgame->seats_ = q.getval();
			all_games.insert(std::make_pair(pgame->id_, pgame));
		}

		std::string sql = "SELECT `gameid`,	`roomid`,	`room_name`,\
		`room_desc`,	`thumbnail`,	`srvip`,		`srvport`, `require` \
		FROM `setting_game_room_info`";
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret1) {
			if (!result || vret1.empty()) return;
			query_result_reader q1(vret1[0]);
			
			while (q1.fetch_row())
			{
				game_room_inf inf;
				inf.game_id_ = q1.getval();
				inf.room_id_ = q1.getval();
				COPY_STR(inf.room_name_, q1.getstr());
				COPY_STR(inf.room_desc_, q1.getstr());
				COPY_STR(inf.thumbnail_, q1.getstr());
				COPY_STR(inf.ip_, q1.getstr());
				inf.port_ = q1.getval();
				COPY_STR(inf.require_, q1.getstr());
				if (all_games.find(inf.game_id_) != all_games.end()) {
					all_games[inf.game_id_]->rooms_.push_back(inf);
				}
			}
		});
	});

	return error_success;
}

void	clean()
{
	glb_exit = true;
	task.reset();
	koko_channels.clear();
	online_players.clear();

	//继续执行没保存完的sql语句ewr9i
	pending_login_users_.lst_.clear();
	//http_requests.lst_.clear();
	db_delay_helper_.pop_and_execute();

	timer_srv->stop();
	timer_srv.reset();

	the_net->stop();
	the_net.reset();

	the_internal_net->stop();
	the_internal_net.reset();

	i_log_system::get_instance()->stop();
	transaction_controller::get_instance()->stop();
	if (monitor_thread){
		monitor_thread->join();
	}

	if (dbthread_){
		dbthread_->join();
	}
	transaction_controller::get_instance()->free_instance();
}

void		load_chat_emotion()
{
	auto_chats_emotion.clear();
	FILE* fp = NULL;
	fopen_s(&fp, "bot_talk_em.txt", "r");
	if(!fp){	return;} 

	vector<char> data_read;
	char c[128];
	while (!feof(fp))
	{
		int s = fread(c, sizeof(char), 128, fp);
		for (int i = 0 ; i < s; i++)
		{
			data_read.push_back(c[i]);
		}
	}
	fclose(fp);

	split_str<std::string>(data_read.data() + 3, "\n", auto_chats_emotion, true);
}

void		load_auto_chat()
{
	auto_chats.clear();
	FILE* fp = NULL;
	fopen_s(&fp, "bot_talk.txt", "r");
	if(!fp){	return;} 

	vector<char> data_read;
	char c[128];
	while (!feof(fp))
	{
		int s = fread(c, sizeof(char), 128, fp);
		for (int i = 0 ; i < s; i++)
		{
			data_read.push_back(c[i]);
		}
	}
	fclose(fp);

	split_str<std::string>(data_read.data() + 3, "\n", auto_chats, true);
}

void		load_bots()
{
	std::vector<std::string > bot_names;
	FILE* fp = NULL;
	fopen_s(&fp, "bot_names.txt", "r");
	if(!fp){	return;} 

	vector<char> data_read;
	char c[128];
	while (!feof(fp))
	{
		int s = fread(c, sizeof(char), 128, fp);
		for (int i = 0 ; i < s; i++)
		{
			data_read.push_back(c[i]);
		}
	}
	fclose(fp);

	split_str<std::string>(data_read.data() + 3, "\n", bot_names, true);
	if (bot_names.empty()) return;
	
	free_bots_.clear();
	for (int i = 0; i < 1000; i++){
		player_ptr pp(new koko_player());
		int r = rand_r(9);
		if (r < 5){
			pp->nickname_ = bot_names[rand_r(bot_names.size() - 1)];
		}
		else{
			pp->nickname_ = bot_names[rand_r(bot_names.size() - 1)] + boost::lexical_cast<std::string>(rand_r(100));
		}
		pp->uid_ = get_guid();
		pp->platform_uid_ = pp->uid_;
		pp->iid_ = 0;
		pp->isagent_ = 0;
		pp->isbot_ = 1;
		pp->vip_value_ = 0;
		free_bots_.push_back(pp);
		if ((i % 100) == 99){
			i_log_system::get_instance()->write_log(loglv_info, "is generating bot, %d.", i + 1);
		}
	}
	i_log_system::get_instance()->write_log(loglv_info, "all bots loaded, size=%d.", free_bots_.size());
}

void		load_all_present()
{
	std::string sql = "select `pid`, `cat`, `name`, `describes`, `ico`, `vip_level`, `price`, `item`, `is_delay`, `min_buy_num`, `max_buy_num`, `buy_record_sec`, `forbid_buy`, `day_limit` from setting_goods_list";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		glb_present_data.clear();
		while (q.fetch_row())
		{
			present_info inf;
			inf.pid_ = q.getval();
			inf.cat_ = q.getval();
			COPY_STR(inf.name_, q.getstr());
			COPY_STR(inf.desc_, q.getstr());
			COPY_STR(inf.ico_, q.getstr());
			inf.vip_level_ = q.getval();

			//价格
			std::string price_str = q.getstr();
			std::vector<int> price;
			split_str(price_str, ",", price, true);
			if (price.size() % 3 != 0) {
				i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_goods_list table! pid:%d, price size invalid:%d", inf.pid_, price.size());
			}

			for (int i = 0; i <= int(price.size() - 3); i += 3)
			{
				item_unit it;
				it.cat_ = price[i];
				it.item_id_ = price[i + 1];
				it.item_num_ = price[i + 2];

				if (it.cat_ != 0) {
					i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_goods_list table! pid:%d, goods cat invalid:%d", inf.pid_, it.cat_);
					continue;
				}
				inf.price_.push_back(it);
			}

			//物品
			std::string item_str = q.getstr();
			std::vector<int> item;
			split_str(item_str, ",", item, true);
			if (item.size() % 3 != 0) {
				i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_goods_list table! pid:%d, item size invalid:%d", inf.pid_, item.size());
			}

			for (int i = 0; i <= int(item.size() - 3); i += 3)
			{
				item_unit it;
				it.cat_ = item[i];
				it.item_id_ = item[i + 1];
				it.item_num_ = item[i + 2];

				if (it.cat_ != 0 && it.cat_ != 1 && it.cat_ != 2) {
					i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_goods_list table! pid:%d, goods cat invalid:%d", inf.pid_, it.cat_);
					continue;
				}
				inf.item_.push_back(it);
			}

			//是否延迟到账
			inf.is_delay_ = q.getval();
			inf.min_buy_num_ = q.getval();
			inf.max_buy_num_ = q.getval();
			inf.buy_record_sec = q.getval();
			inf.forbid_buy_ = q.getval();
			inf.day_limit_ = q.getval();

			glb_present_data.insert(std::make_pair(inf.pid_, inf));
		}
	});
}

void		load_all_item()
{
	std::string sql = "select `item_id`, `type`, `name`, `desc`, `ico`, `is_use`, `to_item`, `is_send`, `send_vip_level`, `min_send_num`, `max_send_num` from setting_item_list";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		glb_item_data.clear();
		while (q.fetch_row())
		{
			item_info inf;
			inf.item_id_ = q.getval();
			inf.type_ = q.getval();
			COPY_STR(inf.name_, q.getstr());
			COPY_STR(inf.desc_, q.getstr());
			COPY_STR(inf.ico_, q.getstr());

			inf.is_use_ = q.getval();

			//使用价值
			std::string to_item_str = q.getstr();
			std::vector<int> to_item;
			split_str(to_item_str, ",", to_item, true);
			for (int i = 0; i <= int(to_item.size() - 2); i += 2)
			{
				inf.to_item_.push_back(std::make_pair(to_item[i], to_item[i + 1]));
			}

			if (inf.to_item_.size() > 1) {
				i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_item_list table! item size=%d", inf.to_item_.size());
			}

			inf.is_send_ = q.getval();
			inf.send_vip_level_ = q.getval();
			inf.min_send_num_ = q.getval();
			inf.max_send_num_ = q.getval();

			glb_item_data.insert(std::make_pair(inf.item_id_, inf));
		}
	});
}

void		load_all_activity()
{
	std::string sql = "select `activity_id`, `cat`, `name`, `desc`, `price`, `item`, `min_buy_num`, `max_buy_num`, unix_timestamp(`begin_time`), unix_timestamp(`end_time`) from setting_activity_list";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		glb_activity_data.clear();
		while (q.fetch_row())
		{
			activity_info inf;
			inf.activity_id_ = q.getval();
			inf.cat_ = q.getval();
			COPY_STR(inf.name_, q.getstr());
			COPY_STR(inf.desc_, q.getstr());

			//价格
			std::string price_str = q.getstr();
			std::vector<int> price;
			split_str(price_str, ",", price, true);
			if (price.size() % 3 != 0) {
				i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_activity_list table! pid:%d, price size invalid:%d", inf.activity_id_, price.size());
				continue;
			}

			for (int i = 0; i <= int(price.size() - 3); i += 3)
			{
				item_unit it;
				it.cat_ = price[i];
				it.item_id_ = price[i + 1];
				it.item_num_ = price[i + 2];

				if (it.cat_ != 0) {
					i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_activity_list table! pid:%d, goods cat invalid:%d", inf.activity_id_, it.cat_);
					continue;
				}
				inf.price_.push_back(it);
			}

			//物品
			std::string item_str = q.getstr();
			std::vector<int> item;
			split_str(item_str, ",", item, true);
			if (item.size() % 3 != 0) {
				i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_activity_list table! pid:%d, item size invalid:%d", inf.activity_id_, item.size());
				continue;
			}

			for (int i = 0; i <= int(item.size() - 3); i += 3)
			{
				item_unit it;
				it.cat_ = item[i];
				it.item_id_ = item[i + 1];
				it.item_num_ = item[i + 2];

				if (it.cat_ != 0 && it.cat_ != 1 && it.cat_ != 2) {
					i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_activity_list table! pid:%d, goods cat invalid:%d", inf.activity_id_, it.cat_);
					continue;
				}
				inf.item_.push_back(it);
			}

			//是否延迟到账
			inf.min_buy_num_ = q.getval();
			inf.max_buy_num_ = q.getval();
			inf.begin_time_ = q.getbigint();
			inf.end_time_ = q.getbigint();

			glb_activity_data.insert(std::make_pair(inf.activity_id_, inf));
		}
	});
}

void		load_all_lucky_dial()
{
	std::string sql = "select `lucky_id`, `probability`, `prize_id`, `prize_num`, `limit_sys_num`, `limit_user_num`, `limit_second`, `max_lucky_value` from setting_lucky_dial";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		glb_lucky_dial_data.clear();
		int biggest_count = 0;
		while (q.fetch_row())
		{
			lucky_dial_info inf;
			inf.lucky_id_ = q.getval();
			inf.probability_ = q.getval();
			inf.prize_id_ = q.getval();
			inf.prize_num_ = q.getval();
			inf.limit_sys_num_ = q.getval();
			inf.limit_user_num_ = q.getval();
			inf.limit_second_ = q.getval();
			inf.max_lucky_value_ = q.getval();

			if (inf.max_lucky_value_ > 0) {
				biggest_count++;
			}

			glb_lucky_dial_data.insert(std::make_pair(inf.lucky_id_, inf));
		}

		if (biggest_count != 1) {
			i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_lucky_dial table! biggest prize count=%d", biggest_count);
		}
	});
}

void	load_system_prize()
{
	std::string sql = "select `prize_name`, `key`, `pid`, `prize` from setting_system_prize";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		glb_system_prize.clear();
		glb_present_bag.clear();
		while (q.fetch_row())
		{
			system_prize inf;
			inf.prize_name_ = q.getstr();
			inf.key_ = q.getval();
			inf.pid_ = q.getval();
			std::string prize_str = q.getstr();

			std::vector<int> prize;
			split_str(prize_str, ",", prize, true);
			if (prize.size() % 3 != 0) {
				i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_system_prize table! prize count=%d", prize.size());
			}

			for (int i = 0; i <= int(prize.size() - 3); i += 3)
			{
				item_unit one;
				one.cat_ = prize[i];
				one.item_id_ = prize[i + 1];
				one.item_num_ = prize[i + 2];

				if (one.item_id_ >= 0 && one.item_num_ > 0) {
					inf.prize_.push_back(one);
				}
			}

			glb_present_bag.insert(std::make_pair(inf.pid_, inf.prize_));
			glb_system_prize[inf.prize_name_].insert(std::make_pair(inf.key_, inf));
		}
	});
}

int get_vip_level(int vip_value_, int vip_limit_)
{
	auto conf = glb_vip_privilege;
	int vip_level = 0;
	for (auto it = conf.begin(); it != conf.end(); it++)
	{
		if (vip_value_ >= it->second.vip_value_) {
			vip_level = it->first;
		}
		else {
			break;
		}
	}

	if (vip_level > vip_limit_) {
		vip_level = vip_limit_;
	}

	return vip_level;
}

int do_vip_change(std::string uid)
{
	std::string sql = "select nickname, vip_value, vip_limit from user_account where uid = '" + uid + "'";
	db_delay_helper_.get_result(sql, [uid](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		std::string nickname;
		int vipvalue = 0, viplimit = 0;
		query_result_reader q(vret[0]);
		if (q.fetch_row()) {
			nickname = q.getstr();
			vipvalue = q.getval();
			viplimit = q.getval();

			int viplevel = 0;
			player_ptr pp = get_player(uid);
			if (pp) {
				pp->vip_value_ = vipvalue;
				pp->vip_limit_ = viplimit;
				pp->nickname_ = nickname;

				viplevel = pp->vip_level();
			}
			else {
				viplevel = get_vip_level(vipvalue, viplimit);
			}

			
			std::string sql = "select lucky_ticket, date(last_login_time) = date(now()) from user_last_login_time where uid = '" + uid + "' and date(`last_login_time`) = date(now())";
			db_delay_helper_.get_result(sql, [uid, nickname, viplevel](bool result, const std::vector<result_set_ptr>& vret) {
				if (!result || vret.empty()) return;
				int today_lucky_ticket = 0;	//今日已发回馈券数
				int is_today_login = 0;
				query_result_reader q(vret[0]);
				if (q.fetch_row()) {
					today_lucky_ticket = q.getval();
					is_today_login = q.getval();

					if (!is_today_login) {
						today_lucky_ticket = 0;
					}
				}

				if (today_lucky_ticket < glb_vip_privilege[viplevel].lucky_dial_times_) {
					int remain_num = glb_vip_privilege[viplevel].lucky_dial_times_ - today_lucky_ticket;

					user_mail mail;
					mail.uid_ = uid;
					mail.mail_type_ = 0;
					mail.title_type_ = 5;
					mail.title_ = "这里有一份VIP奖励需求您领取哦！";
					mail.content_ = "尊敬的VIP" + lx2s(viplevel) + "级用户“" + boost::locale::conv::between(nickname, "GB2312", "UTF-8") + "”：\n\t\t您好！小k为您送上您今日的VIP登录奖励，请及时查收哦！";
					mail.state_ = 0;
					mail.attach_state_ = 1;
					std::vector<int> vec;
					vec.push_back(item_id_lucky_ticket);
					vec.push_back(remain_num);
					mail.attach_ = combin_str(vec, ",");
					mail.save_days_ = 30;
					send_mail(mail, false);

					//更新状态
					{
						if (is_today_login) {
							BEGIN_INSERT_TABLE("user_last_login_time");
							SET_FIELD_VALUE("uid", uid);
							SET_FINAL_VALUE("lucky_ticket", remain_num);
							WITH_END_CONDITION("on duplicate key update last_login_time = now(), lucky_ticket = lucky_ticket + " + lx2s(remain_num));
							EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
						}
						else {
							BEGIN_INSERT_TABLE("user_last_login_time");
							SET_FIELD_VALUE("uid", uid);
							SET_FINAL_VALUE("lucky_ticket", remain_num);
							WITH_END_CONDITION("on duplicate key update last_login_time = now(), lucky_ticket = " + lx2s(remain_num));
							EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
						}
					}
				}
			});
		}
	});
	
	return error_success;
}

int do_head_change(std::string uid)
{
	player_ptr pp = get_player(uid);
	if (pp) {
		std::string head_ico;
		int headframe_id;
		Query q(*db_);
		std::string sql = "select headpic_name, headframe_id from user_account where uid = '" + uid + "'";
		if (q.get_result(sql) && q.fetch_row()) {
			head_ico = q.getstr();
			headframe_id = q.getval();
			q.free_result();

			pp->head_ico_ = head_ico;
			pp->headframe_id_ = headframe_id;
		}
	}

	return error_success;
}

template<class socket_t>
void	broadcast_room_msg(int gameid, msg_base<socket_t> &msg);
extern std::map<int, std::map<std::string, priv_game_channel>> private_game_rooms;
void check_game_server()
{
	auto it = registed_servers.begin();
	while (it != registed_servers.end())
	{
		std::map<std::string, internal_server_inf>& mpv = it->second;
		int gameid_ = it->first;
		it++;
		auto it1 = mpv.begin();
		while (it1 != mpv.end())
		{
			internal_server_inf& srv = it1->second;
			std::string instance_ = it1->first; it1++;

			{
				msg_ping<none_socket> msg;
				if (srv.wsock_.lock()) {
					int ret = send_msg(srv.wsock_.lock(), msg, false, true);
					if (ret == error_success) {
						continue;
					}
				}
			}

			if (gameid_ >= 0) {
				if (registed_servers[gameid_].find(instance_) != registed_servers[gameid_].end()){
					registed_servers[gameid_].erase(instance_);
					i_log_system::get_instance()->write_log(loglv_info, "game server removeed:%d, instance:%s",
						gameid_,
						instance_.c_str());
				}
			}

			auto itp = private_game_rooms.find(gameid_);
			if (itp != private_game_rooms.end()) {
				std::map<std::string, priv_game_channel>& mp = itp->second;
				priv_game_channel& chn = mp[instance_];
				auto itr = chn.rooms_.begin();
				while (itr != chn.rooms_.end())
				{
					msg_private_room_remove msg;
					msg.gameid_ = gameid_;
					msg.roomid_ = itr->second->room_id_;
					broadcast_room_msg(itr->second->game_id_, msg);
					itr++;
				}
				mp.clear();
			}
			it1++;
		}
	}
}

extern int		load_all_channel_servers(std::string  server_tag_, bool sync);
int		do_run_dbthread()
{
	int idle = 0;
	time_counter ts;
	ts.restart();
	std::string servertag = the_config.server_tag_;
	channel_servers.reset(new std::vector<channel_server>());
	load_all_channel_servers(servertag, true);
	while (!glb_exit)
	{
		bool busy = false;
		if (!db_delay_helper_.pop_and_execute())
			idle++;
		else { idle = 0; }

		if (ts.elapse() > 2000) {
			ts.restart();
			load_all_channel_servers(servertag, true);
		}

		function_time_counter ftc("db_thread_func", 500);
		busy = db_thread_func();

		boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
	}
	return 0;

}
int		run_dbthread()
{
	int ret = 0;
	__try {
		ret = do_run_dbthread();
	}
	__except (MyUnhandledExceptionFilter(GetExceptionInformation()),
		EXCEPTION_EXECUTE_HANDLER) {
		glb_exit = true;
	}
	return ret;
}

int		run_monitor()
{
	int idle = 0;
	while (!glb_exit)
	{
		for (int i = 0; i < 10; i++)
		{
			if (performance_counter[i] > 1000) {
				i_log_system::get_instance()->write_log(loglv_warning, "module %d stucked for %d ms.", i, performance_counter[i].load());
				break;
			}
		}
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	}
	return 0;
}

int fps_counter = 0, fps = 0;
int run()
{
	time_counter ts, ts2;
	i_log_system::get_instance()->start();
	glb_exit = false;
	timer_srv.reset(new boost::asio::io_service());
	the_net.reset(new net_server<koko_socket>());
	the_internal_net.reset(new net_server<game_srv_socket>());
	channel_servers.reset(new std::vector<channel_server>());
	boost::system::error_code ec;
	the_config.load_from_file();

	db_.reset(new utf8_data_base(the_config.accdb_host_, the_config.accdb_user_,the_config.accdb_pwd_, the_config.accdb_name_));
	if(!db_->grabdb()){
		i_log_system::get_instance()->write_log(loglv_warning, "database start failed!!!!");
		return -1;
	}

	db_delay_helper_.set_db_ptr(db_);

	load_bots();
	load_auto_chat();
	load_chat_emotion();
	load_all_present();
	load_all_item();
	load_all_activity();
	load_vip_privilege();
	load_system_prize();
	load_all_lucky_dial();

	if (start_network() != 0){
		goto _EXIT;
	}

	int idle_count = 0;

	task_on_5sec* ptask = new task_on_5sec(*timer_srv);
	task.reset(ptask);
	ptask->schedule(2000);
	ptask->routine();

	monitor_thread.reset(new boost::thread(run_monitor));
	dbthread_.reset(new boost::thread(run_dbthread));

	ts.restart(); ts2.restart();
	while(!glb_exit)
	{
		bool busy = false;
		fps_counter++;
		for (int i = 0; i < 12; i++)
		{
			performance_counter[i] = 0;
		}
		{
			function_time_counter ftc("transaction_controller::get_instance()->step()", 20);
			busy = (transaction_controller::get_instance()->step() > 0);
		}
		{
			function_time_counter ftc("db_delay_helper_.poll()", 20);
			db_delay_helper_.poll();
		}
		if (ts2.elapse() > 1000) {
			std::string str;
			for each (auto& itg in registed_servers)
			{
				if (!itg.second.empty()) {
					str += lx2s(itg.first) + ",";
				}
			}

			ts2.restart();
			Database& db = *db_;
			BEGIN_REPLACE_TABLE("system_server_auto_balance");
			SET_FIELD_VALUE("srvid", the_config.public_srvid_);
			SET_FIELD_VALUE("name", "unset");
			SET_FIELD_VALUE("public_ip", the_config.public_ip_);
			SET_FIELD_VALUE("port", the_config.client_port_);
			SET_FIELD_VALUE("for_device", the_config.for_device_);
			SET_FIELD_VALUE("gameid", str);
			SET_FIELD_VALUE("server_group", the_config.server_tag_);
			SET_FINAL_VALUE("online_user", online_players.size());
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}
		
		boost::posix_time::ptime p1 = boost::posix_time::microsec_clock::local_time();
		{
			function_time_counter ftc("timer_srv->poll()", 20);
			timer_srv->reset();
			timer_srv->poll();
		}
		

		performance_counter[0] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("the_net->poll()", 20);
			the_net->ios_.reset();
			the_net->ios_.poll();
		}

		performance_counter[1] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("the_internal_net->poll()", 20);
			the_internal_net->ios_.reset();
			the_internal_net->ios_.poll();
		}
		

		performance_counter[2] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("handle_pending_logins", 20);
			handle_pending_logins();
		}

		performance_counter[3] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("pickup_player_msgs", 20);
			pickup_player_msgs(busy);
		}

		performance_counter[4] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("handle_player_msgs", 20);
			handle_player_msgs();
		}

		performance_counter[5] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("update_players", 20);
			update_players();
		}

		std::map<int, int> handled;
		performance_counter[6] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		{
			function_time_counter ftc("handle_internal_msgs", 20);
			pickup_internal_msgs(busy);
			handle_internal_msgs(handled);
		}
		
		performance_counter[7] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		smsg_ptr pmsg;
		broadcast_msg_.pop_front(pmsg);
		if(pmsg.get())
			broadcast_msg(pmsg);

		performance_counter[8] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();

		auto it = glb_trades.begin();
		while (it != glb_trades.end())
		{
			if (it->second->tc_.elapse() > 2000 &&
				it->second->trade_state_ != 0){
				it->second->time_out();
			}
			//失败了
			if (it->second->trade_state_ == 3){
				auto porder = it->second;
				i_log_system::get_instance()->write_log(loglv_debug,
					"msg_koko_trade_inout failed. dir = %d, uid = %s, sn = %s, count = %lld, ret = %lld",
					porder->msg->dir_,
					porder->msg->uid_.c_str(),
					porder->msg->sn_.c_str(),
					porder->msg->count_,
					porder->value_);

				porder->do_rollback();
				it = glb_trades.erase(it);
			}
			//成功了
			else if (it->second->trade_state_ == 2)	{
				auto porder = it->second;
				i_log_system::get_instance()->write_log(loglv_debug,
					"msg_koko_trade_inout complete. dir = %d, uid = %s, sn = %s, count = %lld, ret = %lld",
					porder->msg->dir_,
					porder->msg->uid_.c_str(),
					porder->msg->sn_.c_str(),
					porder->msg->count_,
					porder->value_);

				it = glb_trades.erase(it);
			}
			else {
				it++;
			}
		}

		{
			function_time_counter ftc("handle_thread_send_cli_msg", 20);
			performance_counter[9] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
			handle_thread_send_cli_msg();

			performance_counter[10] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
			handle_thread_send_srv_msg();
		}

		performance_counter[11] = (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds();
		if (performance_counter[11] > 500){
			i_log_system::get_instance()->write_log(loglv_warning, "big main loop cost:%dms [0]:%d [1]:%d [2]:%d [3]:%d [4]:%d [5]:%d [6]:%d [7]:%d [8]:%d [9]:%d [10]:%d [11]:%d",
				performance_counter[11].load(),
				performance_counter[0].load(),
				performance_counter[1].load(),
				performance_counter[2].load(),
				performance_counter[3].load(),
				performance_counter[4].load(),
				performance_counter[5].load(),
				performance_counter[6].load(),
				performance_counter[7].load(),
				performance_counter[8].load(),
				performance_counter[9].load(),
				performance_counter[10].load(),
				performance_counter[11].load());

			int nz = std::accumulate(koko_channels.begin(), koko_channels.end(), 0,
				[](int val, const std::pair<int, channel_ptr>& it)->int {
				return it.second->players_.size() + val;
			});

			i_log_system::get_instance()->write_log(loglv_info, "bots:%d, online:%d srv: %d "
				"glb_running_task_: %d "
				"koko_channels: %d "
				"channels_players: %d ",
				free_bots_.size(),
				online_players.size(),
				registed_servers.size(),
				task_info::glb_running_task_.size(),
				koko_channels.size(),
				nz);

			std::string str;
			auto it = handled.begin();
			while (it != handled.end())
			{
				str += lx2s<int>(it->first) + "->" + lx2s<int>(it->second);
				it++;
			}
			i_log_system::get_instance()->write_log(loglv_info, "internal msg handled:%s", str.c_str());

		}

		if (!busy){
			boost::posix_time::milliseconds ms(1);
			boost::this_thread::sleep(ms);
		}

		int elp = ts.elapse();
		if (elp > 1000){
			fps = fps_counter * 1000 / (ts.elapse());
			fps_counter = 0;
			ts.restart();
		}
	}
_EXIT:
	if (monitor_thread.get()){
		monitor_thread->join();
	}
	monitor_thread.reset();
	if (dbthread_){
		dbthread_->join();
	}
	dbthread_.reset();
	transaction_controller::get_instance()->stop();
	i_log_system::get_instance()->stop();
	return 0;
}

static int dmp = 0;
LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	HANDLE lhDumpFile = CreateFileA(("crash" + boost::lexical_cast<std::string>(dmp++) + ".dmp").c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL ,NULL);

	MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
	loExceptionInfo.ExceptionPointers = ExceptionInfo;
	loExceptionInfo.ThreadId = GetCurrentThreadId();
	loExceptionInfo.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),lhDumpFile, MiniDumpWithFullMemory, &loExceptionInfo, NULL, NULL);

	CloseHandle(lhDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main()
{
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE)
	{
		return -1;
	}
	__try{
		if (run() == 0){
			cout <<"exited!"<< endl;
		}
		else
			cout <<"exited with error!"<< endl;
 	}
	__except(MyUnhandledExceptionFilter(GetExceptionInformation()),
		EXCEPTION_EXECUTE_HANDLER){

 	}
	clean();
	return 0;
}
extern int		load_all_channel_servers(std::string  server_tag_, bool sync);
int task_on_5sec::routine()
{
	msg_handled = 0;
	msg_recved = 0;
	unsended_data = 0;

	load_all_games();
	load_all_present();
	load_all_item();
	load_all_activity();
	load_vip_privilege();
	load_system_prize();
	load_all_lucky_dial();
	load_all_channel_servers(the_config.server_tag_, false);

	{
		std::string sql = "select `id`, `game_type`, `uid`, `description` from used_v2_radio order by `id`";
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			int maxid = 0;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				maxid = q.getval();
				int gameid = q.getval();
				std::string uid = q.getstr();
				std::string cont = q.getstr();

				boost::shared_ptr<msg_chat_deliver>msg(new msg_chat_deliver());
				msg->channel_ = gameid;
				msg->msg_type_ = 4;
				msg->content_ = cont;

				if (gameid > 0) {
					auto it = koko_channels.find(gameid);
					if (it != koko_channels.end()) {
						it->second->broadcast_msg(*msg);
					}
				}
				else {
					if (!uid.empty()) {
						player_ptr pp = get_player(uid);
						if (pp.get() && pp->from_socket_.lock()) {
							send_protocol(pp->from_socket_.lock(), *msg);
						}
					}
					else {
						broadcast_msg(msg);
					}
				}
			}

			{
				BEGIN_DELETE_TABLE("used_v2_radio");
				WITH_END_CONDITION("where `id` <= " + lx2s(maxid));
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
		});
	}

	//同步用户货币变化
	{
		std::string sql = "select rowid, uid, itemid from user_item_changes "
		"where for_server = '" + the_config.public_srvid_ + "'  order by rowid asc";
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			std::vector<__int64> v;
			while (q.fetch_row())
			{
				__int64 rowid = q.getbigint();
				std::string uid = q.getstr();
				int	itemid = q.getval();
				//如果用户存在,则更新
				if (get_player(uid).get()) {
					sync_user_item(uid, itemid);
				}

				if (itemid == item_id_vipvalue || itemid == item_id_viplimit) {
					do_vip_change(uid);
				}
				else if (itemid == item_id_head || itemid == item_id_headframe) {
					do_head_change(uid);
				}

				msg_koko_user_data_change msg;
				msg.itemid_ = itemid;
				msg.uid_ = uid;

				for (auto it : registed_servers)
				{
					std::map<std::string, internal_server_inf>& mp = it.second;
					for (auto it1 : mp)
					{
						if (it1.second.wsock_.lock()) {
							send_protocol(it1.second.wsock_.lock(), msg);
						}
					}
				}

				v.push_back(rowid);

				if (v.size() > 100) {
					std::string rowids = combin_str(v, ",", false);
					BEGIN_DELETE_TABLE("user_item_changes");
					WITH_END_CONDITION("where `rowid` in (" + rowids + ")");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
					v.clear();
				}
			}

			if (v.size() > 0) {
				std::string rowids = combin_str(v, ",", false);
				BEGIN_DELETE_TABLE("user_item_changes");
				WITH_END_CONDITION("where `rowid` in (" + rowids + ")");
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
		});
	}

	//广播系统消息
	{
		std::string sql = "select rowid, content, msg_type, from_uid, from_nickname, from_tag "
			"from system_allserver_broadcast "
			"where for_server= '" + the_config.public_srvid_ + "' order by rowid";
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			int maxid = -1;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				int rowid = q.getval();
				if (maxid < rowid) {
					maxid = rowid;
				}
				std::string content = q.getstr();
				int	msg_tp = q.getval();
				std::string from_uid = q.getstr();
				std::string from_nick = q.getstr();
				int fromtag = q.getval();

				msg_chat_deliver* msg = new msg_chat_deliver();
				if (msg_tp == 0) {
					msg->channel_ = -1;
					msg->msg_type_ = 2;
				}
				else {
					msg->channel_ = -1;
					msg->msg_type_ = 0;
					msg->from_uid_ = from_uid;
					msg->nickname_ = from_nick;
					msg->from_tag_ = fromtag;
				}

				msg->content_ = content.c_str();
				msg->time_stamp_ = ::time(nullptr);
				broadcast_msg_.push_back(smsg_ptr(msg));
			}

			if (maxid >= 0) {
				BEGIN_DELETE_TABLE("system_allserver_broadcast");
				WITH_END_CONDITION("where `rowid` <= " + lx2s(maxid) + " and `for_server` = '" + the_config.public_srvid_ + "'");
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
		});
	}

	//删除过期的系统邮件
	{
		std::string sql = "select id from user_mail where uid = 'system' and expire_time != 0 and expire_time < now()";
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			std::vector<int> v;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				int id = q.getval();
				v.push_back(id);
			}

			if (v.size() > 0) {
				std::string ids = combin_str(v, ",", false);
				{
					BEGIN_DELETE_TABLE("user_mail");
					WITH_END_CONDITION("where `id` in (" + ids + ")");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}

				{
					BEGIN_DELETE_TABLE("user_mail_slave");
					WITH_END_CONDITION("where `id` in (" + ids + ")");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}
				v.clear();
			}
		});
	}

	//删除过期的玩家邮件(最多保留30天)
	{
		BEGIN_DELETE_TABLE("user_mail");
		WITH_END_CONDITION("where `uid` != 'system' and `attach_state` != 1 and unix_timestamp(now()) - unix_timestamp(`create_time`) > 30 * 24 * 3600");
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}

	//新邮件通知
	{
		std::string sql = "select rowid, mail_id from user_mail_system_notice where for_server= '" + the_config.public_srvid_ + "' order by rowid asc";
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			std::vector<int> v;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				int rowid = q.getval();
				int id = q.getval();

				//推送邮件
				{
					std::string sql = "select id, uid, mail_type, title_type, title, state, attach_state, unix_timestamp(create_time) from user_mail where id = " + lx2s(id);
					if (!result) return;
					db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
						query_result_reader q2(vret[0]);
						if (q2.fetch_row()) {
							std::string uid;
							boost::shared_ptr<msg_user_mail_list> pmsg(new msg_user_mail_list());
							pmsg->id_ = q2.getval();
							uid = q2.getstr();
							pmsg->mail_type_ = q2.getval();
							pmsg->title_type_ = q2.getval();
							pmsg->title_ = q2.getstr();
							pmsg->state_ = q2.getval();
							pmsg->attach_state_ = q2.getval();
							pmsg->timestamp_ = q2.getbigint();

							//推送邮件
							if (uid == "system") {
								broadcast_msg(pmsg);
							}
							else if (!uid.empty()) {
								auto pp = get_player(uid);
								if (pp && pp->from_socket_.lock().get()) {
									send_protocol(pp->from_socket_.lock(), *pmsg);
								}
							}
						}
					});
				}

				v.push_back(rowid);
			}

			if (v.size() > 0) {
				std::string rowids = combin_str(v, ",", false);
				{
					BEGIN_DELETE_TABLE("user_mail_system_notice");
					WITH_END_CONDITION("where `rowid` in (" + rowids + ")");
					EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
				}
			}
		});
	}

	check_game_server();

	i_log_system::get_instance()->write_log(loglv_info, "service is running ok. fps = %d", fps);
	return task_info::routine_ret_continue;
}
