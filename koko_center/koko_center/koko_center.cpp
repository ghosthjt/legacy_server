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
#include "msg.h"
#include "error.h"
#include "md5.h"
#include "player.h"
#include "msg_server.h"
#include "msg_client.h"
#include "log.h"
#include "server_config.h"
#include "http_request.h"
#include <DbgHelp.h>
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/json_parser.hpp"
#include "http_request.cpp"
#include "md5.cpp"
#include "simple_xml_parse.cpp"
#include "net_socket_native.cpp"

#include "json_helper.cpp"
#include "base64Con.cpp"
#include "base64.h"
#include <boost/locale.hpp>
#include "db_delay_helper.cpp"
#include "i_log_system.cpp"
#include "file_system.cpp"
#include "net_socket_basic.cpp"
#include "net_socket_remote.cpp"

#pragma auto_inline (off)
#pragma comment(lib, "dbghelp.lib")

extern std::string	md5_hash(std::string sign_pat);

remote_socket_ptr socket_cr(net_server& srv)
{
	return boost::make_shared<koko_socket>(srv);
}

class koko_net_server : public net_server
{
public:
	koko_net_server() :net_server(socket_cr) {}
	bool on_connection_accepted(remote_socket_ptr remote)
	{
		i_log_system::get_instance()->write_log(loglv_info, "connection accept, ip:%s", remote->remote_ip().c_str());
		return true;
	}
};
extern boost::thread_specific_ptr<std::vector<channel_server>> channel_servers;
boost::shared_ptr<utf8_data_base>	db_;
db_delay_helper	db_delay_helper_;
boost::shared_ptr<boost::asio::io_service> timer_srv;

std::unordered_map<unsigned int, task_wptr> task_info::glb_running_task_;
boost::shared_ptr<koko_net_server> the_net;

boost::atomic_uint	msg_recved(0);
boost::atomic_uint	msg_handled(0);
boost::atomic_uint	unsended_data(0);

typedef boost::shared_ptr<msg_base<koko_socket_ptr>> msg_ptr;		//从客户端发过来的消息类型
typedef boost::shared_ptr<msg_base<none_socket>> smsg_ptr;			//发送给客户端的消息类型

std::list<msg_ptr>	glb_msg_list_;

std::unordered_map<std::string, player_ptr> online_players;
safe_list<player_ptr> pending_login_users_;

safe_list<msg_ptr>		sub_thread_process_msg_lst_;

safe_list<smsg_ptr>		broadcast_msg_;
service_config the_config;
task_ptr task;

typedef boost::shared_ptr<game_info> gamei_ptr;
std::map<int, gamei_ptr> all_games;
std::map<int, present_info>	glb_present_data;
std::map<int, item_info> glb_item_data;
extern std::map<int, user_privilege>	glb_vip_privilege;

safe_list<std::pair<koko_socket_ptr, smsg_ptr>> glb_thread_send_cli_msg_lst_;

extern int	update_user_item(std::string reason, 
	std::string sn,
	std::string uid, int op, 
	int itemid, __int64 count, __int64& ret,
	std::vector<std::string> sync_to_server);
extern int	do_multi_costs(std::string reason, std::string uid, 
	std::map<int, int> cost,
	std::vector<std::string> sync_to_server);
extern int	sync_user_item(std::string uid, int itemid);
extern void	load_vip_privilege();
extern LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo);
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

void sql_trim(char* str)
{
	for (unsigned int i = 0; i < strlen(str); i++)
	{
		if (str[i] == '\''){
			str[i] = ' ';
		}
	}
}

void	send_all_present_to_player(boost::shared_ptr<koko_socket> psock)
{
	auto it = glb_present_data.begin();
	while (it !=  glb_present_data.end())
	{
		present_info& inf = it->second; it++;
		msg_present_data msg;
		msg.pid_ = inf.pid_;
		msg.cat_ = inf.cat_;
		msg.name_ = inf.name_;
		msg.desc_ = inf.desc_;
		msg.ico_ = inf.ico_;
		
		for (int i = 0; i < inf.price_.size(); i++)
		{
			msg.price_ = msg.price_ + lx2s(inf.price_[i].item_id_) + "," + lx2s(inf.price_[i].item_num_) + ",";
		}

		send_protocol(psock, msg);
	}
}


template<class remote_t>
void	response_user_login(player_ptr pp, remote_t pconn, msg_user_login_ret& msg)
{	
	msg.gold_ = pp->gold_;
	msg.game_gold_ = pp->gold_game_;
	msg.game_gold_free_ = pp->gold_free_;
	msg.iid_ = pp->iid_;
	msg.vip_level_ = pp->vip_level();
	msg.sequence_ = pp->seq_;
	msg.idcard_ = pp->idnumber_;

	msg.phone = pp->mobile_;
	msg.phone_verified_ = pp->mobile_v_;

	msg.email_ = pp->email_;
	msg.email_verified_ = pp->email_v_;
	msg.age_ = pp->age_;
	msg.gender_ = pp->gender_;
	msg.level_ = pp->level_;
	msg.byear_ = pp->byear_;
	msg.bmonth_ = pp->bmonth_;
	msg.region1_ = pp->region1_; 
	msg.region2_ = pp->region2_;
	msg.region3_ = pp->region3_;
	msg.bday_ = pp->bday_;
	msg.isagent_ = pp->isagent_;
	msg.isagent_ |= pp->is_newbee_ << 17;

	msg.address_ = pp->address_;
	msg.nickname_ = pp->nickname_;
	msg.uid_ = pp->uid_;
	msg.token_ = pp->token_;
	msg.app_key_ = pp->app_key_;
	msg.party_name_ = pp->party_name_;
	send_protocol(pconn, msg);
}

