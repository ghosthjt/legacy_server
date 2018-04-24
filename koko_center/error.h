#pragma once
#include "boost/thread.hpp"
#include "boost/smart_ptr.hpp"

#define GET_CLSID(m) m##_id

enum 
{
	error_wrong_sign = -1000,	//数字签名不对
	error_wrong_password,			//密码不对
	error_sql_execute_error,	//sql语句执行失败
	error_no_record,					//数据库中没有相应记录
	error_user_banned,				//用户被禁止
	error_account_exist,			//注册账户名已存在
	error_server_busy,				//服务器正忙
	error_cant_find_player,		//找不到玩家
	error_cant_find_match,		//找不到比赛项目
	error_cant_find_server,		//找不到服务器
	error_msg_ignored,			//消息被忽略
	error_cancel_timer,			//定时器被取消
	error_cannt_regist_more,	//不能再注册了
	error_email_inusing,		//邮箱地址被使用
	error_mobile_inusing,		//手机号被使用
	error_wrong_verify_code,	//验证码错误
	error_time_expired,			//过期了
	error_invalid_data,			//数据不合法
	error_acc_name_invalid,		//用户名不合法
	error_cant_find_room,		//找不到房间

	
	error_success = 0,			//请求处理成功
	error_business_handled = 1,	//请求被处理
	
	
	error_invalid_request,		//请求非法
	error_not_enough_gold,		//金币不足
	error_not_enough_gold_game,	//游戏币不足
	error_not_enough_gold_free,	
	error_cant_find_coordinate = 6,	//找不到协同服务器
	error_no_173_account = 7,
	error_no_173_pretty,
	error_cannot_send_present,	//不能发送礼物
	error_cannot_recv_present = 10,	//不能接收礼物
	error_not_enough_item,		//道具不足
	error_activity_is_invalid,	//活动已失效
	error_already_buy_present,	//已经购买了，不能再次购买
	error_cannt_del_attach_mail,	//不能删除带有附件的邮件
	error_mail_state_invalid,		//邮件状态不合法
	error_not_enough_viplevel,		//VIP等级不够
	error_beyond_today_limit,			//超过今日上限
	error_count_invalid,					//数量不合法
	error_mysql_execute_uncertain = 13,
};

enum
{
	GET_CLSID(msg_ping) = 0xFFFF,
	GET_CLSID(msg_request_client_item_ret) = 993,
	GET_CLSID(msg_server_time) = 996,
	GET_CLSID(msg_sequence_send_over) = 997,
	GET_CLSID(msg_koko_trade_inout_ret) = 998,
	GET_CLSID(msg_user_login_ret_delegate) = 999,
	GET_CLSID(msg_user_login_ret) = 1000,
	GET_CLSID(msg_common_reply) = 1001,//占位符。
	GET_CLSID(msg_common_reply_internal) = 6000,//占位符。
	GET_CLSID(msg_player_info) = 1002,
	GET_CLSID(msg_player_leave) = 1003,
	GET_CLSID(msg_chat_deliver) = 1004,
	GET_CLSID(msg_same_account_login) = 1005,
	GET_CLSID(msg_live_show_start) = 1006,
	GET_CLSID(msg_turn_to_show) = 1007,
	GET_CLSID(msg_host_apply_accept) = 1008,
	GET_CLSID(msg_channel_server) = 1009,
	GET_CLSID(msg_image_data) = 1010,
	GET_CLSID(msg_user_image) = 1011,
	GET_CLSID(msg_account_info_update) = 1012,
	GET_CLSID(msg_verify_code) = 1013,
	GET_CLSID(msg_check_data_ret) = 1014,
	GET_CLSID(msg_live_show_stop) = 1015,
	GET_CLSID(msg_sync_item) = 1016,
	GET_CLSID(msg_region_data) = 1017,
	GET_CLSID(msg_srv_progress) = 1018,
	GET_CLSID(msg_sync_token) = 1019,
	GET_CLSID(msg_confirm_join_game_deliver) = 1020,
	GET_CLSID(msg_173user_login_ret) = 1021,
	GET_CLSID(msg_room_create_ret) = 1022,
	GET_CLSID(msg_enter_private_room_ret) = 1023,
	GET_CLSID(msg_get_bank_info_ret) = 1024,
	GET_CLSID(msg_present_data) = 1025,
	GET_CLSID(msg_private_room_remove) = 1026,
	GET_CLSID(msg_private_room_players) = 1027,
	GET_CLSID(msg_switch_game_server) = 1028,
	GET_CLSID(msg_query_data_ret) = 1029,
	GET_CLSID(msg_use_present_ret) = 1030,
	GET_CLSID(msg_get_present_record_ret) = 1031,
	GET_CLSID(msg_get_buy_item_record_ret) = 1032,
	GET_CLSID(msg_sync_item_data) = 1033,
	GET_CLSID(msg_sync) = 1034,
	GET_CLSID(msg_verify_user_ret) = 20017,
	GET_CLSID(msg_common_broadcast) = 1035,
	GET_CLSID(msg_get_private_room_record_ret) = 1036,	////

