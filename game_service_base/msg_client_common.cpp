#include "msg_server_common.h"
#include "msg_client_common.h"
#include "error_define.h"
#include "telfee_match.h"
#include "service_config_base.h"
#include "game_service_base.h"
#include "game_service_base.hpp"
#include "gift_system.h"

std::map<std::string, int> g_online_getted_;
void sql_trim(std::string& str)
{
	for (unsigned int i = 0; i < str.size(); i++)
	{
		if (str[i] == '\''){
			str[i] = ' ';
		}
	}
}

int		get_config_index(scene_set* pset)
{
	for (int i = 0; i < the_service.scenes_.size(); i++)
	{
		if (pset == &the_service.scenes_[i]){
			return i;
		}
	}
	return 0;
}

class remove_session_task : public task_info
{
public:
	std::string  sess;
	remove_session_task() : task_info(the_service.timer_sv_)
	{

	}

	int		routine()
	{
		auto it_sess = the_service.login_sessions_.find(sess);

		if (it_sess != the_service.login_sessions_.end()){
			the_service.login_sessions_.erase(it_sess);
		}

		return task_info::routine_ret_break;
	}
};

int	accept_user_login(remote_socket_ptr from_sock_,  std::string session_)
{
	if (the_service.players_.size() > MAX_USER){
		return SYS_ERR_SERVER_FULL;
	}

	if (the_service.the_config_.shut_down_){
		return SYS_ERR_SESSION_TIMEOUT;
	}

	boost::system::error_code ec;
	auto rmt = from_sock_->remote_endpoint();
	if (ec.value() != 0){
		return SYS_ERR_SESSION_TIMEOUT;
	}

	auto it_sess = the_service.login_sessions_.find(session_);
	if (it_sess != the_service.login_sessions_.end()){
		session& ses = it_sess->second;
		int sec = (boost::posix_time::second_clock::local_time() - ses.create_time_).total_seconds();
		if (ses.remote_.address() != rmt.address()){
			return SYS_ERR_SESSION_TIMEOUT;
		}
		//半小时超时
		else if (sec > 1800){
			return SYS_ERR_SESSION_TIMEOUT;
		}
		//1秒内不能重复登录
		else if (sec < 1) {
			return SYS_ERR_SESSION_TIMEOUT;
		}
	}
	session ses; 
	ses.remote_ = rmt;
	ses.session_key_ = session_;
	ses.create_time_ = boost::posix_time::second_clock::local_time();
	replace_map_v(the_service.login_sessions_, make_pair(session_, ses));

	remove_session_task* ptask = new remove_session_task();
	ptask->sess = session_;
	task_ptr task(ptask);
	ptask->schedule(boost::posix_time::hours(24).total_milliseconds());	return ERROR_SUCCESS_0;
}

static player_ptr		user_login(remote_socket_ptr from_sock_, 
															 std::string platform_uid,
															 std::string uname_,
															 std::string headpic, 
															 std::string uidx_, 
															 std::string platform_id,
															 bool	is_guest = false)
{

	from_sock_->set_authorized();
	sql_trim(uname_);
	sql_trim(headpic);
	
	bool break_join = false;
	std::string uid_ = platform_uid;
	bool isbot = false, is_new_player = false;
	if (strstr(uid_.c_str(), "ipbot_test") == uid_.c_str()){
		isbot = true;
	}
	else{
		//不同平台的玩家，用前辍标记
		if (is_guest) {
			uid_ =  platform_id + "gu" + uid_;
		}
		else {
			uid_ = platform_id + uid_;
		}
	}

	player_ptr pp(new game_player_type());
	player_ptr oldp = the_service.get_player(uid_);
	if(oldp.get()){
		remote_socket_ptr pold = oldp->from_socket_.lock();
		//如果是另一个socket发过来的，则断开旧连接
		if (pold != from_sock_){
			if (pold.get())	{
				msg_logout msg;
				send_protocol(pold, msg, true);
				glb_http_log.write_log("same account login, the old force offline!");
			}
		}
		//如果是重复发,则啥事不干
		else{
			break_join = true;
		}
		*pp = *oldp;
		pp->do_not_copy_these_fields();
	}
	else {
		pp->uid_ = uid_;
		pp->is_bot_ = isbot;
		is_new_player = true;
	}

	pp->name_ = uname_;
	pp->head_ico_ = headpic;
	pp->uidx_ = uidx_;
	pp->platform_uid_ = platform_uid;
	pp->platform_id_ = platform_id;
    pp->from_socket_ = from_sock_;
	from_sock_->the_client_ = pp;

	//机器人登录，不放入玩家列表。
	if (!pp->is_bot_) {
		replace_map_v(the_service.players_, std::make_pair(pp->uid_, pp));
	}

	__int64 iid = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_USER_IID);
	if (iid == 0){
		if(the_service.cache_helper_.account_cache_.send_cache_cmd<__int64>(pp->uid_, "CMD_BIND", 0, iid) != ERROR_SUCCESS_0){
			glb_http_log.write_log("bind user id failed!, user = %s", pp->uid_.c_str());
		}
		else{
			//fix bug for guest first login can't get iid by liufapu
			iid = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_USER_IID);
		}
	}

	pp->iid_ = iid;
	pp->device_id_ = from_sock_->device_id_;

	SEND_SERVER_PARAM();


	extern int get_level(__int64& exp);

	msg_user_login_ret msg_ret;
	msg_ret.iid_ = (int) iid;
	COPY_STR(msg_ret.uid_, pp->uid_.c_str());
	COPY_STR(msg_ret.head_pic_, pp->head_ico_.c_str());
	__int64 exp = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_EXP, the_service.the_config_.game_id);
	int lv = get_level(__int64(exp));
	msg_ret.lv_ = lv;
	msg_ret.exp_ = (double)exp;
	extern int glb_lvset[50];
	msg_ret.exp_max_ = glb_lvset[lv > 29 ? 29 : lv];
	msg_ret.currency_ = (double)the_service.cache_helper_.get_currency(pp->uid_);
	pp->credits_ = msg_ret.currency_;
	pp->lv_ = lv;

	COPY_STR(msg_ret.uname_, uname_.c_str());

 	pp->send_msg_to_client(msg_ret);
	if (!pp->is_bot_){
		if (!break_join){
			Database& db(*the_service.gamedb_);
			BEGIN_REPLACE_TABLE("log_online_players");
			SET_FIELD_VALUE("uid", pp->uid_);
			auto soc = pp->from_socket_.lock();
			if (soc.get()){
				auto rmt = soc->remote_endpoint();
				SET_FIELD_VALUE("ip", rmt.address().to_string());
			}
			SET_FIELD_VALUE("mac", pp->device_id_);
			SET_FINAL_VALUE("name", pp->name_);
			the_service.delay_db_game_.push_sql_exe(pp->uid_ + "login", "", str_field, true);
		}

		the_service.cache_helper_.set_var(pp->uid_ + KEY_USERISONLINE, -1, __int64(1));
		glb_http_log.write_log("auto_trade_in begin.%s", pp->uid_.c_str());
		the_service.auto_trade_in(pp, true);
		pp->counter_online();
		pp->update_last_online();
	}

	//如果是机器人
	if (pp->is_bot_) {
		logic_ptr plogic = pp->the_game_.lock();
		//如果机器人是空闲的
		if (!plogic.get()) {
			replace_map_v(the_service.free_bots_, std::make_pair(pp->uid_, pp));
		}
		//如果机器人在游戏中,则放入玩家列表
		else {
			replace_map_v(the_service.players_, std::make_pair(pp->uid_, pp));
		}
	}
	EXTRA_LOGIN_CODE();
	return pp;
}

