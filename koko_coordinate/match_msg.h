#pragma once
#include "msg.h"
#include "msg_from_client.h"
#include "koko_socket.h"
#include "game_srv_socket.h"
#include "match.h"
#include "error.h"

/************************************************************************/
/* 发给客户端的消息
/************************************************************************/
//比赛信息查询结果
class msg_match_info : public msg_base<none_socket>
{
public:
	int		match_id_;
	int		registered_;
	int		register_count_;
	int		time_left_;
	int		state_;
	char	ip_[max_name];
	int		port_;
	msg_match_info()
	{
		head_.cmd_ = GET_CLSID(msg_match_info);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data(registered_, data_s);
		write_data(register_count_, data_s);
		write_data(time_left_, data_s);
		write_data(state_, data_s);
		write_data<char>(ip_, max_name, data_s);
		write_data(port_, data_s);
		return 0;
	}
};

//通知客户端比赛开始了
class msg_match_start : public msg_base<none_socket>
{
public:
	int		match_id_;
	char	ins_id_[max_guid];
	int		register_count_;
	msg_match_start()
	{
		head_.cmd_ = GET_CLSID(msg_match_start);
	}
	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data<char>(ins_id_, max_guid, data_s);
		write_data(register_count_, data_s);
		return 0;
	}
};

class msg_confirm_join_game : public msg_match_start
{
public:
	msg_confirm_join_game()
	{
		head_.cmd_ = GET_CLSID(msg_confirm_join_game);
	}
};

class msg_match_data : public msg_base<none_socket>
{
public:
	match_info	inf;
	msg_match_data()
	{
		head_.cmd_ = GET_CLSID(msg_match_data);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(inf.id_, data_s);
		write_data(inf.match_type_, data_s);
		write_data(inf.game_id_, data_s);
		write_data<char>((char*)inf.match_name_.c_str(), 128, data_s);
		write_data<char>((char*)inf.thumbnail_.c_str(), 128, data_s);
		write_data<char>((char*)inf.help_url_.c_str(), 128, data_s);
		write_data<char>((char*)inf.prize_desc_.c_str(), 256, data_s);
		write_data(inf.start_type_, data_s);
		write_data(inf.require_count_, data_s);
		write_data(inf.start_stripe_, data_s);
		write_data(inf.end_type_, data_s);
		write_data(inf.end_data_, data_s);
		write_data<char>((char*)inf.srvip_.c_str(), 64, data_s);
		write_data(inf.srvport_, data_s);
		write_data(inf.cost_.size(), data_s);
		auto it = inf.cost_.begin();
		while (it != inf.cost_.end())
		{
			write_data(it->first, data_s);
			write_data(it->second, data_s);
			it++;
		}
		return 0;
	}
};

class	msg_game_data : public msg_base<none_socket>
{
public:
	game_info		inf;
	msg_game_data()
	{
		head_.cmd_ = GET_CLSID(msg_game_data);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(inf.id_, data_s);
		write_data(inf.type_, data_s);
		write_data<char>(inf.dir_, max_guid, data_s);
		write_data<char>(inf.exe_, max_guid, data_s);
		write_data<char>(inf.update_url_, 128, data_s);
		write_data<char>(inf.help_url_, 128, data_s);
		write_data<char>(inf.game_name_, 128, data_s);
		write_data<char>(inf.thumbnail_, 128, data_s);
		write_data<char>(inf.solution_, 16, data_s);
		write_data(inf.no_embed_, data_s);
		write_data<char>(inf.catalog_, 128, data_s);
		return 0;
	}
};


class msg_host_data : public msg_base<none_socket>
{
public:
	host_info inf;
	msg_host_data()
	{
		head_.cmd_ = GET_CLSID(msg_host_data);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(inf.roomid_, data_s);
		write_data<char>(inf.roomname_, 128, data_s);
		write_data<char>(inf.host_uid_, 64, data_s);
		write_data<char>(inf.thumbnail_, 128, data_s);
		write_data<char>(inf.avaddr_.ip_, 32, data_s);
		write_data(inf.avaddr_.port_, data_s);
		write_data<char>(inf.game_addr_.ip_, 32, data_s);
		write_data(inf.game_addr_.port_, data_s);
		write_data(inf.gameid_, data_s);
		write_data(inf.popular_, data_s);
		write_data(inf.online_players_, data_s);
		write_data(inf.is_online_, data_s);
		write_data(inf.avshow_start_, data_s);
		return 0;
	}
};