	GET_CLSID(msg_user_login_prize_list) = 1037,
	GET_CLSID(msg_user_login_prize) = 1038,

	GET_CLSID(msg_user_headframe_list) = 1039,		//我的头像框
	GET_CLSID(msg_user_head_and_headframe) = 1040,	//当前使用的头像和头像框
	GET_CLSID(msg_user_activity_list) = 1041,		//活动列表
	GET_CLSID(msg_user_recharge_record) = 1042,		//充值记录

	GET_CLSID(msg_lucky_dial_prize) = 1043,			//转盘奖品
	GET_CLSID(msg_user_serial_lucky_state) = 1044,

	GET_CLSID(msg_user_mail_list) = 1046,
	GET_CLSID(msg_user_mail_detail) = 1047,
	GET_CLSID(msg_user_mail_op) = 1048,

	GET_CLSID(msg_client_data) = 1049,
	GET_CLSID(msg_common_recv_present) = 1050,	//通用接口:获取物品

	GET_CLSID(msg_common_present_state) = 1051,			//礼物状态
	GET_CLSID(msg_common_present_state_change) = 1052,	//礼物状态改变

	GET_CLSID(msg_user_rank_data) = 1053,				//排行榜
	GET_CLSID(msg_user_head_list) = 1054,				//我的头像
	GET_CLSID(msg_user_day_buy_data) = 1055,			//今日已购买数量

	GET_CLSID(msg_black_account) = 1056,				//黑名单账号
};

enum
{
	GET_CLSID(msg_buy_item_notice)	= 990,
	GET_CLSID(msg_use_item_notice)	= 991,
	GET_CLSID(msg_sync_rank_notice)	= 992,
	GET_CLSID(msg_request_client_item) = 993,
	GET_CLSID(msg_koko_trade_inout) = 998,
	GET_CLSID(msg_mobile_login) = 99,
	GET_CLSID(msg_user_login) = 100,
	GET_CLSID(msg_user_register) = 101,
	GET_CLSID(msg_join_channel) = 102,
	GET_CLSID(msg_chat) = 103,
	GET_CLSID(msg_leave_channel) = 104,
	GET_CLSID(msg_host_apply) = 105,
	GET_CLSID(msg_show_start) = 106,
	GET_CLSID(msg_user_login_channel) = 107,
	GET_CLSID(msg_upload_screenshoot) = 108,
	GET_CLSID(msg_show_stop) = 109,
	GET_CLSID(msg_change_account_info) = 110,
	GET_CLSID(msg_get_verify_code) = 111,
	GET_CLSID(msg_check_data) = 112,
	GET_CLSID(msg_user_login_delegate) = 113,
	GET_CLSID(msg_send_present) = 114,
	GET_CLSID(msg_get_token) = 115,
	GET_CLSID(msg_action_record) = 116,
	GET_CLSID(msg_get_game_coordinate) = 117,
	GET_CLSID(msg_173user_login) = 118,
	GET_CLSID(msg_create_private_room) = 119,
	GET_CLSID(msg_enter_private_room) = 120,
	GET_CLSID(msg_send_live_present) = 121,
	GET_CLSID(msg_get_bank_info) = 122,
	GET_CLSID(msg_set_bank_psw) = 123,
	GET_CLSID(msg_bank_op) = 124,
	GET_CLSID(msg_trade_173_gold)=125,
	GET_CLSID(msg_transfer_gold_to_game)=126,
	GET_CLSID(msg_get_private_room_list)=127,
	GET_CLSID(msg_buy_item)=128,
	GET_CLSID(msg_get_private_room_record) = 129,	////
	GET_CLSID(msg_token_login) = 130,
	GET_CLSID(msg_scan_qrcode) = 1029,
	GET_CLSID(msg_alloc_game_server) = 1030,
	GET_CLSID(msg_use_present) = 1031,
	GET_CLSID(msg_query_data) = 1032,
	GET_CLSID(msg_get_present_record) = 1033,
	GET_CLSID(msg_get_buy_item_record) = 1034,
	GET_CLSID(msg_modify_acc_name) = 1035,
	GET_CLSID(msg_modify_nick_name) = 1036,

