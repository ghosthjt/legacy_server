#pragma once
#include "msg.h"
#include "utility.h"

enum
{
    GET_CLSID(msg_json_form) = 0xCCCC,
    GET_CLSID(msg_register_to_world) = 19999,
    GET_CLSID(msg_match_score) = 20002,
    GET_CLSID(msg_private_room_sync) = 20023,
    GET_CLSID(msg_private_room_psync) = 20024,
    GET_CLSID(msg_ping) = 0xFFFF,
    GET_CLSID(msg_user_login_ret) = 1000,
    GET_CLSID(msg_common_reply) = 1001,				//������Ϣid��Ҫ�̶������������ڹ�������
    GET_CLSID(msg_common_reply_srv) = 6000,	//������Ϣid��Ҫ�̶������������ڹ�������
    GET_CLSID(msg_logout) = 1034,
    GET_CLSID(msg_currency_change) = 1007,				//������Ϣid��Ҫ�̶������������ڹ�������
    GET_CLSID(msg_server_parameter) = 1107,
    GET_CLSID(msg_low_currency) = 1109,
    GET_CLSID(msg_player_seat) = 1110,
    GET_CLSID(msg_room_info),
    GET_CLSID(msg_player_leave),
    GET_CLSID(msg_deposit_change2),
    GET_CLSID(msg_chat_deliver),

    GET_CLSID(msg_everyday_gift),
    GET_CLSID(msg_levelup),
    GET_CLSID(msg_set_account_ret),
    GET_CLSID(msg_recommend_data),
    GET_CLSID(msg_trade_data),
    GET_CLSID(msg_game_info),
    GET_CLSID(msg_server_info),
    GET_CLSID(msg_rank_data),
    GET_CLSID(msg_everyday_sign_info),
    GET_CLSID(msg_everyday_sign_rewards),
    GET_CLSID(msg_luck_wheel_info),
    GET_CLSID(msg_luck_wheel_record),
    GET_CLSID(msg_subsidy_info),
    GET_CLSID(msg_subsidy_result),
	msg_serverid_this_game_begin = 1500,
};

class msg_set_account_ret : public msg_base<none_socket>
{
public:
	char			uid_[max_guid];
	unsigned int	iid_;
	msg_set_account_ret()
	{
		head_.cmd_ = GET_CLSID(msg_set_account_ret);
    }

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(uid_, max_guid, data_s);
		write_data(iid_, data_s);
		return 0;
	}
};

class msg_user_login_ret:public msg_base<none_socket>
{
public:
	int			iid_;
	int			lv_;
	double		currency_;
	double		exp_;
	double		exp_max_;
	char		uname_[max_name];
	char		uid_[max_guid];
	char		head_pic_[max_guid];
	msg_user_login_ret()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_ret);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(iid_, data_s);
		write_jvalue(lv_, data_s);
		write_jvalue(currency_, data_s);
		write_jvalue(exp_, data_s);
		write_jvalue(exp_max_, data_s);
		write_jvalue(uname_, data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(head_pic_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(iid_, data_s);
		write_data(lv_, data_s);
		write_data(currency_, data_s);
		write_data(exp_, data_s);
		write_data(exp_max_, data_s);
		write_data<char>(uname_, max_name, data_s);
		write_data<char>(uid_, max_guid, data_s);
		write_data<char>(head_pic_, max_guid, data_s);
		return 0;
	}
};

//�˺����ݸı䣬�����ʱ���ã����յ������Ϣʱ���Ե���
class msg_currency_change : public msg_base<none_socket>
{
public:
	enum{
		why_sync,
		why_everyday_login,
		why_levelup,
		why_rnd_result,
		why_online_prize,
		why_trade_complete,
	};
	double			credits_;			//����ʽ��� 8�ֽ�
	int				why_;					//0,ͬ��(����)��1,ÿ�յ�¼����(�仯��)��2���ȼ�װ����(�仯��), 3,���񽱽�

	msg_currency_change()
	{
		credits_ = 0;
		head_.cmd_ = GET_CLSID(msg_currency_change);
		why_ = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(credits_, data_s);
		write_jvalue(why_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(credits_, data_s);
		write_data(why_, data_s);
		return 0;
	}
};

class msg_low_currency : public msg_base<none_socket>
{
public:
	long long require_;
	msg_low_currency()
	{
		head_.cmd_ = GET_CLSID(msg_low_currency);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(require_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(require_, data_s);
		return 0;
	}
};

//�˺��������ط���¼��ʾ
class msg_logout : public msg_base<none_socket>
{
public:
	char uid_[max_guid];	//40�ֽ�
	msg_logout()
	{
		head_.cmd_ = GET_CLSID(msg_logout);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uid_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(uid_, max_guid, data_s);
		return 0;
	}
};

//��ұ�֤��仯
class msg_deposit_change2 : public msg_currency_change
{
public:
	int		pos_;
	int		display_type_;		//0������ҪƮ�� 1,ҪƮ��, 2 ��֤��仯
	msg_deposit_change2()
	{
		head_.cmd_ = GET_CLSID(msg_deposit_change2);
		display_type_ = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_currency_change::write(data_s);
		write_jvalue(pos_, data_s);
		write_jvalue(display_type_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_currency_change::write(data_s, l);
		write_data(pos_, data_s);
		write_data(display_type_, data_s);
		return 0;
	}
};

class msg_player_leave : public msg_base<none_socket>
{
public:
	int				pos_;		//�ĸ�λ���뿪��Ϸ
	int				why_;		//why = 0,��������˳���Ϸ, 1 �����˳���Ϸ 2 ��Ϸ�����峡�˳���Ϸ�� 3��T����Ϸ

	msg_player_leave()
	{
		head_.cmd_ = GET_CLSID(msg_player_leave);
		why_ = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(pos_, data_s);
		write_jvalue(why_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(pos_, data_s);
		write_data(why_, data_s);
		return 0;
	}
};


class msg_chat_deliver : public msg_base<none_socket>
{
public:
	int						channel_;						//Ƶ��
	char					name_[max_name];		//˭
	int						sex_;								//��ʱû��
	char					content_[256];			//˵��ʲô
	msg_chat_deliver()
	{
		head_.cmd_ = GET_CLSID(msg_chat_deliver);
		sex_ = -1;
		memset(name_, 0, max_name);
		memset(content_, 0, 256);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(channel_, data_s);
		write_jvalue(name_, data_s);
		write_jvalue(sex_, data_s);
		write_jvalue(content_, data_s);
		return 0;
	}

	void		set_content(std::string s)
	{
		if(s.length() > 256) return;
		COPY_STR(content_, s.c_str());
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(channel_, data_s);
		write_data<char>(name_, max_name, data_s);
		write_data<int>(sex_, data_s);
		write_data<char>(content_, 256, data_s, false);
		return 0;
	}
};

class msg_everyday_gift : public msg_base<none_socket>
{
public:
	msg_everyday_gift()
	{
		head_.cmd_ = GET_CLSID(msg_everyday_gift);
		getted_ = 0;
		conitnue_days_ = 0;
		type_ = 0;
	}