template<class socket_t>
void	send_image(msg_image_data& msg_shoot, std::vector<char> sdata, socket_t sock)
{
	msg_shoot.TAG_ = 1;
	for (int i = 0; i < (int)sdata.size();)
	{
		int dl = sdata.size() - i;
		dl = std::min<int>(2048, dl);
		char data[2048] = { 0 };
		memcpy(data, sdata.data() + i, dl);
		msg_shoot.data_.insert(msg_shoot.data_.end(), data, data + dl);
		i += dl;
		msg_shoot.len_ = dl;
		send_protocol(sock, msg_shoot);
		msg_shoot.TAG_ = 0;
		msg_shoot.data_.clear();
	}
	msg_shoot.TAG_ = 2;
	msg_shoot.len_ = 0;
	send_protocol(sock, msg_shoot);
}

msg_ptr create_msg_from_client(unsigned short cmd)
{
	DECLARE_MSG_CREATE()
	REGISTER_CLS_CREATE(msg_user_login);
	REGISTER_CLS_CREATE(msg_user_register);
	REGISTER_CLS_CREATE(msg_change_account_info);
	REGISTER_CLS_CREATE(msg_get_verify_code);
	REGISTER_CLS_CREATE(msg_check_data);
	REGISTER_CLS_CREATE(msg_action_record);
	REGISTER_CLS_CREATE(msg_get_game_coordinate);
	REGISTER_CLS_CREATE(msg_get_token);
	REGISTER_CLS_CREATE(msg_173user_login);
	REGISTER_CLS_CREATE(msg_send_live_present);
	REGISTER_CLS_CREATE(msg_get_bank_info);
	REGISTER_CLS_CREATE(msg_set_bank_psw);
	REGISTER_CLS_CREATE(msg_bank_op);
	REGISTER_CLS_CREATE(msg_transfer_gold_to_game);
	REGISTER_CLS_CREATE(msg_send_present);
	REGISTER_CLS_CREATE(msg_mobile_login);
	REGISTER_CLS_CREATE(msg_scan_qrcode);
	REGISTER_CLS_CREATE(msg_modify_acc_name);
	REGISTER_CLS_CREATE(msg_modify_nick_name);
	REGISTER_CLS_CREATE(msg_token_login);
	END_MSG_CREATE()
	return  ret_ptr;
}

bool glb_exit = false;
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
		koko_socket_ptr psock = boost::dynamic_pointer_cast<koko_socket>(remotes[i]);
		bool got_msg = false;

		unsigned int pick_count = 0;
		do
		{
			got_msg = false;
			auto pmsg = pickup_msg_from_socket(psock, create_msg_from_client, got_msg);
			if (pmsg.get()){
				busy = true;
				pick_count++;
				pmsg->from_sock_ = psock;
				glb_msg_list_.push_back(pmsg);
				msg_recved++;
			}
		}while(got_msg && pick_count < 200);
	}
}

template<class remote_t>
int		verify_user_info(msg_user_login_t<remote_t>* plogin, bool check_sign = true)
{
	Database& db = *db_;
	if (check_sign){
		std::string	sign_pat = plogin->acc_name_ + std::string(plogin->pwd_hash_) + plogin->machine_mark_ + "{51B539D8-0D9A-4E35-940E-22C6EBFA86A8}";
		std::string sign_key = md5_hash(sign_pat);
		std::string sign_remote;
		sign_remote.insert(sign_remote.begin(), plogin->sign_, plogin->sign_ + 32);
		if (sign_key != sign_remote){
			return error_wrong_sign;
		}
	}

	char accname[max_guid] = {0}, macmk[max_guid] = {0};
	mysql_real_escape_string(&db.grabdb()->mysql, accname, plogin->acc_name_.c_str(), strlen(plogin->acc_name_.c_str()));

	if (isbaned(plogin->acc_name_)){
		return error_user_banned;
	}

	return error_success;
}

void		load_all_item()
{
	glb_item_data.clear();
	std::string sql = "select `item_id`, `type`, `name`, `desc`, `ico`, `is_use`, `to_item`, `is_send`, `min_send_num`, `max_send_num` from setting_item_list";
	Query q(*db_);
	if (!q.get_result(sql)) return;
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
		for (int i = 0; i < int(to_item.size() - 1); i += 2)
		{
			inf.to_item_.push_back(std::make_pair(to_item[i], to_item[i + 1]));
		}

		if (inf.to_item_.size() > 2) {
			i_log_system::get_instance()->write_log(loglv_error, "there is a error in setting_goods_list table! item size=%d", inf.to_item_.size());
		}

		inf.is_send_ = q.getval();
		inf.min_send_num_ = q.getval();
		inf.max_send_num_ = q.getval();

		glb_item_data.insert(std::make_pair(inf.item_id_, inf));
	}
}

int get_account_from_login_name(std::string login_name, std::string & uid, std::string & acc_name, std::string & pwd, std::string & nick)
{
	char saccname[max_guid] = { 0 };
	int len = mysql_real_escape_string(&db_->grabdb()->mysql, saccname, login_name.c_str(), strlen(login_name.c_str()));
	Database& db = *db_;
	Query q(db);
	std::string sql = "select uid, uname, pwd, nickname from user_account where uname = '" + std::string(saccname) + "'"
		" or iid = '" + std::string(saccname) + "'"
		" or (mobile_number = '" + std::string(saccname) + "' and mobile_verified = 1)"
		" or (mail = '" + std::string(saccname) + "' and mail_verified = 1)";
	if (!q.get_result(sql))
		return error_sql_execute_error;

	if (!q.fetch_row()) {
		return error_no_record;
	}

	uid = q.getstr();
	acc_name = q.getstr();
	pwd = q.getstr();
	nick = q.getstr();

	q.free_result();

	return error_success;
}

int get_pwd(std::string uid, std::string & pwd)
{
	Database& db = *db_;
	Query q(db);
	std::string sql = "select pwd from user_account where uid = '" + uid + "'";

	if (!q.get_result(sql))
		return error_sql_execute_error;

	if (!q.fetch_row()) {
		return error_no_record;
	}

	pwd = q.getstr();
	q.free_result();

	return error_success;
}

int		check_user_uname(std::string uname)
{
	if (!(uname.size() >= 6 && uname.size() <= 12)) {
		return error_invalid_data;
	}

	boost::cmatch what;
	boost::regex expr("^[a-zA-Z]+[0-9][a-zA-Z0-9]*|[0-9]+[a-zA-Z]+[a-zA-Z0-9]*$");
	bool matched = boost::regex_match(uname.c_str(), what, expr);
	if (!matched) {
		return error_invalid_data;
	}

	return error_success;
}

