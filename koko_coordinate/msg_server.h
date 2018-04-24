#pragma once
#include "msg.h"

class		msg_player_info : public msg_base<none_socket>
{
public:
	std::string		uid_;
	std::string		nickname_;
	__int64			iid_;
	__int64			gold_;
	int				vip_level_;
	int				channel_;	
	int				gender_;
	int				level_;
	int				isagent_;
	int				isinit_;
	msg_player_info()
	{
		head_.cmd_ = GET_CLSID(msg_player_info);
		channel_ = 0;
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(nickname_, data_s);
		write_jvalue(iid_, data_s);
		write_jvalue(gold_, data_s);
		write_jvalue(vip_level_, data_s);
		write_jvalue(channel_, data_s);
		write_jvalue(gender_, data_s);
		write_jvalue(level_, data_s);
		write_jvalue(isagent_, data_s);
		write_jvalue(isinit_, data_s);
		return 0;
	}
};

class		msg_user_login_ret : public msg_player_info
{
public:
	std::string		token_;
	__int64			sequence_;
	std::string		email_;		//email
	std::string		phone;				//�ֻ���
	std::string		address_;			//��ַ
	__int64			game_gold_;				//��Ϸ��
	__int64			game_gold_free_;
	int				email_verified_;		//�����ַ����֤ 0δ��֤��1����֤
	int				phone_verified_;		//�ֻ���������֤ 0δ��֤��1����֤
	int				byear_, bmonth_, bday_;	//����������
	int				region1_,region2_,region3_;
	int				age_;
	std::string		idcard_;			//���֤��
	msg_user_login_ret()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_player_info::write(data_s);
		write_jvalue(token_, data_s);
		write_jvalue(sequence_, data_s);
		write_jvalue(email_, data_s);
		write_jvalue(phone, data_s);
		write_jvalue(address_, data_s);
		write_jvalue(game_gold_, data_s);
		write_jvalue(game_gold_free_, data_s);
		write_jvalue(email_verified_, data_s);
		write_jvalue(phone_verified_, data_s);
		write_jvalue(byear_, data_s);
		write_jvalue(bmonth_, data_s);
		write_jvalue(bday_, data_s);
		write_jvalue(region1_, data_s);
		write_jvalue(region2_, data_s);
		write_jvalue(region3_, data_s);
		write_jvalue(age_, data_s);
		write_jvalue(idcard_, data_s);
		return 0;
	}
};

class msg_user_login_ret_delegate : public msg_user_login_ret
{
public:
	int			sn_;
	msg_user_login_ret_delegate()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_ret_delegate);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_user_login_ret::write(data_s);
		write_jvalue(sn_, data_s);
		return 0;
	}
};


class		msg_player_leave : public msg_base<none_socket>
{
public:
	int				channel_;
	std::string		uid_;
	msg_player_leave()
	{
		head_.cmd_ = GET_CLSID(msg_player_leave);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(channel_, data_s);
		write_jvalue(uid_, data_s);
		return 0;
	}
};

class		msg_same_account_login : public msg_base<none_socket>
{
public:
	msg_same_account_login()
	{
		head_.cmd_ = GET_CLSID(msg_same_account_login);
	}
};

class		msg_broadcast_base : public msg_base<none_socket>
{
public:
	int			msg_type_;		//0 ���죬1 ���2 ϵͳ��Ϣ��4 �㲥 5 ����
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(msg_type_, data_s);
		return 0;
	}
};

class		msg_chat_deliver : public msg_broadcast_base
{
public:
	int				channel_;
	int				from_tag_;	//���ͷ���ǩ 0�������,1�ٷ��ͷ�
	std::string		from_uid_;
	std::string		nickname_;
	int				to_tag_;		//���շ���ǩ
	std::string		to_uid_;
	std::string		to_nickname_;
	std::string		content_;
	__int64			iid_;	//���ID
	__int64			time_stamp_;		//ʱ���	
	msg_chat_deliver()
	{
		head_.cmd_ = GET_CLSID(msg_chat_deliver);
		to_tag_ = 0;
		time_stamp_ = 0;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_broadcast_base::write(data_s);
		write_jvalue(channel_, data_s);
		write_jvalue(from_tag_, data_s);
		write_jvalue(from_uid_, data_s);
		write_jvalue(nickname_, data_s);
		write_jvalue(to_tag_, data_s);
		write_jvalue(to_uid_, data_s);
		write_jvalue(to_nickname_, data_s);
		write_jvalue(content_, data_s);
		write_jvalue(iid_, data_s);
		write_jvalue(time_stamp_, data_s);
		return 0;
	}
};

