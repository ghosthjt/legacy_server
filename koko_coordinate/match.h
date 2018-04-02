#pragma once
#include <string>
#include "boost/smart_ptr.hpp"
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "error.h"

struct none_socket;
template<class t> class msg_base;

//比赛过程:报名->比赛，结束，筛选->下一轮,比赛，结束，筛选->比赛结束->结算比赛->下一次比赛开始报名
//有关比赛中玩家强行退出比赛的处理，考虑采用拖管模式。
//玩家如果同意参加比赛，但却因各种原因没有进游戏服务器，这种情况下，需要进入拖管模式，以人工AI方式进行比赛。
//每个玩家同时只能参加1场比赛，不可以同时参加好几场。
//boost::asio::deadline_timer在每次调用async_wait之前，一定要调用expires_at() 或expires_from_now(),
//否则效果和expires_from_now(0)是一样的效果
enum match_type
{
	match_type_time_restrict,
	match_type_continual,
	match_type_eliminate,

};


class time_counter
{
public:
	void		restart()
	{
		pt = boost::posix_time::microsec_clock::local_time();
	}

	int			elapse()
	{
		return (int)(boost::posix_time::microsec_clock::local_time() - pt).total_milliseconds();
	}
protected:
	boost::posix_time::ptime pt;
};
//比赛过程数据
struct matching_data
{
	int		game_state_; //0-初始，1-游戏中，2-游戏结束, 3-已掉线
	int		score_;
	std::string		uid_;
	int		game_id_;
	std::string		game_ins_;
	matching_data()
	{
		game_state_ = 0;
		score_ = 0;
	}
};

typedef boost::shared_ptr<matching_data> mdata_ptr;

//每场比赛都由报名开始
class  game_match_base : public match_info
{
public:
	enum 
	{
		state_wait_register,
		state_wait_player_ready,
		state_matching,
		state_balance,
	};
	std::map<std::string, int>	registers_;//uid,是否同意参赛，0-报名，1-已发送询问是否参赛，2-同意，3不同意 4 暂停参加
	std::map<std::string, mdata_ptr>	scores_;
	game_match_base(boost::asio::io_service& ios);
	
	void		clean();

	int			register_this(std::string uid);

	int			update_score(std::string uid, int score, int op = 0);

	int			start();

	int			confirm_join(std::string uid, int agree);
	int			cancel_register(std::string uid);

	int			get_time_left()
	{
		return timer_.expires_from_now().total_seconds();
	}
	int			get_state()
	{
		return state_;
	}
	virtual int			sync_state_to_client(std::string uid);
	void		eliminate_player(std::string uid);
	int			send_msg_to_gamesrv(int match_id, msg_base<none_socket>& msg);

	template<class pr_t>
	int			broadcast_msg(msg_base<none_socket>& msg, pr_t pr)
	{
		auto it = registers_.begin();
		while (it != registers_.end())
		{
			player_ptr pp = get_player(it->first);
			if (pp.get() && pr(pp)){
				auto pcnn = pp->from_socket_.lock();
				if (pcnn.get())	{
					send_msg(pcnn, msg);
				}
			}
			it++;
		}

		return error_success;
	}

protected:
	boost::asio::io_service& ios_;
	boost::asio::deadline_timer timer_, update_timer_;
	int			state_;

	std::map<std::string, std::vector<mdata_ptr>> running_games_;

	std::string		ins_id_; //每一场比赛都会换ins_id_;

	int			this_turn_start();

	void		this_turn_over(int reason);
	//结算阶段完成，进入下一轮比赛
	int			wait_balance_complete(boost::system::error_code ec);

	//更新比赛
	int			update(boost::system::error_code ec);

	//注册时间到了，开始比赛
	int			wait_register_complete(boost::system::error_code ec);
	//等待玩家回复超时
	void		wait_player_ready_timeout(boost::system::error_code ec);

	int			wait_register();

	//让客户端进入游戏
	void		confirm_join_game();
	//结算比赛
	void		balance(std::string eliminate_uid);
	void		save_game_data(matching_data& md, int exp, int exp_master, int exp_cons);
	void		save_match_result(matching_data& md, int rank, std::string reward_type, std::string count);
	void		save_register(std::string uid, std::map<int, int> cost);
	void		do_distribute(std::vector<mdata_ptr>& group);

	virtual int		check_this_turn_start();
	virtual int		check_this_turn_over(){	return 0;	}
	virtual int		can_register(std::string uid) {return 0;}
	virtual int		pay_for_register(std::string uid);
	virtual int		update_logic();
	virtual int		do_extra_turn_start_staff(){return 0;}
	virtual int		do_extra_turn_over_staff() {return 0;}
	virtual void	distribute_player(std::vector<std::pair<std::string, mdata_ptr>>& v);
};

//闯关赛
class		game_match_continual : public game_match_base
{
public:
	int		current_level_;
	int		level_cap_;
	int		cancel_;
	game_match_continual(boost::asio::io_service& ios):
		game_match_base(ios)
	{
		cancel_ = false;
	}

protected:
	int		check_this_turn_over()
	{
		//如果到达最大关了，或者玩家不再继续了，结束
		if (current_level_ >= level_cap_ || cancel_){
			return 1;
		}
		return 0;
	}
};

//定时结束的比赛
//定时结束比赛的分配玩家的方式是自由进入，定时由本服务器来通知游戏结束
class		game_match_time_restrict : public game_match_base
{
public:
	game_match_time_restrict(boost::asio::io_service& ios): game_match_base(ios)
	{
		is_over_ = 0;
		end_type_ = 0;
	}
protected:
	int		is_over_;
	int		do_extra_turn_start_staff();

	int		check_this_turn_over()
	{
		return is_over_;
	}
	//通知服务器结束比赛
	void  forece_match_over(boost::system::error_code ec)
	{
		notify_server_to_end_game();
	}
	//等待玩家分数
	void  wait_for_feedback(boost::system::error_code ec)
	{
		is_over_ = true;
	}

	void	notify_server_to_end_game();
};

//淘汰赛,每局比赛结束方式都是游戏主动结束，不由本服务器来控制结束时间
class  game_match_eliminate : public game_match_base
{
public:
	game_match_eliminate(boost::asio::io_service& ios):game_match_base(ios)
	{
		is_over_ = false;
		score_line_ = 0;
		phase_ = 0;
	}

protected:
	__int64			score_line_;	//继续参赛资格线
	bool				is_over_;
	int					phase_;	//0淘汰赛阶段, 1 决赛等待阶段, 2决赛阶段
	time_counter	tm_counter_;
	int		update_logic();
	//开始淘汰
	void	eliminate();

	int		check_this_turn_over();
	
	int		do_extra_turn_start_staff();
	
	int		do_extra_turn_over_staff();
	bool	is_all_finished();
};

typedef boost::shared_ptr<game_match_base> match_ptr;


class match_manager
{
public:
	std::map<int, match_ptr> all_matches;
	boost::asio::io_service& ios_;
	static boost::shared_ptr<match_manager> get_instance(boost::asio::io_service* ios = nullptr);
	static void  free_instance();
	int			load_all_matches();

	int			begin_all_matches();
	int			stop_all_matches();
	int			register_match(int matchid, std::string uid);
	int			update_score(int matchid, int score, int op, std::string uid);
	int			confirm_math(int matchid, std::string uid, int agree);
	int			cancel_register(int matchid, std::string uid);
	match_manager(boost::asio::io_service& ios) :ios_(ios)
	{

	}
};