int		check_nick_name(std::string nick_name)
{
	std::string gbk_nick = boost::locale::conv::from_utf(nick_name, "GBK");
	if (!(gbk_nick.size() >= 4 && gbk_nick.size() <= 18)) {
		return error_invalid_data;
	}

	std::wstring w_nick = boost::locale::conv::utf_to_utf<wchar_t>(nick_name);
	for (int i = 0; i < w_nick.size(); i++)
	{
		wchar_t ch = w_nick[i];
		if ((int)ch > 128 && !((int)ch >= 0x4E00 && (int)ch <= 0x9FA5)) {
			return error_invalid_data;
		}
		else if ((int)ch > 0 && (int)ch < 128) {
			if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch == '-') || (ch == '_'))) {
				return error_invalid_data;
			}
		}
	}

	return error_success;
}

int		handle_get_verify_code(msg_get_verify_code* pverify)
{
	std::string sql = "call random_select_verify_code(8);";
	Database& db = *db_;
	Query q(db);

	if (!q.get_result(sql)){
		return error_success;
	}
	std::vector<vector<char>> anwsers;
	std::vector<char> image, image2, tmp_iamge;
	msg_verify_code msg;
	
	int rows = q.num_rows();
	int ans = rand_r(rows - 1);
	int i = 0;
	//混淆答案
	while (q.fetch_row())
	{
		if (i == ans){
			msg.question_ = q.getstr();
			q.getblob("image", image);
			q.getblob("image2", image2);
		}
		else{
			q.getstr();
			tmp_iamge.clear();
			q.getblob("image", tmp_iamge);
			if (!tmp_iamge.empty()){
				anwsers.push_back(tmp_iamge);
			}
			tmp_iamge.clear();

			q.getblob("image2", tmp_iamge);
			if (!tmp_iamge.empty()){
				anwsers.push_back(tmp_iamge);
			}
		}
		i++;
	}

	send_protocol(pverify->from_sock_, msg);

	std::random_shuffle(anwsers.begin(), anwsers.end());
	int index = -1;
	if (!image.empty()){
		index  = rand_r(std::min<int>(7, anwsers.size() - 1));
		anwsers.insert(anwsers.begin() + index, image);
	}

	int index2 = -1;
	if (!image2.empty()){
		index2  = rand_r(std::min<int>(7, anwsers.size() - 1));
		anwsers.insert(anwsers.begin() + index2, image2);
		//如果插在前一副图的前面，则前面一副图的index+1
		if (index2 <= index){
			index++;
		}
	}
	
	for (int i = 0; i < (int)anwsers.size() && i < 8; i++)
	{
		vector<char>& img = anwsers[i];
		msg_image_data msg_img;
		msg_img.this_image_for_ = -2;
		send_image(msg_img, img, pverify->from_sock_);
	}

	std::string vc;
	if (index >= 0)
		vc += boost::lexical_cast<std::string>(index) + ",";

	if (index2 >= 0)
		vc += boost::lexical_cast<std::string>(index2) + ",";

	pverify->from_sock_->verify_code_.store(vc);
	return error_success;
}

int		handle_user_register(msg_user_register* pregister)
{
	char saccname[max_guid] = {0}, spwd[max_guid] = {0}, smmk[max_guid] = {0}, ssprd[max_guid] = {0};
	mysql_real_escape_string(&db_->grabdb()->mysql, saccname, pregister->acc_name_.c_str(), strlen(pregister->acc_name_.c_str()));
	mysql_real_escape_string(&db_->grabdb()->mysql, spwd, pregister->pwd_hash_.c_str(), strlen(pregister->pwd_hash_.c_str()));
	mysql_real_escape_string(&db_->grabdb()->mysql, smmk, pregister->machine_mark_.c_str(), strlen(pregister->machine_mark_.c_str()));
	mysql_real_escape_string(&db_->grabdb()->mysql, ssprd, pregister->spread_from_.c_str(), strlen(pregister->spread_from_.c_str()));
	int verify = verify_user_info(pregister);
	if (verify != error_success){
		return verify;
	}

	Query q(*db_);
	bool iswhite = false;
	std::string sql = "select 1 from setting_ip_pass where `state` = 0 and func = 0 and ip = '" + pregister->from_sock_->remote_ip() + "'";
	if (q.get_result(sql) && q.fetch_row()){
		iswhite = true;
	}
	q.free_result();

	sql = "select count(*), unix_timestamp(max(create_time)) from user_account where machine_mark = '" + std::string(smmk) +
		"'";

	if (q.get_result(sql) && q.fetch_row() && !iswhite){
		int c  = q.getval();
		__int64 tmst = q.getbigint();
		//1小时只能注册一个号,一个机器最多只能注册5个号.
		if (c > 10 || (::time(nullptr) - tmst) < 3600){
			return error_cannt_regist_more;
		}
	}
	q.free_result();

	{
		boost::cmatch what;
		std::string number = pregister->acc_name_;
		if (pregister->type_ == 1){
			boost::regex expr("^1[3578][0-9]{9}$");
			bool matched = boost::regex_match(number.c_str(), what, expr);
			if (!matched){
				return error_invalid_data;
			}
		}
		else if(pregister->type_ == 2){
			boost::regex expr("^[a-zA-Z0-9]+@[a-zA-Z0-9]+\\.[a-zA-Z0-9]+$");
			bool matched = boost::regex_match(number.c_str(), what, expr);
			if (!matched){
				return error_invalid_data;
			}
		}
		else if(pregister->type_ == 0){
			boost::regex expr("^[a-zA-Z0-9_]{3,}$");
			bool matched = boost::regex_match(number.c_str(), what, expr);
			if (!matched){
				return error_invalid_data;
			}
		}
		else if (pregister->type_ == 4){
			if (!number.empty()){
				return error_invalid_data;
			}
			memset(spwd, 0, max_guid);
		}
	}

	if (pregister->type_ == 1){
		int ret = error_success;
		if (pregister->from_sock_ ->mverify_code_ == ""){
			ret = error_wrong_verify_code;
		}
		else if(pregister->from_sock_->mverify_code_ != pregister->verify_code){
			ret = error_wrong_verify_code;
		}
		else if ((boost::posix_time::microsec_clock::local_time() - pregister->from_sock_->mverify_time_).total_seconds() > 120){
			ret = error_time_expired;
		}
		if (ret != error_success){
			return ret;
		}
	}

	sql = "call register_user(" + boost::lexical_cast<std::string>(pregister->type_) + 
		", '" + std::string(saccname) + "', '" + 
		std::string(spwd) + "', '" +
		std::string(smmk) + "','" +
		pregister->from_sock_->remote_ip() + "','" +
		std::string(ssprd) + "', @ret, @uid);select @ret, @uid";

	std::string uid;
	if (!q.get_result(sql))
		return error_sql_execute_error;
	//账号已存在
	if (q.fetch_row()){
		if (q.getval() != error_success){
			return error_account_exist;
		}
		uid = q.getstr();
	}
	q.free_result();

	__int64 outc = 0, c = 0;
	sql = "select int_val from setting_server_parameters where param_id = 1";
	if (q.fetch_row()){
		c = q.getval();
	}
	q.free_result();

	update_user_item("注册赠送", "localcall", uid, item_operator_add, item_id_gold, c, outc, std::vector<std::string>());

	msg_common_reply<koko_socket_ptr> msg;
	msg.rp_cmd_ = GET_CLSID(msg_user_register);
	msg.err_ = 0;
	send_protocol(pregister->from_sock_, msg);

	return error_success;
}

