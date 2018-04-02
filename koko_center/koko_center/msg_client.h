#pragma once
#include "koko_socket.h"
#include "msg_from_client.h"

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

//登录
class msg_user_login : public msg_user_login_t<koko_socket_ptr>
{
public:
	msg_user_login()
	{
		head_.cmd_ = GET_CLSID(msg_user_login);
	}
	int			handle_this();
};

class msg_173user_login : public msg_user_login_t<koko_socket_ptr>
{
public:
	int			ispretty_;
	int			auto_register_;
	msg_173user_login()
	{
		head_.cmd_ = GET_CLSID(msg_173user_login);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_user_login_t<koko_socket_ptr>::read(data_s);
		read_jvalue(ispretty_, data_s);
		read_jvalue(auto_register_, data_s);
		return 0;
	}
	int			handle_this();
};


class	msg_mobile_login : public msg_user_login
{
public:
	std::string		dev_type;
	msg_mobile_login()
	{
		head_.cmd_ = GET_CLSID(msg_mobile_login); //99
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_user_login::read(data_s);
		read_jvalue(dev_type, data_s);
		return 0;
	}
};


//注册账号
class msg_user_register : public msg_user_login
{
public:
	int				type_;	//0用户名,1手机，2邮箱
	std::string		verify_code;
	std::string		spread_from_;
	msg_user_register()
	{
		head_.cmd_ = GET_CLSID(msg_user_register);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_user_login::read(data_s);
		read_jvalue(type_, data_s);
		read_jvalue(verify_code, data_s);
		read_jvalue(spread_from_, data_s);
		return 0;
	}

	int			handle_this();
};

//更改用户数据
class msg_change_account_info : public msg_authrized<koko_socket_ptr>
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
	msg_change_account_info()
	{
		head_.cmd_ = GET_CLSID(msg_change_account_info);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(gender_, data_s);
		read_jvalue(byear_, data_s);
		read_jvalue(bmonth_, data_s);
		read_jvalue(bday_, data_s);
		read_jvalue(address_, data_s);
		read_jvalue(nick_name_, data_s);
		read_jvalue(age_, data_s);
		read_jvalue(mobile_, data_s);
		read_jvalue(email_, data_s);
		read_jvalue(idcard_, data_s);
		read_jvalue(region1_, data_s);
		read_jvalue(region2_, data_s);
		read_jvalue(region3_, data_s);
		return 0;
	}
	int		handle_this();
};

//生成验证码
class	msg_get_verify_code : public msg_from_client<koko_socket_ptr>
{
public:
	int				type_;		//0图片验证码 1手机验证码 2 邮件验证码
	std::string		mobile_;
	msg_get_verify_code()
	{
		head_.cmd_ = GET_CLSID(msg_get_verify_code);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(type_, data_s);
		read_jvalue(mobile_, data_s);
		return 0;
	}

	int		handle_this();
};

class	 msg_check_data : public msg_from_client<koko_socket_ptr>
{
public:
	enum 
	{
		check_account_name_exist,
		check_email_exist,
		check_mobile_exist,
		check_verify_code,		//图片验证码
		check_mverify_code,		//短信验证码
	};