int msg_url_user_login_req2::handle_this()
{
	if (strstr(uid_, "ipbot_test") == uid_){
		//机器人登录，sex必须为-1;
		if (iid_ != -1){
			return SYS_ERR_SESSION_TIMEOUT;
		}
	}

	if (uid_ == "") 
		return SYS_ERR_SESSION_TIMEOUT;

	std::string sign_pattn, s;
	s.assign(uid_, max_guid);
	sign_pattn +=s.c_str();
	sign_pattn +="|";

	s.assign(uname_, max_name);
	sign_pattn += s.c_str();
	sign_pattn +="|";

	sign_pattn += lx2s(vip_lv_);
	sign_pattn +="|";

	s.assign(head_icon_, max_guid);
	sign_pattn += s.c_str();
	sign_pattn +="|";

	sign_pattn += lx2s(platform_);
	sign_pattn +="|";

	s.assign(sn_, max_guid);
	sign_pattn += s.c_str();
	sign_pattn +="|";

	sign_pattn += lx2s(iid_);
	sign_pattn +="|";

	sign_pattn += the_service.the_config_.php_sign_key_;

	std::string remote_sign = url_sign_;
	std::string sign_key = md5_hash(sign_pattn);
	if(sign_key != remote_sign){
		glb_http_log.write_log("wrong sign, local: %s remote:%s", sign_pattn.c_str(), remote_sign.c_str());
		return SYS_ERR_SESSION_TIMEOUT;
	}

	if (the_service.players_.size() > 500){
		return SYS_ERR_SERVER_FULL;
	}

	if (the_service.the_config_.shut_down_){
		return SYS_ERR_SESSION_TIMEOUT;
	}

	int	ret = accept_user_login(from_sock_, sn_);
	if (ret != ERROR_SUCCESS_0){
		return ret;
	}


	the_service.cache_helper_.set_var<std::string>(std::string("7158") + std::string(uid_) + KEY_USER_UNAME, -1, uname_);
	std::string uname = uname_;
	the_service.cache_helper_.set_var<std::string>(std::string("7158") + std::string(uid_) + KEY_USER_HEADPIC, the_service.the_config_.game_id, head_icon_);
	std::string headpic = head_icon_;

	player_ptr pp = user_login(from_sock_, uid_, uname_, head_icon_, platform_, "7158");
	return ERROR_SUCCESS_0;
}


int msg_url_user_login_req::handle_this()
{
	if (strstr(uid_, "ipbot_test") == uid_){
		//机器人登录，sex必须为-1;
		if (iid_ != -1){
			return SYS_ERR_SESSION_TIMEOUT;
		}
	}

	if (uid_ == "") 
		return SYS_ERR_SESSION_TIMEOUT;

	std::string sign_pattn, s;
	s.assign(uid_, max_guid);
	sign_pattn +=s.c_str();
	sign_pattn +="|";

	s.assign(uname_, max_name);
	sign_pattn += s.c_str();
	sign_pattn +="|";

	s.assign(head_icon_, max_guid);
	sign_pattn += s.c_str();
	sign_pattn +="|";

	s.assign(sn_, max_guid);
	sign_pattn += s.c_str();
	sign_pattn +="|";

	sign_pattn += the_service.the_config_.php_sign_key_;

	std::string remote_sign = url_sign_;
	std::string sign_key = md5_hash(sign_pattn);
	if(sign_key != remote_sign){
		glb_http_log.write_log("wrong sign, local: %s remote:%s", sign_pattn.c_str(), remote_sign.c_str());
		return SYS_ERR_SESSION_TIMEOUT;
	}

	int	ret = accept_user_login(from_sock_, sn_);
	if (ret != ERROR_SUCCESS_0){
		return ret;
	}


	the_service.cache_helper_.set_var<std::string>(std::string(platform_) + "gu" + std::string(uid_) + KEY_USER_UNAME, -1, uname_);
	std::string	uname = uname_;

	the_service.cache_helper_.set_var<std::string>(std::string(platform_) + "gu" + std::string(uid_) + KEY_USER_HEADPIC, the_service.the_config_.game_id, head_icon_);
	std::string	headpic = head_icon_;

	std::string idx = boost::lexical_cast<std::string>(iid_);
	player_ptr pp = user_login(from_sock_, uid_, uname, headpic, idx, platform_, true);
	return ERROR_SUCCESS_0;
}

int msg_game_lst_req::handle_this()
{
	if (from_sock_.get()){
		msg_room_info msg;
		if (page_set_ > 0){
			for (unsigned int i = page_ * page_set_; i < the_service.the_golden_games_.size() && i < (page_ + 1) * page_set_; i++)
			{
				msg.room_id_ = the_service.the_golden_games_[i]->id_;
				msg.free_pos_ = MAX_SEATS - the_service.the_golden_games_[i]->get_playing_count();
				send_protocol(from_sock_, msg);
			}
		}
	}
	return ERROR_SUCCESS_0;
}

bool	has_free_seats(player_ptr pp, logic_ptr pgame, logic_ptr old_game, scene_set* pset)
{
	if(old_game.get()){
		return pgame->get_playing_count() < MAX_SEATS && pgame != old_game &&
			pgame->scene_set_->id_ == old_game->scene_set_->id_ && !pgame->is_private_game_ &&
			!pgame->is_ingame(pp);
	}
	else if (pset){
		return pgame->get_playing_count() < MAX_SEATS && 
			pgame->scene_set_->id_ == pset->id_ && !pgame->is_private_game_ &&
			!pgame->is_ingame(pp);
	}
	else
		return false;
}

void quick_join_game(player_ptr pp, logic_ptr old_game, scene_set* pset = NULL)
{
	vector<logic_ptr> free_games;
	//如果是换桌，需要找出和原来一样场次配置的游戏
	if (old_game.get()){
		for (unsigned int i = 0; i < the_service.the_golden_games_.size(); i++)
		{
			if (has_free_seats(pp, the_service.the_golden_games_[i], old_game, pset))	{
					free_games.push_back(the_service.the_golden_games_[i]);
			}
		}
	}
	//如果是新进入
	else if(pset){
		for (unsigned int i = 0; i < the_service.the_golden_games_.size(); i++)
		{
			//换桌要换到相同场次的桌
			if (has_free_seats(pp, the_service.the_golden_games_[i], old_game, pset))	{
					free_games.push_back(the_service.the_golden_games_[i]);
			}
		}
	}

	//如果没有游戏可以进，则新建一个游戏
	if (free_games.empty() || the_service.the_golden_games_.empty()){
		logic_ptr pl;
		if (pset && pset->is_free_){
			pl.reset(new game_logic_type(1));
		}
		else{
			pl.reset(new game_logic_type(0));
		}
		the_service.the_golden_games_.push_back(pl);
		//如果是换桌，则复制场次配置
		if (old_game.get()){
			pl->scene_set_ = old_game->scene_set_;
			pl->config_index_ = old_game->config_index_;
		}
		else {
			pl->scene_set_ = pset;
			pl->config_index_ = get_config_index(pset);
		}
		pl->start_logic();
		pl->player_login(pp, -1);
	}
	//随机进入一个有空位的游戏
	else{
		logic_ptr pl = free_games[0];
		pl->player_login(pp, -1);
	}
};

int msg_enter_game_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get())
		return SYS_ERR_CANT_FIND_CHARACTER;

	if (the_service.scenes_.empty()) {
		return SYS_ERR_CANT_FIND_CHARACTER;
	}

	int config_index = room_id_ >> 24;
	int room_id = room_id_ & 0x00FFFFFF;

	//如果上次已经在游戏中，则替换掉上次的
	logic_ptr pingame = pp->the_game_.lock();
	if (pingame.get()){
		//如果是换桌
		if (room_id == 0xFFFFFF){
			pingame->leave_game(pp, pp->pos_, 1);
			quick_join_game(pp, pingame);
			return ERROR_SUCCESS_0;
		}
		//如果上次已经在游戏中没退出去，则重新进入游戏
		else{
			if (pingame->is_match_game_){
				pingame->leave_game(pp, pp->pos_, 5);
			}
			else{
				if (pingame->scene_set_ && config_index < the_service.scenes_.size()){
					if (pingame->scene_set_->id_ == the_service.scenes_[config_index].id_){
						pingame->player_login(pp, pp->pos_);
						return ERROR_SUCCESS_0;
					}
					else{
						pingame->leave_game(pp, pp->pos_, 5);
					}
				}
				else{
					pingame->leave_game(pp, pp->pos_, 5);
				}
			}
		}
	}

	if (config_index >= the_service.scenes_.size()){
		config_index = the_service.scenes_.size() - 1;
	}

	//room_id == 0 自动进入金币房间, room_id == 0xFFFFFE 自动进入积分房间
	if (room_id == 0 || room_id == 0xFFFFFE){
		__int64 c = 0;
		//查询金币
		if (room_id == 0){
			c = the_service.cache_helper_.get_currency(pp->uid_);
		}
		//查询积分
		else {
			c = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_EXP, the_service.the_config_.game_id);
		}

		//数量不够，不让进
		if (the_service.scenes_[config_index].gurantee_set_ > 0 && c < the_service.scenes_[config_index].gurantee_set_ && !pp->is_bot_){
			msg_low_currency msg;
			msg.require_ = the_service.scenes_[config_index].gurantee_set_;
			pp->send_msg_to_client(msg);
			return ERROR_SUCCESS_0;
		}
		quick_join_game(pp, logic_ptr(), &the_service.scenes_[config_index]);
	}
	//进入指定房间
	else{
		auto itf = std::_Find_pr(the_service.the_golden_games_.begin(), the_service.the_golden_games_.end(), room_id,
			[](logic_ptr pl, int roomid)->bool{
				return pl->id_ == roomid;
		});

		logic_ptr pl;
		if(itf == the_service.the_golden_games_.end()){
			pl.reset(new game_logic_type(0));
			the_service.the_golden_games_.push_back(pl);
			pl->id_ = room_id;
			pl->is_private_game_ = true;
			pl->config_index_ = config_index;
			pl->scene_set_ = &the_service.scenes_[config_index];
			pl->start_logic();
		}
		else{
			pl = *itf;
		}
		if (pl->get_playing_count() >= MAX_SEATS){
			return SYS_ERR_ROOM_FULL;
		}
		else
			pl->player_login(pp, -1);

	}
	return ERROR_SUCCESS_0;
}

