#pragma once
#include "koko_socket.h"
#include "game_srv_socket.h"
#include "msg_from_client.h"
#include "i_transaction.h"

template<class remote_t>
class msg_user_login_t : public msg_from_client<remote_t>
{
public:
	std::string		acc_name_;
	std::string		pwd_hash_;
	std::string		machine_mark_;

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(acc_name_, data_s);
		read_jvalue(pwd_hash_, data_s);
		read_jvalue(machine_mark_, data_s);
		return 0;
	}
};

class msg_user_login_channel : public msg_from_client<koko_socket_ptr>
{
public:
	std::string		uid_;
	__int64			sn_;
	std::string		token_;
	std::string		device_;
	msg_user_login_channel()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_channel);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(sn_, data_s);
		read_jvalue(token_, data_s);
		read_jvalue(device_, data_s);
		return 0;
	}
	int			handle_this();
};

class msg_user_login_delegate : public msg_user_login_t<srv_socket_ptr>
{
public:
	int			sn_;
	msg_user_login_delegate()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_delegate);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_user_login_t::read(data_s);
		read_jvalue(sn_, data_s);
		return 0;
	}
	int			handle_this();
};

class msg_join_channel : public msg_authrized<koko_socket_ptr>
{
public:
	int				channel_;
	unsigned int	sn_;
	msg_join_channel()
	{
		head_.cmd_ = GET_CLSID(msg_join_channel);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(channel_, data_s);
		read_jvalue(sn_, data_s);
		return 0;
	}
	int			handle_this();
};

class msg_leave_channel : public msg_join_channel
{
public:
	msg_leave_channel()
	{
		head_.cmd_ = GET_CLSID(msg_leave_channel);
	}
	int			handle_this();
};

//
class	msg_chat : public msg_authrized<koko_socket_ptr>
{
public:
	int				channel_;		//Ƶ��id
	std::string		to_;			//�������,����������Ϊ��
	std::string		content_;		//����
	msg_chat()
	{
		head_.cmd_ = GET_CLSID(msg_chat);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(channel_, data_s);
		read_jvalue(to_, data_s);
		read_jvalue(content_, data_s);
		return 0;
	}
	int			handle_this();
};


//��֤�û�
class msg_verify_user : public msg_base<srv_socket_ptr>
{
public:
	std::string		sn_;
	std::string		uid;
	msg_verify_user()
	{
		head_.cmd_ = GET_CLSID(msg_verify_user);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		read_jvalue(sn_, data_s);
		read_jvalue(uid, data_s);
		return 0;
	}
	int			handle_this();
};

class msg_koko_trade_inout_ret;
class		msg_koko_trade_inout : public msg_base<srv_socket_ptr>
{
public:
	std::string		uid_;
	int				dir_;				//��λ2�ֽ� 0 ��ƽ̨������Ϸ���ֻ��ң� 1 ��ƽ̨������Ϸ���л���,  2 ����Ϸ����ƽ̨, ��λ2�ֽ�Ϊ����ID
	__int64			count_;				//������
	__int64			time_stamp_;		//ʱ���
	std::string		sn_;
	std::string		sign_;
	std::string		game_name_;
	int				game_id_;
	msg_koko_trade_inout()
	{
		head_.cmd_ = GET_CLSID(msg_koko_trade_inout);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(dir_, data_s);
		read_jvalue(count_, data_s);
		read_jvalue(time_stamp_, data_s);
		read_jvalue(sn_, data_s);
		read_jvalue(sign_, data_s);
		return 0;
	}

	int		handle_this();
	void	handle_this2();
	void	do_trade(msg_koko_trade_inout_ret* pmsg, std::map<std::string, __int64>& handled_trade);
};

