#pragma once
#include "MemCacheClient.h"
#include "memcache_defines.h"

#define KEY_USERDEF_TODAY_WIN		"_USERDEF_TODAY_WIN"

class cache_async_callback
{
public:
	std::string			sn_;
	virtual void	routine(boost::shared_ptr<msg_cache_cmd_ret<mem_connector_ptr>> pmsg)
	{

	}
	virtual bool	is_complete() { return true;}
};
typedef boost::shared_ptr<cache_async_callback> async_optr;

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
class	 game_service_base;

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
class memcache_helper
{
public:
	game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>& the_service_;
	mem_cache_client				account_cache_;
	std::map<std::string, async_optr>	async_ops_;

	memcache_helper(
		game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>& serivce,
		int game_id):the_service_(serivce), account_cache_(game_id)
	{

	}

	int									async_apply_cost(std::string uid, longlong count, async_optr = async_optr(), bool take_all = false)
	{
		if (count < 0) {
			glb_http_log.write_log("warning! apply_cost not accept a negative value! function returned!");
			return GAME_ERR_NOT_ENOUGH_CURRENCY;
		}
		int out_sn = 0;
		std::string ukey = uid;
		ukey += KEY_CUR_GOLD;
		if (take_all) {
			account_cache_.async_send_cache_cmd(ukey, CMD_COST_ALL, count, out, &out_sn);
		}
		else {
			account_cache_.async_send_cache_cmd(ukey, CMD_COST, count, out, &out_sn);
		}

		if (cb.get()) {
			cb->sn_ = boost::lexical_cast<std::string>(out_sn);
			async_ops_.insert(std::make_pair(cb->sn_, cb));
		}
		return ERROR_SUCCESS_0;
	}

	int									apply_cost(std::string uid, 
		longlong count, 
		longlong& out_count, 
		bool take_all = false, 
		bool sync_to_client = true)
	{	
		if (count < 0){
			glb_http_log.write_log("warning! apply_cost not accept a negative value! function returned!");
			return GAME_ERR_NOT_ENOUGH_CURRENCY;
		}

		std::string ukey = uid;
		ukey += KEY_CUR_GOLD;
		longlong out = 0;
		if (take_all){
			int ret = account_cache_.send_cache_cmd(ukey, CMD_COST_ALL, count, out);
			if(ret == ERROR_SUCCESS_0){
				out_count = out;
				auto pp = the_service_.get_player(uid);
				if (pp.get() && sync_to_client){
					pp->sync_account(0);
				}
			}
			else{
				return ret;
			}
		}
		else{
			int ret = account_cache_.send_cache_cmd(ukey, CMD_COST, count, out);
			if(ret == ERROR_SUCCESS_0){
				out_count = out;
				auto pp = the_service_.get_player(uid);
				if (pp.get() && sync_to_client){
					pp->sync_account(out_count);
				}
			}
			else{
				return ret;
			}
		}
		return ERROR_SUCCESS_0;
	}

	int									async_give_currency(std::string uid, longlong count, async_optr cb = async_optr())
	{
		if (count < 0) {
			glb_http_log.write_log("warning! give_currency accept a positive value! function returned!");
			return GAME_ERR_NOT_ENOUGH_CURRENCY;
		}
		int out_sn = 0;
		std::string ukey = uid;
		ukey += KEY_CUR_GOLD;
		account_cache_.async_send_cache_cmd(ukey, CMD_ADD, count, &out_sn);

		if (cb.get()) {
			cb->sn_ = boost::lexical_cast<std::string>(out_sn);
			async_ops_.insert(std::make_pair(cb->sn_, cb));
		}
	}

	int									give_currency(std::string uid, longlong count, longlong& out_count, bool sync_to_client = true)
	{
		if (count < 0){
			glb_http_log.write_log("warning! give_currency accept a positive value! function returned!");
			return GAME_ERR_NOT_ENOUGH_CURRENCY;
		}
		std::string ukey = uid;
		ukey += KEY_CUR_GOLD;

		longlong out = 0;
		int ret = account_cache_.send_cache_cmd(ukey, CMD_ADD, count, out);
		boost::posix_time::ptime p1 = boost::posix_time::microsec_clock::local_time();
		//等待5秒,等cache重连
		while (ret == -2 && (boost::posix_time::microsec_clock::local_time() - p1).total_milliseconds() < 5000) {
			account_cache_.ios_.reset();
			account_cache_.ios_.poll();
			ret = account_cache_.send_cache_cmd(ukey, CMD_ADD, count, out);
		}

		if (ret == ERROR_SUCCESS_0) {
			out_count = out;
			player_ptr pp = the_service_.get_player(uid);
			if (pp.get() && sync_to_client) {
				pp->sync_account(out_count);
			}
		}

		glb_http_log.write_log("give currency: uid->%s, count->%lld, opc->%d, out->%lld",
			uid.c_str(),
			count,
			ret, 
			out_count);
		return ERROR_SUCCESS_0;
	}

