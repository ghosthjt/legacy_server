#pragma once
#include <boost/smart_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/atomic.hpp>

#include <list>

class koko_socket;
class koko_player : public boost::enable_shared_from_this<koko_player>
{
public:
	std::string			uid_;
	std::string			platform_uid_;
	int					vip_value_;
	int					vip_limit_;
	__int64				gold_, gold_game_, gold_free_;
	__int64				iid_;
	std::string			head_ico_;
	int					headframe_id_;
	std::vector<char>	headpic_;
	std::string			nickname_;
	std::string			token_;
	__int64				seq_;
	int					gender_, age_, level_, mobile_v_, email_v_, byear_, bmonth_, bday_;
	int					region1_, region2_, region3_;
	int					isagent_;
	int					is_newbee_;
	int					isbot_;
	std::string			idnumber_, mobile_, email_, address_, sec_mobile_;
	boost::weak_ptr<koko_socket> from_socket_;
	std::list<int>	in_channels_;
	boost::atomic_int	is_connection_lost_; //标记用户已掉线，如果5秒内不重连，会走掉线流程
	boost::atomic<boost::posix_time::ptime> connect_lost_tick_; 
	
	std::vector<char>	screen_shoot_;		//主播截图
	std::vector<int>	host_rooms_;
	std::string			from_devtype_;		//使用的设备:PC,IOS,ANDR
	std::string			last_game_srv;
	int					last_game_id_;
	std::string			app_key_;			//新增
	std::string			party_name_;		//新增
	koko_player();
	void						connection_lost();
	int							vip_level();
};

typedef boost::shared_ptr<koko_player> player_ptr;