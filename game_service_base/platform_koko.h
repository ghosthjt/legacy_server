#pragma once
#include "net_service.h"
#include "msg_client_common.h"
#include "utility.h"
#include "log.h"
#include "logic_base.hpp"

extern std::string	md5_hash(std::string sign_pat);
extern log_file<cout_and_file_output> glb_http_log;

enum 
{
	trade_state_init,
	trade_state_prepared,
	trade_state_request_sended,
	trade_state_responsed,
	trade_state_success,
};

enum 
{
	ret_code_unknown = -1, 
	ret_code_success,
	ret_code_internal_failed,	//本地失败
	ret_code_cache_succ,
	ret_code_cache_fail,
	ret_code_timeout,	
	ret_code_response_failed,	//服务器回复了失败结果
};

enum
{
	item_id_gold,			//K币	
	item_id_gold_game,		//K豆
	item_id_gold_free,		//K券
	item_id_gold_game_lock,
	item_id_gold_bank,		//保险箱K币
	item_id_gold_game_bank,//保险箱K豆

	item_id_present_id_begin = 1000,

};

template<class service_t>
class koko_currency_trade : public boost::enable_shared_from_this<koko_currency_trade<service_t>>
{
public:
	std::string					uid_;
	std::string					platform_uid_;
	int							cid_;
	bool						islogin_;
	int							delay_;
	__int64						start_tick_;
	int							retry_;
	std::string					sn_;

	koko_currency_trade(service_t& srv, int cid, int dir) :
		the_service(srv)
	{
		trade_state_ = trade_state_init;
		dir_ = dir;
		count_ = 0;
		sn_ = get_guid();
		is_complete_ = false;
		islogin_ = false;
		cid_ = cid;
		delay_ = 10;
		retry_ = 0;
		start_tick_ = 0;
	}

	bool	is_complete()
	{
		return is_complete_;
	}
	
	bool	is_succ()
	{
		return trade_state_ == trade_state_success;
	}

	virtual	int		trade_success(msg_koko_trade_inout_ret& msg)
	{
		return ERROR_SUCCESS_0;
	}

	virtual int		send_request()
	{
		retry_++;
		msg_koko_trade_inout msg;
		msg.count_ = count_;
		//这么做是为了兼容以前的版本.
		if (cid_ == item_id_gold_game) {
			msg.dir_ = dir_ | (item_id_gold << 16);
		}
		else if (cid_ == item_id_gold) {
			msg.dir_ = dir_ | (item_id_gold_game << 16);
		}
		else {
			msg.dir_ = dir_ | (cid_ << 16);
		}

		msg.time_stamp_ = ::time(nullptr);
		COPY_STR(msg.sn_, sn_.c_str());
		COPY_STR(msg.uid_, platform_uid_.c_str());
		build_sign(msg);
		return send_msg(the_service.world_connector_, msg, false, true);
	}

	//收到回复
	int		handle_response(msg_koko_trade_inout_ret& msg)
	{
		if (is_complete_) return ERROR_SUCCESS_0;
		is_complete_ = true;

		trade_state_ = trade_state_responsed;
		if (msg.result_ == 0 || msg.result_ == 1){
			is_complete_ = true;
			int ret = trade_success(msg);
			if (ret == ERROR_SUCCESS_0){
				save_trade(ret_code_cache_succ);
				trade_state_ = trade_state_success;
			}
			else{
				save_trade(ret_code_cache_fail);
			}
		}
		else{
			trade_failed((msg.result_ << 8) | ret_code_response_failed);
		}

		glb_http_log.write_log("user_koko_auto_trade_in succ, %s", uid_.c_str());
		return ERROR_SUCCESS_0;
	}