class msg_koko_trade_inout_ret : public msg_base<none_socket>
{
public:
	std::string		sn_;
	int				phase_;
	int				result_;				//0 ����ɹ�, 1 �Ѵ����				
	__int64			count_;

	msg_koko_trade_inout_ret()
	{
		head_.cmd_ = GET_CLSID(msg_koko_trade_inout_ret);
		count_ = 0;
		phase_ = -1;
		result_ = -1;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(sn_, data_s);
		write_jvalue(phase_, data_s);
		write_jvalue(result_, data_s);
		write_jvalue(count_, data_s);
		return 0;
	}
};

class		msg_sync_item : public msg_base<none_socket>
{
public:
	int			item_id_;
	__int64		count_;
	msg_sync_item()
	{
		head_.cmd_ = GET_CLSID(msg_sync_item);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(item_id_, data_s);
		write_jvalue(count_, data_s);
		return 0;
	}
};

class		msg_room_create_ret : public msg_base<none_socket>
{
public:
	private_room_inf inf;
	msg_room_create_ret()
	{
		head_.cmd_ = GET_CLSID(msg_room_create_ret);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		json_msg_helper::write_value("game_id_", inf.game_id_, data_s);
		json_msg_helper::write_value("room_id_", inf.room_id_, data_s);
		json_msg_helper::write_value("room_name_", inf.room_name_, data_s);
		json_msg_helper::write_value("player_count_", inf.player_count_, data_s);
		json_msg_helper::write_value("seats_", inf.seats_, data_s);
		json_msg_helper::write_value("config_index_", inf.config_index_, data_s);
		json_msg_helper::write_value("ip_", inf.ip_, data_s);
		json_msg_helper::write_value("port_", inf.port_, data_s);
		json_msg_helper::write_value("master_", inf.master_, data_s);
		json_msg_helper::write_value("need_psw_", inf.need_psw_, data_s);
		json_msg_helper::write_value("master_nick_", inf.master_nick_, data_s);
		return 0;
	}
};

class		msg_private_room_remove : public msg_base<none_socket>
{
public:
	int			gameid_;
	int			roomid_;
	msg_private_room_remove()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_remove);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(gameid_, data_s);
		write_jvalue(roomid_, data_s);
		return 0;
	}
};

class		msg_private_room_players : public msg_base<none_socket>
{
public:
	int				roomid_;
	std::string		players_[8];
	int				data1_[8];
	int				data2_[8];
	msg_private_room_players()
	{
		head_.cmd_ = GET_CLSID(msg_private_room_players);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(roomid_, data_s);
		std::vector<std::string> players;
		players.insert(players.end(), players_, players_ + 8);
		write_jvec(players, data_s);
		write_jarr(data1_, 8, data_s);
		write_jarr(data2_, 8, data_s);
		return 0;
	}
};

class	msg_switch_game_server : public msg_base<none_socket>
{
public:
	std::string		ip_;
	int				port_;
	msg_switch_game_server()
	{
		head_.cmd_ = GET_CLSID(msg_switch_game_server);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(ip_, data_s);
		write_jvalue(port_, data_s);
		return 0;
	}
};

class msg_query_data_ret : public msg_base<none_socket>
{
public:
	int				data_id_;
	std::string		params_;
	std::string		data_;
	msg_query_data_ret()
	{
		head_.cmd_ = GET_CLSID(msg_query_data_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(data_id_, data_s);
		write_jvalue(params_, data_s);
		write_jvalue(data_, data_s);
		return 0;
	}
};

class msg_use_present_ret : public msg_base<none_socket>
{
public:
	int				present_id_;
	int				count_;
	std::string		prize_item_;
	msg_use_present_ret()
	{
		head_.cmd_ = GET_CLSID(msg_use_present_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(present_id_, data_s);
		write_jvalue(count_, data_s);
		write_jvalue(prize_item_, data_s);
		return 0;
	}
};

//ƽ̨������ݷ����仯.��Ϸģ����Ҫ������������ˢ��
class		msg_koko_user_data_change : public msg_base<none_socket>
{
public:
	std::string		uid_;
	int				itemid_;
	msg_koko_user_data_change()
	{
		head_.cmd_ = GET_CLSID(msg_koko_user_data_change);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(itemid_, data_s);
		return 0;
	}
};

class	msg_get_present_record_ret : public msg_base<none_socket>
{
public:
	int				type_;			//1-�ͳ���¼ 2-��ȡ��¼
	int				tag_;			//0�µĿ�ʼ��1��������
	std::string		nickname_;
	__int64			iid_;
	std::string		present_name_;	//��������
	int				count_;			//����
	__int64			time_stamp_;	//ʱ���

	msg_get_present_record_ret()
	{
		head_.cmd_ = GET_CLSID(msg_get_present_record_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(type_, data_s);
		write_jvalue(tag_, data_s);
		write_jvalue(nickname_, data_s);
		write_jvalue(iid_, data_s);
		write_jvalue(present_name_, data_s);
		write_jvalue(count_, data_s);
		write_jvalue(time_stamp_, data_s);
		return 0;
	}
};

class	msg_get_buy_item_record_ret : public msg_base<none_socket>
{
public:
	int				tag_;			//0�µĿ�ʼ��1��������
	int				item_id_;		//��Ʒid
	int				item_num_;		//����
	int				status_;		//״̬(0-�ɹ���1-������2-ʧ��)
	__int64			time_stamp_;	//ʱ���
	std::string		comment_;		//��ע

	msg_get_buy_item_record_ret()
	{
		head_.cmd_ = GET_CLSID(msg_get_buy_item_record_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(tag_, data_s);
		write_jvalue(item_id_, data_s);
		write_jvalue(item_num_, data_s);
		write_jvalue(status_, data_s);
		write_jvalue(time_stamp_, data_s);
		write_jvalue(comment_, data_s);
		return 0;
	}
};

class		msg_sync_item_data : public msg_base<none_socket>
{
public:
	int				item_id_;
	int				key_;		//0-ʣ��ʱ��
	std::string		value_;
	msg_sync_item_data()
	{
		head_.cmd_ = GET_CLSID(msg_sync_item_data);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(item_id_, data_s);
		write_jvalue(key_, data_s);
		write_jvalue(value_, data_s);
		return 0;
	}
};

//��֤�û�
class msg_verify_user_ret : public msg_base<none_socket>
{
public:
	std::string		sn_;
	int				result_;
	msg_verify_user_ret()
	{
		head_.cmd_ = GET_CLSID(msg_verify_user_ret);
		result_ = -1;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(sn_, data_s);
		write_jvalue(result_, data_s);
		return 0;
	}
};

class		msg_common_broadcast : public msg_base<none_socket>
{
public:
	enum
	{
		broadcast_type_send_present = 1,
	};

	int				type_;		//1-������Ʒ
	std::string		text_;		//��Ϣ����(�ö��ŷָ����ݣ����ݸ�ʽ��type_ֵ��ͬ����ͬ)
								//��ʽ��1.����ǳ�|�û�ID|����ʱ��|��Ʒ����|��ƷID
	msg_common_broadcast()
	{
		head_.cmd_ = GET_CLSID(msg_common_broadcast);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(type_, data_s);
		write_jvalue(text_, data_s);
		return 0;
	}
};

class		msg_get_private_room_record_ret : public msg_base<none_socket>
{
public:
	int				room_id_;
	int				player_count_;
	int				seats_;
	std::string		master_;
	int				total_turn_;
	std::string		params_;
	int				state_;		//0-��������״̬ 1-�ȴ����� 2-�ȴ�����

	msg_get_private_room_record_ret()
	{
		head_.cmd_ = GET_CLSID(msg_get_private_room_record_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(room_id_, data_s);
		write_jvalue(player_count_, data_s);
		write_jvalue(seats_, data_s);
		write_jvalue(master_, data_s);
		write_jvalue(total_turn_, data_s);
		write_jvalue(params_, data_s);
		write_jvalue(state_, data_s);
		return 0;
	}
};

class  msg_enter_private_room_ret : public msg_switch_game_server
{
public:
	int			gameid_;
	int			roomid_;
	int			succ_;			//0�����ڷ���,1�����п�,2��������,3���벻��ȷ
	msg_enter_private_room_ret()
	{
		head_.cmd_ = GET_CLSID(msg_enter_private_room_ret);
		succ_ = -1;
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_switch_game_server::write(data_s);
		write_jvalue(gameid_, data_s);
		write_jvalue(roomid_, data_s);
		write_jvalue(succ_, data_s);
		return 0;
	}
};

class  msg_user_login_prize_list : public msg_base<none_socket>
{
public:
	int			serial_days_;		//������¼����
	int			serial_state_;		//�����λ������λ-3�죬ʮλ-6�죬��λ-9��  0-δ��ȡ 1-����ȡ��
	int			login_day_;			//��ǰ�ڼ���(1~7)
	int			login_state_;		//��¼����״̬��0-δ��ȡ 1-����ȡ
	msg_user_login_prize_list()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_prize_list);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(serial_days_, data_s);
		write_jvalue(serial_state_, data_s);
		write_jvalue(login_day_, data_s);
		write_jvalue(login_state_, data_s);
		return 0;
	}
};

class  msg_user_login_prize : public msg_base<none_socket>
{
public:
	int			prize_type_;		//0-ÿ��ǩ������ 1:3������ǩ������ 2:6������ǩ������ 3:9������ǩ������ 
	msg_user_login_prize()
	{
		head_.cmd_ = GET_CLSID(msg_user_login_prize);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(prize_type_, data_s);
		return 0;
	}
};

class  msg_user_head_and_headframe : public msg_base<none_socket>
{
public:
	std::string		head_ico_;		//ͷ��ID
	int				headframe_id_;	//ͷ���ID
	msg_user_head_and_headframe()
	{
		head_.cmd_ = GET_CLSID(msg_user_head_and_headframe);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(head_ico_, data_s);
		write_jvalue(headframe_id_, data_s);
		return 0;
	}
};

class  msg_user_headframe_list : public msg_base<none_socket>
{
public:
	int			headframe_id_;		//ͷ���ID
	msg_user_headframe_list()
	{
		head_.cmd_ = GET_CLSID(msg_user_headframe_list);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(headframe_id_, data_s);
		return 0;
	}
};

class  msg_user_activity_list : public msg_base<none_socket>
{
public:
	int			activity_id_;
	int			state_;			//0-���� 1-����
	msg_user_activity_list()
	{
		head_.cmd_ = GET_CLSID(msg_user_activity_list);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(activity_id_, data_s);
		write_jvalue(state_, data_s);
		return 0;
	}
};

class	msg_user_recharge_record : public msg_base<none_socket>
{
public:
	int				tag_;				//0�µĿ�ʼ��1��������
	int				recharge_type_;		//0-��ʯ 2-K��
	int				recharge_num_;		//����
	int				status_;			//״̬(0:ʧ�� 1:����ɹ� 2:������,3����ɹ����ӱ�ʧ��,4����ɹ��ӱҳɹ�)
	__int64			time_stamp_;		//ʱ���
	msg_user_recharge_record()
	{
		head_.cmd_ = GET_CLSID(msg_user_recharge_record);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(recharge_type_, data_s);
		write_jvalue(recharge_num_, data_s);
		write_jvalue(status_, data_s);
		write_jvalue(time_stamp_, data_s);
		return 0;
	}
};

class	msg_lucky_dial_prize : public msg_base<none_socket>
{
public:
	std::string		lucky_id_;		//����ID(���ж��ID������|�ָ�)
	msg_lucky_dial_prize()
	{
		head_.cmd_ = GET_CLSID(msg_lucky_dial_prize);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(lucky_id_, data_s);
		return 0;
	}
};

class	msg_user_serial_lucky_state : public msg_base<none_socket>
{
public:
	int				serial_times_;			//��ת����
	int				luck_value_;			//��ǰ����ֵ
	__int64			reset_timestamp_;		//����ʱ���
	msg_user_serial_lucky_state()
	{
		head_.cmd_ = GET_CLSID(msg_user_serial_lucky_state);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(serial_times_, data_s);
		write_jvalue(luck_value_, data_s);
		write_jvalue(reset_timestamp_, data_s);
		return 0;
	}
};

class	msg_user_mail_list : public msg_base<none_socket>
{
public:
	int				id_;				//�ʼ�ID
	int				mail_type_;			//0-ϵͳ�ʼ� 1-�����ʼ� 2-�����ʼ�
	int				title_type_;		//�������ͣ�ϵͳ�ʼ���0~5��ϵͳ�����¡���¼����������������������ʼ���6����������ʼ���7�����ѣ�
	std::string		title_;				//����
	int				state_;				//�ʼ�״̬��0-δ�� 1-�Ѷ�
	int				attach_state_;		//����״̬��0-û�и��� 1-�и�����δ��ȡ 2-�и���������ȡ 
	__int64			timestamp_;			//�ʼ�ʱ���
	msg_user_mail_list()
	{
		head_.cmd_ = GET_CLSID(msg_user_mail_list);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(id_, data_s);
		write_jvalue(mail_type_, data_s);
		write_jvalue(title_type_, data_s);
		write_jvalue(state_, data_s);
		write_jvalue(title_, data_s);
		write_jvalue(attach_state_, data_s);
		write_jvalue(timestamp_, data_s);
		return 0;
	}
};

class	msg_user_mail_detail : public msg_base<none_socket>
{
public:
	int				id_;				//�ʼ�ID
	std::string		content_;			//����
	std::string		hyperlink_;			//��������
	std::string		attach_;			//����
	msg_user_mail_detail()
	{
		head_.cmd_ = GET_CLSID(msg_user_mail_detail);
	}