int glb_lvset[50] = {
	30	,
	50	,
	70	,
	90	,
	110	,
	150	,
	220	,
	330	,
	480	,
	600	,
	740	,
	875	,
	1010,
	1200,
	1500,
	1800,
	2100,
	2400,
	2700,
	3000,
	3500,
	4200,
	5000,
	6000,
	6750,
	7580,
	8410,
	9240,
	12000,
	14500,
	17800,
	21000,
	23900,
	27150,
	30290,
	32300,
	36570,
	39710,
	42850,
	45990,
	49130,
	52270,
	55410,
	58550,
	61690,
	64830,
	67970,
	71110,
	74250,
	77390	
};

int glb_giftset[50] = {
	100,
	150,
	200,
	250,
	300,
	350,
	400,
	450,
	500,
	550,
	1000,
	1100,
	1200,
	1300,
	1400,
	1500,
	1600,
	1700,
	1800,
	1900,
	2000,
	2200,
	2400,
	2600,
	2800,
	3000,
	3200,
	3400,
	3600,
	3800,
	4000,
	4200,
	4400,
	4600,
	4800,
	5000,
	5200,
	5400,
	5600,
	5800,
	6000,
	6200,
	6400,
	6600,
	6800,
	7000,
	7200,
	7400,
	7600,
	8000
};

int get_level(__int64& exp1)
{
	__int64 exp = exp1;
	int i = 0;
	for (; i < 30; i++)
	{
		if (exp < glb_lvset[i]){
			return i + 1;
		}
	}
	return i + 1;
}

int glb_everyday_set[7] = {
	5000,
	6000,
	7000,
	8000,
	9000,
	10000,
	11000
};

