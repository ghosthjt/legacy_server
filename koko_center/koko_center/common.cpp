#include "msg.h"
#include "error.h"
#include "boost/lexical_cast.hpp"
#include "msg_server.h"
#include "player.h"
#include "net_service.h"
#include "Database.h"
#include "Query.h"
#include "utf8_db.h"
#include "koko_socket.h"
#include "utility.h"
#include "db_delay_helper.h"
#include "md5.h"
#include "boost/thread.hpp"


extern db_delay_helper	db_delay_helper_;
typedef boost::shared_ptr<msg_base<none_socket>> smsg_ptr;					//发送给客户端的消息类型
extern safe_list<std::pair<koko_socket_ptr, smsg_ptr>> glb_thread_send_cli_msg_lst_;

extern player_ptr get_player(std::string uid);
extern boost::shared_ptr<utf8_data_base>	db_;
extern db_delay_helper& get_delaydb();
extern void send_msg_to_uid(std::string uid, msg_base<none_socket>& msg);

boost::thread_specific_ptr<std::vector<channel_server>> channel_servers;

std::map<int, user_privilege>	glb_vip_privilege;

std::vector<std::string>	all_channel_servers()
{
	std::vector<std::string> ret;
	if (!channel_servers.get()){
		return ret;
	}

	auto& chn_srvs = *channel_servers;
	for (int i = 0; i < chn_srvs.size(); i++)
	{
		ret.push_back(chn_srvs[i].instance_);
	}
	return ret;
}

int		load_all_channel_servers(std::string  server_tag_, bool sync)
{
	std::string sql = "select srvid, public_ip, port, name, for_device, online_user, gameid"
		" from system_server_auto_balance "
		" where unix_timestamp(update_time) > (unix_timestamp() - 6)"
		" and server_group = '" + server_tag_ + "'";

	if (!sync){
		db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
			channel_servers->clear();

			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				channel_server srv;
				srv.instance_ = q.getstr();
				srv.ip_ = q.getstr();
				srv.port_ = q.getval();
				q.getstr();
				srv.device_type_ = q.getstr();
				srv.online_ = q.getval();
				std::string ids = q.getstr();
				split_str(ids, ",", srv.game_ids_, true);
				if (srv.game_ids_.empty()) {
					srv.game_ids_.push_back(-1);
				}
				channel_servers->push_back(srv);
			}
		});
	}
	else {
		channel_servers->clear();
		Query q(*db_);
		if (!q.get_result(sql)) return 0;
		while (q.fetch_row())
		{
			channel_server srv;
			srv.instance_ = q.getstr();
			srv.ip_ = q.getstr();
			srv.port_ = q.getval();
			q.getstr();
			srv.device_type_ = q.getstr();
			srv.online_ = q.getval();
			std::string ids = q.getstr();
			split_str(ids, ",", srv.game_ids_, true);
			if (srv.game_ids_.empty()) {
				srv.game_ids_.push_back(-1);
			}
			channel_servers->push_back(srv);
		}
	}
	return 0;
}

int			sync_user_item(std::string uid, int itemid)
{
	std::string sql;
	if (itemid == item_id_gold){
		sql += "call update_gold('localcall','"+ uid +"'," 
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(0) + ", @ret);";
		sql += "select @ret;";
	}
	else if (itemid == item_id_gold_game){
		sql += "call update_gold('localcall','"+ uid +"'," 
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(1) + ","
			+ boost::lexical_cast<std::string>(0) + ", @ret);";
		sql += "select @ret;";
	}
	else if (itemid == item_id_gold_free){
		sql += "call update_gold('localcall','"+ uid +"'," 
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(2) + ","
			+ boost::lexical_cast<std::string>(0) + ", @ret);";
		sql += "select @ret;";
	}
	else if (itemid == item_id_gold_bank){
		sql += "call update_gold('localcall','"+ uid +"'," 
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(4) + ","
			+ boost::lexical_cast<std::string>(0) + ", @ret);";
		sql += "select @ret;";
	}
	else if (itemid == item_id_gold_game_bank){
		sql += "call update_gold('localcall','"+ uid +"'," 
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(5) + ","
			+ boost::lexical_cast<std::string>(0) + ", @ret);";
		sql += "select @ret;";
	}

	else if (itemid == item_id_gold_game_lock){
		sql = "select gold_game_lock from user_account where uid = '" + boost::lexical_cast<std::string>(uid) + "'";
	}
	else if (itemid == item_id_vipvalue) {
		sql = "select vip_value from user_account where uid = '" + uid + "'";
	}
	else if (itemid == item_id_viplimit) {
		sql = "select vip_limit from user_account where uid = '" + uid + "'";
	}
	else{
		sql = "call update_user_item('localcall','"+ uid +"'," 
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(itemid) + ",@ret);";
		sql += "select @ret;";
	}

	db_delay_helper_.get_result(sql, [uid, itemid](bool result, const std::vector<result_set_ptr> &vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		if (!q.fetch_row()) return;

		__int64 ret = q.getbigint();
		if (ret >= 0) {
			msg_sync_item msg;
			msg.item_id_ = itemid;
			msg.count_ = ret;
			send_msg_to_uid(uid, msg);
		}
	});
	return error_success;
}


