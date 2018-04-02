#include "msg_client.h"
#include "error.h"
#include "safe_list.h"
#include "player.h"
#include <unordered_map>
#include "msg_server.h"
#include "utility.h"
#include "server_config.h"
#include "Database.h"
#include "db_delay_helper.h"
#include "log.h"
#include "utf8_db.h"
#include "Query.h" 
#include "http_request.h"
#include "server_config.h"
#include "md5.h"
#include "i_log_system.h"

extern std::vector<std::string>	all_channel_servers();
extern std::string	md5_hash(std::string sign_pat);
typedef boost::shared_ptr<msg_base<koko_socket_ptr>> msg_ptr;
extern safe_list<msg_ptr>		sub_thread_process_msg_lst_;
extern player_ptr get_player(std::string uid);
extern service_config the_config;
extern db_delay_helper& get_delaydb();
extern boost::shared_ptr<utf8_data_base>	db_;
extern boost::thread_specific_ptr<std::vector<channel_server>> channel_servers;

extern int	update_user_item(std::string reason, 
	std::string sn,
	std::string uid, int op, 
	int itemid, __int64 count, __int64& ret,
	std::vector<std::string> sync_to_server);
extern std::map<int, present_info>	glb_present_data;
extern std::map<int, item_info> glb_item_data;
extern int	cost_gold(std::string reason, std::string uid, __int64 count,
	std::vector<std::string> sync_to_server);

int msg_user_login::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_info,  "handle user login msg, uid=%s", acc_name_.c_str());
	if(from_sock_->is_login_.load())
		return error_success;

	if (sub_thread_process_msg_lst_.size() > 100){
		return error_server_busy;
	}
	i_log_system::get_instance()->write_log(loglv_info, "user login comfirmed, uid=%s", acc_name_.c_str());
	msg_srv_progress msg;
	msg.pro_type_ = 0;
	msg.step_ = 0;
	send_protocol(from_sock_, msg);

	from_sock_->is_login_ = true;
	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));
	return error_success;
}

int msg_user_register::handle_this()
{
	if(from_sock_->is_register_.load())
		return error_success;

	if (sub_thread_process_msg_lst_.size() > 100){
		return error_server_busy;
	}

	std::string sign = ((std::string(acc_name_) + pwd_hash_) + machine_mark_) + the_config.signkey_;
	sign = md5_hash(sign);
	std::string rsign;
	rsign.insert(rsign.end(), sign_, sign_ + 32);
	if (sign != rsign){
		return error_server_busy;
	}

	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));

	from_sock_->is_register_ = true;
	return error_success;
}

int msg_change_account_info::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return error_cant_find_player;

	bool mail_used = false, mobile_used = false, nickname_used = false;
	if (pp->email_ != email_){
		std::string sql = "select * from user_account where mail = ('" + std::string(email_) + "')";
		Query q(*db_);
		if (q.get_result(sql) && q.fetch_row()){
			mail_used = true;
		}
		else{
			pp->email_ = email_;
		}
	}

	if (pp->nickname_ != nick_name_){
		std::string sql = "select * from user_account where nickname = trim('" + std::string(nick_name_) + "')";
		Query q(*db_);
		if (q.get_result(sql) && q.fetch_row()){
			nickname_used = true;
		}
		else{
			pp->nickname_ = nick_name_;
		}
	}

	pp->nickname_ = nick_name_;
	pp->byear_ = byear_;
	pp->bmonth_ = bmonth_;
	pp->bday_ = bday_;
	pp->age_ = age_;
	pp->address_ = address_;
	pp->idnumber_ = idcard_;
	pp->gender_ = gender_;
	pp->sec_mobile_ = mobile_;

	BEGIN_UPDATE_TABLE("user_account");

	if (!nickname_used){
		SET_FIELD_VALUE("nickname", nick_name_);
	}

	SET_FIELD_VALUE("byear", byear_);
	SET_FIELD_VALUE("bmonth", bmonth_);
	SET_FIELD_VALUE("bday", bday_);
	SET_FIELD_VALUE("age", age_);
	SET_FIELD_VALUE("address", address_);
	SET_FIELD_VALUE("addr_province", region1_);
	SET_FIELD_VALUE("addr_city", region2_);
	SET_FIELD_VALUE("addr_region", region3_);

	if (!pp->mobile_v_ && !mobile_used){
		SET_FIELD_VALUE("sec_mobile", mobile_);
	}
	
	if (!pp->email_v_ && !mail_used){
		SET_FIELD_VALUE("mail", email_);
	}

	SET_FIELD_VALUE("idnumber", idcard_);
	SET_FINAL_VALUE("gender", gender_);
	WITH_END_CONDITION("where uid = '" + pp->uid_ + "'")
	EXECUTE_NOREPLACE_DELAYED("", get_delaydb());

	msg_account_info_update msg;
	msg.gender_ = gender_;
	msg.byear_ = byear_;
	msg.bmonth_ = bmonth_;
	msg.bday_ = bday_;
	msg.address_ = address_;
	msg.nick_name_ = nick_name_;
	msg.age_ = age_;
	msg.mobile_ = pp->mobile_;
	msg.email_ = pp->email_;
	msg.idcard_ = idcard_;
	msg.region1_ = region1_;
	msg.region2_ = region2_;
	msg.region3_ = region3_;
	send_protocol(from_sock_, msg);
	
	if (mobile_used){
		return error_mobile_inusing;
	}
	else if (mail_used){
		return error_email_inusing;
	}
	else
		return error_success;
}