class		msg_action_record : public msg_from_client<koko_socket_ptr>
{
public:
	enum
	{
		OP_TYPE_INSERT,
		OP_TYPE_UPDATE,
	};
	int				op_type_;		//0-�¼�¼ 1-����
	int				action_id_;
	int				action_data_;
	int				action_data2_;
	std::string		action_data3_;
	std::string		action_data4_;
	std::string		action_data5_;
	msg_action_record()
	{
		head_.cmd_ = GET_CLSID(msg_action_record);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(op_type_, data_s);
		read_jvalue(action_id_, data_s);
		read_jvalue(action_data_, data_s);
		read_jvalue(action_data2_, data_s);
		read_jvalue(action_data3_, data_s);
		read_jvalue(action_data4_, data_s);
		read_jvalue(action_data5_, data_s);
		return 0;
	}

	int		handle_this();
};

class		msg_get_token : public msg_authrized<koko_socket_ptr>
{
public:
	msg_get_token()
	{
		head_.cmd_ = GET_CLSID(msg_get_token);
	}
	int		handle_this();
};

class		msg_buy_item_notice : public msg_base<srv_socket_ptr>
{
public:
	std::string		uid_;
	int				item_id_;
	int				item_num_;
	msg_buy_item_notice()
	{
		head_.cmd_ = GET_CLSID(msg_buy_item_notice);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(item_id_, data_s);
		read_jvalue(item_num_, data_s);
		return 0;
	}

	int		handle_this();
};