int		handle_modify_acc_name(msg_modify_acc_name* pmodify)
{
	//校验新用户名
	bool is_valid = true;
	std::string new_acc_name = pmodify->new_acc_name_;
	if (!(new_acc_name.size() >= 6 && new_acc_name.size() <= 12)) {
		is_valid = false;
	}
	else {
		boost::cmatch what;
		boost::regex expr("^[a-zA-Z]+[0-9][a-zA-Z0-9]*|[0-9]+[a-zA-Z]+[a-zA-Z0-9]*$");
		bool matched = boost::regex_match(new_acc_name.c_str(), what, expr);
		if (!matched) {
			is_valid = false;
		}
	}

	if (is_valid == false){
		return error_acc_name_invalid;
	}

	//校验密码
	//std::string pwd;
	//int ret = get_pwd(pmodify->uid_, pwd);
	//if (ret != error_success) {
	//	glb_http_log.write_log("get_account_from_login_name error:%n", ret);
	//	return error_cant_find_player;
	//}

	//if (md5_hash(pwd) != pmodify->pwd_hash_) {
	//	return error_wrong_password;
	//}

	//查询用户名是否已存在
	{
		char sdata[256] = { 0 };
		int len = mysql_real_escape_string(&db_->grabdb()->mysql, sdata, pmodify->new_acc_name_.c_str(), strlen(pmodify->new_acc_name_.c_str()));
		std::string sql = "select 1 from user_account where uname = '" + std::string(sdata) + "'";
		Query q(*db_);
		if (q.get_result(sql) && q.fetch_row()) {
			return error_account_exist;
		}
	}
	
	//修改用户名
	{
		Database& db = *db_;
		BEGIN_UPDATE_TABLE("user_account");
		SET_FINAL_VALUE("uname", new_acc_name);
		WITH_END_CONDITION("where uid = '" + pmodify->uid_ + "'");
		EXECUTE_IMMEDIATE();
	}

	return error_business_handled;
}

int		handle_modify_nick_name(msg_modify_nick_name* pmodify)
{
	int ret = check_nick_name(pmodify->nick_name_);

	if (ret != error_success) {
		return ret;
	}

	//修改昵称
	{
		Database& db = *db_;
		BEGIN_UPDATE_TABLE("user_account");
		SET_FINAL_VALUE("nickname", pmodify->nick_name_);
		WITH_END_CONDITION("where uid = '" + pmodify->uid_ + "'");
		EXECUTE_IMMEDIATE();
	}

	return error_business_handled;
}

extern bool isbaned(std::string uid);
template<class remote_t>
int load_user_from_db(msg_user_login_t<remote_t>* plogin, player_ptr& pp, bool check_sign)
{
	int verify = verify_user_info(plogin, check_sign);
	if (verify != error_success){
		return verify;
	}
	char saccname[max_guid] = {0};
	int len =	mysql_real_escape_string(&db_->grabdb()->mysql,saccname, plogin->acc_name_.c_str(), strlen(plogin->acc_name_.c_str()));
	Database& db = *db_;
	Query q(db);
	std::string sql = "select uid, pwd, nickname, iid, gold, gold_game, gold_free, vip_value, vip_limit,"
		" idnumber, mobile_number, mobile_verified, mail, mail_verified, level, addr_province, addr_city, addr_region,"
		" gender, byear, bmonth, bday, address, age, is_agent,"
		" unix_timestamp(create_time), sec_mobile, machine_mark, partyName from user_account where uname = '" + std::string(saccname) + "'"
		" or iid = '" + std::string(saccname) + "'"
		" or (mobile_number = '" + std::string(saccname) + "' and mobile_verified = 1)"
		" or (mail = '" + std::string(saccname) + "' and mail_verified = 1)";
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
	std::string secmobile = q.getstr();

	if (mobile == ""){
		mobile = secmobile;
	}
	std::string macmark = q.getstr();
	std::string party_name = q.getstr();

	std::vector<char> headpic;

	if (pwd != plogin->pwd_hash_){
		return error_wrong_password;
	}

	if (nickname == ""){
		nickname = "newplayer";
	}

	q.free_result();

	plogin->from_sock_->set_authorized();
	__int64 seq = 0;	
	::time(&seq);

	{
		BEGIN_UPDATE_TABLE("user_account");
		SET_FINAL_VALUE("is_online", 1);
		WITH_END_CONDITION("where uid = '" + uid + "'");
		EXECUTE_IMMEDIATE();
	}
	{
		BEGIN_INSERT_TABLE("log_user_loginout");
		SET_FIELD_VALUE("uid", uid);
		SET_FINAL_VALUE("islogin", 1);
		EXECUTE_IMMEDIATE();
	}

	std::string token = md5_hash(uid + boost::lexical_cast<std::string>(seq) + the_config.token_generate_key_);

	pp->uid_ = uid;
	pp->connect_lost_tick_ = boost::posix_time::microsec_clock::local_time();
	pp->is_connection_lost_ = 0;
	pp->iid_ = iid;
	pp->nickname_ = nickname;
	pp->headpic_ = headpic;
	pp->gold_ = gold;
	pp->vip_value_ = vip_value;
	pp->vip_limit_ = vip_limit;
	pp->token_ = token;
	pp->seq_ = seq;
	pp->idnumber_ = idnumber;
	pp->age_ = age;
	pp->mobile_ = mobile;
	pp->isagent_ = isagent;
	pp->is_newbee_ = (::time(nullptr) - create_tm) < boost::posix_time::hours(48).total_seconds();
	pp->email_ = mail;
	pp->region1_ = r1;
	pp->region2_ = r2;
	pp->region3_ = r3;
	pp->gold_game_ = gold_game;
	pp->mobile_v_ = mobile_verified;
	pp->email_v_ = mail_verified;
	pp->byear_ = byear;
	pp->bmonth_ = bmonth;
	pp->bday_ = bday;
	pp->address_ = addr;
	pp->gender_ = gender;
	pp->gold_free_ = gold_free;
	pp->sec_mobile_ = secmobile;
	pp->party_name_ = party_name;
	
	if (plogin->head_.cmd_ == GET_CLSID(msg_mobile_login)){
		msg_mobile_login* pml = (msg_mobile_login*) plogin;
		pp->from_devtype_ = pml->dev_type;
	}

	return error_success;
}