	int			conitnue_days_;	//���type==3,������Ҫ��ȡ�ȼ�
	int			getted_;	
	int			type_;		//��msg_get_prize_req���type��ͬ����
    int			extra_data_;		//���type ==3,������ʣ���ʱ

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(conitnue_days_, data_s);
		write_jvalue(getted_, data_s);
		write_jvalue(type_, data_s);
		write_jvalue(extra_data_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(conitnue_days_, data_s);
		write_data(getted_, data_s);
		write_data(type_, data_s);
		write_data(extra_data_, data_s);
		return 0;
	}
};

class msg_levelup : public msg_base<none_socket>
{
public:
	int lv_;
	double exp_;
	double exp_max_;
	int	pos_;
	msg_levelup()
	{
		head_.cmd_ = GET_CLSID(msg_levelup);
		lv_ = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(lv_, data_s);
		write_jvalue(exp_, data_s);
		write_jvalue(exp_max_, data_s);
		write_jvalue(pos_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(pos_, data_s);
		write_data(lv_, data_s);
		write_data(exp_, data_s);
		write_data(exp_max_, data_s);
		return 0;
	}
};

//�Ƽ���Ϣ����
class msg_recommend_data : public  msg_base<none_socket>
{
public:
	short SS;	//��ʼ���
	short	SE;	//�������	
	char uid_[max_guid];
	char uname_[max_name];
	char head_pic_[max_guid];
	int reward_getted_;	//�����Ƿ�����ȡ 0��δ��ȡ��1��ȡ
	msg_recommend_data()
	{
		head_.cmd_ = GET_CLSID(msg_recommend_data);
		SS = 0;
		SE = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(SS, data_s);
		write_jvalue(SE, data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(uname_, data_s);
		write_jvalue(head_pic_, data_s);
		write_jvalue(reward_getted_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(SS, data_s);
		write_data(SE, data_s);
		write_data<char>(uid_, max_guid, data_s);
		write_data<char>(uname_, max_name, data_s);
		write_data<char>(head_pic_, max_guid, data_s);
		write_data(reward_getted_, data_s);
		return 0;
	}
};

class msg_trade_data : public  msg_base<none_socket>
{
public:
	short SS;	//��ʼ���
	short	SE;	//�������	
	int		iid_;					//����id
	double	gold_;				//Ǯ
	double	time;					//ʱ��

	msg_trade_data()
	{
		head_.cmd_ = GET_CLSID(msg_trade_data);
		SS = 0;
		SE = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(SS, data_s);
		write_jvalue(SE, data_s);
		write_jvalue(iid_, data_s);
		write_jvalue(gold_, data_s);
		write_jvalue(time, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(SS, data_s);
		write_data(SE, data_s);
		write_data(iid_, data_s);
		write_data(gold_, data_s);
		write_data(time, data_s);
		return 0;
	}
};

class msg_server_info : public msg_base<none_socket>
{
public:
	int		player_count_;
	int		is_in_server_;
	msg_server_info()
	{
		head_.cmd_ = GET_CLSID(msg_server_info);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(player_count_, data_s);
		write_jvalue(is_in_server_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(player_count_, data_s);
		write_data(is_in_server_, data_s);
		return 0;
	}
};

class msg_game_info : public msg_base<none_socket>
{
public:
	char	game_id_[max_guid];
	int		extra_data1_;
    MSG_CONSTRUCT(msg_game_info);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(game_id_, data_s);
		write_jvalue(extra_data1_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(game_id_, max_guid, data_s);
		write_data(extra_data1_, data_s);
		return 0;
	}
};

class msg_room_info : public msg_base<none_socket>
{
public:
	int			room_id_;		//����id
	int			free_pos_;	//����λ��
	int			observers_;	//�۲�������

	msg_room_info()
	{
		head_.cmd_ = GET_CLSID(msg_room_info);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(room_id_, data_s);
		write_jvalue(free_pos_, data_s);
		write_jvalue(observers_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(room_id_, data_s);
		write_data(free_pos_, data_s);
		write_data(observers_, data_s);
		return 0;
	}
};

//�������λ��
class msg_player_seat : public msg_base<none_socket>
{
public:
	char			uid_[max_guid];	//˭
	char			head_pic_[max_guid];
	char			uname_[max_name];
	int				pos_;						//�����ĸ�λ��
	int				iid_;
	char			gun_type_[max_name];	//ǹ����
	int				gun_level_;	//ǹ�ȼ�
	int				lv_;
	msg_player_seat()
	{
		head_.cmd_ = GET_CLSID(msg_player_seat);
		memset(gun_type_, 0, max_name);
		gun_level_ = 0; lv_ = 0;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(head_pic_, data_s);
		write_jvalue(uname_, data_s);
		write_jvalue(pos_, data_s);
		write_jvalue(iid_, data_s);
		write_jvalue(gun_type_, data_s);
		write_jvalue(gun_level_, data_s);
		write_jvalue(lv_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(uid_, max_guid, data_s);
		write_data<char>(head_pic_, max_guid, data_s);
		write_data<char>(uname_, max_name, data_s);
		write_data(pos_, data_s);
		write_data<char>(gun_type_, max_name, data_s);
		write_data(gun_level_, data_s);
		write_data(lv_, data_s);
		return 0;
	}
};

class msg_rank_data : public msg_base<none_socket>
{
public:
	char	uname_[max_name];
	int		rank_type_;
	int		tag_; //0�µĿ�ʼ��1��������
	double	data_;
    MSG_CONSTRUCT(msg_rank_data);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uname_, data_s);
		write_jvalue(rank_type_, data_s);
		write_jvalue(tag_, data_s);
		write_jvalue(data_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(uname_, max_name, data_s);
		write_data(rank_type_, data_s);
		write_data(tag_, data_s);
		write_data(data_, data_s);
		return 0;
	}
};

class msg_everyday_sign_info : public msg_base<none_socket>
{
public:
    int     getted_;        //�Ƿ�����ǩ�� 0δǩ�� 1��ǩ��
    int     sign_days_;     //��ǩ������
    int     sign_result_;   //0��Ч 1ǩ���ɹ�
    MSG_CONSTRUCT(msg_everyday_sign_info);

    int			write(boost::property_tree::ptree& data_s)
    {
        msg_base::write(data_s);
        write_jvalue(getted_, data_s);
        write_jvalue(sign_days_, data_s);
        write_jvalue(sign_result_, data_s);
        return 0;
    }
    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data(getted_, data_s);
        write_data(sign_days_, data_s);
        write_data(sign_result_, data_s);
        return 0;
    }
};

class msg_everyday_sign_rewards : public msg_base<none_socket>
{
public:
    char    rewards_[1024];  //�ɵ͵��߷ָ���[:] [|] [\n]
    MSG_CONSTRUCT(msg_everyday_sign_rewards);

    int			write(boost::property_tree::ptree& data_s)
    {
        msg_base::write(data_s);
        write_jvalue(rewards_, data_s);
        return 0;
    }
    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data<char>(rewards_, sizeof(rewards_), data_s);
        return 0;
    }
};

class msg_luck_wheel_info : public msg_base<none_socket>
{
public:
    int     count_;         //ʣ�����
    int     reward_index_;  //��ǰ�齱��Ʒ������-1�������ֶ���Ч
    int     show_reward_;    //0����ʾ�������� 1��ʾ��������
    MSG_CONSTRUCT(msg_luck_wheel_info);

    int			write(boost::property_tree::ptree& data_s)
    {
        msg_base::write(data_s);
        write_jvalue(count_, data_s);
        write_jvalue(reward_index_, data_s);
        write_jvalue(show_reward_, data_s);
        return 0; 
    }
    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data(count_, data_s);
        write_data(reward_index_, data_s);
        write_data(show_reward_, data_s);
        return 0;
    }
};

class msg_luck_wheel_record : public msg_base<none_socket>
{
public:
    char    content_[1024];  //�ָ���[|] [\n]
    MSG_CONSTRUCT(msg_luck_wheel_record);

    int			write(boost::property_tree::ptree& data_s)
    {
        msg_base::write(data_s);
        write_jvalue(content_, data_s);
        return 0;
    }
    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data<char>(content_, sizeof(content_), data_s);
        return 0;
    }
};

class msg_subsidy_info : public msg_base<none_socket>
{
public:
    int     is_ready_;          //�Ƿ����ȡ 0������ȡ 1����ȡ
    int     obtain_count_;      //��������ȡ����
    int     total_count_;       //�����ܴ���
    char    rewards_[64];       //�������� �ָ���[|]
    MSG_CONSTRUCT(msg_subsidy_info);