	GET_CLSID(msg_verify_user) = 1213,
	GET_CLSID(msg_send_present_by_pwd) = 1037,

	GET_CLSID(msg_req_sync_item) = 1038,		////

	GET_CLSID(msg_get_login_prize_list) = 1039,	//获取我的登录奖励列表
	GET_CLSID(msg_get_login_prize) = 1040,		//获取我的登录奖励

	GET_CLSID(msg_get_headframe_list) = 1041,		//获取我的头像框列表
	GET_CLSID(msg_set_head_and_headframe) = 1042,	//设置我的头像和头像框

	GET_CLSID(msg_buy_item_by_activity) = 1043,		//活动购买
	GET_CLSID(msg_get_activity_list) = 1044,		//获取我的活动列表
	GET_CLSID(msg_get_recharge_record) = 1045,		//充值记录

	GET_CLSID(msg_play_lucky_dial) = 1046,				//转盘
	GET_CLSID(msg_get_serial_lucky_state) = 1047,		//获取累计状态

	GET_CLSID(msg_get_mail_detail) = 1049,				//获取邮件详情(然后设为已读)
	GET_CLSID(msg_del_mail) = 1050,						//删除邮件
	GET_CLSID(msg_del_all_mail) = 1051,					//删除所有邮件(当前页签)
	GET_CLSID(msg_get_mail_attach) = 1052,				//获取邮件附件(然后删除邮件)

	GET_CLSID(msg_client_get_data) = 1053,				//客户端获取值
	GET_CLSID(msg_client_set_data) = 1054,				//客户端设置值
	GET_CLSID(msg_common_get_present) = 1055,			//获取礼包

	GET_CLSID(msg_send_mail_to_player) = 1056,			//游戏服务器给玩家发送邮件

	GET_CLSID(msg_get_rank_data) = 1057,				//排行榜
	GET_CLSID(msg_get_head_list) = 1058,				//获取我的头像列表
};