std::string build_sql(std::string sn,
	std::string uid, int op,
	int itemid, __int64 count)
{
	std::string sql;
	if (itemid == item_id_gold) {
		sql += " call update_gold('" + sn + "','" + uid + "',"
			+ boost::lexical_cast<std::string>(op) + ","
			+ boost::lexical_cast<std::string>(0) + ","
			+ boost::lexical_cast<std::string>(count) + ", @ret);";
		sql += "select @ret;";
	}
	else if (itemid == item_id_gold_game) {
		sql += " call update_gold('" + sn + "','" + uid + "',"
			+ boost::lexical_cast<std::string>(op) + ","
			+ boost::lexical_cast<std::string>(1) + ","
			+ boost::lexical_cast<std::string>(count) + ", @ret);";
		sql += "select @ret; ";
	}
	else if (itemid == item_id_gold_free) {
		sql += " call update_gold('" + sn + "','" + uid + "',"
			+ boost::lexical_cast<std::string>(op) + ","
			+ boost::lexical_cast<std::string>(2) + ","
			+ boost::lexical_cast<std::string>(count) + ", @ret);";
		sql += "select @ret; ";
	}
	else if (itemid == item_id_gold_bank) {
		sql += " call update_gold('" + sn + "','" + uid + "',"
			+ boost::lexical_cast<std::string>(op) + ","
			+ boost::lexical_cast<std::string>(4) + ","
			+ boost::lexical_cast<std::string>(count) + ", @ret);";
		sql += "select @ret; ";
	}
	else if (itemid == item_id_gold_game_bank) {
		sql += " call update_gold('" + sn + "','" + uid + "',"
			+ boost::lexical_cast<std::string>(op) + ","
			+ boost::lexical_cast<std::string>(5) + ","
			+ boost::lexical_cast<std::string>(count) + ", @ret);";
		sql += "select @ret; ";
	}
	else {
		sql = " call update_user_item('" + sn + "','" + uid + "',"
			+ boost::lexical_cast<std::string>(op) + ","
			+ boost::lexical_cast<std::string>(count) + ","
			+ boost::lexical_cast<std::string>(itemid) + ",@ret);";
		sql += "select @ret;  ";
	}
	return sql;
}