void	send_all_match_to_player(player_ptr pp)
{
	std::string sql = "SELECT `match_type` ,`id`,`game_id`,`match_name`,`thumbnail`,`help_url`,"
			"`prize_desc`,`require_count`,`start_type`,`start_stipe`,`day_or_time`,`h`,`m`,"
			"`s`,`need_players_per_game`,`eliminate_phase1_basic_score`,`eliminate_phase1_inc`,"
			"`eliminate_phase2_count_`,`end_type`,`end_data`,`prizes`,`cost`, `srvip`, `srvport`"
			" FROM `setting_match_list`";

	Query q(*db_);
	if (q.get_result(sql)){
		while (q.fetch_row()){
			int tp = q.getval();
			boost::shared_ptr<match_info> mp(new match_info());
			mp->match_type_ = tp;
			mp->id_ = q.getval();
			mp->game_id_ = q.getval();
			mp->match_name_ = q.getstr();
			mp->thumbnail_ = q.getstr();
			mp->help_url_ = q.getstr();
			mp->prize_desc_ = q.getstr();
			mp->require_count_ = q.getval();
			mp->start_type_ = q.getval();
			mp->start_stripe_ = q.getval();
			mp->wait_register_ = q.getval();
			mp->h = q.getval();
			mp->m = q.getval();
			mp->s = q.getval();
			mp->need_players_per_game_ = q.getval();
			mp->eliminate_phase1_basic_score_ = q.getval();
			mp->eliminate_phase1_inc = q.getval();
			mp->elininate_phase2_count_ = q.getval();
			mp->end_type_ = q.getval();
			mp->end_data_ = q.getval();
			std::string vp = q.getstr();
			std::string vc = q.getstr();
			mp->srvip_ = q.getstr();
			mp->srvport_ = q.getval();

			std::vector<std::string> tmpv;
			std::vector<int> tmpvv;
			if(!split_str(vp, ";", tmpv, true)){
				tmpv.clear();
				i_log_system::get_instance()->write_log(loglv_error, "match prize config error: %s matchid:%d", vc.c_str(), mp->id_);
			}

			for (int i = 0; i < (int)tmpv.size(); i++)
			{
				tmpvv.clear();
				split_str(tmpv[i], ",", tmpvv, true);
				std::vector<std::pair<int, int>> vv;
				for (int ii = 0; ii < (int)tmpvv.size() - 1; ii += 2)
				{
					vv.push_back(std::make_pair(tmpvv[ii], tmpvv[ii + 1]));
				}
				mp->prizes_.push_back(vv);
			}
			tmpvv.clear();

			if(!split_str(vc, ",", tmpvv, true)){
				i_log_system::get_instance()->write_log(loglv_error, "match cost config error: %s matchid:%d", vc.c_str(), mp->id_);
				tmpvv.clear();
			}
			while (tmpvv.size() >= 2)
			{
				mp->cost_.insert(std::make_pair(tmpvv[0], tmpvv[1]));
				tmpvv.erase(tmpvv.begin());
				tmpvv.erase(tmpvv.begin());
			}

			msg_match_data msg;
			msg.inf = *mp;
			send_protocol(pp->from_socket_.lock(), msg);
		}
	}
}

int		handle_173user_login(msg_173user_login* plogin)
{
	return error_success;
}