extern boost::shared_ptr<boost::asio::io_service> timer_srv;

void	on_verify_code_failed(koko_socket_ptr response_, http_context& contex)
{
	response_->mverify_code_.clear();
	response_->mverify_error_ = "http error:" + boost::lexical_cast<std::string>(contex.last_error_);
}

void	on_verify_code_data(koko_socket_ptr response_, http_context& contex)
{
	std::string		ret_code, content;
	std::string		pat = "\"errorCode\":\"";
	char* c = strstr((char*)contex.response_body_.c_str(), pat.c_str());
	//如果找到
	if (c){
		char* e = strstr(c + pat.length(), "\"");
		if (e){
			ret_code.insert(ret_code.end(), c + pat.length(), e);
		}
	}

	pat = "\"content\":\"";
	c = strstr((char*)contex.response_body_.c_str(), pat.c_str());
	//如果找到
	if (c){
		char* e = strstr(c + pat.length(), "\"");
		content.insert(content.end(), c + pat.length(), e);
	}

	if (ret_code == "10000"){
		response_->mverify_code_ = content;
		response_->mverify_time_ = boost::posix_time::microsec_clock::local_time();
	}
	else{
		response_->mverify_error_ = content;
		response_->mverify_code_.clear();
	}

}

extern service_config the_config;
int msg_get_verify_code::handle_this()
{
	if(type_ == 0){
		if (sub_thread_process_msg_lst_.size() > 100){
			return error_server_busy;
		}

		sub_thread_process_msg_lst_.push_back(
			boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));
	}
	else{
		boost::shared_ptr<http_request> req(new http_request(*timer_srv));
		char buff[128] = {0};
		if (type_ == 1)	{
			sprintf_s(buff, 128, "/kokoportal/user/sendCode/%s/2.htm", mobile_.c_str());
		}
		else{
			sprintf_s(buff, 128, "/kokoportal/user/sendCode/%s/1.htm", mobile_.c_str());
		}
		
		std::string request = build_http_request(the_config.shortmsg_host_, buff);
		req->request(request, the_config.shortmsg_host_, the_config.shortmsg_port_);
		auto psock = from_sock_;
		http_request_monitor::get_instance()->add_monitor(req,
			[psock](http_context & cont) {
			on_verify_code_data(psock, cont);
		}, 
			[psock](http_context & cont) {
			on_verify_code_failed(psock, cont);
		});
	}
	return error_success;
}