	int		do_trade() {
		int ret = save_trade();
		ret |= prepare_trade();
		trade_state_ = trade_state_prepared;

		if (ret == ERROR_SUCCESS_0) {
			ret = send_request();
			trade_state_ = trade_state_request_sended;

			if (ret == ERROR_SUCCESS_0) {
				save_trade(ret_code_success);
				start_tick_ = ::time(nullptr);
				the_service.koko_trades_.insert(std::make_pair(sn_, shared_from_this()));
			}
			else {
				trade_failed(ret_code_internal_failed);
				glb_http_log.write_log("user_koko_auto_trade_in failed on send_request, %s", uid_.c_str());
			}
		}
		else {
			trade_failed(ret_code_internal_failed);
		}
		return ret;
	}
protected:
	service_t&					the_service;
	int							trade_state_;
	int							dir_;	//0 从平台兑入游戏部分货币， 1 从平台兑入游戏所有货币,  2 从游戏兑入平台。
	__int64						count_;
	bool						is_complete_;
	int				save_trade(int ret_code = ret_code_unknown)
	{	
		Database& db = *the_service.gamedb_;
		BEGIN_INSERT_TABLE("trade_inout");
		SET_FIELD_VALUE("uid", uid_);
		SET_FIELD_VALUE("dir", dir_ | (cid_ << 16));//0  out, 1 in
		SET_FIELD_VALUE("state", trade_state_);
		SET_FIELD_VALUE("count", count_);
		SET_FIELD_VALUE("return_code", ret_code);
		SET_FINAL_VALUE("ordernum", sn_);
		EXECUTE_IMMEDIATE();
		return ERROR_SUCCESS_0;
	}
	virtual int		prepare_trade()
	{
		return ERROR_SUCCESS_0;
	}

	virtual int		trade_failed(int ret)
	{
		is_complete_ = true;
		return save_trade(ret);
	}


	void			build_sign(msg_koko_trade_inout& msg)
	{
		std::string pat = msg.uid_;
		pat += boost::lexical_cast<std::string>(msg.dir_);
		pat += boost::lexical_cast<std::string>(msg.count_);
		pat += boost::lexical_cast<std::string>(msg.time_stamp_);
		pat += msg.sn_;
		pat += the_service.the_config_.world_key_;
		std::string si = md5_hash(pat);
		COPY_STR(msg.sign_, si.c_str());
	}
};

template<class service_t>
class  user_koko_auto_trade_in : public koko_currency_trade<service_t>
{
public:
	user_koko_auto_trade_in(service_t& srv) :
		koko_currency_trade(srv, item_id_gold_game, 1)
	{
	}
	virtual	int		trade_success(msg_koko_trade_inout_ret& msg)
	{
		count_ = msg.count_;
		int ret = ERROR_SUCCESS_0;
		__int64 new_value = 0;
		if (cid_ == item_id_gold_game) {
			ret = the_service.cache_helper_.give_currency(uid_, msg.count_, new_value, false);
		}

		//通知钱已兑换
		player_ptr pp = the_service.get_player(uid_);
		if (pp.get()) {
			if (islogin_) {
				msg_currency_change msg;
				msg.credits_ = (double)new_value;
				msg.why_ = msg_currency_change::why_trade_complete;
				auto psock = pp->from_socket_.lock();
				if (psock.get()) {
					send_protocol(psock, msg);
				}
			}
			else {
				auto plogic = pp->the_game_.lock();
				if (plogic.get()) {
					plogic->sync_info_to_client(pp->uid_);
				}
			}
		}
		return ret;
	}
};

template<class service_t>
class  user_koko_auto_trade_out : public koko_currency_trade<service_t>
{
public:
	user_koko_auto_trade_out(service_t& srv):
		koko_currency_trade(srv, item_id_gold_game, 2)
	{
		count_ = 0;
	}
protected:
	//先从cache里扣钱,
	virtual int		prepare_trade()
	{
		__int64 outc = 0;
		int ret = the_service.cache_helper_.apply_cost(uid_, 0, outc, true, false);
		if (ret == ERROR_SUCCESS_0){
			count_ = outc;
		}
		return ret;
	}

