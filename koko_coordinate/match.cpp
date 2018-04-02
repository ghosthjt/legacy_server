#include "match.h"
#include "error.h"
#include "match_msg.h"
#include "player.h"
#include "db_delay_helper.h"
#include "utility.h"
#include "match_msg.h"
#include "boost/date_time.hpp"
#include "log.h"
#include "boost/asio/basic_deadline_timer.hpp"
#include "utf8_db.h"
#include "Query.h"
#include <unordered_map>
#include "server_config.h"

boost::shared_ptr<match_manager> gins;
typedef boost::shared_ptr<game_info> gamei_ptr;
typedef boost::shared_ptr<msg_base<koko_socket_ptr>> msg_ptr;
extern safe_list<msg_ptr>		sub_thread_process_msg_lst_;
extern boost::shared_ptr<utf8_data_base>	db_;
extern std::map<int, internal_server_inf> registed_servers;
extern log_file<cout_output> glb_log;
extern db_delay_helper<std::string, int>& get_delaydb();
extern std::map<int, gamei_ptr> all_games;
extern player_ptr get_player(std::string uid);
extern service_config the_config;
//op = 0 add,  1 cost, 2 cost all
//op = 0 add,  1 cost, 2 cost all
extern int		update_user_item(std::string reason, std::string uid, int op, int itemid, __int64 count, __int64& ret, bool sync_to_client = false);
extern int		do_multi_costs(std::string reason, std::string uid, std::map<int, int> cost);
bool is_score_greater_1(std::pair<std::string, mdata_ptr>& a, std::pair<std::string, mdata_ptr>& b)
{
	return a.second->score_ > b.second->score_;
}

bool is_score_greater(mdata_ptr& a, mdata_ptr& b)
{
	return a->score_ > b->score_;
}

typedef boost::shared_ptr<msg_base<none_socket>> smsg_ptr;	
extern void		broadcast_msg(smsg_ptr msg);

int		game_match_base::send_msg_to_gamesrv(int match_id, msg_base<none_socket>& msg)
{
	auto it1 = match_manager::get_instance()->all_matches.find(match_id);
	if (it1 == match_manager::get_instance()->all_matches.end())
		return error_cant_find_match;


	auto itf = registed_servers.find(it1->second->game_id_);
	if (itf == registed_servers.end()){
		return error_cant_find_server;
	}

	auto psock = itf->second.wsock_.lock();
	if (psock.get()){
		return send_msg(psock, msg);
	}
	else{
		return error_cant_find_player;
	}
}

void game_match_base::eliminate_player(std::string uid)
{
	balance(uid);
}

int			send_msg_to_player(std::string uid, msg_base<none_socket>& msg)
{
	player_ptr pp = get_player(uid);
	if (!pp.get())
		return error_cant_find_player;

	auto psock = pp->from_socket_.lock();
	if (psock.get()){
		return send_msg(psock, msg);
	}
	return error_cant_find_player;
}

int game_match_base::check_this_turn_start()
{
	auto it = registers_.begin();
	int ready_count = 0, reply_count = 0;
	while (it != registers_.end())
	{
		if (it->second == 2){
			ready_count++;
			reply_count++;
		}
		else if (it->second == 3 || it->second == 4){
			reply_count++;
		}
		it++;
	}

	auto itf = registed_servers.find(game_id_);
	if (itf == registed_servers.end()){
		return -1;
	}

	//如果所有报名的人都回复了是否参赛，并且愿意参赛的人数>需要人数,则开赛
	if (reply_count >= registers_.size()){
		if(	ready_count >= require_count_)
			return 1;
		else{
			//如果所有人都回复了参数，但人数不够要求，则重新等待
			return 2;
		}
	}

	return 0;
}

void game_match_base::wait_player_ready_timeout(boost::system::error_code ec)
{
	if (ec.value() == 0){
		//未回复的玩家设置为不同意参赛
		auto it = registers_.begin();
		int ready_count = 0, reply_count = 0;
		while (it != registers_.end())
		{
			if (it->second == 1){
				it->second = 3;
			}
			it++;
		}

		int c = check_this_turn_start();
		if (c == 1){
			this_turn_start();
		}
		else{
			this_turn_over(2);
		}
	}
}

int game_match_base::wait_register_complete(boost::system::error_code ec)
{
	if (ec.value() != 0) return error_cancel_timer;
	if (registers_.size() >= require_count_){
		state_ = state_wait_player_ready;//进入比赛状态
		
		confirm_join_game();
		
		timer_.expires_from_now(boost::posix_time::seconds(30));
		timer_.async_wait(boost::bind(
			&game_match_base::wait_player_ready_timeout,
			boost::dynamic_pointer_cast<game_match_base>(shared_from_this()),
			boost::asio::placeholders::error));
		{
			BEGIN_UPDATE_TABLE("log_match_execute_record");
			SET_FINAL_VALUE("state", state_wait_player_ready);
			WITH_END_CONDITION("where match_instance = '" + ins_id_ +"'");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}

		sync_state_to_client("");

		{
			BEGIN_UPDATE_TABLE("setting_match_list");
			SET_FIELD_VALUE("ms_state", state_wait_player_ready);
			str_field += " `ms_state_start` = now(), ";
			SET_FINAL_VALUE("ms_state_duration", get_time_left());
			WITH_END_CONDITION(("where id = " + boost::lexical_cast<std::string>(id_)));
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
		}
	}
	//人数不够，重新开始等待
	else{
		this_turn_over(1);
	}

	return error_success;
}