	longlong						get_currency(std::string uid)
	{
		std::string ukey = uid;
		ukey += KEY_CUR_GOLD;
		longlong out = 0;
		int ret = account_cache_.send_cache_cmd<longlong>(ukey, CMD_GET, 0, out);
		if(ret == ERROR_SUCCESS_0){
			return out;
		}
		return 0;
	}

	template<class T>
	void										async_cmd(std::string key, std::string cmd, T val, async_optr cb = async_optr(), int game_id = -1)
	{
		int out_sn = 0;
		if (game_id < 0) {
			account_cache_.async_send_cache_cmd(key, cmd, val, &out_sn);
		}
		else {
			account_cache_.async_send_cache_cmd(lx2s(game_id) + key, cmd, val, &out_sn);
		}

		if (cb.get()) {
			cb->sn_ = boost::lexical_cast<std::string>(out_sn);
			async_ops_.insert(std::make_pair(cb->sn_, cb));
		}
	}

	template<class T>
	T										get_var(std::string key, int game_id = -1)
	{
		T out;
		int ret = ERROR_SUCCESS_0;
		if (game_id < 0){
			ret = account_cache_.send_cache_cmd(key, CMD_GET, 0, out);
		}
		else{
			ret = account_cache_.send_cache_cmd(lx2s(game_id) + key , CMD_GET, 0, out);
		}
		return out;
	}

	template<>	__int64	get_var<__int64>(std::string key, int game_id)
	{
		__int64 out = 0;
		int ret = ERROR_SUCCESS_0;
		if (game_id < 0){
			ret = account_cache_.send_cache_cmd(key, CMD_GET, 0, out);
		}
		else{
			ret = account_cache_.send_cache_cmd(lx2s(game_id) + key , CMD_GET, 0, out);
		}
		return out;
	}

	int						async_add_var(std::string key, int game_id, longlong val_inc, async_optr cb = async_optr())
	{
		longlong out = 0;
		int out_sn = 0;
		if (val_inc < 0) {
			return GAME_ERR_NOT_ENOUGH_CURRENCY;
		}

		if (game_id < 0) {
			account_cache_.async_send_cache_cmd(key, CMD_ADD, val_inc, &out_sn);
		}
		else
			account_cache_.async_send_cache_cmd(lx2s(game_id) + key, CMD_ADD, val_inc, &out_sn);

		if (cb.get()) {
			cb->sn_ = boost::lexical_cast<std::string>(out_sn);
			async_ops_.insert(std::make_pair(cb->sn_, cb));
		}
		return ERROR_SUCCESS_0;
	}

	longlong						add_var(std::string key, int game_id, longlong val_inc)
	{
		longlong out = 0;
		int ret = ERROR_SUCCESS_0;
		if (game_id < 0){
			ret = account_cache_.send_cache_cmd(key, CMD_ADD, val_inc, out);
		}
		else
			ret = account_cache_.send_cache_cmd(lx2s(game_id) + key , CMD_ADD, val_inc, out);

		if(ret == ERROR_SUCCESS_0){
			return out;
		}
		return 0;
	}

	int							async_cost_var(std::string key, int game_id, longlong val_inc, async_optr cb = async_optr())
	{
		longlong out = 0;
		int out_sn = 0;
		if (val_inc < 0) {
			return GAME_ERR_NOT_ENOUGH_CURRENCY;
		}

		if (game_id < 0) {
			account_cache_.async_send_cache_cmd(key, CMD_COST, val_inc, &out_sn);
		}
		else
			account_cache_.async_send_cache_cmd(lx2s(game_id) + key, CMD_COST, val_inc, &out_sn);

		if (cb.get()) {
			cb->sn_ = boost::lexical_cast<std::string>(out_sn);
			async_ops_.insert(std::make_pair(cb->sn_, cb));
		}
		return ERROR_SUCCESS_0;
	}

	longlong						cost_var(std::string key, int game_id, longlong val_inc, bool take_all = false)
	{
		longlong out = 0;
		int ret = ERROR_SUCCESS_0;
		std::string str_cmd = CMD_COST;
		if (take_all){
			str_cmd = CMD_COST_ALL;
		}

		if (game_id < 0){
			ret = account_cache_.send_cache_cmd(key, str_cmd, val_inc, out);
		}
		else
			ret = account_cache_.send_cache_cmd(lx2s(game_id) + key, str_cmd, val_inc, out);

		if(ret == ERROR_SUCCESS_0){
			return out;
		}
		return 0;
	}