int func_check_verify_code(std::string vcode, koko_socket_ptr fromsock, bool is_check)
{
	if (is_check){
		if (fromsock->verify_code_.load() == ""){
			return error_wrong_verify_code;
		}
	}
	else{
		if (fromsock->verify_code_backup_.load() == ""){
			return error_wrong_verify_code;
		}
	}

	std::vector<std::string> v1, v2;
	split_str(vcode, ",", v1, true);
	std::string vc;
	if (is_check){
		vc = fromsock->verify_code_.load();
	}
	else{
		vc = fromsock->verify_code_backup_.load();
	}
	
	split_str(vc, ",", v2, true);

	auto it1 = v1.begin();
	while (it1 != v1.end())
	{
		auto it2 = v2.begin();
		bool	finded = false;
		while (it2 != v2.end())
		{
			if (*it1 == *it2){
				it2 = v2.erase(it2);
				finded = true;
			}
			else{
				it2++;
			}
		}
		if (finded){
			it1 = v1.erase(it1);
		}
		else{
			it1++;
		}
	}
	//必须v1和v2中的每一项都相同，验证码才能通过
	if (!v1.empty() || !v2.empty()){
	 return error_wrong_verify_code;
	}

	//每个图片验证码只能验证一次
	if (is_check){
		fromsock->verify_code_backup_.store(fromsock->verify_code_.load());
		fromsock->verify_code_.store("");
	}
	else{
		fromsock->verify_code_backup_.store("");
	}
	return error_success;
}

int msg_check_data::handle_this()
{
	Query q(*db_);

	char sdata[256] = {0};
	int ret = error_success;
	do {
		int len =	mysql_real_escape_string(&db_->grabdb()->mysql,sdata, query_sdata_.c_str(), strlen(query_sdata_.c_str()));

		if (query_type_ == check_account_name_exist){
			std::string sql = "select 1 from user_account where uname = '" + std::string(sdata) + "'";
			if (q.get_result(sql) && q.fetch_row()){
				ret = error_account_exist;
			}
		}
		else if (query_type_ == check_email_exist)	{
			std::string sql = "select 1 from user_account where mail = '" + std::string(sdata) + "'";
			if (q.get_result(sql) && q.fetch_row()){
				ret = error_email_inusing;
			}
		}
		else if (query_type_ == check_mobile_exist)	{
			std::string sql = "select 1 from user_account where mobile_number = '" + std::string(sdata) + "'";
			if (q.get_result(sql) && q.fetch_row()){
				ret = error_mobile_inusing;
			}
		}
		else if (query_type_ == check_verify_code){
			ret = func_check_verify_code(query_sdata_, from_sock_, true);
		}
		else if (query_type_ == check_mverify_code){
			if (from_sock_->mverify_code_ == ""){
				ret = error_wrong_verify_code;
			}
			else if(from_sock_->mverify_code_ != query_sdata_){
				ret = error_wrong_verify_code;
			}
			else if ((boost::posix_time::microsec_clock::local_time() - from_sock_->mverify_time_).total_seconds() > 120){
				ret = error_time_expired;
			}
		}
	} while (0);

	msg_check_data_ret msg;
	msg.query_type_ = query_type_;
	msg.result_ = ret;
	send_protocol(from_sock_, msg);

	return error_success;
}

static std::map<std::string, __int64> handled_trade;
extern std::string	md5_hash(std::string sign_pat);

int msg_action_record::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();

	Database& db(*db_);
	BEGIN_INSERT_TABLE("log_client_action");
	if (pp.get()){
		SET_FIELD_VALUE("uid", pp->uid_);
	}
	SET_FIELD_VALUE("action_id", action_id_);
	SET_FIELD_VALUE("action_data", action_data_);
	SET_FIELD_VALUE("action_data2", action_data2_);
	SET_FIELD_VALUE("action_data3", action_data3_);
	SET_FINAL_VALUE("from_ip", from_sock_->remote_ip());
	EXECUTE_IMMEDIATE();

	return error_success;
}

int msg_get_token::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;

	__int64 seq = 0;	::time(&seq);

	std::string token = md5_hash(pp->uid_ + boost::lexical_cast<std::string>(seq) + the_config.token_generate_key_);
	
	msg_sync_token msg;
	msg.token_ = token;
	msg.sequence_ = seq;
	send_protocol(from_sock_, msg);

	return error_success;
}

