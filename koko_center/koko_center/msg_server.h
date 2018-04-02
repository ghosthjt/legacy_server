#pragma once
#include "msg.h"
#include "error.h"
#include "base64Con.h"

class	msg_game_data : public msg_base<none_socket>
{
public:
	game_info		inf;
	msg_game_data()
	{
		head_.cmd_ = GET_CLSID(msg_game_data);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		json_msg_helper::write_value("id_",inf.id_, data_s);
		json_msg_helper::write_value("type_",inf.type_, data_s);
		json_msg_helper::write_value("dir_",inf.dir_, data_s);
		json_msg_helper::write_value("exe_",inf.exe_, data_s);
		json_msg_helper::write_value("update_url_",inf.update_url_, data_s);
		json_msg_helper::write_value("help_url_",inf.help_url_, data_s);
		json_msg_helper::write_value("game_name_",inf.game_name_, data_s);
		json_msg_helper::write_value("thumbnail_",inf.thumbnail_, data_s);
		json_msg_helper::write_value("solution_",inf.solution_, data_s);
		json_msg_helper::write_value("no_embed_",inf.no_embed_, data_s);
		json_msg_helper::write_value("catalog_",inf.catalog_, data_s);
		json_msg_helper::write_value("order_",inf.order_, data_s);
		return 0;
	}
};

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
		isinit_ = 0;
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
	std::string		phone;					//手机号
	std::string		address_;			//地址
	__int64			game_gold_;					//游戏币
	__int64			game_gold_free_;
	int				email_verified_;		//邮箱地址已验证 0未验证，1已验证
	int				phone_verified_;		//手机号验已验证 0未验证，1已验证
	int				byear_, bmonth_, bday_;	//出生年月日
	int				region1_,region2_,region3_;
	int				age_;
	std::string		idcard_;				//身份证号
	std::string		app_key_;
	std::string		party_name_;
	
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
		write_jvalue(app_key_, data_s);
		write_jvalue(party_name_, data_s);
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