class		msg_use_item_notice : public msg_base<srv_socket_ptr>
{
public:
	std::string		uid_;
	int				item_id_;
	int				item_num_;
	msg_use_item_notice()
	{
		head_.cmd_ = GET_CLSID(msg_use_item_notice);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(item_id_, data_s);
		read_jvalue(item_num_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_sync_rank_notice : public msg_base<srv_socket_ptr>
{
public:
	std::string		uid_;
	int				item_id_;
	int				item_num_;
	msg_sync_rank_notice()
	{
		head_.cmd_ = GET_CLSID(msg_sync_rank_notice);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(item_id_, data_s);
		read_jvalue(item_num_, data_s);
		return 0;
	}

	int		handle_this();
};

class		msg_request_client_item : public msg_base<srv_socket_ptr>
{
public:
	std::string		uid_;
	int				item_id_;
	msg_request_client_item()
	{
		head_.cmd_ = GET_CLSID(msg_request_client_item);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(item_id_, data_s);
		return 0;
	}

	int		handle_this();
};

/************************************************************************/
/* �Զ��巿�����
/************************************************************************/

class		msg_enter_private_room : public msg_authrized<koko_socket_ptr>
{
public:
	int				gameid_;
	int				roomid_;
	std::string		psw_;
	msg_enter_private_room()
	{
		head_.cmd_ = GET_CLSID(msg_enter_private_room);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(gameid_, data_s);
		read_jvalue(roomid_, data_s);
		read_jvalue(psw_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_private_room_sync : public msg_base<srv_socket_ptr>
{
public:
	int				room_id_;
	int				player_count_;
	int				config_index_;
	int				seats_;
	std::string		master_;
	std::string		master_nick_;
	int				total_turn_;	////
	std::string		params_;		////
	int				state_;			////
	msg_private_room_sync()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_sync);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(room_id_, data_s);
		read_jvalue(player_count_, data_s);
		read_jvalue(config_index_, data_s);
		read_jvalue(seats_, data_s);
		read_jvalue(master_, data_s);
		read_jvalue(master_nick_, data_s);
		read_jvalue(total_turn_, data_s);
		read_jvalue(params_, data_s);
		read_jvalue(state_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_private_room_psync :public msg_base<srv_socket_ptr>
{
public:
	int				room_id_;
	std::string		uid_;
	std::string		nick_name_;
	msg_private_room_psync()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_psync);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(room_id_, data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(nick_name_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_private_room_list : public msg_authrized<koko_socket_ptr>
{
public:
	int				game_id_;
	msg_get_private_room_list()
	{
		head_.cmd_ = GET_CLSID(msg_get_private_room_list);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(game_id_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_private_room_record : public msg_authrized<koko_socket_ptr>
{
public:
	int				game_id_;
	msg_get_private_room_record()
	{
		head_.cmd_ = GET_CLSID(msg_get_private_room_record);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(game_id_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_alloc_game_server : public msg_get_private_room_list
{
public:
	std::string		params;
	msg_alloc_game_server()
	{
		head_.cmd_ = GET_CLSID(msg_alloc_game_server);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_get_private_room_list::read(data_s);
		read_jvalue(params, data_s);
		return 0;
	}
	int		handle_this();
};

class msg_server_broadcast : public msg_base<srv_socket_ptr>
{
public:
	std::string		content_;
	msg_server_broadcast()
	{
		head_.cmd_ = GET_CLSID(msg_server_broadcast);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(content_, data_s);
		return 0;
	}
	int					handle_this();
};

//ע�᱾������
class msg_register_to_world : public msg_base<srv_socket_ptr>
{
public:
	int				gameid_;
	std::string		open_ip_;
	int				open_port_;
	std::string		instance_;
	int				roomid_start_;
	std::string		key_;
	msg_register_to_world()
	{
		head_.cmd_ = GET_CLSID(msg_register_to_world);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(gameid_, data_s);
		read_jvalue(open_ip_, data_s);
		read_jvalue(open_port_, data_s);
		read_jvalue(instance_, data_s);
		read_jvalue(roomid_start_, data_s);
		read_jvalue(key_, data_s);
		return 0;
	}
	int					handle_this();
};

class		msg_buy_item : public  msg_authrized<koko_socket_ptr>
{
public:
	int				item_id_;
	int				item_count_;
	std::string		comment_;	//��ע
	msg_buy_item()
	{
		head_.cmd_ = GET_CLSID(msg_buy_item);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(item_id_, data_s);
		read_jvalue(item_count_, data_s);
		read_jvalue(comment_, data_s);
		return 0;
	}
	int		handle_this();
};

class	msg_send_present : public msg_authrized<koko_socket_ptr>
{
public:
	int				channel_;
	int				present_id_;
	int				count_;
	std::string		to_;
	msg_send_present()
	{
		head_.cmd_ = GET_CLSID(msg_send_present);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(channel_, data_s);
		read_jvalue(present_id_, data_s);
		read_jvalue(count_, data_s);
		read_jvalue(to_, data_s);
		return 0;
	}
	int			handle_this();
	void		do_handle_this(player_ptr pp, trans_ptr ptrans, boost::shared_ptr<msg_send_present> pthis);
};

class msg_query_data : public msg_from_client<koko_socket_ptr>
{
public:
	int				data_id_;		//0��ʾ��ѯ�ǳ� 1��ʾKȯ
	std::string		params;
	msg_query_data()
	{
		head_.cmd_ = GET_CLSID(msg_query_data);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(data_id_, data_s);
		read_jvalue(params, data_s);
		return 0;
	}
	int			handle_this();
};

class msg_use_present : public msg_authrized<koko_socket_ptr>
{
public:
	int			present_id_;
	int			count_;
	msg_use_present()
	{
		head_.cmd_ = GET_CLSID(msg_use_present);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(count_, data_s);
		read_jvalue(present_id_, data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_get_present_record : public msg_authrized<koko_socket_ptr>
{
public:
	int			type_;  //1-��ȡ��¼ 2-�ͳ���¼
	msg_get_present_record()
	{
		head_.cmd_ = GET_CLSID(msg_get_present_record);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(type_, data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_get_buy_item_record : public msg_authrized<koko_socket_ptr>
{
public:
	int			game_id_;  //��Ϸid:0-������Ϸ
	msg_get_buy_item_record()
	{
		head_.cmd_ = GET_CLSID(msg_get_buy_item_record);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(game_id_, data_s);
		return 0;
	}
	int			handle_this();
};

//K�Ҷһ���K��
class	 msg_transfer_gold_to_game : public msg_authrized<koko_socket_ptr>
{
public:
	__int64		count_;
	msg_transfer_gold_to_game()
	{
		head_.cmd_ = GET_CLSID(msg_transfer_gold_to_game);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(count_, data_s);
		return 0;
	}
	int		handle_this();
};


class	msg_send_present_by_pwd : public msg_send_present
{
public:
	std::string		bank_pwd_;
	msg_send_present_by_pwd()
	{
		head_.cmd_ = GET_CLSID(msg_send_present_by_pwd);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_send_present::read(data_s);
		read_jvalue(bank_pwd_, data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_req_sync_item : public msg_authrized<koko_socket_ptr>
{
public:
	int			item_id_;  //-1-������Ʒ������0-��ƷID
	msg_req_sync_item()
	{
		head_.cmd_ = GET_CLSID(msg_req_sync_item);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(item_id_, data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_private_room_remove_sync : public msg_base<srv_socket_ptr>
{
public:
	int			room_id_;
	msg_private_room_remove_sync()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_remove_sync);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(room_id_, data_s);
		return 0;
	}
	int			handle_this();
};

class		msg_enter_private_room_notice : public msg_base<srv_socket_ptr>
{
public:
	int				room_id_;
	std::string		uid_;
	msg_enter_private_room_notice()
	{
		head_.cmd_ = GET_CLSID(msg_enter_private_room_notice);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(room_id_, data_s);
		read_jvalue(uid_, data_s);
		return 0;
	}
	int		handle_this();
};

class	msg_get_login_prize_list : public msg_authrized<koko_socket_ptr>
{
public:
	msg_get_login_prize_list()
	{
		head_.cmd_ = GET_CLSID(msg_get_login_prize_list);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_get_login_prize : public msg_authrized<koko_socket_ptr>
{
public:
	int			type_;	//0-ÿ��ǩ������ 1:3������ǩ������ 2:6������ǩ������ 3:9������ǩ������ 
	msg_get_login_prize()
	{
		head_.cmd_ = GET_CLSID(msg_get_login_prize);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(type_, data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_get_headframe_list : public msg_authrized<koko_socket_ptr>
{
public:
	msg_get_headframe_list()
	{
		head_.cmd_ = GET_CLSID(msg_get_headframe_list);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		return 0;
	}
	int			handle_this();
};

class	msg_set_head_and_headframe : public msg_authrized<koko_socket_ptr>
{
public:
	std::string		head_ico_;		//ͷ��ID
	int				headframe_id_;	//ͷ���ID
	msg_set_head_and_headframe()
	{
		head_.cmd_ = GET_CLSID(msg_set_head_and_headframe);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(head_ico_, data_s);
		read_jvalue(headframe_id_, data_s);
		return 0;
	}
	int			handle_this();
};

class		msg_buy_item_by_activity : public  msg_authrized<koko_socket_ptr>
{
public:
	int			activity_id_;
	int			num_;
	msg_buy_item_by_activity()
	{
		head_.cmd_ = GET_CLSID(msg_buy_item_by_activity);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(activity_id_, data_s);
		read_jvalue(num_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_activity_list : public  msg_authrized<koko_socket_ptr>
{
public:
	enum
	{
		STATE_ACTIVATE,
		STATE_INVALID,
	};
	msg_get_activity_list()
	{
		head_.cmd_ = GET_CLSID(msg_get_activity_list);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_recharge_record : public  msg_authrized<koko_socket_ptr>
{
public:
	enum
	{
		RECHARGE_DIAMOND,
		RECHARGE_GOLDS,
	};
	msg_get_recharge_record()
	{
		head_.cmd_ = GET_CLSID(msg_get_recharge_record);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_play_lucky_dial : public  msg_authrized<koko_socket_ptr>
{
public:
	int			item_id_;		//���ĵ���Ʒ	
	int			times_;			//������1 or 5
	msg_play_lucky_dial()
	{
		head_.cmd_ = GET_CLSID(msg_play_lucky_dial);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(item_id_, data_s);
		read_jvalue(times_, data_s);
		return 0;
	}
	int		handle_this();
	int		one_lucky_dial(player_ptr pp, std::map<int, lucky_dial_info>& dial_conf, int& lucky_id_, int& serial_times_, int& lucky_value_);
};

class		msg_get_serial_lucky_state : public  msg_authrized<koko_socket_ptr>
{
public:
	msg_get_serial_lucky_state()
	{
		head_.cmd_ = GET_CLSID(msg_get_serial_lucky_state);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_mail_detail : public  msg_authrized<koko_socket_ptr>
{
public:
	int			id_;
	msg_get_mail_detail()
	{
		head_.cmd_ = GET_CLSID(msg_get_mail_detail);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(id_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_del_mail : public  msg_authrized<koko_socket_ptr>
{
public:
	int			id_;
	msg_del_mail()
	{
		head_.cmd_ = GET_CLSID(msg_del_mail);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(id_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_del_all_mail : public  msg_authrized<koko_socket_ptr>
{
public:
	int			mail_type_;		//0-ϵͳ�ʼ� 1-�����ʼ� 2-�����ʼ�
	msg_del_all_mail()
	{
		head_.cmd_ = GET_CLSID(msg_del_all_mail);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(mail_type_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_mail_attach : public  msg_authrized<koko_socket_ptr>
{
public:
	int			id_;
	msg_get_mail_attach()
	{
		head_.cmd_ = GET_CLSID(msg_get_mail_attach);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(id_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_client_get_data : public  msg_authrized<koko_socket_ptr>
{
public:
	std::string		key_;
	msg_client_get_data()
	{
		head_.cmd_ = GET_CLSID(msg_client_get_data);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(key_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_client_set_data : public  msg_authrized<koko_socket_ptr>
{
public:
	std::string		key_;
	std::string		value_;
	msg_client_set_data()
	{
		head_.cmd_ = GET_CLSID(msg_client_set_data);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(key_, data_s);
		read_jvalue(value_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_common_get_present : public  msg_authrized<koko_socket_ptr>
{
public:
	int			present_id_;
	msg_common_get_present()
	{
		head_.cmd_ = GET_CLSID(msg_common_get_present);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(present_id_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_send_mail_to_player : public  msg_base<srv_socket_ptr>
{
public:
	std::string		uid_;
	int				mail_type_;		//�ʼ����ͣ�0-ϵͳ�ʼ� 1-�����ʼ� 2-�����ʼ�
	int				title_type_;	//�������ͣ�0~5:ϵͳ�ʼ���ϵͳ�����¡���¼�����������������6:�����ʼ������7:�����ʼ������ѣ�
	std::string		title_;			//����(GB2312����)
	std::string		content_;		//����(GB2312����)
	int				attach_state_;	//����״̬:0-û�и��� 1-����δ��ȡ 2-��������ȡ
	std::string		attach_;		//����
	int				save_days_;		//��������
	msg_send_mail_to_player()
	{
		head_.cmd_ = GET_CLSID(msg_send_mail_to_player);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(mail_type_, data_s);
		read_jvalue(title_type_, data_s);
		read_jvalue(title_, data_s);
		read_jvalue(content_, data_s);
		read_jvalue(attach_state_, data_s);
		read_jvalue(attach_, data_s);
		read_jvalue(save_days_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_get_rank_data : public  msg_authrized<koko_socket_ptr>
{
public:
	int			type_;		//1-���Ѱ�
	int			flag_;		//0-���հ� 1-�ܰ�
	msg_get_rank_data()
	{
		head_.cmd_ = GET_CLSID(msg_get_rank_data);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(type_, data_s);
		read_jvalue(flag_, data_s);
		return 0;
	}
	int		handle_this();
};

class	msg_get_head_list : public msg_authrized<koko_socket_ptr>
{
public:
	msg_get_head_list()
	{
		head_.cmd_ = GET_CLSID(msg_get_head_list);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		return 0;
	}
	int			handle_this();
};