void save_item_update_result(std::string uid, int op, int itemid,
	std::vector<std::string> sync_to_server, __int64 ret,
	std::string reason, __int64 count)
{
	reason = ::utf8(reason);
	{
		BEGIN_REPLACE_TABLE("user_item_changes");
		SET_FIELD_VALUE("uid", uid);
		SET_FIELD_VALUE("for_server", "center");
		SET_FINAL_VALUE("itemid", itemid);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	for (unsigned i = 0; i < sync_to_server.size(); i++)
	{
		BEGIN_REPLACE_TABLE("user_item_changes");
		SET_FIELD_VALUE("uid", uid);
		SET_FIELD_VALUE("for_server", sync_to_server[i]);
		SET_FINAL_VALUE("itemid", itemid);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}

	//扣全部钱时没有钱,则不记录.
	if (!(op == 2 && ret == 0)) {
		BEGIN_REPLACE_TABLE("log_player_gold_changes");
		SET_FIELD_VALUE("uid", uid);
		SET_FIELD_VALUE("op", op);
		SET_FIELD_VALUE("cid", itemid);
		SET_FIELD_VALUE("count", count);
		SET_FIELD_VALUE("reason", reason);
		SET_FINAL_VALUE("oret", ret);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
}

//op = 0 add,  1 cost, 2 cost all
//return -1 count < 0 ，-2 数值太大
int			update_user_item(std::string reason,
	std::string sn,
	std::string uid, int op, 
	int itemid, __int64 count, __int64& ret,
	std::vector<std::string> sync_to_server)
{
	std::string sql = build_sql(sn, uid, op, itemid, count);
	Query q(*db_);

	if (q.get_result(sql)){
		if (q.fetch_row()){
			ret = q.getbigint();
			if (ret >= 0){
				save_item_update_result(uid, op, itemid, sync_to_server, ret, reason, count);
				q.free_result();
				return error_success;
			}
			else{
				q.free_result();
				return (int)ret;
			}
		}
		else{
			return error_success;
		}
	}
	else{
		return error_sql_execute_error;
	}
}

void update_user_item_async(std::string reason,
	std::string sn,
	std::string uid, int op,
	int itemid, __int64 count, std::function<void(int, __int64)> cb,
	std::vector<std::string> sync_to_server)
{
	std::string sql = build_sql(sn, uid, op, itemid, count);

	auto fn = [uid, op, itemid, sync_to_server, cb, reason, count]
	(bool result, const std::vector<result_set_ptr> &vresult) {
		if (!result || vresult.empty()) {
			cb(-1, 0);
		}
		query_result_reader q(vresult[0]);
		if (q.fetch_row()) {
			__int64 ret = q.getbigint();
			if (ret >= 0) {
				cb(0, ret);
				save_item_update_result(uid, op, itemid, sync_to_server, ret, reason, count);
			}
			else {
				cb(ret, 0);
			}
		}
		else {
			cb(0, 0);
		}
	};

	db_delay_helper_.get_result(sql, fn);
}

int			cost_gold(std::string reason, std::string uid, __int64 count, std::vector<std::string> sync)
{
	__int64 outc = 0;
	int ret = update_user_item(reason, "localcall", uid, item_operator_cost, item_id_gold_game, count, outc, sync);

	return ret;
}


int		 do_multi_costs(std::string reason, std::string uid, 
	std::map<int, int> cost, 
	std::vector<std::string> sync)
{
	int		errid = -1;
	auto it = cost.begin();
	std::map<int, int> succ;
	__int64 out_count = 0;
	int ret = error_success;
	while (it != cost.end())
	{
		ret |= update_user_item(reason, "localcall", uid, item_operator_cost, it->first, it->second, out_count, sync);
		if (ret == error_success){
			succ.insert(*it);
			it++;
		}
		else{
			errid = it->first;
			break;
		}
	}

	//出错了，回滚操作,返回失败
	if (ret != error_success){
		auto it2 = succ.begin();
		while (it2 != succ.end())
		{
			update_user_item(reason, "localcall", uid, item_operator_add, it2->first, it2->second, out_count, sync);
			it2++;
		}
	}

	if (ret != error_success && errid >= 0){
		if (errid == item_id_gold){
			ret = error_not_enough_gold;
		}
		else if (errid == item_id_gold_game){
			return error_not_enough_gold_game;
		}
	}
	return ret;
}

std::string			md5_hash(std::string sign_pat)
{
	CMD5 md5;
	unsigned char	out_put[16];
	md5.MessageDigest((unsigned char*)sign_pat.c_str(), sign_pat.length(), out_put);

	std::string sign_key;
	for (int i = 0; i < 16; i++)
	{
		char aa[4] = {0};
		sprintf_s(aa, 4, "%02x", out_put[i]);
		sign_key += aa;
	}
	return sign_key;
}

void	send_all_items_to_player(std::string uid, boost::shared_ptr<koko_socket> psock)
{
	sync_user_item(uid, item_id_gold);
	sync_user_item(uid, item_id_gold_game);
	sync_user_item(uid, item_id_gold_free);
	sync_user_item(uid, item_id_gold_game_lock);
	sync_user_item(uid, item_id_vipvalue);
	sync_user_item(uid, item_id_viplimit);

	//非时长物品
	{
		std::string sql = "select `item_id`, `count` from `user_item` where `uid` = '" + uid + "'"
			" and `item_id` in (select `item_id` from `setting_item_list` where `type` = '0')";
		db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				msg_sync_item msg;
				msg.item_id_ = q.getval();
				msg.count_ = q.getbigint();
				send_protocol(psock, msg);
			}
		});
	}
	
	//时长物品(排除过期物品)
	{
		std::string sql = "select `item_id`, `count` from `user_item` where `uid` = '" + uid + "'"
			" and `item_id` in (select `item_id` from `setting_item_list` where `type` = '1')" +
			" and unix_timestamp(`expire_time`) - unix_timestamp(now()) > 0";
		db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				boost::shared_ptr<msg_sync_item> psend(new msg_sync_item());
				psend->item_id_ = q.getval();
				psend->count_ = q.getbigint();
				send_protocol(psock, *psend);
			}

			{
				boost::shared_ptr<msg_sequence_send_over<none_socket>> psend(new msg_sequence_send_over<none_socket>());
				psend->cmd_ = GET_CLSID(msg_sync_item);
				send_protocol(psock, *psend);
			}
		});
	}
}

