#pragma once
#include <string>
#include <vector>
#include <map>
#include "simple_xml_parse.h"

using namespace nsp_simple_xml;

#define READ_XML_VALUE(p, n)\
	n = doc.get_value(p#n, n)

#define WRITE_XML_VALUE(p, n)\
	doc.add_node(p, #n, n)


struct scene_set_base
{
	int			id_;
	__int64		ante_;		   //底注
	__int64		gurantee_set_; //入场限场
	scene_set_base()
	{
		id_ = 0;
		ante_ = 0;
		gurantee_set_ = 0;
	}
};

struct online_config
{
	int time_;
	int prize_;
};

class service_config_base
{
public:
	int							game_id;					//游戏ID			
	std::string					game_name;					//游戏名称
	std::string					instance_;
	std::string					accdb_host_;
	std::string					accdb_user_;
	std::string					accdb_pwd_;
	short						accdb_port_;
	std::string					accdb_name_;

	std::string					db_host_;
	std::string					db_user_;
	std::string					db_pwd_;
	short						db_port_;
	std::string					db_name_;

	std::string					php_sign_key_;
	std::string					client_ip_;
	unsigned short				client_port_;
	std::string					cache_ip_;
	unsigned short				cache_port_;

	std::string					world_ip_;
	unsigned short				world_port_;
	int							world_id_;
	std::string					world_key_;

	int							wait_reconnect;

	int							shut_down_;			//是否处于关闭状态

	std::vector<online_config>	online_prize_;

	std::map<std::string, std::string> broadcast_telfee;

	__int64						max_trade_cap_;
	int							trade_tax_;
	__int64						alert_email_set_;
	__int64						max_lose_golds_;

    std::map<std::string, std::string> global_config_;



	int				register_account_reward_;
	int				register_fee_;

	__int64	stock_lowcap_,
		stock_lowcap_max_,
		stock_upcap_start_,
		stock_upcap_max_,
		stock_decay_;

	service_config_base()
	{
		restore_default();
		register_account_reward_ = 0;
		alert_email_set_ = 999999999;
		register_fee_ = 0;
		world_key_ = "{327BE8D2-4A23-428F-B006-5A755288D5AB}";
	}

	virtual ~service_config_base()
	{

	}

	int save_default()
	{
		return 0;
	}
	virtual int	load_from_file(std::string path)
	{
		xml_doc doc;
		if (!doc.parse_from_file(path.c_str())) {
			restore_default();
			save_default();
		}
		else {
			READ_XML_VALUE("config/", db_port_);
			READ_XML_VALUE("config/", db_host_);
			READ_XML_VALUE("config/", db_user_);
			READ_XML_VALUE("config/", db_pwd_);
			READ_XML_VALUE("config/", db_name_);
			READ_XML_VALUE("config/", game_name);
			READ_XML_VALUE("config/", game_id);
			READ_XML_VALUE("config/", client_ip_);
			READ_XML_VALUE("config/", client_port_);
			READ_XML_VALUE("config/", php_sign_key_);
			READ_XML_VALUE("config/", accdb_port_);
			READ_XML_VALUE("config/", accdb_host_);
			READ_XML_VALUE("config/", accdb_user_);
			READ_XML_VALUE("config/", accdb_pwd_);
			READ_XML_VALUE("config/", accdb_name_);
			READ_XML_VALUE("config/", cache_ip_);
			READ_XML_VALUE("config/", cache_port_);
			READ_XML_VALUE("config/", world_ip_);
			READ_XML_VALUE("config/", world_port_);
			READ_XML_VALUE("config/", world_id_);
			READ_XML_VALUE("config/", world_key_);
			READ_XML_VALUE("config/", alert_email_set_);
			READ_XML_VALUE("config/", max_lose_golds_);
			READ_XML_VALUE("config/", register_fee_);
			READ_XML_VALUE("config/", instance_);
		}
		return verify();
	}
	virtual void restore_default()
	{
		db_host_ = "192.168.17.230";
		db_user_ = "hunter";
		db_pwd_ = "V@!*A#x!1cop%re0";
		db_port_ = 3306;
		db_name_ = "fishing";

		accdb_host_ = "192.168.17.224";
		accdb_user_ = "root";
		accdb_pwd_ = "123456";
		accdb_port_ = 3306;
		accdb_name_ = "account_server";

		cache_ip_ = "192.168.17.238";
		cache_port_ = 9999;

		client_ip_ = "192.168.17.31";
		client_port_ = 15000;
		php_sign_key_ = "z5u%wi31^_#h=284u%keg+ovc+j6!69wpwqe#u9*st5os5$=j2";
		alert_email_set_ = -10000000;
		max_lose_golds_ = 200000000;

		wait_reconnect = 30000;
	}
	virtual void	refresh() = 0;
	//验证配置合法性,
	virtual int  verify()
	{
		if (client_ip_ == "" || cache_ip_ == ""){
			return SYS_ERR_READ_CONFIG_ERR;
		}
		return ERROR_SUCCESS_0;
	}
    //从server_parameters_global表中获取全局配置
    template<typename T>
    T get_global_config(const std::string& key, const T& def_value)
    {
        auto ite = global_config_.find(key);
        if (ite != global_config_.end())
            return boost::lexical_cast<T>(ite->second);
        return def_value;
    }
};
