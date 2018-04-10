#pragma once
#include "msg.h"
#include "boost/shared_ptr.hpp"

template<class remote_t>
class msg_from_client : public msg_base<remote_t>
{
public:
	unsigned char    sign_[64];
	msg_from_client()
	{
		memset(sign_, 0, 64);
	}

	int				read(boost::property_tree::ptree& jsval) 
	{
		msg_base::read(jsval);
		json_msg_helper::read_arr_value("sign_", (char*)sign_, 64, jsval);
		return 0;
	}

	int				write(boost::property_tree::ptree& jsval)
	{
		msg_base::write(jsval);
		std::string s(sign_, sign_ + 32);
		json_msg_helper::write_value("sign_", s, jsval);
		return 0;
	}

	int				read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data<unsigned char>(sign_, 32, data_s);
		return 0;
	}

	int				write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data<unsigned char>(sign_, 32, data_s);
		return 0;
	}
  virtual ~msg_from_client() {}
};

//所有登录后发的数据包都需要从此继承

template<class remote_t>
class msg_authrized : public  msg_from_client<remote_t>
{
public:

};