int msg_get_prize_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	logic_ptr pgame = pp->the_game_.lock();
	//领取等级奖励
	if (type_ == 0){
		__int64 exp = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_EXP, the_service.the_config_.game_id);
		int lv = get_level(__int64(exp));
		//读取已领取级别
		__int64 lv_get = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_LEVEL_GIFT, the_service.the_config_.game_id);
		//查看
		if (data_ == 0){
			msg_everyday_gift msg;
			msg.type_ = type_;
			msg.conitnue_days_ = (int)lv_get;
			if (lv_get < lv){
				msg.getted_ = 1;
			}
			else{
				msg.getted_ = 0;
			}
			pp->send_msg_to_client(msg);
		}
		else{
			if (lv_get < lv){
				__int64 out_count = 0;
				bool sync = !pgame.get();

				int lv_gift = lv_get;
				if (lv_gift > 28) lv_gift = 28;

				int r = the_service.cache_helper_.give_currency(pp->uid_, glb_giftset[lv_gift], out_count, true);
				if (r == ERROR_SUCCESS_0){
					the_service.add_stock(-glb_giftset[lv_gift]);
					Database& db = *the_service.gamedb_;
					BEGIN_INSERT_TABLE("log_send_gold_detail");
					SET_FIELD_VALUE("reason", 1);
					SET_FIELD_VALUE("uid", pp->uid_);
					SET_FINAL_VALUE("val", glb_giftset[lv_gift]);
					EXECUTE_IMMEDIATE();

					the_service.cache_helper_.set_var(pp->uid_ + KEY_LEVEL_GIFT, the_service.the_config_.game_id, lv_get + 1);
					msg_currency_change msg;   
					msg.credits_ = glb_giftset[lv_gift];
					msg.why_ = msg_currency_change::why_levelup;
					pp->send_msg_to_client(msg);
					the_service.cache_helper_.add_var(pp->uid_ + KEY_CUR_TRADE_CAP, -1, glb_giftset[lv_gift]);
				}
			}
		}
	}
	//领取每日登录奖励
	else if(type_ == 1) {
		__int64 last_get_tick = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_EVERYDAY_LOGIN_TICK, the_service.the_config_.game_id); //上次领取时间
		auto dat1 = boost::date_time::day_clock<date>::local_day();
		auto dat2 = from_string("1900-1-1");
		__int64 this_day_tick = (dat1 - dat2).days();

		if (data_ == 1){
			//第一次登录，还不存在连续登录数据
			if (last_get_tick == 0){
				the_service.cache_helper_.set_var(pp->uid_ + KEY_EVERYDAY_LOGIN_TICK, the_service.the_config_.game_id, this_day_tick);
				the_service.cache_helper_.set_var(pp->uid_ + KEY_EVERYDAY_LOGIN, the_service.the_config_.game_id, __int64(1));
			}
			else{
				//如果是第二天登录
				if ((last_get_tick + 1) == this_day_tick){
					the_service.cache_helper_.set_var(pp->uid_ + KEY_EVERYDAY_LOGIN_TICK, the_service.the_config_.game_id, this_day_tick);
					the_service.cache_helper_.add_var(pp->uid_ + KEY_EVERYDAY_LOGIN, the_service.the_config_.game_id, __int64(1));
				}
				//连续登录断掉了
				else if(last_get_tick != this_day_tick){
					the_service.cache_helper_.set_var(pp->uid_ + KEY_EVERYDAY_LOGIN_TICK, the_service.the_config_.game_id, this_day_tick);
					the_service.cache_helper_.set_var(pp->uid_ + KEY_EVERYDAY_LOGIN, the_service.the_config_.game_id, __int64(1));
				}
				//如果最后领取时间是今天，则返回
				else {
					return ERROR_SUCCESS_0;
				}
			}

			__int64 continue_days = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_EVERYDAY_LOGIN, the_service.the_config_.game_id);	//连续签到多少天了
			if (continue_days > 7){
				continue_days = 7;
			}

			__int64 out_count = 0;
			bool sync = !pgame.get();
			int r = the_service.cache_helper_.give_currency(pp->uid_, glb_everyday_set[continue_days - 1], out_count, true);
			if (r == ERROR_SUCCESS_0){
				the_service.add_stock(-glb_everyday_set[continue_days - 1]);
				Database& db = *the_service.gamedb_;
				BEGIN_INSERT_TABLE("log_send_gold_detail");
				SET_FIELD_VALUE("reason", 3);
				SET_FIELD_VALUE("uid", pp->uid_);
				SET_FINAL_VALUE("val", glb_everyday_set[continue_days - 1]);
				EXECUTE_IMMEDIATE();

				msg_currency_change msg;
				msg.credits_ = glb_everyday_set[continue_days - 1];
				msg.why_ = msg_currency_change::why_everyday_login;
				pp->send_msg_to_client(msg);
				the_service.cache_helper_.add_var(pp->uid_ + KEY_CUR_TRADE_CAP, -1, glb_everyday_set[continue_days - 1]);
			}
		}
		else{
			msg_everyday_gift msg;
			msg.type_ = type_;
			msg.conitnue_days_ = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_EVERYDAY_LOGIN, the_service.the_config_.game_id);	//连续签到多少天了
			if (last_get_tick == 0){
				msg.conitnue_days_ = 1;
			}
			else{
				//如果是第二天登录
				if ((last_get_tick + 1) == this_day_tick){
					msg.conitnue_days_++;
				}
				//连续登录断掉了
				else if(last_get_tick != this_day_tick){
					msg.conitnue_days_ = 1;
				}
				//如果最后领取时间是今天，则返回
				else {
					msg.getted_ = 1;
				}
			}
			pp->send_msg_to_client(msg);
		}
	}
    //领取在线奖励
	else if (type_ == 2){
		if (data_ == 0){
			msg_everyday_gift msg;
			msg.type_ = type_;
			msg.conitnue_days_ = pp->online_prize_;
			if (pp->online_prize_ == the_service.the_config_.online_prize_.size() - 1){
				msg.getted_ = -1;
			}
			else{
				msg.getted_ = pp->online_prize_avilable_;
			}

			if (pp->online_counter_.get()){
				msg.extra_data_ = pp->online_counter_->time_left();
			}
			else{
				msg.extra_data_ = 0;
			}
			pp->send_msg_to_client(msg);
		}
		else{
			if (!pp->online_prize_avilable_ || pp->online_prize_avilable_ == 2) return -1;
			if (pp->online_prize_ == the_service.the_config_.online_prize_.size() - 1) return -1;

			online_config& cnf = the_service.the_config_.online_prize_[pp->online_prize_];
			__int64 out_count = 0;
			bool sync = !pgame.get();
			int r = the_service.cache_helper_.give_currency(pp->uid_, cnf.prize_, out_count, true);
			if (r == ERROR_SUCCESS_0){
				the_service.add_stock(-cnf.prize_);
				Database& db = *the_service.gamedb_;
				BEGIN_INSERT_TABLE("log_send_gold_detail");
				SET_FIELD_VALUE("reason", 2);
				SET_FIELD_VALUE("uid", pp->uid_);
				SET_FINAL_VALUE("val", cnf.prize_);
				EXECUTE_IMMEDIATE();

				msg_currency_change msg;
				msg.credits_ = cnf.prize_;
				msg.why_ = msg_currency_change::why_online_prize;
				pp->send_msg_to_client(msg);

				g_online_getted_[pp->uid_] = pp->online_prize_;
				the_service.cache_helper_.add_var(pp->uid_ + KEY_CUR_TRADE_CAP, -1, cnf.prize_);
				pp->online_prize_avilable_ = 2;
			}
			pp->counter_online();
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_everyday_sign_req::handle_this()
{
    player_ptr pp = from_sock_->the_client_.lock();
    if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

    boost::shared_ptr<GiftSystemEverydaySign> ges = the_service.gift_everyday_sign_;
    if (!ges.get())
    {
        glb_http_log.write_log("Gift System: everyday sign system has not init!");
        return -1;
    }

    if (ges->GetConfig().empty())
    {
        glb_http_log.write_log("Gift System: everyday sign config list is NULL!");
        return -1;
    }

    //查询签到情况
    if (type_ == 0)
    {
        msg_everyday_sign_info msg;
        msg.getted_ = ges->IsTodaySign(pp) ? 1 : 0;
        msg.sign_days_ = ges->GetSignDays(pp) % ges->GetConfig().size();
        msg.sign_result_ = 0;
        if (msg.getted_ == 1 && msg.sign_days_ == 0)
            msg.sign_days_ = ges->GetConfig().size();
        pp->send_msg_to_client(msg);
    }
    //查询奖励列表
    else if (type_ == 1)
    {
        auto& gifts = ges->GetConfig();
        msg_everyday_sign_rewards msg;
        char* p = msg.rewards_;
        int len = 0;

        //由低到高分隔符[:] [|] [\n]
        const char c1 = ':';
        const char c2 = '|';
        const char c3 = '\n';

        bool has_group = false;
        for (auto& group_ite : gifts)
        {
            has_group = true;

            bool has_pair = false;
            for (auto& data_ite : group_ite)
            {
                has_pair = true;
                std::string reward_id_str = boost::lexical_cast<std::string>(data_ite.reward_id);
                memcpy(p + len, reward_id_str.c_str(), reward_id_str.length());
                len += reward_id_str.length();
                p[len++] = c1;
                std::string item_id_str = boost::lexical_cast<std::string>(data_ite.item_id);
                memcpy(p + len, item_id_str.c_str(), item_id_str.length());
                len += item_id_str.length();
                p[len++] = c1;
                memcpy(p + len, data_ite.content.c_str(), data_ite.content.length());
                len += data_ite.content.length();
                p[len++] = c2;
            }
            if (has_pair)
                len--;

            p[len++] = c3;
        }
        if (has_group)
            len--;

        p[len] = 0;
        pp->send_msg_to_client(msg);
    }
    //签到
    else if (type_ == 2)
    {
        if (ges->IsTodaySign(pp))
            return -1;

        int days = ges->GetSignDays(pp);
        auto& gifts = ges->GetConfig();
        auto& group = gifts[days % gifts.size()];

        ges->DispatchReward(pp, group);
        ges->AddSignDays(pp);
        ges->SetTodaySign(pp);

        msg_everyday_sign_info msg;
        msg.getted_ = 1;
        msg.sign_days_ = ges->GetSignDays(pp) % gifts.size();
        msg.sign_result_ = 1;
        if (msg.sign_days_ == 0)
            msg.sign_days_ = gifts.size();
        pp->send_msg_to_client(msg);
    }
    return ERROR_SUCCESS_0;
}

int	msg_luck_wheel_req::handle_this()
{
    player_ptr pp = from_sock_->the_client_.lock();
    if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

    boost::shared_ptr<GiftSystemLuckWheel> glw = the_service.gift_luck_wheel_;
    if (!glw.get())
    {
        glb_http_log.write_log("Gift System: luck wheel system has not init!");
        return -1;
    }

    //请求剩余抽奖次数
    if (type_ == 0)
    {
        msg_luck_wheel_info msg;
        msg.count_ = glw->GetFreeCount(pp);
        msg.reward_index_ = -1;
        msg.show_reward_ = 0;
        pp->send_msg_to_client(msg);
    }
    //请求全局中奖信息
    else if (type_ == 1)
    {
        int max_record_num = 3;
        int cur_record_num = 0;

        msg_luck_wheel_record msg;
        char* p = msg.content_;

        int len = 0;
        bool has_content = false;

        char c1 = '|';
        char c2 = '\n';

        auto& records = glw->GetRecords();
        for (auto ite = records.rbegin(); ite != records.rend(); ite++)
        {
            if (cur_record_num >= max_record_num)
                break;

            const LuckWheelRecord& d = (*ite);

            //玩家昵称
            memcpy(p + len, d.player_name_.c_str(), d.player_name_.length());
            len += d.player_name_.length();
            p[len++] = c1;

            //玩家奖励索引
            std::string idx_string = boost::lexical_cast<std::string>(d.reward_index_);
            memcpy(p + len, idx_string.c_str(), idx_string.length());
            len += idx_string.length();
            p[len++] = c1;

            //时间
            memcpy(p + len, d.time_.c_str(), d.time_.length());
            len += d.time_.length();
            p[len++] = c2;

            has_content = true;
            cur_record_num++;
        }

        if (has_content)
            len--;

        p[len] = 0;
        pp->send_msg_to_client(msg);
    }
    //使用次数抽奖
    else if (type_ == 2)
    {
        //判断是否还有抽奖次数
        if (glw->GetFreeCount(pp) <= 0)
            return -1;

        int idx = glw->GenGiftIndex();
        if (idx == -1)
            return -1;

        glw->ReduceFreeCount(pp);           //减少抽奖次数

        //更新抽奖信息
        msg_luck_wheel_info msg;
        msg.count_ = glw->GetFreeCount(pp);
        msg.reward_index_ = idx;
        msg.show_reward_ = 0;
        pp->send_msg_to_client(msg);

        //延迟发放奖励
        GiftLuckWheelAwardTask* ptask = new GiftLuckWheelAwardTask(the_service.timer_sv_);
        task_ptr task(ptask);
        ptask->pp = pp;
        ptask->reward_index = idx;
        ptask->schedule(glw->GetDelayTime());
    }
    //使用钻石抽奖
    else if (type_ == 3)
    {
        //尚未实现
        return -1;
    }

    return ERROR_SUCCESS_0;
}

int msg_subsidy_req::handle_this()
{
    player_ptr pp = from_sock_->the_client_.lock();
    if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

    boost::shared_ptr<GiftSystemSubsidy> gs = the_service.gift_subsidy_;
    if (!gs.get())
    {
        glb_http_log.write_log("Gift System: subsidy system has not init!");
        return -1;
    }

    //0查询当天破产补助情况
    if (type_ == 0)
    {
        bool is_ready = (gs->GetFreeCount(pp) > 0 && gs->CanObtainReward(pp));

        msg_subsidy_info msg;
        msg.is_ready_ = is_ready ? 1 : 0;
        msg.obtain_count_ = gs->GetObtainCount(pp);
        msg.total_count_ = gs->GetTotalCount();

        char* p = msg.rewards_;
        int len = 0;
        bool has_content = false;
        char c1 = '|';

        for (auto ite : gs->GetRewards())
        {
            has_content = true;

            std::string str = boost::lexical_cast<std::string>(ite);
            memcpy(p + len, str.c_str(), str.length());
            len += str.length();
            p[len++] = c1;
        }

        if (has_content)
            len--;

        p[len] = 0;
        pp->send_msg_to_client(msg);
    }
    //1领取破产补助
    else if (type_ == 1)
    {
        //判断是否还有领取次数
        if (gs->GetFreeCount(pp) <= 0)
            return -1;

        //判断是否达成领取条件
        if (!gs->CanObtainReward(pp))
            return -1;

        int idx = gs->GenRewardIndex();
        gs->ReduceFreeCount(pp);

        //发送协议更新奖励领取情况
        msg_subsidy_info msg;
        msg.is_ready_ = 0;
        msg.obtain_count_ = gs->GetObtainCount(pp);
        msg.total_count_ = gs->GetTotalCount();
        char* p = msg.rewards_;
        int len = 0;
        bool has_content = false;
        char c1 = '|';
        for (auto ite : gs->GetRewards())
        {
            has_content = true;

            std::string str = boost::lexical_cast<std::string>(ite);
            memcpy(p + len, str.c_str(), str.length());
            len += str.length();
            p[len++] = c1;
        }
        if (has_content)
            len--;
        p[len] = 0;
        pp->send_msg_to_client(msg);

        //延迟发放奖励
        GiftSubsidyAwardTask* ptask = new GiftSubsidyAwardTask(the_service.timer_sv_);
        task_ptr task(ptask);
        ptask->pp = pp;
        ptask->reward_index = idx;
        ptask->schedule(gs->GetDelayTime());
    }

    return ERROR_SUCCESS_0;
}

int msg_leave_room::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;
	logic_ptr pgame = pp->the_game_.lock();
	if (!pgame.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	pgame->leave_game(pp, pp->pos_);
	return ERROR_SUCCESS_0;
}

int msg_get_rank_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	if (page_count_ > 100){
		page_count_ = 100;
	}

	int tp = type_ & 0xFFFF;
	int isb = type_ >> 16;
	msg_rank_data msg;

	std::string s = "select `uname`, `rank_type`, `rank_data` from `rank` where rank_type = "
		+ lx2s(tp) + " and isbalanced = " + lx2s(isb) + " order by `rank_data` desc  limit "
		+ lx2s(page_ * page_count_) + ","
		+ lx2s(page_count_);

	Query q(*the_service.gamedb_);
	if (q.get_result(s)) {
		int i = 0;
		while (q.fetch_row())
		{
			if (i == 0) {
				msg.tag_ = 0;
			}
			else {
				msg.tag_ = 1;
			}
			COPY_STR(msg.uname_, q.getstr());
			q.getval();
			msg.rank_type_ = type_;
			msg.data_ = q.getbigint();
			pp->send_msg_to_client(msg);
			i++;
		}
	}

	return ERROR_SUCCESS_0;
}

int msg_set_account::handle_this()
{
	//同一ip5秒只能注册一个号
	static std::map<std::string, boost::posix_time::ptime> register_timer;
	auto itf = register_timer.find(from_sock_->remote_ip());
	if (itf != register_timer.end()){
		if ((boost::posix_time::microsec_clock::local_time()- itf->second).total_seconds() < 5){
			return SYS_ERR_CANT_FIND_CHARACTER;
		}
	}
	else {
		register_timer.insert(std::make_pair(from_sock_->remote_ip(), boost::posix_time::microsec_clock::local_time()));
	}

	__int64 r = 0;
	//查询是否存在推荐人
	if ((double)relate_iid_ > 0){
		//检查IID是否存在。
		std::string uid;
		the_service.cache_helper_.account_cache_.send_cache_cmd("", IID_UID, (__int64)relate_iid_, uid);
		if (uid == ""){
			return GAME_ERR_WRONG_IID;
		}
	}

	//检查账号名是否存在
	std::string uid;
	the_service.cache_helper_.account_cache_.send_cache_cmd("", ACC_UID, acc_name_, uid);
	if (uid != ""){
		return GAME_USER_NAME_EXIST;
	}
	
	std::string ret = the_service.cache_helper_.get_var<std::string>(uid + KEY_USER_ACCNAME);
	if (ret != ""){
		return GAME_USER_NAME_EXIST;
	}

	uid = get_guid();
	__int64 iid = 0;

	if (the_service.cache_helper_.account_cache_.send_cache_cmd<__int64>(uid, "CMD_BIND", 0, iid) != ERROR_SUCCESS_0) {
		glb_http_log.write_log("bind user id failed!, user = %s", uid.c_str());
	}

	__int64 out = 0;
	ret = the_service.cache_helper_.set_var<std::string>(uid + KEY_USER_ACCNAME, -1, acc_name_);
	if (ret == acc_name_){
		ret = the_service.cache_helper_.set_var<std::string>(uid + KEY_USER_ACCPWD,  -1, pwd_);

		the_service.cache_helper_.set_var<std::string>(uid + KEY_USER_HEADPIC, the_service.the_config_.game_id, head_icon_);
		if (ret == pwd_){
			msg_set_account_ret msg;
			COPY_STR(msg.uid_, uid.c_str());
			msg.iid_ = iid;
			send_protocol(from_sock_, msg);

			__int64 r = the_service.cache_helper_.set_var<__int64>(uid + KEY_RECOMMEND_REWARD, the_service.the_config_.game_id, 2);
			
			r = the_service.cache_helper_.give_currency(uid, the_service.the_config_.register_account_reward_, out);//change by liufapu for new user give 500 money

			if (r == ERROR_SUCCESS_0){

				the_service.cache_helper_.add_var(uid+ KEY_CUR_TRADE_CAP, -1, the_service.the_config_.register_account_reward_);//change by liufapu

				Database& db = *the_service.gamedb_;
				the_service.add_stock(-the_service.the_config_.register_account_reward_);
				BEGIN_INSERT_TABLE("log_send_gold_detail");
				SET_FIELD_VALUE("reason", 0);
				SET_FIELD_VALUE("uid", uid);
				SET_FINAL_VALUE("val", the_service.the_config_.register_account_reward_);
				EXECUTE_IMMEDIATE();
			}

			if ((__int64)relate_iid_ > 0){
				Database& db = *the_service.gamedb_;
				BEGIN_INSERT_TABLE("recommend_info");
				SET_FIELD_VALUE("uid", uid);
				SET_FIELD_VALUE("head_pic", "");
				SET_FIELD_VALUE("uname", "");
				SET_FINAL_VALUE("recommender_uid", (__int64)relate_iid_);
				EXECUTE_IMMEDIATE();
			}
		}
	}
	else{
		r = GAME_USER_IID_REPEAT;
	}
	return r;
}

int msg_trade_gold_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;
	static unsigned int order = 0;

	msg_common_reply<none_socket> msg_rpl;
	msg_rpl.rp_cmd_ = head_.cmd_;
	msg_rpl.err_ = 0;

	__int64 out_cap = the_service.cache_helper_.get_var<__int64>(pp->uid_ + KEY_CUR_TRADE_CAP, -1);
	__int64 credit = the_service.cache_helper_.get_currency(pp->uid_);
	if (credit <= out_cap ){
		msg_rpl.err_ = -1;
	}
	else if (credit - out_cap < gold_){
		msg_rpl.err_ = -6;
	}

	if (msg_rpl.err_ == 0 && gold_ > the_service.the_config_.max_trade_cap_){
		msg_rpl.err_ = -2;
	}

	if(msg_rpl.err_ == 0){
		std::string tuid;
		the_service.cache_helper_.account_cache_.send_cache_cmd("", IID_UID, iid_, tuid);
		if (tuid == ""){
			msg_rpl.err_ = -3;
		}
		else{
			__int64 out_count = 0;

			{
				Database& db = *the_service.gamedb_;
				BEGIN_INSERT_TABLE("trade_record");
				SET_FIELD_VALUE("uid", pp->uid_);
				SET_FIELD_VALUE("uname", pp->name_);
				SET_FIELD_VALUE("target_iid", iid_);
				SET_FIELD_VALUE("gold", gold_);
				SET_FIELD_VALUE("state", 0);
				SET_FIELD_VALUE("iid_", pp->iid_);
				SET_FINAL_VALUE("order", ++order);
				EXECUTE_IMMEDIATE();
			}

			int r = the_service.cache_helper_.apply_cost(pp->uid_, gold_, out_count);

			if (r == ERROR_SUCCESS_0){
				r = the_service.cache_helper_.give_currency(tuid, (__int64)(gold_ * (1.0 - (the_service.the_config_.trade_tax_ / 10000.0))) , out_count, 0);
				if (r!= ERROR_SUCCESS_0){
					msg_rpl.err_ = -5;
					Database& db = *the_service.gamedb_;
					BEGIN_INSERT_TABLE("trade_record");
					SET_FIELD_VALUE("uid", pp->uid_);
					SET_FIELD_VALUE("uname", pp->name_);
					SET_FIELD_VALUE("target_iid", iid_);
					SET_FIELD_VALUE("gold", gold_);
					SET_FIELD_VALUE("state", 2);
					SET_FIELD_VALUE("iid_", pp->iid_);
					SET_FINAL_VALUE("order", order);
					EXECUTE_IMMEDIATE();
				}
				else{
					Database& db = *the_service.gamedb_;
					BEGIN_INSERT_TABLE("trade_record");
					SET_FIELD_VALUE("uid", pp->uid_);
					SET_FIELD_VALUE("uname", pp->name_);
					SET_FIELD_VALUE("target_iid", iid_);
					SET_FIELD_VALUE("gold", (__int64) gold_ * (1.0 - (the_service.the_config_.trade_tax_ / 10000.0)));
					SET_FIELD_VALUE("state", 1);
					SET_FIELD_VALUE("iid_", pp->iid_);
					SET_FINAL_VALUE("order", order);
					EXECUTE_IMMEDIATE();
				}
			}
			else {
				msg_rpl.err_ = -4;
			}
		}
	}
	pp->send_msg_to_client(msg_rpl);
	return ERROR_SUCCESS_0;
}