enum 
{
	GET_CLSID(msg_register_to_world) = 19999,
	GET_CLSID(msg_match_info) = 20000,
	GET_CLSID(msg_match_start) = 20001,
	GET_CLSID(msg_match_score) = 20002,
	GET_CLSID(msg_match_end) = 20003,
	GET_CLSID(msg_player_distribute) = 20004,
	GET_CLSID(msg_confirm_join_game) = 20005,
	GET_CLSID(msg_confirm_join_game_reply) = 20006,
	GET_CLSID(msg_match_register) = 20007,
	GET_CLSID(msg_match_cancel) = 20008,
	GET_CLSID(msg_query_info) = 20009,
	GET_CLSID(msg_match_data) = 20010,
	GET_CLSID(msg_game_data) = 20011,
	GET_CLSID(msg_host_data) = 20012,
	GET_CLSID(msg_game_room_data) = 20013,
	GET_CLSID(msg_my_match_score) = 20014,
	GET_CLSID(msg_my_game_data) = 20015,
	GET_CLSID(msg_match_champion) = 20016,
	GET_CLSID(msg_match_report) = 20017,
	GET_CLSID(msg_match_over) = 20018,
	GET_CLSID(msg_query_data_begin) = 20019,
	GET_CLSID(msg_player_eliminated_by_game) = 20020,
	GET_CLSID(msg_confirm_join_deliver) = 20021,
	GET_CLSID(msg_server_broadcast) = 20022,
	GET_CLSID(msg_private_room_sync) = 20023,
	GET_CLSID(msg_private_room_psync) = 20024,
	GET_CLSID(msg_acc_info_invalid) = 20025,
	GET_CLSID(msg_private_room_remove_sync) = 20026,////
	GET_CLSID(msg_enter_private_room_notice) = 20027,		////
	GET_CLSID(msg_koko_user_data_change) = 30000,
};

enum 
{
	item_id_gold = 0,				//钻石
	item_id_gold_game = 1,			//金币
	item_id_gold_free = 2,			//话费券
	item_id_gold_game_lock = 3,		//已兑入游戏的货币量
	item_id_gold_bank = 4,			//保险箱K币
	item_id_gold_game_bank = 5,		//保险箱K豆
	
	item_id_gold_transfer = 101,	//金币总转移
	item_id_vipvalue = 102,			//vip成长值
	item_id_viplimit = 103,			//vip最大限制值
	item_id_head = 104,				//头像
	item_id_headframe = 105,		//头像框

	item_id_present_id_begin = 1000,

	item_id_lucky_ticket = 1014,	//回馈券
};

enum{
	item_operator_add,
	item_operator_cost,
	item_operator_cost_all,
};

enum 
{
	client_action_logingame,
	client_action_logoutgame,
};

enum
{
	ITEM_TYPE_COMMON,		//普通商品
	ITEM_TYPE_HEADFRAME,	//头像框商品
	ITEM_TYPE_HEAD,			//头像商品
};

struct end_point
{
	char	ip_[32];
	int		port_;
};

//平台启动时，会下载主播，游戏，比赛的配置文件
//客户端就有了处理依据

//主播配置信息
struct host_info
{
	__int64	roomid_;	//房间ID
	char		roomname_[128];	//房间名称
	char		host_uid_[64]; //主播ID
	char		thumbnail_[128]; //缩略图
	end_point	avaddr_; //视频服务器地址
	end_point	game_addr_; //游戏服务器地址
	int			gameid_;//关联游戏id
	int			popular_;
	int			online_players_;
	int			is_online_;
	int			avshow_start_;
	host_info()
	{
		popular_ = 0;
		online_players_ = 0;
		is_online_ = 0;
		gameid_ = 0;
		roomid_ = 0;
		roomname_[0] = 0;
		host_uid_[0] = 0;
		thumbnail_[0] = 0;
		avshow_start_ = 0;
	}
};

struct game_room_inf
{
	int						game_id_;
	int						room_id_;						//>1000时,是自定义房间id,否则是入场配置限制id
	char					room_name_[64];
	char					room_desc_[64];
	char					ip_[32];
	int						port_;
	char					thumbnail_[128];			//缩略图
	char					require_[256];
	game_room_inf()
	{
		memset(this, 0, sizeof(*this));
	}
};

//游戏配置信息，平台启动时，会下载
struct game_info
{
	enum 
	{
		game_cat_classic,
		game_cat_match,
		game_cat_host,
	};
	enum 
	{

	};
	int						id_;							//游戏id	
	int						type_;						//0 客户端游戏，1 flash游戏
	char					dir_[64];					//目录，只允许填一个目录名，不允许填路径
	char					exe_[64];					//可执行程序名
	char					update_url_[128];	//更新路径
	char					help_url_[128];		//介绍路径
	char					game_name_[128];	//游戏名称
	char					thumbnail_[128];	//略缩图
	char					solution_[16];		//游戏的设计分辨率,如果为noembed,表示不嵌入平台运行
	int						no_embed_;
	char					catalog_[128];
	int						seats_;
	int						order_;
	std::vector<game_room_inf> rooms_;
};

