#pragma once
#include <string>
#include "boost/smart_ptr.hpp"
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "error.h"

struct none_socket;
template<class t> class msg_base;

//��������:����->������������ɸѡ->��һ��,������������ɸѡ->��������->�������->��һ�α�����ʼ����
//�йر��������ǿ���˳������Ĵ������ǲ����Ϲ�ģʽ��
//������ͬ��μӱ�������ȴ�����ԭ��û�н���Ϸ����������������£���Ҫ�����Ϲ�ģʽ�����˹�AI��ʽ���б�����
//ÿ�����ͬʱֻ�ܲμ�1��������������ͬʱ�μӺü�����
//boost::asio::deadline_timer��ÿ�ε���async_wait֮ǰ��һ��Ҫ����expires_at() ��expires_from_now(),
//����Ч����expires_from_now(0)��һ����Ч��
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
//������������
struct matching_data
{
	int		game_state_; //0-��ʼ��1-��Ϸ�У�2-��Ϸ����, 3-�ѵ���
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

//ÿ���������ɱ�����ʼ
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
	std::map<std::string, int>	registers_;//uid,�Ƿ�ͬ�������0-������1-�ѷ���ѯ���Ƿ������2-ͬ�⣬3��ͬ�� 4 ��ͣ�μ�
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

	std::string		ins_id_; //ÿһ���������ỻins_id_;

	int			this_turn_start();

	void		this_turn_over(int reason);
	//����׶���ɣ�������һ�ֱ���
	int			wait_balance_complete(boost::system::error_code ec);

	//���±���
	int			update(boost::system::error_code ec);

	//ע��ʱ�䵽�ˣ���ʼ����
	int			wait_register_complete(boost::system::error_code ec);
	//�ȴ���һظ���ʱ
	void		wait_player_ready_timeout(boost::system::error_code ec);

	int			wait_register();

	//�ÿͻ��˽�����Ϸ
	void		confirm_join_game();
	//�������
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

//������
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
		//������������ˣ�������Ҳ��ټ����ˣ�����
		if (current_level_ >= level_cap_ || cancel_){
			return 1;
		}
		return 0;
	}
};

//��ʱ�����ı���
//��ʱ���������ķ�����ҵķ�ʽ�����ɽ��룬��ʱ�ɱ���������֪ͨ��Ϸ����
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
	//֪ͨ��������������
	void  forece_match_over(boost::system::error_code ec)
	{
		notify_server_to_end_game();
	}
	//�ȴ���ҷ���
	void  wait_for_feedback(boost::system::error_code ec)
	{
		is_over_ = true;
	}

	void	notify_server_to_end_game();
};

//��̭��,ÿ�ֱ���������ʽ������Ϸ�������������ɱ������������ƽ���ʱ��
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
	__int64			score_line_;	//���������ʸ���
	bool				is_over_;
	int					phase_;	//0��̭���׶�, 1 �����ȴ��׶�, 2�����׶�
	time_counter	tm_counter_;
	int		update_logic();
	//��ʼ��̭
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