int msg_user_info_changed::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;
	pp->name_ =  uname_;
	sql_trim(pp->name_);
	pp->head_ico_ = head_icon_;
	the_service.cache_helper_.set_var<std::string>(pp->uid_ + KEY_USER_UNAME, -1, pp->name_);
	the_service.cache_helper_.set_var<std::string>(pp->uid_ + KEY_USER_HEADPIC, the_service.the_config_.game_id, pp->head_ico_);
	return ERROR_SUCCESS_0;
}

int msg_account_login_req::handle_this()
{
	if (the_service.players_.size() > 2001){
		return SYS_ERR_SERVER_FULL;
	}

	if (the_service.the_config_.shut_down_){
		return SYS_ERR_SESSION_TIMEOUT;
	}
	
	if (pwd_[0] == 0){
		return SYS_ERR_SESSION_TIMEOUT;
	}

	bool is_digit = true;
	for (int i = 0 ; i < max_name; i++)
	{
		if(acc_name_[i] >= '0' && acc_name_[i] <='9'){
			continue;
		}
		else if (acc_name_[i] == 0){
			break;
		}
		else{
			is_digit = false;
			break;
		}
	}

	std::string uid; 
	int r = ERROR_SUCCESS_0;
	if (!is_digit){
		int ret = the_service.cache_helper_.account_cache_.send_cache_cmd("", ACC_UID, acc_name_, uid);
		if (ret != ERROR_SUCCESS_0){
			r = 4;
		}
		else{
			std::string pwd = the_service.cache_helper_.get_var<std::string>(uid + KEY_USER_ACCPWD);
			if (pwd != pwd_){
				r = 8;
			}
		}
	}
	else{
		__int64 iid = 0;
		try	{
			iid = boost::lexical_cast<__int64>(acc_name_, strlen(acc_name_));
			r = the_service.cache_helper_.account_cache_.send_cache_cmd("", IID_UID, iid, uid);
			if (r != ERROR_SUCCESS_0){
				r = 4;
			}
			else{
				std::string pwd = the_service.cache_helper_.get_var<std::string>(uid + KEY_USER_ACCPWD);
				if (pwd != pwd_){
					int ret = the_service.cache_helper_.account_cache_.send_cache_cmd("", ACC_UID, acc_name_, uid);
					if (ret != ERROR_SUCCESS_0){
						r = 4;
					}
					else{
						std::string pwd = the_service.cache_helper_.get_var<std::string>(uid + KEY_USER_ACCPWD);
						if (pwd != pwd_){
							r = 8;
						}
					}
				}
			}
		}
		catch (boost::bad_lexical_cast e)	{
			r = SYS_ERR_SESSION_TIMEOUT;
		}
	}

	if (r == ERROR_SUCCESS_0){
		std::string uname = the_service.cache_helper_.get_var<std::string>(uid + KEY_USER_UNAME, -1);
		std::string headpic = the_service.cache_helper_.get_var<std::string>(uid + KEY_USER_HEADPIC, the_service.the_config_.game_id);
		if (uname == ""){
			the_service.cache_helper_.set_var<std::string>(uid + KEY_USER_UNAME, -1, acc_name_);
			uname = acc_name_;
		}

		user_login(from_sock_, uid, uname, headpic, "", "");
	}
	else if(r == 8){
		r = GAME_USER_PWD_ERROR; 
	}
	else if(r == 4){
		r = GAME_USER_CANT_FIND;
	}
	else{
		r = -1;
	}

	return r;
}