	//如果请求koko平台失败,则返钱
	virtual int		trade_failed(int ret)
	{
		is_complete_ = true;
		__int64 outc = 0;
		ret = the_service.cache_helper_.give_currency(uid_, count_, outc, false);
		if (ret == ERROR_SUCCESS_0){
			save_trade(ret_code_cache_succ);
			count_ = 0;
		}
		else{
			save_trade(ret_code_cache_fail);
		}
		return ERROR_SUCCESS_0;
	}

	virtual	int		trade_success(msg_koko_trade_inout_ret& msg)
	{
		count_ = 0;
		return ERROR_SUCCESS_0;
	}
};

template<class service_t, class callback_t>
class  koko_add_kb : public koko_currency_trade<service_t>
{
public:
	koko_add_kb(service_t& srv, __int64 count, callback_t cb) :
		koko_currency_trade(srv, item_id_gold, 2)
	{
		cb_ = cb;
		count_ = count;
		if (count_ < 0) {
			count = 0;
		}
	}
protected:
	callback_t cb_;
	virtual int		prepare_trade()
	{
		return ERROR_SUCCESS_0;
	}

	//如果请求koko平台失败,则返钱
	virtual int		trade_failed(int ret)
	{
		msg_koko_trade_inout_ret msg;
		msg.result_ = -1;
		msg.count_ = 0;
		return cb_(msg);
	}

	virtual	int		trade_success(msg_koko_trade_inout_ret& msg)
	{
		return cb_(msg);
	}
};

template<class service_t, class callback_t>
class  koko_cost_kb : public koko_add_kb<service_t, callback_t>
{
public:
	koko_cost_kb(service_t& srv, __int64 count, callback_t cb) :
		koko_add_kb(srv, item_id_gold, 0)
	{
	}
};


template<class service_t>
class platform_koko
{
public:
	service_t&		the_service;

	platform_koko(service_t& srv):the_service(srv)
	{

	}

	template<class player_t>
	void		auto_trade_in(player_t p, bool islogin = false)
	{
		user_koko_auto_trade_in<service_t>* ptrade = new user_koko_auto_trade_in<service_t>(the_service);
		boost::shared_ptr<user_koko_auto_trade_in<service_t>> pt(ptrade);
		ptrade->platform_uid_ = p->platform_uid_;
		ptrade->uid_ = p->uid_;
		ptrade->islogin_ = islogin;
		ptrade->do_trade();
	}

	template<class player_t>
	void		auto_trade_out(player_t p)
	{
		user_koko_auto_trade_out<service_t>* ptrade = new user_koko_auto_trade_out<service_t>(the_service);
		boost::shared_ptr<user_koko_auto_trade_out<service_t>> pt(ptrade);
		ptrade->platform_uid_ = p->platform_uid_;
		ptrade->uid_ = p->uid_;
		ptrade->do_trade();
	}

// 	template<class player_t, class callback_t>
// 	void		add_kb(player_t p, __int64 count, callback_t cb)
// 	{
// 		koko_add_kb<service_t, callback_t>* ptrade = new koko_add_kb<service_t, callback_t>(the_service, count, cb);
// 		boost::shared_ptr<koko_add_kb<service_t, callback_t>> pt(ptrade);
// 		ptrade->platform_uid_ = p->platform_uid_;
// 		ptrade->uid_ = p->uid_;
// 		ptrade->do_trade();
// 	}
// 
// 	template<class player_t, class callback_t>
// 	void		cost_kb(player_t p, __int64 count, callback_t cb)
// 	{
// 		koko_cost_kb<service_t, callback_t>* ptrade = new koko_cost_kb<service_t, callback_t>(the_service, count, cb);
// 		boost::shared_ptr<koko_cost_kb<service_t, callback_t>> pt(ptrade);
// 		ptrade->platform_uid_ = p->platform_uid_;
// 		ptrade->uid_ = p->uid_;
// 		ptrade->do_trade();
// 	}
};