//更改用户数据
class msg_account_info_update : public msg_base<none_socket>
{
public:
	int				gender_;
	int				byear_;
	int				bmonth_;
	int				bday_;
	std::string		address_;
	std::string		nick_name_;
	int				age_;
	std::string		mobile_;
	std::string		email_;
	std::string		idcard_;
	int				region1_, region2_,region3_;
	msg_account_info_update()
	{
		head_.cmd_ = GET_CLSID(msg_account_info_update);
		address_[0] = 0;
		mobile_[0] = 0;
		email_[0] = 0;
		idcard_[0] = 0;
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(gender_, data_s);
		write_jvalue(byear_, data_s);
		write_jvalue(bmonth_, data_s);
		write_jvalue(bday_, data_s);
		write_jvalue(address_, data_s);
		write_jvalue(nick_name_, data_s);
		write_jvalue(age_, data_s);
		write_jvalue(mobile_,data_s);
		write_jvalue(email_, data_s);
		write_jvalue(idcard_, data_s);
		write_jvalue(region1_, data_s);
		write_jvalue(region2_, data_s);
		write_jvalue(region3_, data_s);
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

class		msg_broadcast_base : public msg_base<none_socket>
{
public:
	int			msg_type_;		//0 聊天，1 密语，2 系统消息，4 广播 5 礼物
	
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
	int				channel_;		//-1,全屏道
	int				from_tag_;	//发送方标签 0正常玩家,1官方客服
	std::string		from_uid_;
	std::string		nickname_;
	int				to_tag_;		//接收方标签
	std::string		to_uid_;
	std::string		to_nickname_;
	std::string		content_;
	msg_chat_deliver()
	{
		head_.cmd_ = GET_CLSID(msg_chat_deliver);
		to_tag_ = 0;
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

class		msg_black_account : public msg_base<none_socket>
{
public:
	msg_black_account()
	{
		head_.cmd_ = GET_CLSID(msg_black_account);
	}
};

class msg_channel_server : public msg_base<none_socket>
{
public:
	std::string		ip_;
	int				port_;
	int				for_game_;
	msg_channel_server()
	{
		head_.cmd_ = GET_CLSID(msg_channel_server);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(ip_, data_s);
		write_jvalue(port_, data_s);
		write_jvalue(for_game_, data_s);
		return 0;
	}
};

//主播图像
class msg_image_data : public msg_base<none_socket>
{
public:
	int				this_image_for_;		//> 0 avroomid, -1 个人头像, -2 验证码图片
	int				TAG_;	//1开始，2，结束，0，中间
	int				len_;
	std::string		data_;
	msg_image_data()
	{
		head_.cmd_ = GET_CLSID(msg_image_data);
	}

	virtual stream_buffer	alloc_io_buffer()
	{
		boost::shared_array<char> buff(new char[4096]);
		return stream_buffer(buff, 0, 4096);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(this_image_for_, data_s);
		write_jvalue(TAG_, data_s);
		std::string dat = base64_encode((unsigned char const*)data_.c_str(), len_);
		write_jvalue(dat, data_s);
		return 0;
	}
};

//继承自msg_host_screenshoot
//此时avroom_id字段的含义:
//0-用户头像，其它以后再扩展
class msg_user_image : public msg_image_data
{
public:
	std::string		uid_;
	msg_user_image()
	{
		head_.cmd_ = GET_CLSID(msg_user_image);
		this_image_for_ = -1;
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_image_data::write(data_s);
		write_jvalue(uid_, data_s);
		return 0;
	}
};

class msg_verify_code : public msg_base<none_socket>
{
public:
	std::string		question_;
	std::string		anwsers_;
	msg_verify_code()
	{
		head_.cmd_ = GET_CLSID(msg_verify_code);
		anwsers_[0] = 0;
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(question_, data_s);
		write_jvalue(anwsers_, data_s);
		return 0;
	}
};

class msg_check_data_ret : public msg_base<none_socket>
{
public:
	int			query_type_;
	int			result_;
	msg_check_data_ret()
	{
		head_.cmd_ = GET_CLSID(msg_check_data_ret);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(query_type_, data_s);
		write_jvalue(result_, data_s);
		return 0;
	}
};

class msg_koko_trade_inout_ret : public msg_base<none_socket>
{
public:
	std::string		sn_;
	int				result_;				//0 处理成功, 1 已处理过				
	__int64			count_;

	msg_koko_trade_inout_ret()
	{
		head_.cmd_ = GET_CLSID(msg_koko_trade_inout_ret);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(sn_, data_s);
		write_jvalue(result_, data_s);
		write_jvalue(count_, data_s);
		return 0;
	}
};

class		msg_sync_item : public msg_base<none_socket>
{
public:
	int				item_id_;
	__int64			count_;
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

//服务器进度
class		msg_srv_progress : public msg_base<none_socket>
{
public:
	int		pro_type_;		//类型 0 登录进度
	int		step_;				//进度
	int		step_max_;		//进度最大值
	msg_srv_progress()
	{
		head_.cmd_ = GET_CLSID(msg_srv_progress);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(pro_type_, data_s);
		write_jvalue(step_, data_s);
		write_jvalue(step_max_, data_s);
		return 0;
	}
};

class msg_sync_token : public msg_base<none_socket>
{
public:
	msg_sync_token()
	{
		head_.cmd_ = GET_CLSID(msg_sync_token);
	}

	__int64			sequence_;
	std::string		token_;

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(sequence_, data_s);
		write_jvalue(token_, data_s);
		return 0;
	}
};

//通知客户端比赛开始了
class msg_confirm_join_game_deliver : public msg_base<none_socket>
{
public:
	int				match_id_;
	std::string		ins_id_;
	int				register_count_;
	std::string		ip_;
	int				port_;
	msg_confirm_join_game_deliver()
	{
		head_.cmd_ = GET_CLSID(msg_confirm_join_game_deliver);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(match_id_, data_s);
		write_jvalue(ins_id_, data_s);
		write_jvalue(register_count_, data_s);
		write_jvalue(ip_, data_s);
		write_jvalue(port_, data_s);
		return 0;
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

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		json_msg_helper::write_value("id_", inf.id_, data_s);
		json_msg_helper::write_value("match_type_", inf.match_type_, data_s);
		json_msg_helper::write_value("game_id_", inf.game_id_, data_s);
		json_msg_helper::write_value("match_name_", inf.match_name_, data_s);
		json_msg_helper::write_value("thumbnail_", inf.thumbnail_, data_s);
		json_msg_helper::write_value("help_url_", inf.help_url_, data_s);
		json_msg_helper::write_value("prize_desc_", inf.prize_desc_, data_s);
		json_msg_helper::write_value("start_type_", inf.start_type_, data_s);
		json_msg_helper::write_value("require_count_", inf.require_count_, data_s);
		json_msg_helper::write_value("start_stripe_", inf.start_stripe_, data_s);
		json_msg_helper::write_value("end_type_", inf.end_type_, data_s);
		json_msg_helper::write_value("end_data_", inf.end_data_, data_s);
		json_msg_helper::write_value("srvip_", inf.srvip_, data_s);
		json_msg_helper::write_value("srvport_", inf.srvport_, data_s);
		json_msg_helper::write_value("costsize", inf.cost_.size(), data_s);

		std::vector<int> i1, i2;
		auto it = inf.cost_.begin();
		while (it != inf.cost_.end())
		{
			i1.push_back(it->first);
			i2.push_back(it->second);
			it++;
		}
		json_msg_helper::write_arr_value("costids", i1, data_s);
		json_msg_helper::write_arr_value("costcounts", i2, data_s);
		return 0;
	}

};

class	 msg_get_bank_info_ret :  public msg_base<none_socket>
{
public:
	__int64			bank_gold_;
	__int64			bank_gold_game_;
	int				psw_set_;
	msg_get_bank_info_ret()
	{
		head_.cmd_ = GET_CLSID(msg_get_bank_info_ret);
	}
	
	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(bank_gold_, data_s);
		write_jvalue(bank_gold_game_, data_s);
		write_jvalue(psw_set_, data_s);
		return 0;
	}
};

class  msg_present_data : public msg_base<none_socket>
{
public:
	int				pid_;
	int				cat_;		//0普通礼物,1-银商礼物
	std::string		name_;
	std::string		desc_;
	std::string		ico_;
	std::string		price_;	//价格

	msg_present_data()
	{
		head_.cmd_ = GET_CLSID(msg_present_data);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(pid_, data_s);
		write_jvalue(cat_, data_s);
		write_jvalue(name_, data_s);
		write_jvalue(desc_, data_s);
		write_jvalue(ico_, data_s);
		write_jvalue(price_, data_s);
		return 0;
	}
};

class		msg_sync_item_data : public msg_base<none_socket>
{
public:
	int				item_id_;
	int				key_;	//0-剩余时长
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

class		msg_acc_info_invalid : public msg_base<none_socket>
{
public:
	int			type_;	//1-用户名不合法 2-昵称不合法
	msg_acc_info_invalid()
	{
		head_.cmd_ = GET_CLSID(msg_acc_info_invalid);
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(type_, data_s);
		return 0;
	}
};