class msg_game_room_data : public msg_base<none_socket>
{
public:
	game_room_inf inf;
	msg_game_room_data()
	{
		head_.cmd_ = GET_CLSID(msg_game_room_data);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(inf.game_id_, data_s);
		write_data(inf.room_id_, data_s);
		write_data<char>(inf.room_name_, 64, data_s);
		write_data<char>(inf.room_desc_, 64, data_s);
		write_data<char>(inf.thumbnail_, 128, data_s);
		write_data<char>(inf.ip_, 32, data_s);
		write_data(inf.port_, data_s);
		write_data<char>(inf.require_, 256, data_s);
		return 0;
	}
};

class		msg_my_game_data : public msg_base<none_socket>
{
public:
	int gameid_,		
		lv, exp,
		exp_max,
		exp_master, 
		exp_master_max,
		exp_cons,
		exp_cons_max;
	msg_my_game_data()
	{
		head_.cmd_ = GET_CLSID(msg_my_game_data);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(gameid_, data_s);
		write_data(lv, data_s);
		write_data(exp, data_s);
		write_data(exp_max, data_s);
		write_data(exp_master, data_s);
		write_data(exp_master_max, data_s);
		write_data(exp_cons, data_s);
		write_data(exp_cons_max, data_s);
		return 0;
	}
};

class		msg_match_champion : public msg_base<none_socket>
{
public:
	int		gameid_;
	char	nickname_[max_guid];
	char	head_ico_[max_guid];
	char  match_name_[max_guid];
	int		prize_id_[3];
	int		count_[3];
	msg_match_champion()
	{
		head_.cmd_ = GET_CLSID(msg_match_champion);
		prize_id_[0] = -1;
		prize_id_[1] = -1;
		prize_id_[2] = -1;
		head_ico_[0] = 0;
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(gameid_, data_s);
		write_data(nickname_, max_guid, data_s);
		write_data(head_ico_, max_guid, data_s);
		write_data(match_name_, max_guid, data_s);
		write_data<int>(prize_id_, 3, data_s, true);
		write_data<int>(count_, 3, data_s, true);
		return 0;
	}
};

class msg_match_report : public msg_base<none_socket>
{
public:
	int					match_id_;
	int					my_rank_;
	int					rankmax_;
	int					prize_[3];
	int					count_[3];
	msg_match_report()
	{
		head_.cmd_ = GET_CLSID(msg_match_report);
		prize_[0] = -1;
		prize_[1] = -1;
		prize_[2] = -1;
		count_[0] = 0;
		count_[1] = 0;
		count_[2] = 0;
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data(my_rank_, data_s);
		write_data(rankmax_, data_s);
		write_data<int>(prize_, 3, data_s, true);
		write_data<int>(count_, 3, data_s, true);
		return 0;
	}
};

//通知游戏服务器比赛结束了
class msg_match_over : public msg_base<none_socket>
{
public:
	int		match_id_;
	int		reason_; //0 正常结束， 1 报名人数不够结束， 2 同意参加的人数不够结束
	msg_match_over()
	{
		head_.cmd_ = GET_CLSID(msg_match_over);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data(reason_, data_s); 
		return 0;
	}
};

class msg_confirm_join_deliver : public msg_base<none_socket>
{
public:
	int			match_id_;
	int			total_;
	int			current_;
	msg_confirm_join_deliver()
	{
		head_.cmd_ = GET_CLSID(msg_confirm_join_deliver);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data(total_, data_s);
		write_data(current_, data_s); 
		return 0;
	}
};

/************************************************************************/
/* 来自客户端的消息                                                                     */
/************************************************************************/
class msg_confirm_join_game_reply : public msg_authrized<koko_socket_ptr>
{
public:
	int		match_id_;
	int		agree_;		//0不同意,1同意 2 暂停
	msg_confirm_join_game_reply()
	{
		head_.cmd_ = GET_CLSID(msg_confirm_join_game_reply);
	}