int		handle_user_login(msg_user_login* plogin)
{
	msg_srv_progress msg;
	msg.pro_type_ = 0;
	msg.step_ = 1;
	send_protocol(plogin->from_sock_, msg);

	player_ptr pp(new koko_player());
	int ret = load_user_from_db(plogin, pp, true);
	if (ret != error_success){
		i_log_system::get_instance()->write_log(loglv_error, "user login failed, ret=%d, %s", ret, plogin->acc_name_.c_str());
		return ret;
	}
	else{
		pp->from_socket_ = plogin->from_sock_;
		plogin->from_sock_->the_client_ = pp;

		msg.pro_type_ = 0;
		msg.step_ = 2;
		send_protocol(plogin->from_sock_, msg);

		Database& db = *db_;
		Query q(db);

		msg.pro_type_ = 0;
		msg.step_ = 3;
		send_protocol(plogin->from_sock_, msg);

		send_all_match_to_player(pp);

		msg.pro_type_ = 0;
		msg.step_ = 4;
		send_protocol(plogin->from_sock_, msg);
		
		i_log_system::get_instance()->write_log(loglv_info, "user login succ:%s", plogin->acc_name_.c_str());
		pending_login_users_.push_back(pp);
	}

	std::string uid;
	std::string uname;
	std::string pwd;
	std::string nick;
	ret = get_account_from_login_name(plogin->acc_name_, uid, uname, pwd, nick);
	if (ret != error_success) {
		i_log_system::get_instance()->write_log(loglv_info, "get_account_from_login_name error:%n", ret);
		return ret;
	}

	//判断用户名是否合法
	{
		ret = check_user_uname(uname);

		if (ret != error_success) {
			msg_acc_info_invalid msg;
			msg.type_ = 1;	//账号不合法
			send_protocol(plogin->from_sock_, msg);
			return error_success;
		}
	}

	//判断昵称是否合法
	{
		ret = check_nick_name(nick);

		if (ret != error_success) {
			msg_acc_info_invalid msg;
			msg.type_ = 2;	//账号不合法
			send_protocol(plogin->from_sock_, msg);
		}
	}

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

int			do_load_user(std::string sql, player_ptr& pp, bool check_psw, std::string pwd_check)
{
	Database& db = *db_;
	Query q(db);

	if (!q.get_result(sql))
		return error_sql_execute_error;

	if (!q.fetch_row()) {
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

	if (check_psw && pwd != pwd_check) {
		return error_wrong_password;
	}

	if (nickname == "") {
		nickname = "newplayer";
	}

	q.free_result();

	__int64 seq = 0;
	::time(&seq);

	std::string token = md5_hash(uid + boost::lexical_cast<std::string>(seq) + the_config.token_generate_key_);

	pp->uid_ = uid;
	pp->connect_lost_tick_ = boost::posix_time::microsec_clock::local_time();
	pp->is_connection_lost_ = 0;
	pp->iid_ = iid;
	pp->nickname_ = nickname;
	pp->headpic_ = headpic;
	pp->gold_ = gold;
	pp->vip_value_ = vip_value;
	pp->vip_limit_ = vip_limit;
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

int load_user_by_uid(std::string uid, player_ptr& pp, bool check_sign)
{
	std::string sql = "select uid, pwd, nickname, iid, gold, gold_game, gold_free, vip_value, vip_limit,"
		" idnumber, mobile_number, mobile_verified, mail, mail_verified, level, addr_province, addr_city, addr_region,"
		" gender, byear, bmonth, bday, address, age, is_agent, "
		" unix_timestamp(create_time), headpic from user_account where uid = '" + std::string(uid) + "'";

	return do_load_user(sql, pp, false, "");
}

int		handle_qrcode_login(msg_scan_qrcode* plogin)
{
	std::string cndt = " where request_id = '"
		+ std::string(plogin->clientid_) + "' and public_key = '" + std::string(plogin->passcode_) + "'";
	std::string sql = "select uid, app_key, unix_timestamp(create_time) from user_login_request " + cndt;
	Query q(*db_);
	if (q.get_result(sql) && q.fetch_row()){
		std::string uid = q.getstr();
		std::string app_key = q.getstr();
		__int64 tmstmp = q.getbigint();

		//120秒之后就超时
		if ((::time(nullptr) - tmstmp) >= 120){
			return error_sql_execute_error;
		}
		
		player_ptr pp(new koko_player());
		int ret = load_user_by_uid(uid, pp, true);
		if (ret != error_success) {
			i_log_system::get_instance()->write_log(loglv_info, "user data load fail by msg_user_login_channel request.");
			return ret;
		}
		i_log_system::get_instance()->write_log(loglv_info, "user data loaded by msg_user_login_channel request.");
		plogin->from_sock_->set_authorized();
		pp->from_socket_ = plogin->from_sock_;
		pp->app_key_ = app_key;
		plogin->from_sock_->the_client_ = pp;

		pending_login_users_.push_back(pp);
	}
	q.free_result();
	q.execute("delete from user_login_request " + cndt);
	return error_success;
}

int		handle_token_login(msg_token_login* plogin)
{
	player_ptr pp(new koko_player());
	int ret = load_user_by_uid(plogin->uid_, pp, true);
	if (ret != error_success) {
		i_log_system::get_instance()->write_log(loglv_debug, "user data load fail by msg_user_login_channel request.");
		return ret;
	}
	i_log_system::get_instance()->write_log(loglv_debug, "user data loaded by msg_user_login_channel request.");
	plogin->from_sock_->set_authorized();
	pp->from_socket_ = plogin->from_sock_;
	plogin->from_sock_->the_client_ = pp;

	pending_login_users_.push_back(pp);
	return error_success;
}

int			db_thread_func_2()
{
	msg_ptr pmsg;
	if (sub_thread_process_msg_lst_.pop_front(pmsg)) {
		int ret = error_success;
		if (pmsg->head_.cmd_ == GET_CLSID(msg_user_login) ||
			pmsg->head_.cmd_ == GET_CLSID(msg_mobile_login)) {
			ret = handle_user_login((msg_user_login*)pmsg.get());
			if (ret != error_success) {
				pmsg->from_sock_->is_login_ = false;
			}
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_scan_qrcode)) {
			ret = handle_qrcode_login((msg_scan_qrcode*)pmsg.get());
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_user_register)) {
			ret = handle_user_register((msg_user_register*)pmsg.get());
			pmsg->from_sock_->is_register_ = false;
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_get_verify_code)) {
			ret = handle_get_verify_code((msg_get_verify_code*)pmsg.get());
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_modify_acc_name)) {
			ret = handle_modify_acc_name((msg_modify_acc_name*)pmsg.get());
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_modify_nick_name)) {
			ret = handle_modify_nick_name((msg_modify_nick_name*)pmsg.get());
		}
		else if (pmsg->head_.cmd_ == GET_CLSID(msg_token_login)) {
			ret = handle_token_login((msg_token_login*)pmsg.get());
		}
		if (ret != error_success) {
			msg_common_reply<none_socket> msg;
			msg.rp_cmd_ = pmsg->head_.cmd_;
			msg.err_ = ret;
			send_protocol(pmsg->from_sock_, msg);
		}
	}
	return error_success;
}

