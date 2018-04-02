#pragma once
#include "net_service.h"
#include "boost/bind.hpp"
#include <unordered_map>
#include <list>
#include "Database.h"
#include "Query.h"
#include "utf8_db.h"
#include "boost/thread.hpp"
#include "msg_from_client.h"
#include "db_delay_helper.h"
#include "MemCacheClient.h"
#include "game_ids.h"
#include "boost/date_time.hpp"
#include "schedule_task.h"
#include "boost/atomic.hpp"
#include "md5.h"
#include <map>
#include "mem_cache_helper.h"
#include "dice_controller.h"

typedef long long longlong;
class Database;
typedef boost::shared_ptr<Database>	db_ptr;
typedef boost::shared_ptr<boost::thread> thread_ptr;
class telfee_match_info;
typedef boost::shared_ptr<telfee_match_info> match_ptr;

class IGiftDispatcher;
class GiftSystemEverydaySign;
class GiftSystemLuckWheel;
class GiftSystemSubsidy;
extern log_file<cout_and_file_output> glb_http_log;
class tast_on_5sec : public task_info
{
public:
	boost::atomic_uint	msg_recved;
	boost::atomic_uint	msg_handled;
	tast_on_5sec(boost::asio::io_service& ios):task_info(ios)
	{
		msg_handled = 0;
		msg_recved = 0;
	}
	int routine()
	{
		//glb_http_log.case_status_write_log(glb_http_log.get_log_color("yellow"), "game is running ok, this time msg handled:%d, online players:%d", msg_handled, msg_recved);
		glb_http_log.write_log("game is running ok, this time msg handled:%d, online players:%d", msg_handled, msg_recved);
		msg_handled = 0;
		msg_recved = 0;
		return task_info::routine_ret_continue;
	}
};

class session
{
public:
	ip::tcp::endpoint remote_;
	std::string	session_key_;
	boost::posix_time::ptime create_time_;
	unsigned int		login_time_;//每秒登录次数
	session()
	{
		login_time_ = 0;
	}
};

class world_socket : public native_socket
{
public:
	world_socket(boost::asio::io_service& ios) :
		native_socket(ios)
	{

	}
};
template<class t>
class koko_currency_trade;

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
class	 game_service_base
{
public:
	typedef game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t> this_type;
	typedef boost::shared_ptr<player_t> player_ptr;
	typedef boost::shared_ptr<socket_t> remote_socket_ptr;
	typedef typename msg_base<remote_socket_ptr>::msg_ptr msg_ptr;
	typedef boost::shared_ptr<msg_cache_cmd_ret<mem_connector_ptr>> gold_rank_ptr;
	typedef boost::shared_ptr<koko_currency_trade<this_type>> koko_trade_ptr;

	std::unordered_map<std::string, session>		login_sessions_;
	memcache_helper<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>	cache_helper_;
	std::unordered_map<std::string, player_ptr>		players_;
	db_delay_helper					delay_db_game_;
	std::map<std::string, player_ptr>				free_bots_;
	boost::shared_ptr<tast_on_5sec>					ptask;

    boost::shared_ptr<GiftSystemEverydaySign>       gift_everyday_sign_;
    boost::shared_ptr<GiftSystemLuckWheel>          gift_luck_wheel_;
    boost::shared_ptr<GiftSystemSubsidy>            gift_subsidy_;
    virtual void                                    refresh_gift_system();
    virtual IGiftDispatcher*                        create_gift_dispatcher(int id);
    virtual __int64                                get_player_today_win(player_ptr pp);
	
	boost::shared_ptr<world_socket>					world_connector_;
	std::map<std::string, koko_trade_ptr>			koko_trades_;
	config_t					the_config_;
	config_t					the_new_config_;
	db_ptr						gamedb_, email_db_;
	io_service					timer_sv_, http_sv_;

	vector<logic_t>				the_golden_games_;
#ifdef HAS_TELFEE_MATCH
	std::map<int, match_t>		the_matches_;
#endif // HAS_TELFEE_MATCH

	vector<gold_rank_ptr>		glb_goldrank_;
	vector<sceneset_t>			scenes_;
	sceneset_t					private_scene_set_;
	bool						wait_for_config_update_;
	balance_warden				warden_;
	bool						stoped_;
	net_server<socket_t>		net_;
	explicit game_service_base(int game_id):net_(),
		cache_helper_(*this, game_id)
	{
		wait_for_config_update_ = false;
	}

	virtual ~game_service_base()
	{		

	}
	
	__int64					add_stock(__int64 val, bool init = false)
	{
		return warden_.add_sysbalance(val);
	}

	float						get_personal_rate(player_ptr pp)
	{
		return 1.0;
	}

	//1系统必赢, 0正常, -1 系统必输
	int							cheat_win();
	msg_base<boost::shared_ptr<world_socket>>::msg_ptr create_world_msg(unsigned short cmd);
	virtual	msg_ptr				create_msg(unsigned short cmd) = 0;
	virtual	int					handle_msg(msg_ptr msg, remote_socket_ptr from_sock) = 0;
	virtual match_ptr			create_match(int tpid);
	player_ptr					get_player(std::string uid)
	{
		auto it = players_.find(uid);
		if (it == players_.end())	{
			return player_ptr();
		}
		return it->second;
	}

	player_ptr get_player_in_game( string uid );

#ifdef HAS_TELFEE_MATCH
	match_t get_player_in_match(std::string uid)
	{
		for (auto mc : the_matches_)
		{
			auto itf = mc.second->registers_.find(uid);
			if (itf != mc.second->registers_.end()){
				return mc.second;
			}
		}
		return match_ptr();
	}
#endif

	int							run();

	int							stop();

	void						broadcast_msg(msg_base<none_socket>& msg);
	
	virtual void				on_connection_lost(player_ptr p);
	virtual void				auto_trade_in(player_ptr p, bool islogin);
	virtual void				auto_trade_out(player_ptr p );
	void						main_thread_update();
protected:

	thread_ptr					sub_thread_;


	virtual void				on_main_thread_update() = 0;
	virtual void 				on_sub_thread_update() = 0;
	virtual int					on_run() = 0;
	virtual int					on_stop() = 0;

	virtual void				update_every_5sec();
	void						sub_thread_update();

	int							update_palyers();

	int							process_msg(std::vector<int>& handled);
};

class			no_logic_cls
{
public:
	void		stop_logic(){}
};

class			no_match_cls
{
public:
	int			iid_;
	std::map<std::string, int> registers_;
	vector<boost::shared_ptr<no_logic_cls>> vgames_;
	void		update(){};
};
typedef boost::shared_ptr<no_match_cls> no_match;