struct channel_server
{
	std::string instance_;
	std::string ip_;
	std::string device_type_;
	int		port_;
	int		online_;
	std::vector<int> game_ids_;
};

struct match_info : public boost::enable_shared_from_this<match_info>
{
	int				id_;						//比赛id
	int				match_type_;		//比赛类型 0-定时赛, 1-个人闯关赛,2-淘汰赛
	int				game_id_;				//游戏id，对应game_info的游戏id
	std::string		match_name_;		//比赛名称
	std::string		thumbnail_;			//缩略图
	std::string		help_url_;			//比赛介绍	
	std::string		prize_desc_;		//奖励描述
	int				start_type_	;		//开赛时机,0按时开赛,1人满即开 2随报随开
	int				require_count_;	//开赛最低人数要求
	int				start_stripe_;	//start_type_ = 0时，这个字段有效, 0 间隔时间开, 1 固定时间开
	union 
	{
		int			wait_register_; //start_strip_ = 0 时， 等待注册时间,秒为单位
		struct											//start_strip_ = 1 时，
		{
			int		day, h, m, s;		//间隔天数, 于那天的 时，分，秒
		};
	};

	int						need_players_per_game_;					//每个游戏需要几人参于
	int						eliminate_phase1_basic_score_;	//淘汰赛海选阶段初始分
	int						eliminate_phase1_inc;						//淘汰赛海选阶段基础分增量(低于基础分的被淘汰,以分钟为单位计)
	int						elininate_phase2_count_;				//淘汰赛进入决赛阶段人数

	int						end_type_;		//结束时机 0定时结束，1 决出胜者
	int						end_data_;		//数据 end_type_ = 0, 这里是结束时长, = 1，是决出胜者数

	std::vector<std::vector<std::pair<int, int>>>	prizes_;		//名次(按在数组的次序)，物品id,　数量
	std::map<int, int>	cost_;										//报名费 物品id,　数量
	std::string		srvip_;
	int						srvport_;
	match_info() 
	{
		eliminate_phase1_inc = 0;
		elininate_phase2_count_ = 0;
		eliminate_phase1_basic_score_ = 0;
		need_players_per_game_ = 4;
		end_type_ = 0;
		end_data_ = 0;
		start_stripe_ = 0;
		wait_register_ = 0;
		h = m = s = 0;
	};
	virtual ~match_info(){}
};

template<class data_type>
class atomic_value
{
public:
	void				store(data_type v)
	{
		boost::lock_guard<boost::mutex> lk(mtu_);
		dat_ = v;
	}
	data_type		load()
	{
		data_type ret;
		boost::lock_guard<boost::mutex> lk(mtu_);
		ret = dat_;
		return ret;
	}
protected:
	data_type		dat_;
	boost::mutex	mtu_;
};

template<class socket_t>
void	send_all_game_to_player(socket_t psock)
{
	auto it = all_games.begin();
	while (it != all_games.end())
	{
		gamei_ptr pgame = it->second; it++;
		msg_game_data msg;
		msg.inf = *pgame;
		send_protocol(psock, msg);
	}
}

struct item_unit
{
	int cat_;
	int item_id_;
	int item_num_;
};

struct	present_info
{
	int		pid_;
	int		cat_;		//商品类别:0-背包商品活动 1-头像框商品活动 2-头像商品活动
	char	name_[64];
	char	desc_[256];
	char	ico_[64];
	int		vip_level_;

	std::vector<item_unit>		price_;	//价格
	std::vector<item_unit>		item_;	//价值
	int		is_delay_;			//是否延迟到账
	int		min_buy_num_;		//一次最小购买数量
	int		max_buy_num_;		//一次最大购买数量
	int		buy_record_sec;		//购买记录保留秒数
	int		forbid_buy_;		//是否禁止购买
	int		day_limit_;			//每日限制购买个数
};