extern int		load_all_channel_servers(std::string  server_tag_, bool sync);

int			do_dbthread_func()
{
	std::string srvtag = the_config.server_tag_;
	channel_servers.reset(new std::vector<channel_server>());
	load_all_channel_servers(srvtag, true);
	time_counter ts;
	ts.restart();
	while (!glb_exit)
	{
		unsigned int idle = 0;
		bool busy = false;
		if (!db_delay_helper_.pop_and_execute())
			idle++;
		else { idle = 0; }

		if (ts.elapse() > 2000) {
			load_all_channel_servers(srvtag, true);
			ts.restart();
		}

		db_thread_func_2();

		if (!busy) {
			boost::posix_time::milliseconds ms(1);
			boost::this_thread::sleep(ms);
		}
	}
	return 0;
}


int			db_thread_func()
{
	int ret = 0;
	__try {
		ret = do_dbthread_func();
	}
	__except (MyUnhandledExceptionFilter(GetExceptionInformation()),
		EXCEPTION_EXECUTE_HANDLER) {
		glb_exit = true;
	}
	
	return ret;
};

int	handle_pending_logins()
{
	while (pending_login_users_.size() > 0)
	{
		player_ptr pp;
		if(!pending_login_users_.pop_front(pp))
			break;

		auto itp = online_players.find(pp->uid_);
		if (itp == online_players.end() ){
			online_players.insert(std::make_pair(pp->uid_, pp));
		}
		//如果用户已在线
		else{
			msg_same_account_login msg;
			auto conn = itp->second->from_socket_.lock();
			if (conn == pp->from_socket_.lock()){
				conn->is_login_ = false;
				msg_user_login_ret msg;
				response_user_login(pp, conn, msg);
				return error_success;
			}
			if(conn.get())
				send_protocol(conn, msg, true);
			replace_map_v(online_players, std::make_pair(pp->uid_, pp));
		}

		auto pconn = pp->from_socket_.lock();
		if (pconn.get()){
			i_log_system::get_instance()->write_log(loglv_info, "user pickup to world succ.[%s]", pp->mobile_.c_str());
			pconn->is_login_ = false;
			send_all_game_to_player(pconn);
			send_all_present_to_player(pconn);

			msg_user_login_ret msg;
			response_user_login(pp, pconn, msg);

			msg_user_image msg_headpic;
			msg_headpic.uid_ = pp->uid_;
			send_image(msg_headpic, pp->headpic_, pconn);
			extern void	send_all_items_to_player(std::string uid, boost::shared_ptr<koko_socket> psock);
			send_all_items_to_player(pp->uid_, pconn);
		}
		else{
			i_log_system::get_instance()->write_log(loglv_info, "user pickup to world failed on connection not exist.");
		}
	}
	return 0;
}

int handle_player_msgs()
{
	auto it_cache = glb_msg_list_.begin();
	while (it_cache != glb_msg_list_.end())
	{
		auto cmd = *it_cache;	it_cache++;
		auto pauth_msg = boost::dynamic_pointer_cast<msg_authrized<koko_socket_ptr>>(cmd);
		//如果需要授权的消息，没有得到授权，则退出
		if (pauth_msg.get() && 
			!pauth_msg->from_sock_->is_authorized()){
			pauth_msg->from_sock_->close(false);
		}
		else{
			int ret = cmd->handle_this();
			if (ret != 0){
				msg_common_reply<koko_socket_ptr> msg;
				msg.err_ = ret;
				msg.rp_cmd_ = cmd->head_.cmd_;
				send_protocol(cmd->from_sock_, msg);
			}
		}
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
		i_log_system::get_instance()->write_log(loglv_error, "!!!!!net service start failed!!!!");
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
				it = online_players.erase(it);
		}
		else{
			it++;
		}
	}
}

int		load_all_games()
{
	all_games.clear();
	std::string sql = "SELECT `id`,`tech_type`,\
		`dir`,	`exe`,	`update_url`,	`help_url`,\
		`game_name`,	`thumbnail`,	`solution`, `no_embed`, `catalog`, rankorder \
		FROM `setting_game_list` where `state` = 0 and `tech_type` in (0,1,5) ";
	Query q(*db_);
	if (!q.get_result(sql))
		return error_sql_execute_error;

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
		pgame->order_ = q.getval();
		all_games.insert(std::make_pair(pgame->id_, pgame));
	}
	q.free_result();

	sql = "SELECT `gameid`,	`roomid`,	`room_name`,\
		`room_desc`,	`thumbnail`,	`srvip`,		`srvport`, `require` \
		FROM `setting_game_room_info`";

	if (!q.get_result(sql))
		return error_sql_execute_error;

	while (q.fetch_row())
	{
		game_room_inf inf;
		inf.game_id_ = q.getval();
		inf.room_id_ = q.getval();
		COPY_STR(inf.room_name_, q.getstr());
		COPY_STR(inf.room_desc_, q.getstr());
		COPY_STR(inf.thumbnail_, q.getstr());
		COPY_STR(inf.ip_, q.getstr());
		inf.port_ = q.getval();
		COPY_STR(inf.require_, q.getstr());
		if (all_games.find(inf.game_id_) != all_games.end()){
			all_games[inf.game_id_]->rooms_.push_back(inf);
		}
	}
	q.free_result();

	return error_success;
}

void	clean()
{
	task.reset();

	//继续执行没保存完的sql语句

	db_delay_helper_.pop_and_execute();

	timer_srv->stop();
	timer_srv.reset();

	the_net->stop();
	the_net.reset();
}

class test1 : public boost::enable_shared_from_this<test1>
{
public:

};