channel_server* get_server(int gameid, std::vector<channel_server>& v)
{
	static std::map<int, std::string> gameid_to_server;

	std::string last_game_server = gameid_to_server[gameid];

	for (int i = 0; i < v.size(); i++)
	{
		if (v[i].instance_ == last_game_server) {
			return &v[i];
		}
	}

	auto it = std::max_element(v.begin(), v.end(), 
		[](const channel_server& s1, const channel_server& s2)->bool {
		int h1 = std::hash_value(s1.instance_.c_str());
		int h2 = std::hash_value(s2.instance_.c_str());
		return h1 > h2;
	});

	channel_server& srv = *it;
	gameid_to_server[gameid] = srv.instance_;
	return &srv;
}

template<class socket_t>
int	distribute_channel_server(socket_t psock, int gameid, std::string device_type)
{
	if (!channel_servers.get() || channel_servers->empty()){
		i_log_system::get_instance()->write_log(loglv_error, "fetal error! distribute_channel_server failed, gameid = %d", gameid);
		return error_cant_find_coordinate;
	}

	auto chnsrv = *channel_servers;
	int total_games = 0;
	std::vector<channel_server> v;
	for (int i = 0; i < (int)chnsrv.size(); i++)
	{
		auto it = std::find(chnsrv[i].game_ids_.begin(), chnsrv[i].game_ids_.end(), gameid);
		//如果不是该设备的
		if (!strstr(chnsrv[i].device_type_.c_str(), device_type.c_str())){
			continue;
		}

		if (chnsrv[i].device_type_ == device_type){

		}

		if (it != chnsrv[i].game_ids_.end()){
			v.push_back(chnsrv[i]);
		}
		total_games += chnsrv[i].game_ids_.size();
	}

	if (v.empty() && gameid < 0){
		auto itf = std::max_element(chnsrv.begin(), chnsrv.end(), 
			[](const channel_server& s1, const channel_server& s2)->bool {
			return s1.game_ids_.size() < s2.game_ids_.size();
		});
		v.push_back(*itf);
	}

	if (v.empty() ){
		i_log_system::get_instance()->write_log(loglv_error, "fetal error! channel_servers for game:%d not found, total_games:%d", gameid, total_games);
		return error_cant_find_coordinate;
	}


	channel_server* psv = get_server(gameid, v);
	msg_channel_server msg;
	msg.for_game_ = gameid;
	msg.ip_ = psv->ip_;
	msg.port_ = psv->port_;
	send_protocol(psock, msg);

	i_log_system::get_instance()->write_log(loglv_info, "distribute_channel_server for game:%d succ, total_games=%d, srv=%s:%d ",
		gameid, total_games, psv->ip_.c_str(), psv->port_);

	return error_success;
}

int msg_get_game_coordinate::handle_this()
{
	return distribute_channel_server(from_sock_, gameid_, device_type_);
}

extern int		handle_173user_login(msg_173user_login* plogin);
int msg_173user_login::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_info, "handle 173 user login msg, uid=%s", acc_name_.c_str());
	if(from_sock_->is_login_.load())
		return error_success;

	if (sub_thread_process_msg_lst_.size() > 100){
		return error_server_busy;
	}
	i_log_system::get_instance()->write_log(loglv_info, "173 user login comfirmed, uid=%s", acc_name_.c_str());
	msg_srv_progress msg;
	msg.pro_type_ = 1;
	msg.step_ = 0;
	send_protocol(from_sock_, msg);

	from_sock_->is_login_ = true;
	return	handle_173user_login(this);
}

int msg_send_live_present::handle_this()
{
	return error_success;
}