	template<class RT>
	void									async_set_var(std::string key, int game_id, RT val, async_optr cb = async_optr())
	{
		int out_sn = 0;
		if (game_id < 0) {
			int ret = account_cache_.async_send_cache_cmd<RT>(key, CMD_SET, val, &out_sn);
		}
		else {
			int ret = account_cache_.async_send_cache_cmd<RT>(lx2s(game_id) + key, CMD_SET, val, &out_sn);
		}

		if (cb.get()) {
			cb->sn_ = boost::lexical_cast<std::string>(out_sn);
			async_ops_.insert(std::make_pair(cb->sn_, cb));
		}
	}

	template<class RT>
	RT									set_var(std::string key, int game_id, RT val)
	{
		RT out;
		int ret = ERROR_SUCCESS_0;
		if (game_id < 0){
			ret = account_cache_.send_cache_cmd<RT>(key, CMD_SET, val, out);
		}
		else{
			ret = account_cache_.send_cache_cmd<RT>(lx2s(game_id) + key, CMD_SET, val, out);
		}

		if(ret == ERROR_SUCCESS_0){
			return out;
		}
		return RT();
	}

	void					handle_no_sync_msg()
	{
		//清除同步里的多余数据
		std::vector<msg_ptr> msg_lst;
		account_cache_.pickup_msg(account_cache_.s_, msg_lst);
		msg_lst.clear();

		account_cache_.pickup_msg(account_cache_.async_s_, msg_lst);
		auto it = msg_lst.begin();
		while (it != msg_lst.end())
		{
			msg_ptr msg = *it;
			msg_cache_cmd_ret<mem_connector_ptr>* pmsg = (msg_cache_cmd_ret<mem_connector_ptr>*)msg.get();
			auto itf = async_ops_.find(pmsg->sequence_);
			if (itf != async_ops_.end()) {
				async_optr op = itf->second;
				op->routine(boost::dynamic_pointer_cast<msg_cache_cmd_ret<mem_connector_ptr>>(msg));
				if (op->is_complete()) {
					async_ops_.erase(pmsg->sequence_);
				}
			}
			it++;
		}
	}

	__int64			get_today_win(std::string uid, __int64 & today_win)
	{
		//获取今日赢的K豆数
		std::string today_win_str = get_var<std::string>(uid + KEY_USERDEF_TODAY_WIN);
		std::vector<__int64> v;
		split_str<__int64>(today_win_str, ",", v, true);

		__int64 today_win_gold = 0, last_timestamp = 0;
		__int64 now = ::time(nullptr) + 8 * 60 * 60;

		if (v.size() != 0) {
			if (v.size() != 2) {
				return GAME_ERR_DATA;
			}

			today_win_gold = v[0];
			last_timestamp = v[1];

			//是否跨天
			boost::posix_time::ptime ptime1 = boost::posix_time::from_time_t(now);
			boost::posix_time::ptime ptime2 = boost::posix_time::from_time_t(last_timestamp);
			if (ptime1.date() != ptime2.date()) {
				today_win_gold = 0;
				last_timestamp = now;
			}
		}

		today_win = today_win_gold;
		return ERROR_SUCCESS_0;
	}

	int			add_today_win(std::string uid, __int64 win)
	{
		//获取今日赢的K豆数
		std::string today_win_str = get_var<std::string>(uid + KEY_USERDEF_TODAY_WIN);
		std::vector<__int64> v;
		split_str<__int64>(today_win_str, ",", v, true);

		__int64 today_win_gold = 0, last_timestamp = 0;
		__int64 now = ::time(nullptr) + 8 * 60 * 60;

		if (v.size() != 0) {
			if (v.size() != 2) {
				return GAME_ERR_DATA;
			}

			today_win_gold = v[0];
			last_timestamp = v[1];

			//是否跨天
			boost::posix_time::ptime ptime1 = boost::posix_time::from_time_t(now);
			boost::posix_time::ptime ptime2 = boost::posix_time::from_time_t(last_timestamp);
			if (ptime1.date() != ptime2.date()) {
				today_win_gold = 0;
				last_timestamp = now;
			}
		}

		today_win_gold += win;
		last_timestamp = now;

		std::string new_today_win_str = lx2s(today_win_gold) + "," + lx2s(last_timestamp) + ",";
		set_var(uid + KEY_USERDEF_TODAY_WIN, -1, new_today_win_str);

		return ERROR_SUCCESS_0;
	}

	//检查今日输的K豆是否超过最大值
	int check_today_win_valid(std::string uid, __int64 max_lose_golds)
	{
		__int64 today_win = 0;
		int ret = get_today_win(uid, today_win);
		if (ret != ERROR_SUCCESS_0) {
			return ret;
		}
		
		if (today_win < 0 && abs(today_win) >= abs(max_lose_golds)) {
			return GAME_ERR_LOSE_CAP_EXCEED;
		}

		return ERROR_SUCCESS_0;
	}
};