int run()
{
	timer_srv.reset(new boost::asio::io_service());
	the_net.reset(new koko_net_server());
	i_log_system::get_instance()->start();
//	show_live_helper::get_instance()->set_host("192.168.17.51", "8082");//TESTCODE
	channel_servers.reset(new std::vector<channel_server>());
	boost::system::error_code ec;
	the_config.load_from_file();
	db_.reset(new utf8_data_base(the_config.accdb_host_, the_config.accdb_user_,the_config.accdb_pwd_, the_config.accdb_name_));
	if(!db_->grabdb()){
		std::cout << "database start failed!!!!";
		return -1;
	}

	db_delay_helper_.set_db_ptr(db_);
	
	if (start_network() != 0){
		goto _EXIT;
	}

	int idle_count = 0;

	task_on_5sec* ptask = new task_on_5sec(*timer_srv);
	task.reset(ptask);
	ptask->schedule(1000);
	ptask->routine();

	boost::thread* trd = new boost::thread(db_thread_func);

	while(!glb_exit)
	{
		bool busy = false;

		timer_srv->reset();
		timer_srv->poll();
		
		the_net->ios_.reset();
		the_net->ios_.poll();
		
		db_delay_helper_.poll();
		
		http_request_monitor::get_instance()->step();

		handle_pending_logins();

		pickup_player_msgs(busy);
		handle_player_msgs();

		update_players();

		smsg_ptr pmsg;
		broadcast_msg_.pop_front(pmsg);
		if(pmsg.get())
			broadcast_msg(pmsg);

		boost::posix_time::milliseconds ms(20);
		boost::this_thread::sleep(ms);
		
	}

_EXIT:
	trd->join();
	delete trd;
	i_log_system::get_instance()->stop();
	return 0;
}

static int dmp = 0;
LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	HANDLE lhDumpFile = CreateFileA(("koko_center_crash" + boost::lexical_cast<std::string>(dmp) + ".dmp").c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL ,NULL);

	MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
	loExceptionInfo.ExceptionPointers = ExceptionInfo;
	loExceptionInfo.ThreadId = GetCurrentThreadId();
	loExceptionInfo.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),lhDumpFile,  MiniDumpWithFullMemory, &loExceptionInfo, NULL, NULL);

	CloseHandle(lhDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int cnt, char* args[])
{
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE){
		return -1;
	}

	if (cnt > 1 && strstr(args[1], "-nocatch") != nullptr){
		if (run() == 0){
			cout <<"exited!"<< endl;
		}
		else
			cout <<"exited with error!"<< endl;
	}
	else{
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
	load_vip_privilege();
	load_all_channel_servers(the_config.server_tag_, false);
	
	//跨服通知比赛开始
	{
		std::string sql = "select rowid, uid, matchid, matchins, register_count, ip, port from setting_match_notify order by rowid asc";
		Query q(*db_);
		if(q.get_result(sql) && q.fetch_row()){
			std::vector<__int64> v;
			do{
				__int64 rowid = q.getbigint();
				std::string uid = q.getstr();
				//如果用户存在,则通知比赛开始.
				player_ptr pp = get_player(uid);

				if (pp.get()){
					msg_confirm_join_game_deliver msg;
					msg.match_id_ = q.getval();
					msg.ins_id_ = q.getstr();
					msg.register_count_ = q.getval();
					msg.ip_ = q.getstr();
					msg.port_ = q.getval();

					auto pcnn = pp->from_socket_.lock();
					if (pcnn.get()){
						send_protocol(pp->from_socket_.lock(), msg);
					}
					v.push_back(rowid);
				}
				if (v.size() > 100)	{
					Query q2(*db_);
					std::string rowids = combin_str(v, ",", false);
					sql = "delete from setting_match_notify where rowid in(" + rowids + ")";
					q2.execute(sql);
					v.clear();
				}
			}
			while (q.fetch_row());

			if (v.size() > 0)	{
				Query q2(*db_);
				std::string rowids = combin_str(v, ",", false);
				sql = "delete from setting_match_notify where rowid in(" + rowids + ")";
				q2.execute(sql);
			}
		}
	}

	//黑名单用户T下线.
	{
		std::string sql = "select uid from user_blacklist where create_time > (unix_timestamp(date(now())) - 100) and state = 1";
		Query q(*db_);
		if (q.get_result(sql)){
			while (q.fetch_row())
			{
				std::string uid = q.getstr();
				player_ptr pp = get_player(uid);
				if (pp.get()){
					auto pcnn = pp->from_socket_.lock();
					if (pcnn.get()){
						//通知用户
						msg_black_account msg;
						send_protocol(pcnn, msg, true, false);
					}
				}
			}
		}
	}
	
	//同步用户货币变化
	{
		std::string sql = "select rowid, uid, itemid from user_item_changes "
			"where for_server = 'center'  order by rowid asc";
		Query q(*db_);
		if (q.get_result(sql) && q.fetch_row()) {
			std::vector<__int64> v;
			do {
				__int64 rowid = q.getbigint();
				std::string uid = q.getstr();
				int	itemid = q.getval();
				//如果用户存在,则更新
				if (get_player(uid).get()) {
					sync_user_item(uid, itemid);
					i_log_system::get_instance()->write_log(loglv_debug, "sync item -> uid:%s, itemid=%d", uid.c_str(), itemid);
				}
				v.push_back(rowid);
			} while (q.fetch_row());

			if (v.size() > 0) {
				Query q2(*db_);
				std::string rowids = combin_str(v, ",", false);
				sql = "delete from user_item_changes where rowid in(" + rowids + ")";
				q2.execute(sql);
			}
		}
	}

	int csize = 0, nz = 0;
	if (channel_servers.get()){
		auto chnsrv = *channel_servers;
		nz = std::accumulate(chnsrv.begin(), chnsrv.end(), 0,
			[](int val, const channel_server& it)->int {
			return it.game_ids_.size() + val;
		});
	}
	
	static int aa = 0;
	aa++;
	if (aa >= 5){
		i_log_system::get_instance()->write_log(loglv_debug, "game is running, online_players:%d channel_servers : %d "
			"channel_servers_games:%d"
			"glb_running_task_: %d ",
			online_players.size(),
			csize,
			nz,
			task_info::glb_running_task_.size());
		aa = 0;
	}

	return task_info::routine_ret_continue;
}