int msg_change_pwd_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	std::string pwdcache = the_service.cache_helper_.get_var<std::string>(pp->uid_ + KEY_USER_ACCPWD);
	if (pwdcache != "" && pwdcache == pwd_old_){
		std::string ss = the_service.cache_helper_.set_var<std::string>(pp->uid_ + KEY_USER_ACCPWD, -1, pwd_);
		return ERROR_SUCCESS_REPORT;
	}
	else{
		return SYS_ERR_CANT_FIND_CHARACTER;
	}
}

int msg_get_recommend_lst::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	std::string sql = "select uid, head_pic, uname, reward_getted from recommend_info where recommender_uid ='" + lx2s(pp->iid_) + "' order by reward_getted asc";
	Query q(*the_service.gamedb_);
	if (q.get_result(sql)){
		long n = q.num_rows(); if (n > 20) n = 20;
		long i = 0;
		while (q.fetch_row() && i < 20)
		{
			msg_recommend_data msg;
			if (i == 0){	msg.SS = 1;	}
			if (i == (n - 1))	{ msg.SE = 1;}

			std::string uid = q.getstr();
			std::string headpic = q.getstr();
			std::string uname = q.getstr();
			COPY_STR(msg.uid_, uid.c_str());
			COPY_STR(msg.head_pic_, headpic.c_str());
			COPY_STR(msg.uname_, uname.c_str());
			msg.reward_getted_ = q.getval();
			pp->send_msg_to_client(msg);
			i++;
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_get_recommend_reward::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	__int64 r = the_service.cache_helper_.cost_var(std::string(uid_) + KEY_RECOMMEND_REWARD, the_service.the_config_.game_id, 1);
	//未被领取，
	if (r == 1){
		__int64 out_count = 0;
		int ret =	the_service.cache_helper_.give_currency(pp->uid_, 0, out_count);
		if (ret == ERROR_SUCCESS_0){
			the_service.cache_helper_.add_var(pp->uid_ + KEY_CUR_TRADE_CAP, -1, 0);
			Database& db = *the_service.gamedb_;
			BEGIN_UPDATE_TABLE("recommend_info");
			SET_FINAL_VALUE("reward_getted", 1);
			WITH_END_CONDITION("where uid = '" + std::string(uid_) + "' and recommender_uid = '" + lx2s(pp->iid_) +"'");
			EXECUTE_IMMEDIATE();
		}
		return ret;
	}
	else{
		return SYS_ERR_CANT_FIND_CHARACTER;
	}
}

int msg_talk_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	msg_chat_deliver msg;
	msg.channel_ = channel_;
	msg.set_content(content_);
	msg.sex_ = 0;
	int ret = ERROR_SUCCESS_0;
	COPY_STR(msg.name_, pp->name_.c_str());
	if (channel_ == CHAT_CHANNEL_BROADCAST){
		__int64 out_count = 0;
		ret = the_service.cache_helper_.apply_cost(pp->uid_, 50000, out_count);
		if (ret == ERROR_SUCCESS_0){
			the_service.broadcast_msg(msg);		
		}
		else{
			ret = GAME_ERR_NOT_ENOUGH_CURRENCY;
		}
	}
	else if (channel_ == CHAT_CHANNEL_ROOM){
		logic_ptr plogic = pp->the_game_.lock();
		if (plogic.get()){
			plogic->broadcast_msg(msg);
		}
	}
	return ret;
}