int msg_bank_op::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;

	
	std::string sql = "select bank_psw from user_account where uid = '" + pp->uid_ + "'";
	Query q(*db_);
	q.get_result(sql);
	if (q.fetch_row()){
		if (std::string(psw_) !=  q.getstr()){
			return error_wrong_password;
		}
	}
	else{
		return error_wrong_password;
	}
	int r = error_success;
	__int64 out_count = 0;
	//从保险箱提取到背包
	if (op_ == 0){
		//从保险箱里扣钱
		if (type_ == 0){
			r = update_user_item("保险箱提取到背包", "localcall", pp->uid_, item_operator_cost, item_id_gold_bank, count_, out_count, all_channel_servers());
		}
		else{
			r = update_user_item("保险箱提取到背包", "localcall", pp->uid_, item_operator_cost, item_id_gold_game_bank, count_, out_count, all_channel_servers());
		}

		if (r != error_success){
			return error_not_enough_gold;
		}
		//存入背包
		if (type_ == 0){
			r = update_user_item("保险箱提取到背包", "localcall", pp->uid_, item_operator_add, item_id_gold, count_, out_count, all_channel_servers());
		}
		else{
			r = update_user_item("保险箱提取到背包", "localcall", pp->uid_, item_operator_add, item_id_gold_game, count_, out_count, all_channel_servers());
		}

		if (r != error_success){
			return error_not_enough_gold;
		}

	}
	else{
		//从背包里扣钱
		if (type_ == 0){
			r = update_user_item("背包存入保险箱", "localcall", pp->uid_, item_operator_cost, item_id_gold, count_, out_count, all_channel_servers());
		}
		else{
			r = update_user_item("背包存入保险箱", "localcall", pp->uid_, item_operator_cost, item_id_gold_game, count_, out_count, all_channel_servers());
		}

		if (r != error_success){
			return error_not_enough_gold;
		}

		//存入保险箱
		if (type_ == 0){
			r = update_user_item("背包存入保险箱", "localcall", pp->uid_, item_operator_add, item_id_gold_bank, count_, out_count, all_channel_servers());
		}
		else{
			r = update_user_item("背包存入保险箱", "localcall", pp->uid_, item_operator_add, item_id_gold_game_bank, count_, out_count, all_channel_servers());
		}

		if (r != error_success){
			return error_not_enough_gold;
		}
	}

	return error_business_handled;
}

int msg_set_bank_psw::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;

	std::string sql = "select bank_psw from user_account where uid = '" + pp->uid_ + "'";
	Query q(*db_);
	q.get_result(sql);
	if (!q.fetch_row()){
		return error_wrong_password;
	}	

	std::string psw = q.getstr();
	q.free_result();
	//设置密码
	if (func_ == 0){
		if (psw.empty()){
			sql = "update user_account set bank_psw = '" + std::string(psw_) + "' where uid = '" + pp->uid_ + "'";
			q.execute(sql);
		}
	}
	//修改密码
	else if (func_ == 1){
		if (psw != old_psw_){
			return error_wrong_password;
		}
		sql = "update user_account set bank_psw = '" + std::string(psw_) + "' where uid = '" + pp->uid_ + "'";
		q.execute(sql);
	}
	//验证密码
	else if (func_ == 2){
		//如果银行密码没有设置
		if (psw != psw_){
			return error_wrong_password;
		}
	}

	msg_common_reply<none_socket> msg;
	msg.err_ = func_;
	msg.rp_cmd_ = head_.cmd_;
	send_protocol(from_sock_, msg);

	return error_success;
}

int msg_get_bank_info::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;

	msg_get_bank_info_ret msg;
	std::string sql = "select gold_in_bank, gold_game_in_bank, bank_psw from user_account where uid = '" + pp->uid_ + "'";

	Query q(*db_);
	q.get_result(sql);
	if(q.fetch_row()){
		msg.bank_gold_ = q.getbigint();
		msg.bank_gold_game_ = q.getbigint();
		std::string psw = q.getstr();
		if (psw.empty()){
			msg.psw_set_ = 0;
		}
		else
			msg.psw_set_ = 1;
	}
	else{
		msg.bank_gold_ = 0;
		msg.bank_gold_game_ = 0;
		msg.psw_set_ = 0;
	}
	send_protocol(from_sock_, msg);
	return error_success;
}

int msg_transfer_gold_to_game::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return error_cant_find_player;
	__int64 outc = 0;
	int ret = update_user_item("兑换K豆", "localcall", pp->uid_, item_operator_cost, item_id_gold, count_, outc, all_channel_servers());
	if (ret == error_success){
		ret = update_user_item("兑换K豆", "localcall", pp->uid_, item_operator_add, item_id_gold_game, count_, outc, all_channel_servers());
	}
	return ret;
}