    int			write(boost::property_tree::ptree& data_s)
    {
        msg_base::write(data_s);
        write_jvalue(is_ready_, data_s);
        write_jvalue(obtain_count_, data_s);
        write_jvalue(total_count_, data_s);
        write_jvalue(rewards_, data_s);
        return 0;
    }
    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data(is_ready_, data_s);
        write_data(obtain_count_, data_s);
        write_data(total_count_, data_s);
        write_data(rewards_, sizeof(rewards_), data_s);
        return 0;
    }
};

class msg_subsidy_result : public msg_base<none_socket>
{
public:
    int     reward_num_;        //��õ�K����������
    MSG_CONSTRUCT(msg_subsidy_result);

    int			write(boost::property_tree::ptree& data_s)
    {
        msg_base::write(data_s);
        write_jvalue(reward_num_, data_s);
        return 0;
    }
    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data(reward_num_, data_s);
        return 0;
    }
};


enum 
{
	GET_CLSID(msg_match_info) = msg_serverid_this_game_begin,
	GET_CLSID(msg_ask_for_join_match),
	GET_CLSID(msg_prepare_enter),
	GET_CLSID(msg_time_left),
	GET_CLSID(msg_match_result),
	GET_CLSID(msg_match_progress),
	GET_CLSID(msg_broadcast_info),
};

class msg_ask_for_join_match : public msg_base<none_socket>
{
public:
	int		match_id_;				//��������
    MSG_CONSTRUCT(msg_ask_for_join_match);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(match_id_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		return 0;
	}	
};

class msg_time_left : public msg_base<none_socket>
{
public:
	int		left_;
	int		state_;
    MSG_CONSTRUCT(msg_time_left);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(left_, data_s);
		write_jvalue(state_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(left_, data_s);
		write_data(state_, data_s);
		return 0;
	}	
};

class msg_match_progress : public msg_base<none_socket>
{
public:
	int		my_score_;
	int		player_count_;
	int		high_score_;
	int		my_rank_;
    MSG_CONSTRUCT(msg_match_progress);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(my_score_, data_s);
		write_jvalue(player_count_, data_s);
		write_jvalue(high_score_, data_s);
		write_jvalue(my_rank_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(my_score_, data_s);
		write_data(player_count_, data_s);
		write_data(high_score_, data_s);
		write_data(my_rank_, data_s);
		return 0;
	}	
};

class msg_match_result : public msg_base<none_socket>
{
public:
	int		rank_;		//�ڼ���
	int		money_;		//��ö���Ǯ
	int		telfee_;	//��û��ѣ�ֵ�Ǳ�������id,����id�㻰��ֵ
	char	key_[max_guid];	//�һ���
    MSG_CONSTRUCT(msg_match_result);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(rank_, data_s);
		write_jvalue(money_, data_s);
		write_jvalue(telfee_, data_s);
		write_jvalue(key_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(rank_, data_s);
		write_data(money_, data_s);
		write_data(telfee_, data_s);
		write_data<char>(key_, max_guid, data_s);
		return 0;
	}	
};

class msg_broadcast_info : public msg_base<none_socket>
{
public:
	char				who_[max_name]; //˭
	int					at_where_;		//������ 0,��ҳ���1���ѳ�
	int					at_data_;			//���峡
	long long			get_what_;		//�����ʲô
	long long			get_what2_;		//�������ʲô
    MSG_CONSTRUCT(msg_broadcast_info);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(who_, data_s);
		write_jvalue(at_where_, data_s);
		write_jvalue(at_data_, data_s);
		write_jvalue(get_what_, data_s);
		write_jvalue(get_what2_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(who_, max_name, data_s);
		write_data(at_where_, data_s);
		write_data(at_data_, data_s);
		write_data(get_what_, data_s);
		write_data(get_what2_, data_s);
		return 0;
	}
};

class msg_match_info : public msg_base<none_socket>
{
public:
	int		match_id_;				//��������
	int		self_registered_;	//�ѱ�������
	int		register_count_;	//�ѱ�������
	int		state_;						//״̬,0 ���ܱ�����1 �ȴ���ʼ��2 ������
	unsigned int match_time_left_;		//������ʼʣ��ʱ��
    MSG_CONSTRUCT(msg_match_info);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(match_id_, data_s);
		write_jvalue(self_registered_, data_s);
		write_jvalue(register_count_, data_s);
		write_jvalue(state_, data_s);
		write_jvalue(match_time_left_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data(self_registered_, data_s);
		write_data(register_count_, data_s);
		write_data(state_, data_s);
		write_data(match_time_left_, data_s);
		return 0;
	}
};

class msg_prepare_enter : public msg_base<none_socket>
{
public:
	int		match_id_;
	int		extra_data1_;
    MSG_CONSTRUCT(msg_prepare_enter);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(match_id_, data_s);
		write_jvalue(extra_data1_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(match_id_, data_s);
		write_data(extra_data1_, data_s);
		return 0;
	}	
};

//KOKOƽ̨��������Ϣ
class msg_match_score : public msg_base<none_socket>
{
public:
	char	uid_[max_guid];
	int		match_id_;
	int		score_;
	int		operator_; // 0 - set, 1 - add 2 - emlimated
	msg_match_score()
	{
		head_.cmd_ = GET_CLSID(msg_match_score);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(match_id_, data_s);
		write_jvalue(score_, data_s);
		write_jvalue(operator_, data_s);
		return 0;
	}