	virtual stream_buffer	alloc_io_buffer()
	{
		int len = std::max((int)content_.length() + 1, 1024);
		boost::shared_array<char> buff(new char[len]);
		return stream_buffer(buff, 0, len);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(id_, data_s);
		write_jvalue(content_, data_s);
		write_jvalue(hyperlink_, data_s);
		write_jvalue(attach_, data_s);
		return 0;
	}
};

class	msg_user_mail_op : public msg_base<none_socket>
{
public:
	enum 
	{
		MAIL_OP_SET_READ,
		MAIL_OP_DEL,
	};
	int		op_type_;		//0-�����Ѷ� 1-ɾ��һ���ʼ�
	int		id_;			//�ʼ�ID
	msg_user_mail_op()
	{
		head_.cmd_ = GET_CLSID(msg_user_mail_op);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(op_type_, data_s);
		write_jvalue(id_, data_s);
		return 0;
	}
};

class	msg_client_data : public msg_base<none_socket>
{
public:
	std::string		key_;
	std::string		value_;
	msg_client_data()
	{
		head_.cmd_ = GET_CLSID(msg_client_data);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(key_, data_s);
		write_jvalue(value_, data_s);
		return 0;
	}
};

class	msg_common_recv_present : public msg_base<none_socket>
{
public:
	std::string		type_;
	std::string		item_;
	msg_common_recv_present()
	{
		head_.cmd_ = GET_CLSID(msg_common_recv_present);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(type_, data_s);
		write_jvalue(item_, data_s);
		return 0;
	}
};

class	msg_common_present_state : public msg_base<none_socket>
{
public:
	std::string		present_state_;		//ÿһλ��0-û�и����� 1-���� 2-����
	msg_common_present_state()
	{
		head_.cmd_ = GET_CLSID(msg_common_present_state);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(present_state_, data_s);
		return 0;
	}
};

class	msg_common_present_state_change : public msg_base<none_socket>
{
public:
	int				present_id_;
	int				state_;			//0-û�и����� 1-���� 2-����
	msg_common_present_state_change()
	{
		head_.cmd_ = GET_CLSID(msg_common_present_state_change);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(present_id_, data_s);
		write_jvalue(state_, data_s);
		return 0;
	}
};

class	msg_user_rank_data : public msg_base<none_socket>
{
public:
	int				rank_;
	std::string		uid_;
	std::string		nickname_;
	std::string		head_ico_;
	int				type_;
	int				flag_;
	int				data_;
	msg_user_rank_data()
	{
		head_.cmd_ = GET_CLSID(msg_user_rank_data);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(rank_, data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(nickname_, data_s);
		write_jvalue(head_ico_, data_s);
		write_jvalue(type_, data_s);
		write_jvalue(flag_, data_s);
		write_jvalue(data_, data_s);
		return 0;
	}
};

class  msg_user_head_list : public msg_base<none_socket>
{
public:
	std::string		head_ico_;		//ͷ��ID
	int				remain_sec_;	//ʣ������
	msg_user_head_list()
	{
		head_.cmd_ = GET_CLSID(msg_user_head_list);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(head_ico_, data_s);
		write_jvalue(remain_sec_, data_s);
		return 0;
	}
};

class  msg_user_day_buy_data : public msg_base<none_socket>
{
public:
	int			pid_;			//��ƷID
	int			num_;			//�ѹ�������
	msg_user_day_buy_data()
	{
		head_.cmd_ = GET_CLSID(msg_user_day_buy_data);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(pid_, data_s);
		write_jvalue(num_, data_s);
		return 0;
	}
};

class  msg_request_client_item_ret : public msg_base<none_socket>
{
public:
	std::string		uid_;
	int				item_id_;
	std::string		item_data_;
	msg_request_client_item_ret()
	{
		head_.cmd_ = GET_CLSID(msg_request_client_item_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(uid_, data_s);
		write_jvalue(item_id_, data_s);
		write_jvalue(item_data_, data_s);
		return 0;
	}
};