void	send_items_data_to_player(std::string uid, boost::shared_ptr<koko_socket> psock, int item_id = -1)
{
	//发送时长物品的剩余时长
	{
		std::string sql = "select `item_id`, unix_timestamp(`expire_time`) - unix_timestamp(now()) from `user_item` where `uid` = '" + uid + "'" + 
			" and unix_timestamp(`expire_time`) - unix_timestamp(now()) > 0";
		if (item_id != -1) {
			sql += (" and `item_id` = " + lx2s(item_id));
		}
		else {
			sql += " and `item_id` in (select `item_id` from `setting_item_list` where `type` = 1)";
		}

		db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				msg_sync_item_data msg;
				msg.item_id_ = q.getval();
				msg.key_ = 0;
				__int64 remain_sec = q.getval();
				if (remain_sec <= 0) {
					continue;
				}
				msg.value_ = lx2s(remain_sec);
				send_protocol(psock, msg);
			}
		});
	}
}

bool isbaned(std::string uid)
{
	Database& db = *db_;
	Query q(db);

	std::string sql;

	std::string machine_mark;
	sql = "select machine_mark from user_account where uid = '" + uid + "'";
	//如果账号没有被封,看机器码没有
	if (!q.get_result(sql))
		return true;

	if (q.fetch_row()) {
		machine_mark = q.getstr();
	}
	q.free_result();

	sql = "select 1 from user_blacklist where uid = '" + uid + "' and `state` = 1";
	if (!q.get_result(sql))
		return true;

	if (q.fetch_row()) {
		return true;
	}

	q.free_result();

	if (machine_mark == "") {
		sql = "select 1 from user_blacklist where uid = '" + uid + "' and `state` = 1";
	}
	else
		sql = "select 1 from user_blacklist where (uid = '" + uid + "' or machine_mark = '" + machine_mark + "') and `state` = 1";

	if (!q.get_result(sql))
		return true;

	if (q.fetch_row()) {
		return true;
	}

	return false;
}
void	send_items_to_player(std::string uid, boost::shared_ptr<koko_socket> psock, int item_id = -1)
{
	if (item_id >= 0 && item_id < 100) {
		sync_user_item(uid, item_id);
		return;
	}

	//非时长物品
	{
		std::string sql = "select `item_id`, `count` from `user_item` where `uid` = '" + uid + "'";
		if (item_id != -1) {
			sql += (" and `item_id` = " + lx2s(item_id));
		}
		else {
			sql += " and `item_id` in (select `item_id` from `setting_item_list` where `type` = '0')";
		}
		db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				msg_sync_item msg;
				msg.item_id_ = q.getval();
				msg.count_ = q.getbigint();
				send_protocol(psock, msg);
			}
		});
	}

	//发送时长物品的剩余时长
	{
		std::string sql = "select `item_id`, unix_timestamp(`expire_time`) - unix_timestamp(now()) from `user_item` where `uid` = '" + uid + "'" +
			" and unix_timestamp(`expire_time`) - unix_timestamp(now()) > 0";
		if (item_id != -1) {
			sql += (" and `item_id` = " + lx2s(item_id));
		}
		else {
			sql += " and `item_id` in (select `item_id` from `setting_item_list` where `type` = 1)";
		}

		db_delay_helper_.get_result(sql, [psock](bool result, const std::vector<result_set_ptr>& vret) {
			if (!result || vret.empty()) return;
			query_result_reader q(vret[0]);
			while (q.fetch_row())
			{
				msg_sync_item_data msg;
				msg.item_id_ = q.getval();
				msg.key_ = 0;
				__int64 remain_sec = q.getval();
				if (remain_sec <= 0) {
					continue;
				}
				msg.value_ = lx2s(remain_sec);
				send_protocol(psock, msg);
			}
		});
	}
}

void	load_vip_privilege()
{
	std::string sql = "select `vip_level`, `vip_value`, `login_prize_times`, `lucky_dial_times`, `send_present_limit` from setting_vip_privilege";
	db_delay_helper_.get_result(sql, [](bool result, const std::vector<result_set_ptr>& vret) {
		if (!result || vret.empty()) return;
		query_result_reader q(vret[0]);
		glb_vip_privilege.clear();
		while (q.fetch_row())
		{
			user_privilege inf;
			inf.vip_level_ = q.getval();
			inf.vip_value_ = q.getval();
			inf.login_prize_times_ = q.getval();
			inf.lucky_dial_times_ = q.getval();
			inf.send_present_limit_ = q.getbigint();

			glb_vip_privilege.insert(std::make_pair(inf.vip_level_, inf));
		}
	});
}