struct	item_info 
{
	int		item_id_;
	int		type_;				//道具类别：0-按个数算，1-按时长算
	char	name_[64];
	char	desc_[256];
	char	ico_[64];

	int		is_use_;			//是否可使用

	std::vector<std::pair<int, int>>	to_item_;	//道具使用价值

	int		is_send_;			//是否可赠送
	int		send_vip_level_;	//赠送所需VIP等级
	int		min_send_num_;		//最少赠送数
	int		max_send_num_;		//最大赠送数
};

struct private_room_inf : public game_room_inf
{
	struct seat
	{
		std::string uid_;
		std::string nick_name_;
	};

	typedef	boost::shared_ptr<seat> seat_ptr;
	
	int				config_index_;
	int				need_psw_;
	int				player_count_;
	int				seats_;
	int				total_turn_;	////
	std::string		params_;		////
	int				state_;			////
	std::string		psw_;
	char			master_[64];
	char			master_nick_[64];
	std::map<std::string, seat_ptr> onseats_;
	std::string		srv_instance_;

	private_room_inf()
	{
		player_count_ = 0; 
		seats_ = 0;
		total_turn_ = 0;
		config_index_ = 0;
		need_psw_ = 0;
		game_id_ = -1;
		memset(master_nick_, 0, 64);
		memset(master_, 0, 64);
	}
};
typedef boost::shared_ptr<private_room_inf> priv_room_ptr;

class game_srv_socket;
typedef boost::weak_ptr<game_srv_socket> srv_socket_wptr;

struct internal_server_inf
{
	srv_socket_wptr wsock_;
	std::string		open_ip_;
	unsigned short	open_port_;
	int				game_id_;
	std::string		instance_;	//实例id
	int				roomid_start_;	//自定义房间号起始位置
};

class koko_player;
typedef boost::shared_ptr<koko_player> player_ptr;
struct priv_game_channel
{
	int game_id_;
	std::vector<player_ptr> players_;		//有哪些玩家
	std::map<int, priv_room_ptr> rooms_;	//有哪些房间
};

struct	activity_info
{
	int		activity_id_;		//活动ID
	int		cat_;				//活动商品类别:0 - 背包商品活动 1 - 头像框商品活动 2 - 头像商品活动
	char	name_[64];
	char	desc_[256];

	std::vector<item_unit>		price_;	//价格
	std::vector<item_unit>		item_;	//价值
	int		min_buy_num_;		//一次最小购买数量
	int		max_buy_num_;		//一次最大购买数量
	__int64	begin_time_;		//活动开始时间戳
	__int64 end_time_;			//活动结束时间戳
};

struct	lucky_dial_info
{
	int		lucky_id_;			//幸运ID
	int		probability_;		//概率
	int		prize_id_;			//奖品ID
	int		prize_num_;			//奖品数量
	int		limit_sys_num_;		//每日系统个数
	int		limit_user_num_;	//每日玩家中奖次数
	int		limit_second_;		//中奖间隔
	int		max_lucky_value_;	//最大幸运值（最大奖才设置）
};

struct user_privilege
{
	int		vip_level_;					//等级
	int		vip_value_;					//等级所需成长值
	int		login_prize_times_;			//每日签到奖励倍数
	int		lucky_dial_times_;			//每日幸运转盘免费次数
	int		send_present_limit_;		//每日赠送礼物上限
};

struct system_prize
{
	std::string prize_name_;
	int key_;
	int pid_;		//礼包ID
	std::vector<item_unit> prize_;		//礼包物品
};

struct user_mail
{
	std::string uid_;
	int			mail_type_;		//邮件类型：0-系统邮件 1-礼物邮件 2-好友邮件
	int			title_type_;	//标题类型：0~5:系统邮件（系统、更新、记录、活动、补偿、奖励）6:礼物邮件（礼物）7:好友邮件（好友）
	std::string title_;
	std::string content_;
	std::string hyperlink_;
	int			state_;
	int			attach_state_;
	std::string	attach_;		//附件
	int			save_days_;

	user_mail()
	{
	}
};