	int				query_type_;
	__int64			query_idata_;	
	std::string		query_sdata_;
	msg_check_data()
	{
		head_.cmd_ = GET_CLSID(msg_check_data);
		query_sdata_[0] = 0;
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(query_type_, data_s);
		read_jvalue(query_idata_, data_s);
		read_jvalue(query_sdata_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_action_record : public msg_from_client<koko_socket_ptr>
{
public:
	int				action_id_;
	int				action_data_;		//是不离开游戏
	int				action_data2_;		//是不离开游戏
	std::string		action_data3_;
	msg_action_record()
	{
		head_.cmd_ = GET_CLSID(msg_action_record);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(action_id_, data_s);
		read_jvalue(action_data_, data_s);
		read_jvalue(action_data2_, data_s);
		read_jvalue(action_data3_, data_s);
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

//得到游戏协同服务器
class		msg_get_game_coordinate : public msg_from_client<koko_socket_ptr>
{
public:
	int				gameid_;
	std::string		device_type_;
	msg_get_game_coordinate()
	{
		head_.cmd_ = GET_CLSID(msg_get_game_coordinate);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(gameid_, data_s);
		read_jvalue(device_type_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_send_live_present : public msg_authrized<koko_socket_ptr>
{
public:
	std::string		roomid;
	std::string		usid;
	std::string		to;
	std::string		giftid;
	int				count;
	int				ismygift;
	msg_send_live_present()
	{
		head_.cmd_ = GET_CLSID(msg_send_live_present);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(roomid, data_s);
		read_jvalue(usid, data_s);
		read_jvalue(to, data_s);
		read_jvalue(giftid, data_s);
		read_jvalue(count, data_s);
		read_jvalue(ismygift, data_s);
		return 0;
	}
	int		handle_this();
};

//
class		msg_get_bank_info : public msg_authrized<koko_socket_ptr>
{
public:
	msg_get_bank_info()
	{
		head_.cmd_ = GET_CLSID(msg_get_bank_info);
	}
	int		handle_this();
};

//设置保险箱密码
class		msg_set_bank_psw : public msg_authrized<koko_socket_ptr>
{
public:
	int				func_;		//0-设置密码, 1-修改密码, 2-验证密码
	std::string		old_psw_;
	std::string		psw_;
	msg_set_bank_psw()
	{
		head_.cmd_ = GET_CLSID(msg_set_bank_psw);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(func_, data_s);
		read_jvalue(old_psw_, data_s);
		read_jvalue(psw_, data_s);
		return 0;
	}
	int		handle_this();
};

class		msg_bank_op : public msg_authrized<koko_socket_ptr>
{
public:
	std::string		psw_;		//密码
	short			op_;		//0,提取,1存入
	short			type_;		//0,K币,1K豆
	__int64			count_;	

	msg_bank_op()
	{
		head_.cmd_ = GET_CLSID(msg_bank_op);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(psw_, data_s);
		read_jvalue(op_, data_s);
		read_jvalue(type_, data_s);
		read_jvalue(count_, data_s);
		return 0;
	}
	int		handle_this();
};

//K币兑换成K豆
class	 msg_transfer_gold_to_game : public msg_authrized<koko_socket_ptr>
{
public:
	__int64 count_;

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
};

class	msg_scan_qrcode : public msg_from_client<koko_socket_ptr>
{
public:
	std::string		clientid_;
	std::string		passcode_;
	msg_scan_qrcode()
	{
		head_.cmd_ = GET_CLSID(msg_scan_qrcode);
	}
	
	int			read(boost::property_tree::ptree& data_s)
	{
		msg_from_client::read(data_s);
		read_jvalue(passcode_, data_s);
		read_jvalue(clientid_, data_s);
		return 0;
	}
	int			handle_this();
};

//修改用户名
class msg_modify_acc_name : public msg_authrized<koko_socket_ptr>
{
public:
	std::string		uid_;
	std::string		pwd_hash_;
	std::string		new_acc_name_;
	msg_modify_acc_name()
	{
		head_.cmd_ = GET_CLSID(msg_modify_acc_name);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(pwd_hash_, data_s);
		read_jvalue(new_acc_name_, data_s);
		return 0;
	}
	int			handle_this();
};

//修改用户名
class msg_modify_nick_name : public msg_authrized<koko_socket_ptr>
{
public:
	std::string		uid_;
	std::string		nick_name_;
	msg_modify_nick_name()
	{
		head_.cmd_ = GET_CLSID(msg_modify_nick_name);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_authrized::read(data_s);
		read_jvalue(uid_, data_s);
		read_jvalue(nick_name_, data_s);
		return 0;
	}
	int			handle_this();
};

class msg_token_login : public msg_from_client<koko_socket_ptr>
{
public:
	std::string		uid_;
	__int64			sn_;
	std::string		token_;
	std::string		device_;
	msg_token_login()
	{
		head_.cmd_ = GET_CLSID(msg_token_login);
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