extern std::unordered_map<std::string, player_ptr> online_players;

int game_match_base::wait_register()
{
	state_ = state_wait_register;
	ins_id_ = get_guid();
	if (start_type_ == 0){
		if (start_stripe_ == 0){

			if (wait_register_ <= 0){
				glb_log.write_log("warning...matchid:%s wait_register = 0, default to 60 seconds!", id_);
				wait_register_ = 60;
			}

			timer_.expires_from_now(boost::posix_time::seconds(wait_register_));
			timer_.async_wait(boost::bind(
				&game_match_base::wait_register_complete,
				boost::dynamic_pointer_cast<game_match_base>(shared_from_this()),
				boost::asio::placeholders::error));

		}
		else{
			
			if (day <= 0 && h <= 0 && m <= 0 && s <= 0)	{
				glb_log.write_log("warning...matchid:%s (day h m s) = 0, default to day = 1", id_);
				day = 1;
			}

			boost::gregorian::date dat = date_time::day_clock<boost::gregorian::date>::local_day();
			boost::posix_time::ptime pt_schedule(dat, boost::posix_time::time_duration(h - 8, m, s)); //中国时区+8
			boost::posix_time::ptime pnow = boost::posix_time::microsec_clock::local_time();
			//如
			if (pt_schedule < pnow)	{
				pt_schedule += boost::gregorian::days(day);
			}

			timer_.expires_at(pt_schedule);
			timer_.async_wait(boost::bind(
				&game_match_base::wait_register_complete, 
				boost::dynamic_pointer_cast<game_match_base>(shared_from_this()),
				boost::asio::placeholders::error));
		}
	}
	{
		BEGIN_INSERT_TABLE("log_match_execute_record");
		SET_FIELD_VALUE("match_instance", ins_id_);
		SET_FIELD_VALUE("matchid", id_);
		SET_FINAL_VALUE("state", state_wait_register);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	sync_state_to_client("");
	{
		BEGIN_UPDATE_TABLE("setting_match_list");
		SET_FIELD_VALUE("ms_state", state_wait_register);
		str_field += " `ms_state_start` = now(),";
		SET_FINAL_VALUE("ms_state_duration", get_time_left());
		WITH_END_CONDITION(("where id = " + boost::lexical_cast<std::string>(id_)));
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	return error_success;
}

int game_match_base::wait_balance_complete(boost::system::error_code ec)
{
	if(state_ == state_balance)
		this_turn_over(0);
	return error_success;
}

int game_match_base::update(boost::system::error_code ec)
{
	update_timer_.expires_from_now(boost::posix_time::milliseconds(200));
	update_timer_.async_wait(boost::bind(
		&game_match_base::update,
		boost::dynamic_pointer_cast<game_match_base>(shared_from_this()),
		boost::asio::placeholders::error));

	if (state_ == state_wait_player_ready){
		//取消超时计时，为了在所有玩家都准备好后，提前开始进入比赛，有一个超时计时，超时时未回复是否参赛的玩家，都会被取消参赛。
		confirm_join_game(); //不允许中途进入。如果要允许，取消注释
		int c = check_this_turn_start();
		if (c == 1){	
			this_turn_start();
		}
		else if (c == 2){
			this_turn_over(2);
		}
		else if (c == -1){
			glb_log.write_log("can't find match game server, matchid=%d, gameid=%d", id_, game_id_);
			this_turn_over(1);
		}
	}
	//如果正在比赛
	else if (state_ == state_matching){
		int c = check_this_turn_over();
		//如果要结束比赛
		if (c == 1){
			state_ = state_balance;
			balance("");
			timer_.expires_from_now(boost::posix_time::seconds(10));
			timer_.async_wait(boost::bind(
				&game_match_base::wait_balance_complete,
				boost::dynamic_pointer_cast<game_match_base>(shared_from_this()), 
				boost::asio::placeholders::error));
			{
				BEGIN_UPDATE_TABLE("log_match_execute_record");
				SET_FINAL_VALUE("state", state_balance);
				WITH_END_CONDITION("where match_instance = '" + ins_id_ +"'");
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
			sync_state_to_client("");
			{
				BEGIN_UPDATE_TABLE("setting_match_list");
				SET_FIELD_VALUE("ms_state", state_balance);
				str_field += " `ms_state_start` = now(),";
				SET_FINAL_VALUE("ms_state_duration", get_time_left());
				WITH_END_CONDITION(("where id = " + boost::lexical_cast<std::string>(id_)));
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
		}
		//如果不能结束，则更新逻辑
		else{
			update_logic(); 
		}
	}
	//如果等待玩家注册
	else if (state_ == state_wait_register){
		//如果人满即开,或是个人闯关赛,
		if ((start_type_ == 1 && registers_.size() >= require_count_) || 
				 start_type_ == 2){
			wait_register_complete(ec);
		}
	}
	return error_success;
}

int game_match_base::this_turn_start()
{
	if (state_ == state_matching) return error_success;
	state_ = state_matching;
	do_extra_turn_start_staff();

	{
		std::string sql = "delete from log_match_registers where matchid = " + boost::lexical_cast<std::string>(id_);
		Query q(*db_);
		q.execute(sql);
	}
	{
		BEGIN_UPDATE_TABLE("log_match_execute_record");
		SET_FIELD_VALUE("register_count", registers_.size());
		SET_FIELD_VALUE("join_count", scores_.size());
		SET_FINAL_VALUE("state", state_matching);
		WITH_END_CONDITION("where match_instance = '" + ins_id_ +"'");
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}

	sync_state_to_client("");

	{
		BEGIN_UPDATE_TABLE("setting_match_list");
		SET_FIELD_VALUE("ms_state", state_matching);
		str_field += " `ms_state_start` = now(),";
		SET_FINAL_VALUE("ms_state_duration", get_time_left());
		WITH_END_CONDITION(("where id = " + boost::lexical_cast<std::string>(id_)));
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	return error_success;
}

void game_match_base::this_turn_over(int reason)
{
	msg_match_over msg;
	msg.match_id_ = id_;
	msg.reason_ = reason;

	do_extra_turn_over_staff();
	scores_.clear();
	auto it = registers_.begin();
	while (it != registers_.end())
	{
		send_msg_to_player(it->first, msg);
		if (!(it->second == 0 || it->second == 4)){
			it = registers_.erase(it);
		}
		else
			it++;
	}
	wait_register();
}

void game_match_base::balance(std::string eliminate_uid)
{
	std::vector<std::pair<std::string, mdata_ptr>> v;
	v.insert(v.end(), scores_.begin(), scores_.end());
	std::sort(v.begin(), v.end(), is_score_greater_1);

	for (int i = 0; i < v.size(); i++)
	{
		std::pair<std::string, mdata_ptr>& score = v[i];

		if (eliminate_uid != ""){
			if (score.first != eliminate_uid) continue;
		}

		msg_match_report msg;
		msg.my_rank_ = i;
		msg.rankmax_ = v.size();
		msg.match_id_ = id_;
		__int64 out_count = 0;
		std::string reward_type, reward;
		if (eliminate_uid == "" && i < prizes_.size()){
			for (int ii = 0; ii < prizes_[i].size(); ii++)
			{
				reward_type += boost::lexical_cast<std::string>(prizes_[i][ii].first) + ",";
				reward += boost::lexical_cast<std::string>(prizes_[i][ii].second) + ",";
				update_user_item("比赛场结算", score.second->uid_, 0, prizes_[i][ii].first, prizes_[i][ii].second, out_count, true);
				if (ii < 3){
					msg.prize_[ii] = prizes_[i][ii].first;
					msg.count_[ii] = prizes_[i][ii].second;
				}
			}
		}

		save_match_result(*score.second, i, reward_type,  reward);
		save_game_data(*score.second, 100, 0, 0);
		send_msg_to_player(score.second->uid_, msg);
	}
}

int game_match_base::register_this(std::string uid)
{
	int ret = can_register(uid);
	if (ret != error_success) return ret;

	ret = pay_for_register(uid);
	if (ret != error_success) return ret;

	registers_.insert(std::make_pair(uid, 0));
	save_register(uid, cost_);
	sync_state_to_client(uid);
	return error_success;
}

int game_match_base::update_score(std::string uid, int score, int op /*= 0*/)
{
	int pos = uid.find_first_of("koko");
	if (pos == 0){
		uid.erase(uid.begin(), uid.begin() + strlen("koko"));
	}
	auto it = scores_.find(uid);
	if (it == scores_.end()) return error_success;
	if (op == 0){
		scores_[uid]->score_ = score;
	}
	else
		scores_[uid]->score_ += score;
	
	if (op >> 16){
		scores_[uid]->game_state_ = 2;
	}
	return error_success;
}

game_match_base::game_match_base(boost::asio::io_service& ios) : 
	timer_(ios),
	update_timer_(ios),
	ios_(ios)
{
}

void game_match_base::clean()
{
	boost::system::error_code ec;
	update_timer_.cancel(ec);
	timer_.cancel(ec);
	ios_.poll(ec);
}

void game_match_base::save_game_data(matching_data& md, int exp, int exp_master, int exp_cons)
{
	std::string sql = "select lv, exp, exp_master, exp_cons from user_game_data where uid = '" + md.uid_ + "' and "
		"gameid = " + boost::lexical_cast<std::string>(md.game_id_);
	Query q(*db_);
	if (q.get_result(sql)){
		if (q.fetch_row()){
			int lv = q.getval();
			int expp = q.getval();
			int expm = q.getval();
			int expc = q.getval();

			//计算等级
			expp += exp;
			int expmax = std::pow(2, lv) * 1000;
			while (expmax < expp)	{
				lv++;
				expp -= expmax;
				expmax = std::pow(2, lv) * 1000;
			}

			Database& db = *db_;
			BEGIN_UPDATE_TABLE("user_game_data");
			SET_FIELD_VALUE("uid", md.uid_);
			SET_FIELD_VALUE("gameid", md.game_id_);
			SET_FIELD_VALUE("lv", lv);
			SET_FIELD_VALUE("exp", expp);
			SET_FIELD_VALUE("exp_master", exp_master);
			SET_FINAL_VALUE("exp_cons", exp_cons);
			WITH_END_CONDITION("where uid = '" + md.uid_ + "' and gameid = " + boost::lexical_cast<std::string>(md.game_id_));
			EXECUTE_IMMEDIATE();
		}
		else{
			q.free_result();

			Database& db = *db_;
			BEGIN_INSERT_TABLE("user_game_data");
			SET_FIELD_VALUE("uid", md.uid_);
			SET_FIELD_VALUE("gameid", md.game_id_);
			SET_FIELD_VALUE("exp", exp);
			SET_FIELD_VALUE("exp_master", exp_master);
			SET_FINAL_VALUE("exp_cons", exp_cons);
			EXECUTE_IMMEDIATE();
		}
	}
}

void game_match_base::save_match_result(matching_data& md, int rank, std::string reward_type, std::string count)
{
	BEGIN_INSERT_TABLE("log_match_results");
	SET_FIELD_VALUE("uid", md.uid_);

	player_ptr pp = get_player(md.uid_);
	if (pp.get()){
		SET_FIELD_VALUE("nickname", pp->nickname_);
	}
	else{
		SET_FIELD_VALUE("nickname", "whosyourdaddy");
	}
	SET_FIELD_VALUE("match_id", id_);
	SET_FIELD_VALUE("gameid", md.game_id_);
	SET_FIELD_VALUE("match_instance", ins_id_);
	SET_FIELD_VALUE("match_name", match_name_);
	SET_FIELD_VALUE("score", md.score_);
	SET_FIELD_VALUE("rank", rank);
	SET_FIELD_VALUE("reward_type", reward_type);
	SET_FINAL_VALUE("reward", count);
	EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
}

void game_match_base::save_register(std::string uid, std::map<int, int> cost)
{
	BEGIN_INSERT_TABLE("log_match_registers");
	SET_FIELD_VALUE("uid", uid);
	SET_FIELD_VALUE("matchid", id_);
	SET_FIELD_VALUE("match_instance", ins_id_);

	std::string c1, c2;
	auto it = cost.begin();
	while (it != cost.end())
	{
		c1 += boost::lexical_cast<std::string>(it->first) + ",";
		c2 += boost::lexical_cast<std::string>(it->second) + ",";
		it++;
	}
	SET_FIELD_VALUE("cost_count", c1);
	SET_FINAL_VALUE("cost_id", c2);
	EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
}

extern boost::shared_ptr<utf8_data_base>	db_;
int game_match_base::pay_for_register(std::string uid)
{
	return do_multi_costs("比赛报名费", uid, cost_);
}

int game_match_base::update_logic()
{
	std::vector<std::pair<std::string, mdata_ptr>> v;
	auto it = registers_.begin();
	int ready_count = 0, reply_count = 0;
	while (it != registers_.end())
	{
		if (!get_player(it->first).get())	{
			it->second = 3;
		}

		if (it->second == 2){
			mdata_ptr mdat(new matching_data());
			mdat->game_id_ = game_id_;
			mdat->game_state_ = 1;
			mdat->uid_ = it->first;
			mdat->score_ = eliminate_phase1_basic_score_;
			scores_.insert(std::make_pair(it->first, mdat));
			v.push_back(std::make_pair(it->first, mdat));
		}

		if (it->second == 3 || it->second == 2){
			BEGIN_DELETE_TABLE("log_match_registers");
			WITH_END_CONDITION(" where match_instance = '" + ins_id_ + "' and uid = '" + it->first + "'");
			EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			it = registers_.erase(it);
		}
		else
			it++;
	}
	//发送进入游戏命令到客户端
	distribute_player(v);
	return error_success;
}

int game_match_base::confirm_join(std::string uid, int agree)
{
	auto it = registers_.find(uid);
	if (it != registers_.end()){
		if (agree == 1){
			it->second = 2;
		}
		else if (agree == 2){
			it->second = 4;
		}
		else
			it->second = 3;
	}
	int c = 0;
	it = registers_.begin();
	while (it != registers_.end())
	{
		if (it->second <= 1){
			c++;
		}
		it++;
	}
	
	msg_confirm_join_deliver msg;
	msg.current_ = c;
	msg.match_id_ = id_;
	msg.total_ = registers_.size();
	broadcast_msg(msg, [&](player_ptr pp)->bool{
		return true;
	});

	return error_success;
}

void game_match_base::confirm_join_game()
{
	msg_confirm_join_game msg;
	msg.match_id_ = id_;
	msg.register_count_ = registers_.size();
	COPY_STR(msg.ins_id_, "");
	auto it = registers_.begin();
	while (it != registers_.end())
	{
		if (it->second == 0){
			if(send_msg_to_player(it->first, msg) != error_success){
				BEGIN_INSERT_TABLE("setting_match_notify");
				SET_FIELD_VALUE("uid", it->first);
				SET_FIELD_VALUE("matchid", id_);
				SET_FIELD_VALUE("ip", the_config.public_ip_);
				SET_FIELD_VALUE("port", the_config.client_port_);
				SET_FIELD_VALUE("register_count", registers_.size());
				SET_FINAL_VALUE("matchins", ins_id_);
				EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
			}
			it->second = 1;
		}
		it++;
	}
}

int game_match_base::start()
{
	update_timer_.expires_from_now(boost::posix_time::milliseconds(200));
	update_timer_.async_wait(boost::bind(
		&game_match_base::update,
		boost::dynamic_pointer_cast<game_match_base>(shared_from_this()),
		boost::asio::placeholders::error));
	return wait_register();
}

int game_match_base::cancel_register(std::string uid)
{
	registers_.erase(uid);
	scores_.erase(uid);
	{
		std::string sql = "delete from log_match_registers where uid = '" + uid + "' and matchid = " + boost::lexical_cast<std::string>(id_) ;
		Query q(*db_);
		q.execute(sql);
	}
	sync_state_to_client(uid);
	return error_success;
}

int game_match_base::sync_state_to_client(std::string uid)
{
	if (uid != ""){
		auto it = online_players.find(uid);
		if (it != online_players.end()){
			player_ptr pp = it->second;
			msg_match_info msg;
			msg.match_id_ = id_;
			msg.registered_ = (registers_.find(pp->uid_) != registers_.end());
			msg.register_count_ = registers_.size();
			msg.state_ = get_state();
			msg.time_left_ = get_time_left();
			auto pcnn = pp->from_socket_.lock();
			if (pcnn.get())	{
				send_msg(pcnn, msg);
			}
		}
	}
	else{
		auto it = online_players.begin();
		while (it != online_players.end())
		{
			player_ptr pp = it->second;

			msg_match_info msg;
			msg.match_id_ = id_;
			msg.registered_ = (registers_.find(pp->uid_) != registers_.end());
			msg.register_count_ = registers_.size();
			msg.state_ = get_state();
			msg.time_left_ = get_time_left();
			auto pcnn = pp->from_socket_.lock();
			if (pcnn.get())	{
				send_msg(pcnn, msg);
			}
			it++;
		}
	}
	return error_success;
}

void	game_match_base::do_distribute(std::vector<mdata_ptr>& group)
{
	std::string game_ins = boost::lexical_cast<std::string>(rand_r(999999999));
	for (int j = 0; j < group.size(); j++)
	{
		group[j]->game_ins_ = game_ins;
		group[j]->game_state_ = 1;

		msg_player_distribute msg;
		COPY_STR(msg.uid_, group[j]->uid_.c_str());
		msg.match_id_ = id_;
		COPY_STR(msg.game_ins_, game_ins.c_str());
		msg.score_ = group[j]->score_;
		msg.time_left_ = get_time_left() * 1000;
		int ret = send_msg_to_gamesrv(id_, msg);
		if (ret != error_success)	{
			glb_log.write_log("distribute player to match host failed, match_id:%d, uid:%s, game_ins:%s, error:%d",
				id_,group[j]->uid_.c_str(), game_ins.c_str(), ret);
		}
		else{
			msg_match_start msg_start;
			msg_start.match_id_ = id_;
			COPY_STR(msg_start.ins_id_, game_ins.c_str());
			ret = send_msg_to_player(group[j]->uid_, msg_start);
		}
	}
	running_games_.insert(std::make_pair(game_ins, group));
	//重置，进入下批分配
	group.clear();
}

void game_match_base::distribute_player(std::vector<std::pair<std::string, mdata_ptr>>& v)
{
	std::vector<mdata_ptr> group;
	for (int i = 0; i < v.size(); i++)
	{
		group.push_back(v[i].second);
		//如果本比赛需要分组的人数<=1，说明本比赛不需要分组，则把所有玩家放在一个游戏内进行
		if (group.size() >= need_players_per_game_){
			do_distribute(group);
		}
	}
	do_distribute(group);
}

int game_match_eliminate::update_logic()
{
	eliminate();
	//大于1分钟
	if(tm_counter_.elapse() > boost::posix_time::seconds(60).total_milliseconds()){
		score_line_ += (eliminate_phase1_inc / 60.0);
		tm_counter_.restart();
	}
	return 0; //继续更新
}

void game_match_eliminate::eliminate()
{
	//海选阶段
	if (phase_ == 0){
		std::vector<std::pair<std::string, mdata_ptr>> v;
		//淘汰玩家
		auto it = scores_.begin();
		while (it != scores_.end())
		{
			//不是游戏状态
			if (it->second->game_state_ != 1){
				if (it->second->score_ < score_line_){
					//发送玩家被淘汰
					it = scores_.erase(it);
					continue;
				}
			}
			if (it->second->game_state_ == 2){
					v.push_back(*it);
			}
			it++;
		}
		//人数可以进入决赛了
		if (scores_.size() <= elininate_phase2_count_){
			phase_ = 1;
			//发送消息到客户端，现在处于等待状态
		}
		else{
			//随机分配对手
			std::random_shuffle(v.begin(), v.end());
			distribute_player(v);
		}
	}
	//决赛等待阶段
	else if(phase_ == 1){
		if (is_all_finished()){
			running_games_.clear();
			phase_ = 2;
			std::vector<std::pair<std::string, mdata_ptr>> v;
			v.insert(v.end(), scores_.begin(), scores_.end());
			std::random_shuffle(v.begin(), v.end());
			distribute_player(v);
		}
	}
	//决赛进行阶段
	else if (phase_ == 2){
		//淘汰最后一名
		auto itg = running_games_.begin();
		while (itg == running_games_.end())
		{
			std::vector<mdata_ptr>&v = itg->second;
			bool all_finished = true;
			for (int i = 0; i < v.size(); i++)
			{
				if (v[i]->game_state_ != 2){
					all_finished = false;
					break;
				}
			}
			//本场游戏完成，淘汰最后一名
			if (all_finished){
				if (!v.empty()){
					std::sort(v.begin(), v.end(), is_score_greater);
					scores_.erase(v.back()->uid_);
				}
				itg = running_games_.erase(itg);
			}
			else{
				itg++;
			}
		}
		std::vector<std::pair<std::string, mdata_ptr>> v;
		//淘汰玩家
		auto its = scores_.begin();
		while (its != scores_.end())
		{
			if (its->second->game_state_ == 2){
				v.push_back(*its);
			}
			its++;
		}
		//如果剩余人数<需要决出胜者数，或是剩余玩家不足以完成一场比赛，则不再进行对手匹配，而是等待结束
		if (scores_.size() < end_data_ ||
			scores_.size() < need_players_per_game_){
			phase_ = 3;
			//发送消息到客户端，等待其它玩家结束比赛。
		}
		else{
			//随机分配对手
			std::random_shuffle(v.begin(), v.end());
			distribute_player(v);
		}
	}
	else if (phase_ == 3){
		//本次比赛结束
		if (is_all_finished()){
			is_over_ = true;
		}
	}
}


bool game_match_eliminate::is_all_finished()
{
	bool all_finished = true;
	auto it = scores_.begin();
	while (it != scores_.end())
	{
		if (it->second->game_state_ != 2){
			all_finished = false;
			break;
		}
		it++;
	}
	return all_finished;
}

int game_match_eliminate::do_extra_turn_start_staff()
{
	score_line_ = eliminate_phase1_basic_score_;
	phase_ = 0;
	tm_counter_.restart();
	return error_success;
}

int game_match_eliminate::check_this_turn_over()
{
	return is_over_;
}

int game_match_eliminate::do_extra_turn_over_staff()
{
	return 0;
}

boost::shared_ptr<match_manager> match_manager::get_instance(boost::asio::io_service* ios /*= nullptr*/)
{
	if (!gins.get()){
		gins.reset(new match_manager(*ios));
	}
	return gins;
}

int match_manager::register_match(int matchid, std::string uid)
{
	auto it = all_matches.find(matchid);
	if (it == all_matches.end()){
		return error_cant_find_match;
	}
	match_ptr pmatch = it->second;
	if (!pmatch.get())
		return error_cant_find_match;

	return pmatch->register_this(uid);
}

int match_manager::update_score(int matchid, int score, int op, std::string uid)
{
	auto it = all_matches.find(matchid);
	if (it == all_matches.end()){
		return error_cant_find_match;
	}
	match_ptr pmatch = it->second;
	if (!pmatch.get())
		return error_cant_find_match;

	//被淘汰
	if (op == 2){
		{
			msg_match_over msg;
			msg.match_id_ = matchid;
			msg.reason_ = 0;
			send_msg_to_player(uid, msg);
		}
		{
			msg_match_report msg;
			msg.my_rank_ = -1;
			msg.rankmax_ =  -1;
			msg.match_id_ = matchid;
			send_msg_to_player(uid, msg);
		}
		pmatch->scores_.erase(uid);
	}
	else{
		return	pmatch->update_score(uid, score, op);
	}
	return error_success;
}

// 比赛是不可以重新刷新的，要停掉所有比赛才行
int match_manager::load_all_matches()
{
	std::string sql;
	std::string str = combin_str(the_config.for_games_, ",", false);

	if (str != ""){
		sql = "SELECT `match_type` ,`id`,`game_id`,`match_name`,`thumbnail`,`help_url`,"
			"`prize_desc`,`require_count`,`start_type`,`start_stipe`,`day_or_time`,`h`,`m`,"
			"`s`,`need_players_per_game`,`eliminate_phase1_basic_score`,`eliminate_phase1_inc`,"
			"`eliminate_phase2_count_`,`end_type`,`end_data`,`prizes`,`cost`, `srvip`, `srvport`"
			" FROM `setting_match_list` where game_id in(" + str + ")";
	}
	else{
		sql = "SELECT `match_type` ,`id`,`game_id`,`match_name`,`thumbnail`,`help_url`,"
			"`prize_desc`,`require_count`,`start_type`,`start_stipe`,`day_or_time`,`h`,`m`,"
			"`s`,`need_players_per_game`,`eliminate_phase1_basic_score`,`eliminate_phase1_inc`,"
			"`eliminate_phase2_count_`,`end_type`,`end_data`,`prizes`,`cost`, `srvip`, `srvport`"
			" FROM `setting_match_list`";
	}

	Query q(*db_);
	if (q.get_result(sql)){
		while (q.fetch_row()){
			int tp = q.getval();
			match_ptr mp;
			if (tp == match_type_eliminate)	{
				mp.reset(new game_match_eliminate(ios_));
			}
			else if (tp == match_type_time_restrict){
				mp.reset(new game_match_time_restrict(ios_));
			}
			else if (tp == match_type_continual){
				mp.reset(new game_match_continual(ios_));
			}
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
			if(!split_str<std::string>(vp, ";", tmpv, true)){
				tmpv.clear();
				glb_log.write_log("match prize config error: %s matchid:%d", vc.c_str(), mp->id_);
			}

			for (int i = 0; i < tmpv.size(); i++)
			{
				tmpvv.clear();
				split_str<int>(tmpv[i], ",", tmpvv, true);
				std::vector<std::pair<int, int>> vv;
				for (int ii = 0; ii < tmpvv.size() - 1; ii += 2)
				{
					vv.push_back(std::make_pair(tmpvv[ii], tmpvv[ii + 1]));
				}
				mp->prizes_.push_back(vv);
			}
			tmpvv.clear();

			if(!split_str<int>(vc.c_str(), ",", tmpvv, true)){
				glb_log.write_log("match cost config error: %s matchid:%d", vc.c_str(), mp->id_);
				tmpvv.clear();
			}
			while (tmpvv.size() >= 2)
			{
				mp->cost_.insert(std::make_pair(tmpvv[0], tmpvv[1]));
				tmpvv.erase(tmpvv.begin());
				tmpvv.erase(tmpvv.begin());
			}

			all_matches.insert(std::make_pair(mp->id_, mp));
		}
		return error_success;
	}
	else{
		return error_sql_execute_error;
	}
}

int match_manager::begin_all_matches()
{
	auto it = all_matches.begin();
	while (it != all_matches.end())
	{
		it->second->start();
		it++;
	}
	return error_success;
}

int match_manager::confirm_math(int matchid, std::string uid, int agree)
{
	auto it = all_matches.find(matchid);
	if (it == all_matches.end()){
		return error_cant_find_match;
	}
	match_ptr pmatch = it->second;
	if (!pmatch.get())
		return error_cant_find_match;

	pmatch->confirm_join(uid, agree);
	return error_success;
}

int match_manager::cancel_register(int matchid, std::string uid)
{
	auto it = all_matches.find(matchid);
	if (it == all_matches.end()){
		return error_cant_find_match;
	}
	match_ptr pmatch = it->second;
	if (!pmatch.get())
		return error_cant_find_match;

	return pmatch->cancel_register(uid);
}

void match_manager::free_instance()
{
	if (gins.get()){
		gins->stop_all_matches();
		gins.reset();
	}
}

int match_manager::stop_all_matches()
{
	auto it = all_matches.begin();
	while (it != all_matches.end())
	{
		it->second->clean();
		it++;
	}
	return error_success;
}

int game_match_time_restrict::do_extra_turn_start_staff()
{
	is_over_ = false;
	timer_.expires_from_now(boost::posix_time::seconds(end_data_));
	timer_.async_wait(boost::bind(
		&game_match_time_restrict::forece_match_over,
		boost::dynamic_pointer_cast<game_match_time_restrict>(shared_from_this()), 
		boost::asio::placeholders::error));
	return error_success;
}

void game_match_time_restrict::notify_server_to_end_game()
{
	msg_match_end msg;
	msg.match_id_ = id_;
	send_msg_to_gamesrv(id_, msg);

	auto it = scores_.begin();
	while (it != scores_.end())
	{
		send_msg_to_player(it->first, msg);
		it++;
	}

	timer_.expires_from_now(boost::posix_time::seconds(2));
	timer_.async_wait(boost::bind(
		&game_match_time_restrict::wait_for_feedback,
		boost::dynamic_pointer_cast<game_match_time_restrict>(shared_from_this()),
		boost::asio::placeholders::error));
}

int msg_match_score::handle_this()
{
	return match_manager::get_instance()->update_score(match_id_, score_, operator_, uid_);
}

int msg_confirm_join_game_reply::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return error_cant_find_player;

	return match_manager::get_instance()->confirm_math(match_id_, pp->uid_, agree_);
}

int msg_match_cancel::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return error_cant_find_player;

	int ret = match_manager::get_instance()->cancel_register(match_id_, pp->uid_);
	msg_query_info msg;
	msg.query_type_ = 1;
	msg.query_data_ = match_id_;
	msg.from_sock_ = from_sock_;
	msg.handle_this();
	return ret;
}

int		sort_func(std::pair<std::string, mdata_ptr>& a, std::pair<std::string, mdata_ptr>& b)
{
	return a.second->score_ > b.second->score_;
}

bool  is_equal_func(std::pair<std::string, mdata_ptr>& a, std::string b)
{
	return a.first == b;
}

int msg_query_info::handle_this()
{
	if (query_type_ == 0){
		player_ptr pp = from_sock_->the_client_.lock();
		if (!pp.get()) 
			return error_cant_find_player;
		{
			msg_query_data_begin  msg;
			send_msg(from_sock_, msg);
		}

		//发送比赛列表
		auto it = match_manager::get_instance()->all_matches.begin();
		while (it != match_manager::get_instance()->all_matches.end())
		{
			match_ptr pm = it->second; it++;
			//match_id_是game_id
			if (pm->game_id_ == query_data_){
				msg_match_info msg;
				msg.match_id_ = pm->id_;
				msg.registered_ = (pm->registers_.find(pp->uid_) != pm->registers_.end());
				msg.register_count_ = pm->registers_.size();
				msg.state_ = pm->get_state();
				msg.time_left_ = pm->get_time_left();
				send_msg(from_sock_, msg);
			}
		}

		//发送房间列表
		auto it2 = all_games.find(query_data_);
		if (it2 != all_games.end()){
			gamei_ptr pg = it2->second;
			for (int i = 0; i < pg->rooms_.size(); i++)
			{
				msg_game_room_data msg;
				msg.inf = pg->rooms_[i];
				send_msg(from_sock_, msg);
			}
		}
		//从数据库中查询自身数据以及排行榜数据，放在子线程中运行
		sub_thread_process_msg_lst_.push_back(boost::dynamic_pointer_cast<msg_base<koko_socket_ptr>>(shared_from_this()));
	}
	else if (query_type_ == 1){
		player_ptr pp = from_sock_->the_client_.lock();
		if (!pp.get()) 
			return error_cant_find_player;

		auto it = match_manager::get_instance()->all_matches.find(query_data_);
		if (it != match_manager::get_instance()->all_matches.end()){
			match_ptr pm = it->second;
			msg_match_info msg;
			msg.match_id_ = pm->id_;
			msg.registered_ = (pm->registers_.find(pp->uid_) != pm->registers_.end());
			msg.register_count_ = pm->registers_.size();
			msg.state_ = pm->get_state();
			msg.time_left_ = pm->get_time_left();
			send_msg(from_sock_, msg);
		}
	}
	else if (query_type_ == 2){
		player_ptr pp = from_sock_->the_client_.lock();
		if (!pp.get()) 
			return error_cant_find_player;

		auto it = match_manager::get_instance()->all_matches.find(query_data_);
		if (it == match_manager::get_instance()->all_matches.end())
			return error_cant_find_match;

		match_ptr pm = it->second;
		auto it2 = pm->scores_.find(pp->uid_);
		if (it2 == pm->scores_.end())
			return error_cant_find_match;

		mdata_ptr pdat = it2->second;
		std::vector<std::pair<std::string, mdata_ptr>> v;
		v.insert(v.end(), pm->scores_.begin(), pm->scores_.end());
		std::sort(v.begin(), v.end(), sort_func);

		msg_my_match_score msg;
		msg.match_id_ = pm->id_;
		msg.time_left_ = pm->get_time_left();
		msg.my_score_ = pdat->score_;
		if (!v.empty()){
			msg.max_score_ = v[0].second->score_;
		}
		else{
			msg.max_score_ = 0;
		}

		auto it_rank = std::_Find_pr(v.begin(), v.end(), pp->uid_, is_equal_func);
		if (it_rank != v.end()){
			msg.my_rank_ = it_rank - v.begin();
		}
		else{
			msg.my_rank_ = 0;
		}

		msg.max_rank_ = v.size();
		send_msg(from_sock_, msg);
	}
	else if (query_type_ == 3){
		send_all_game_to_player(from_sock_);
		send_all_match_to_player(from_sock_);
	}
	return error_success;
}

int msg_match_register::handle_this()
{
	player_ptr pp = from_sock_->the_client_.lock();
	if (!pp.get()) 
		return error_cant_find_player;

	int ret = match_manager::get_instance()->register_match(match_id_, pp->uid_);
	msg_query_info msg;
	msg.query_type_ = 1;
	msg.query_data_ = match_id_;
	msg.from_sock_ = from_sock_;
	msg.handle_this();
	return ret;
}

int msg_player_eliminated_by_game::handle_this()
{
	boost::shared_ptr<match_manager> mm = match_manager::get_instance();
	auto it = mm->all_matches.find(match_id_);
	if (it == mm->all_matches.end()){
		return error_success;
	}
	match_ptr pm = it->second;
	pm->eliminate_player(uid_);

	return error_success;
}