	int					read(const char*& data_s, unsigned int l)
	{
		msg_authrized::read(data_s, l);
		read_data(match_id_, data_s);
		read_data(agree_, data_s);
		return 0;
	}
	int					handle_this();
};

//报名比赛
class msg_match_register : public msg_authrized<koko_socket_ptr>
{
public:
	int			match_id_;
	msg_match_register()
	{
		head_.cmd_ = GET_CLSID(msg_match_register);
	}
	int					read(const char*& data_s, unsigned int l)
	{
		msg_authrized::read(data_s, l);
		read_data(match_id_, data_s);
		return 0;
	}
	int			handle_this();
};

//取消报名
class msg_match_cancel : public msg_match_register
{
public:
	msg_match_cancel()
	{
		head_.cmd_ = GET_CLSID(msg_match_cancel);
	}
	int			handle_this();
};

//查询比赛信息
class msg_query_info : public msg_from_client<koko_socket_ptr>
{
public:
	int			query_type_; //0 查询游戏信息 1查询比赛信息 2 查询比赛积分名次, 3 刷新所有数据
	int			query_data_;
	int			exdata_;
	msg_query_info()
	{
		head_.cmd_ = GET_CLSID(msg_query_info);
	}
	int					read(const char*& data_s, unsigned int l)
	{
		msg_from_client::read(data_s, l);
		read_data(query_type_, data_s);
		read_data(query_data_, data_s);
		read_data(exdata_, data_s);
		return 0;
	}
	int			handle_this();
};

/************************************************************************/
/* 来自内部服务器的消息
/************************************************************************/
//更新比赛分数
class msg_match_score : public msg_base<srv_socket_ptr>
{
public:
	char	uid_[max_guid];
	int		match_id_;
	int		score_;
	int		operator_; // 0 - set, 1 - add
	msg_match_score()
	{
		head_.cmd_ = GET_CLSID(msg_match_score);
	}

	int					read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data<char>(uid_, max_guid, data_s);
		read_data(match_id_, data_s);
		read_data(score_, data_s);
		read_data(operator_, data_s);
		return 0;
	}
	int					handle_this();
};


//通知游戏服务器比赛结束了
class msg_match_end : public msg_base<none_socket>
{
public:
	int		match_id_;
	msg_match_end()
	{
		head_.cmd_ = GET_CLSID(msg_match_end);
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		return 0;
	}
};


//通知游戏服务器玩家分配进入游戏
class msg_player_distribute : public msg_base<none_socket>
{
public:
	char	uid_[max_guid];
	int		match_id_;
	char	game_ins_[max_guid];
	int		score_;
	int		time_left_;
	msg_player_distribute()
	{
		head_.cmd_ = GET_CLSID(msg_player_distribute);
	}
	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(uid_, max_guid, data_s);
		write_data(match_id_, data_s);
		write_data<char>(game_ins_, max_guid, data_s);
		write_data(score_, data_s);
		write_data(time_left_, data_s);
		return 0;
	}
};

class msg_my_match_score : public msg_base<none_socket>
{
public:
	int		time_left_;
	int		match_id_;
	int		my_score_;
	int		max_score_;
	int		my_rank_;
	int		max_rank_;
	msg_my_match_score()
	{
		head_.cmd_ = GET_CLSID(msg_my_match_score);
	}
	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(time_left_, data_s);
		write_data(match_id_, data_s);
		write_data(my_score_, data_s);
		write_data(max_score_, data_s);
		write_data(my_rank_, data_s);
		write_data(max_rank_, data_s);
		return 0;
	}
};

class msg_query_data_begin : public msg_base<none_socket>
{
public:
	msg_query_data_begin()
	{
		head_.cmd_ = GET_CLSID(msg_query_data_begin);
	}
};

//游戏服务端通知玩家被淘汰
class msg_player_eliminated_by_game : public msg_base<srv_socket_ptr>
{
public:
	msg_player_eliminated_by_game()
	{
		head_.cmd_ = GET_CLSID(msg_my_match_score);
	}

	int			match_id_;
	char		uid_[max_guid];
	int			my_rank_;

	int			read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data(match_id_, data_s);
		read_data<char>(uid_, max_guid, data_s);
		read_data(my_rank_, data_s);
		return 0;
	}
	int			handle_this();
};