int msg_send_present::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()){
		return error_cant_find_player;
	}

	player_ptr to_pp = get_player(to_);
	if (!to_pp.get()) {
		return error_cant_find_player;
	}

	auto itp = glb_present_data.find(present_id_);
	if (itp == glb_present_data.end()){
		return error_invalid_data;
	}

	__int64 outc = 0;

	int ret = update_user_item("送礼物", "localcall", pp->uid_, item_operator_cost, present_id_, count_, outc, all_channel_servers());
	if (ret != error_success){
		return error_not_enough_gold;
	}

	__int64 pay = itp->second.price_[0].item_num_ * count_;
	//__int64 exc = (1 - itp->second.tax_ /10000.0) * pay;
	__int64 exc = pay;

	ret = update_user_item("送礼物", "localcall", to_, item_operator_add, item_id_gold_game, exc, outc, all_channel_servers());
	//如果钱没有给成功,比如网络问题,数据库问题,应该要保存下来后续再试.目前先不管了.
	if (ret != error_success)	{

	}

	msg_chat_deliver* msg = new msg_chat_deliver();
	msg->channel_ = channel_;
	std::string cont = lx2s(itp->second.pid_) + "|" + lx2s(count_) + "|" + itp->second.name_ + "|" + itp->second.ico_ + "|" + lx2s(exc);
	msg->content_ = cont;

	msg->from_uid_ = pp->uid_;
	msg->nickname_ = pp->nickname_;
	msg->from_tag_ = (pp->vip_level() << 16) | (pp->isagent_ & 0xFFFF);

	msg->to_uid_ = to_pp->uid_;
	msg->to_nickname_ = to_pp->nickname_;
	msg->to_tag_ = (to_pp->vip_level() << 16) | (to_pp->isagent_ & 0xFFFF);
	
	msg->msg_type_ = 5;

	auto cnn = to_pp->from_socket_.lock();
	if (cnn.get()){
		send_protocol(cnn, *msg);
	}

	delete msg;

	Database& db(*db_);
	BEGIN_INSERT_TABLE("log_koko_gift_send");
	SET_FIELD_VALUE("from_uid", pp->uid_);
	SET_FIELD_VALUE("to_uid", to_);
	SET_FIELD_VALUE("present_id", present_id_);
	SET_FIELD_VALUE("count", count_);
	SET_FIELD_VALUE("pay", pay);
	SET_FIELD_VALUE("tax", pay - exc);
	SET_FINAL_VALUE("result", int(ret == 0));
	EXECUTE_IMMEDIATE();

	return error_business_handled;
}

int msg_scan_qrcode::handle_this()
{
	msg_srv_progress msg;
	msg.pro_type_ = 0;
	msg.step_ = 0;
	send_protocol(from_sock_, msg);

	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));

	i_log_system::get_instance()->write_log(loglv_info, "qrcode user login accepted, %s", from_sock_->remote_ip().c_str());
	return error_success;
}

int msg_modify_acc_name::handle_this()
{
	if (sub_thread_process_msg_lst_.size() > 100) {
		return error_server_busy;
	}

	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));

	return error_success;
}

int msg_modify_nick_name::handle_this()
{
	if (sub_thread_process_msg_lst_.size() > 100) {
		return error_server_busy;
	}

	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));

	return error_success;
}

int msg_token_login::handle_this()
{
	i_log_system::get_instance()->write_log(loglv_debug, "user login channel confirmed, %s", from_sock_->remote_ip().c_str());
	__int64 tn = 0;
	::time(&tn);
	__int64 sn = boost::lexical_cast<__int64>(sn_);
	//2小时过期
	if (tn - sn_ > 2 * 3600) {
		return error_wrong_sign;
	}

	std::string sign_pattn = std::string(uid_) + boost::lexical_cast<std::string>(sn_) + the_config.token_verify_key_;
	std::string	tok = md5_hash(sign_pattn);
	if (tok != std::string(token_)) {
		return error_wrong_sign;
	}

	sub_thread_process_msg_lst_.push_back(
		boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));

	i_log_system::get_instance()->write_log(loglv_debug, "user login channel accepted, %s", from_sock_->remote_ip().c_str());
	return error_success;
}