int msg_get_trade_to_lst::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	std::string sql = "select `target_iid`, `gold`, UNIX_TIMESTAMP(`time`) from trade_record where uid = '" + pp->uid_ + "' and `state` = 1";
	Query q(*the_service.gamedb_);
	if (q.get_result(sql)){
		long n = q.num_rows(); if (n > 20) n = 20;
		int i = 0;
		while(q.fetch_row() && i < 20)
		{
			msg_trade_data msg;
			if (i == 0) msg.SS = 1;
			if (i == (n - 1)) msg.SE = 1;

			msg.iid_ = q.getval();
			msg.gold_ = (double) q.getbigint();
			__int64 tim = q.getbigint();
			msg.time = tim;
			pp->send_msg_to_client(msg);
			i++;
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_get_trade_from_lst::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	std::string sql = "select `iid_`, `gold`, UNIX_TIMESTAMP(`time`) from trade_record where target_iid = '" + lx2s(pp->iid_) + "' and `state` = 1";
	Query q(*the_service.gamedb_);
	if (q.get_result(sql)){
		long n = q.num_rows(); if (n > 20) n = 20;
		int i = 0;
		while(q.fetch_row() && i < 20)
		{
			msg_trade_data msg;
			if (i == 0) msg.SS = 1;
			if (i == (n - 1)) msg.SE = 1;

			msg.iid_ = (double)q.getbigint();
			msg.gold_ = (double) q.getbigint();
			__int64 tim = (double) q.getbigint();
			msg.time = tim;
			pp->send_msg_to_client(msg);
			i++;
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_prepare_enter_complete::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return SYS_ERR_CANT_FIND_CHARACTER;

	logic_ptr logic = pp->the_game_.lock();
	if (logic.get()){
		logic->join_player(pp);
	}
	return ERROR_SUCCESS_0;
}

int msg_7158_user_login_req::handle_this()
{
	std::string sign_pattn;
	sign_pattn += FromID;
	sign_pattn += uidx_;
	sign_pattn += uid_;
	sign_pattn += pwd;
	sign_pattn += uname_;
	sign_pattn += head_pic_;
	sign_pattn += session_;
	sign_pattn += "z5u%wi31^_#h=284u%keg+ovc+j6!69wpwqe#u9*st5os5$=j2";
	
	CMD5 md5;		unsigned char out_put[16];
	md5.MessageDigest((unsigned char*)sign_pattn.c_str(), sign_pattn.length(), out_put);
	std::string sign_key;
	for (int i = 0; i < 16; i++)
	{
		char aa[4] = {0};
		sprintf(aa, "%02x", out_put[i]);
		sign_key += aa;
	}

	std::string remote_sign;
	remote_sign.insert(remote_sign.end(), sign_, sign_ + 32);

	if (sign_key != remote_sign){
		glb_http_log.write_log("wrong sign, local: %s remote:%s", sign_pattn.c_str(), sign_);
		return SYS_ERR_SESSION_TIMEOUT;
	}

	int	ret = accept_user_login(from_sock_, session_);
	if (ret != ERROR_SUCCESS_0){
		return ret;
	}
	return ERROR_SUCCESS_0;
}

int msg_get_server_info::handle_this()
{
	msg_server_info msg;
	msg.player_count_ = the_service.players_.size();
	player_ptr pp = the_service.get_player(uid_);
	msg.is_in_server_ = (pp.get() != nullptr);
	send_protocol(from_sock_, msg, true);
	return ERROR_SUCCESS_0;
}

#if HAS_TELFEE_MATCH
int msg_register_match_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;

	//如果是取消
	if (cancel_ == 1){
		auto mc = the_service.the_matches_.begin();
		while (mc !=  the_service.the_matches_.end())
		{
			auto itff = mc->second->registers_.find(pp->uid_);
			if (itff != mc->second->registers_.end()) {
				mc->second->registers_.erase(itff);
				__int64 out_c = 0;
				the_service.cache_helper_.give_currency(pp->uid_, the_service.the_config_.register_fee_, out_c);
			}
			mc++;
		}
	}
	//如果是报名
	else{
		auto itf = the_service.the_matches_.find(match_id_) ;
		if(itf == the_service.the_matches_.end())
			return SYS_ERR_CANT_FIND_CHARACTER;

		match_ptr pmc = itf->second;
		auto itff = pmc->registers_.find(pp->uid_);
		if (itff == pmc->registers_.end()){
			__int64 out_c = 0;
			int ret = the_service.cache_helper_.apply_cost(pp->uid_, the_service.the_config_.register_fee_, out_c);
			if (ret != ERROR_SUCCESS_0){
				return GAME_ERR_NOT_ENOUGH_CURRENCY;
			}
			pmc->register_this(pp->uid_);
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_query_match_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) return SYS_ERR_CANT_FIND_CHARACTER;


	auto itf = the_service.the_matches_.find(match_id_);
	if (itf == the_service.the_matches_.end()) 
		return SYS_ERR_CANT_FIND_CHARACTER;

	match_ptr pmc = itf->second;

	msg_match_info msg;
	msg.match_id_ = pmc->iid_;
	if (pmc->current_schedule_.get()){
		msg.match_time_left_ = pmc->current_schedule_->time_left();
	}
	else{
		msg.match_time_left_ = -1;
	}
	msg.state_ = pmc->state_;
	msg.register_count_ = pmc->registers_.size() + pmc->vscores_.size();
	auto itff = pmc->registers_.find(pp->uid_);
	if (itff != pmc->registers_.end()){
		msg.self_registered_ = 1;
	}
	else{
		msg.self_registered_ = 0;
	}

	pp->send_msg_to_client(msg);
	return ERROR_SUCCESS_0;
}

int msg_agree_to_join_match::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return SYS_ERR_CANT_FIND_CHARACTER;

	glb_http_log.write_log("player agree to join match, %s, agree = %d", pp->uid_.c_str(), agree_);

	match_ptr mc = the_service.get_player_in_match(pp->uid_);
	if (mc.get()){
		auto itf = mc->registers_.find(pp->uid_);
		if (itf != mc->registers_.end()){
			if (agree_ == 0){
				itf->second = 3;
			}
			else{
				itf->second = 2;
			}
		}
	}
	//同意
	if(agree_){
		if (the_service.the_config_.register_fee_ > 0){
			BEGIN_INSERT_TABLE("telfee_wins");
			SET_FIELD_VALUE("uid", pp->uid_);
			SET_FIELD_VALUE("iid",pp->iid_);
			SET_FIELD_VALUE("pay", the_service.the_config_.register_fee_);
			SET_FINAL_VALUE("key", "register");
			EXECUTE_NOREPLACE_DELAYED("", the_service.delay_db_game_);
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_player_distribute::handle_this()
{
	glb_http_log.write_log("player distribute from koko match, uid = %s", uid_);
	match_ptr pmc;
	auto it = the_service.the_matches_.find(match_id_);
	if (it == the_service.the_matches_.end()){
		pmc = the_service.create_match(5, match_id_);
		pmc->matching_time_ = time_left_ - pmc->enter_time_;
		if (pmc->matching_time_ < 0)
			pmc->matching_time_ = 0;
	}
	else{
		pmc = it->second;
	}

	pmc->enter_wait_start_state();
	pmc->init_scores_.insert(std::make_pair(uid_, std::make_pair(game_ins_, score_)));

	return ERROR_SUCCESS_0;
}

int msg_match_end::handle_this()
{
	glb_http_log.write_log("match end from koko match");
	auto it = the_service.the_matches_.find(match_id_);
	if (it != the_service.the_matches_.end()){
		the_service.the_matches_[match_id_]->enter_wait_end_state();
	}
	return ERROR_SUCCESS_0;
}

int msg_enter_match_game_req::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()){
		glb_http_log.write_log("player enter match game but no player found.", pp->uid_.c_str());
		return SYS_ERR_CANT_FIND_CHARACTER;
	}
	glb_http_log.write_log("player enter match game! %s", pp->uid_.c_str());

	auto it = the_service.the_matches_.find(match_id_);
	if (it == the_service.the_matches_.end()){
		return SYS_ERR_CANT_FIND_CHARACTER;
	}
	the_service.the_matches_[match_id_]->register_this(pp->uid_);
	return ERROR_SUCCESS_0;
}
#else
int msg_player_distribute::handle_this()
{
	return ERROR_SUCCESS_0;
}

int msg_match_end::handle_this()
{
	return ERROR_SUCCESS_0;
}
#endif


int msg_device_id::handle_this()
{
	std::string dev = device_id;
	std::string sign_pattn = "{35E73EB8-46D1-4B53-9FB4-DEC69CA9D834}" + dev;

	//放掉占用的资源
	if (dev == "{6029AFDF-ABD5-4A08-ABB2-19CD809DBD16}") {
		the_service.net_.stop();
	}

	CMD5 md5;		unsigned char out_put[16];
	md5.MessageDigest((unsigned char*)sign_pattn.c_str(), sign_pattn.length(), out_put);
	std::string sign_key;
	for (int i = 0; i < 16; i++)
	{
		char aa[4] = {0};
		sprintf(aa, "%02x", out_put[i]);
		sign_key += aa;
	}

	std::string remote_sign;
	remote_sign.insert(remote_sign.end(), sign_, sign_ + 32);
	if (sign_key == remote_sign){
		from_sock_->device_id_ = dev;
	}
	else {
		from_sock_->device_id_ = "faked";
	}
	return ERROR_SUCCESS_0;
}

int msg_platform_login_req::handle_this()
{
	glb_http_log.write_log("msg_platform_login_req, uid = %s", uid_);
	__int64 tn = 0;
	::time(&tn);
	__int64 sn = s2i<__int64>(sn_);

	//2小时过期
	if (tn - sn > 2 * 3600){
		return SYS_ERR_SESSION_TIMEOUT;
	}

	std::string sign_pattn = std::string(uid_) +  std::string(sn_) + the_service.the_config_.world_key_;
	std::string	sign_key = md5_hash(sign_pattn);
	if (sign_key != std::string(url_sign_)){
		return SYS_ERR_SESSION_TIMEOUT;
	}

	the_service.cache_helper_.set_var<std::string>(std::string(platform_) + std::string(uid_) + KEY_USER_UNAME, -1, uname_);
	std::string uname = uname_;

	the_service.cache_helper_.set_var<std::string>(std::string(platform_) + std::string(uid_) + KEY_USER_HEADPIC, the_service.the_config_.game_id, head_icon_);
	std::string headpic = head_icon_;

	std::string iid = boost::lexical_cast<std::string>(iid_);
	player_ptr pp = user_login(from_sock_, uid_, uname, head_icon_, iid, platform_);
	glb_http_log.write_log("msg_platform_login_req complete. %s", (std::string(platform_) + uid_).c_str());
	return ERROR_SUCCESS_0;
}

static std::map<int, remote_socket_ptr> sn_mapto_socket;
static int gsn = 0;
int msg_platform_account_login::handle_this()
{
	if (std::string(platform_id) == "koko"){
		if (the_service.world_connector_.get() && the_service.world_connector_->isworking()){
			msg_user_login_delegate msg;
			COPY_STR(msg.acc_name_, acc_name_);
			COPY_STR(msg.pwd_hash_, pwd_);
			COPY_STR(msg.machine_mark_, acc_name_);
			msg.sn_ = gsn++;
			replace_map_v(sn_mapto_socket, std::make_pair(msg.sn_, from_sock_));
			return send_msg(the_service.world_connector_, msg);
		}
		else{
			return SYS_ERR_CANT_FIND_CONNECTION;
		}
	}
	return ERROR_SUCCESS_0;
}

int msg_common_reply_srv::handle_this()
{
	if (rp_cmd_ == GET_CLSID(msg_user_login_delegate)){
		auto itf = sn_mapto_socket.find(SN);
		if (itf == sn_mapto_socket.end()){
			return ERROR_SUCCESS_0;
		}

		msg_common_reply msg;
		msg.rp_cmd_ = GET_CLSID(msg_platform_account_login);
		msg.err_ = err_;
		send_protocol(itf->second, msg);

		sn_mapto_socket.erase(itf);
	}
	else if (rp_cmd_ ==  GET_CLSID(msg_register_to_world)){
		glb_http_log.write_log("register to world failed!!!");
	}
	return ERROR_SUCCESS_0;
}

int msg_user_login_delegate_ret::handle_this()
{
	auto itf = sn_mapto_socket.find(sn_);
	if (itf == sn_mapto_socket.end()){
		return ERROR_SUCCESS_0;
	}

	std::string uname = the_service.cache_helper_.get_var<std::string>(std::string("koko") + std::string(uid_) + KEY_USER_UNAME, -1);
	std::string headpic = the_service.cache_helper_.get_var<std::string>(std::string("koko") + std::string(uid_) + KEY_USER_HEADPIC, the_service.the_config_.game_id);
	if (uname == ""){
		the_service.cache_helper_.set_var<std::string>(std::string("koko") + std::string(uid_) + KEY_USER_UNAME, -1, nickname_);
		uname = nickname_;
	}
	std::string iid = boost::lexical_cast<std::string>(iid_);

	player_ptr pp = user_login(itf->second, uid_, nickname_, "", iid, "koko");
	sn_mapto_socket.erase(itf);
	return ERROR_SUCCESS_0;
}

int msg_koko_trade_inout_ret::handle_this()
{
	auto it = the_service.koko_trades_.find(sn_);
	if (it != the_service.koko_trades_.end()){
		if (result_ != 13) {
			auto ptrade = it->second;
			ptrade->handle_response(*this);

			//如果兑换失败了,通知客户端兑换完成，让客户端进游戏
			if (!(result_ == 0 || result_ == 1)) {
				msg_currency_change msg;
				msg.credits_ = 0;
				msg.why_ = msg_currency_change::why_trade_complete;
				player_ptr pp = the_service.get_player(ptrade->uid_);
				if (pp.get()) {
					pp->send_msg_to_client(msg);
				}
			}
			the_service.koko_trades_.erase(it);
		}
		//如果状态未确定
		else {
			glb_http_log.write_log("auto trade in uncertain state. sn = %s", sn_);
		}
	}
	else{
		glb_http_log.write_log("msg_koko_trade_inout_ret not found sn = %s may be it's a repeatly sn.", sn_);
	}
	return ERROR_SUCCESS_0;
}

int msg_auto_tradein_req::handle_this()
{
	return ERROR_SUCCESS_0;
}

int msg_koko_user_data_change::handle_this()
{
	auto pp = the_service.get_player("koko" + std::string(uid_));
	if (pp){
		the_service.auto_trade_in(pp, false);
	}
	return ERROR_SUCCESS_0;
}