	int					write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(uid_, max_guid, data_s);
		write_data(match_id_, data_s);
		write_data(score_, data_s);
		write_data(operator_, data_s);
		return 0;
	}
};

class msg_register_to_world : public msg_base<none_socket>
{
public:
	int			gameid_;
	char		open_ip_[max_guid];
	int			open_port_;
	char		instance_[max_guid];
    MSG_CONSTRUCT(msg_register_to_world);

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(gameid_, data_s);
		write_jvalue(open_ip_, data_s);
		write_jvalue(open_port_, data_s);
		write_jvalue(instance_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(gameid_, data_s);
		write_data(open_ip_, max_guid, data_s);
		write_data(open_port_, data_s);
		write_data(instance_, max_guid, data_s);
		return 0;
	}
};

//�㲥
class msg_server_broadcast : public msg_base<none_socket>
{
public:
	char		content_[512];
	msg_server_broadcast()
	{
		head_.cmd_ = 20022;
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(content_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<char>(content_, 512, data_s);
		return 0;
	}
};

class		msg_private_room_sync : public msg_base<none_socket>
{
public:
	msg_private_room_sync()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_sync);
	}
	int				room_id_;
	int				player_count_;
	int				config_index_;
    int				seats_;

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(room_id_, data_s);
		write_jvalue(player_count_, data_s);
		write_jvalue(config_index_, data_s);
		write_jvalue(seats_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(room_id_, data_s);
		write_data(player_count_, data_s);
		write_data(config_index_, data_s);
		write_data(seats_, data_s);
		return 0;
	}
};

class		msg_private_room_psync :public msg_base<none_socket>
{
public:
	int		room_id_;
	char	uid_[max_guid];
	char	nick_name_[max_guid];

	msg_private_room_psync()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_psync);
    }

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(room_id_, data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(nick_name_, data_s);
		return 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(room_id_, data_s);
		write_data(uid_, max_guid,  data_s);
		write_data(nick_name_, max_guid,  data_s);
		return 0;
	}
};