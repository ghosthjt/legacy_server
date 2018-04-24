#pragma once
#include "boost/thread.hpp"
#include "boost/smart_ptr.hpp"

#define GET_CLSID(m) m##_id

enum 
{
	error_wrong_sign = -1000,	//����ǩ������
	error_wrong_password,			//���벻��
	error_sql_execute_error,	//sql���ִ��ʧ��
	error_no_record,					//���ݿ���û����Ӧ��¼
	error_user_banned,				//�û�����ֹ
	error_account_exist,			//ע���˻����Ѵ���
	error_server_busy,				//��������æ
	error_cant_find_player,		//�Ҳ������
	error_cant_find_match,		//�Ҳ���������Ŀ
	error_cant_find_server,		//�Ҳ���������
	error_msg_ignored,			//��Ϣ������
	error_cancel_timer,			//��ʱ����ȡ��
	error_cannt_regist_more,	//������ע����
	error_email_inusing,		//�����ַ��ʹ��
	error_mobile_inusing,		//�ֻ��ű�ʹ��
	error_wrong_verify_code,	//��֤�����
	error_time_expired,			//������
	error_invalid_data,			//���ݲ��Ϸ�
	error_acc_name_invalid,		//�û������Ϸ�
	error_cant_find_room,		//�Ҳ�������

	
	error_success = 0,			//������ɹ�
	error_business_handled = 1,	//���󱻴���
	
	
	error_invalid_request,		//����Ƿ�
	error_not_enough_gold,		//��Ҳ���
	error_not_enough_gold_game,	//��Ϸ�Ҳ���
	error_not_enough_gold_free,	
	error_cant_find_coordinate = 6,	//�Ҳ���Эͬ������
	error_no_173_account = 7,
	error_no_173_pretty,
	error_cannot_send_present,	//���ܷ�������
	error_cannot_recv_present = 10,	//���ܽ�������
	error_not_enough_item,		//���߲���
	error_activity_is_invalid,	//���ʧЧ
	error_already_buy_present,	//�Ѿ������ˣ������ٴι���
	error_cannt_del_attach_mail,	//����ɾ�����и������ʼ�
	error_mail_state_invalid,		//�ʼ�״̬���Ϸ�
	error_not_enough_viplevel,		//VIP�ȼ�����
	error_beyond_today_limit,			//������������
	error_count_invalid,					//�������Ϸ�
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
	GET_CLSID(msg_common_reply) = 1001,//ռλ����
	GET_CLSID(msg_common_reply_internal) = 6000,//ռλ����
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

	GET_CLSID(msg_user_headframe_list) = 1039,		//�ҵ�ͷ���
	GET_CLSID(msg_user_head_and_headframe) = 1040,	//��ǰʹ�õ�ͷ���ͷ���
	GET_CLSID(msg_user_activity_list) = 1041,		//��б�
	GET_CLSID(msg_user_recharge_record) = 1042,		//��ֵ��¼

	GET_CLSID(msg_lucky_dial_prize) = 1043,			//ת�̽�Ʒ
	GET_CLSID(msg_user_serial_lucky_state) = 1044,

	GET_CLSID(msg_user_mail_list) = 1046,
	GET_CLSID(msg_user_mail_detail) = 1047,
	GET_CLSID(msg_user_mail_op) = 1048,

	GET_CLSID(msg_client_data) = 1049,
	GET_CLSID(msg_common_recv_present) = 1050,	//ͨ�ýӿ�:��ȡ��Ʒ

	GET_CLSID(msg_common_present_state) = 1051,			//����״̬
	GET_CLSID(msg_common_present_state_change) = 1052,	//����״̬�ı�

	GET_CLSID(msg_user_rank_data) = 1053,				//���а�
	GET_CLSID(msg_user_head_list) = 1054,				//�ҵ�ͷ��
	GET_CLSID(msg_user_day_buy_data) = 1055,			//�����ѹ�������

	GET_CLSID(msg_black_account) = 1056,				//�������˺�
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

	GET_CLSID(msg_get_login_prize_list) = 1039,	//��ȡ�ҵĵ�¼�����б�
	GET_CLSID(msg_get_login_prize) = 1040,		//��ȡ�ҵĵ�¼����

	GET_CLSID(msg_get_headframe_list) = 1041,		//��ȡ�ҵ�ͷ����б�
	GET_CLSID(msg_set_head_and_headframe) = 1042,	//�����ҵ�ͷ���ͷ���

	GET_CLSID(msg_buy_item_by_activity) = 1043,		//�����
	GET_CLSID(msg_get_activity_list) = 1044,		//��ȡ�ҵĻ�б�
	GET_CLSID(msg_get_recharge_record) = 1045,		//��ֵ��¼

	GET_CLSID(msg_play_lucky_dial) = 1046,				//ת��
	GET_CLSID(msg_get_serial_lucky_state) = 1047,		//��ȡ�ۼ�״̬

	GET_CLSID(msg_get_mail_detail) = 1049,				//��ȡ�ʼ�����(Ȼ����Ϊ�Ѷ�)
	GET_CLSID(msg_del_mail) = 1050,						//ɾ���ʼ�
	GET_CLSID(msg_del_all_mail) = 1051,					//ɾ�������ʼ�(��ǰҳǩ)
	GET_CLSID(msg_get_mail_attach) = 1052,				//��ȡ�ʼ�����(Ȼ��ɾ���ʼ�)

	GET_CLSID(msg_client_get_data) = 1053,				//�ͻ��˻�ȡֵ
	GET_CLSID(msg_client_set_data) = 1054,				//�ͻ�������ֵ
	GET_CLSID(msg_common_get_present) = 1055,			//��ȡ���

	GET_CLSID(msg_send_mail_to_player) = 1056,			//��Ϸ����������ҷ����ʼ�

	GET_CLSID(msg_get_rank_data) = 1057,				//���а�
	GET_CLSID(msg_get_head_list) = 1058,				//��ȡ�ҵ�ͷ���б�
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
	item_id_gold = 0,				//��ʯ
	item_id_gold_game = 1,			//���
	item_id_gold_free = 2,			//����ȯ
	item_id_gold_game_lock = 3,		//�Ѷ�����Ϸ�Ļ�����
	item_id_gold_bank = 4,			//������K��
	item_id_gold_game_bank = 5,		//������K��
	
	item_id_gold_transfer = 101,	//�����ת��
	item_id_vipvalue = 102,			//vip�ɳ�ֵ
	item_id_viplimit = 103,			//vip�������ֵ
	item_id_head = 104,				//ͷ��
	item_id_headframe = 105,		//ͷ���

	item_id_present_id_begin = 1000,

	item_id_lucky_ticket = 1014,	//����ȯ
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
	ITEM_TYPE_COMMON,		//��ͨ��Ʒ
	ITEM_TYPE_HEADFRAME,	//ͷ�����Ʒ
	ITEM_TYPE_HEAD,			//ͷ����Ʒ
};

struct end_point
{
	char	ip_[32];
	int		port_;
};

//ƽ̨����ʱ����������������Ϸ�������������ļ�
//�ͻ��˾����˴�������

//����������Ϣ
struct host_info
{
	__int64	roomid_;	//����ID
	char		roomname_[128];	//��������
	char		host_uid_[64]; //����ID
	char		thumbnail_[128]; //����ͼ
	end_point	avaddr_; //��Ƶ��������ַ
	end_point	game_addr_; //��Ϸ��������ַ
	int			gameid_;//������Ϸid
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
	int						room_id_;						//>1000ʱ,���Զ��巿��id,�������볡��������id
	char					room_name_[64];
	char					room_desc_[64];
	char					ip_[32];
	int						port_;
	char					thumbnail_[128];			//����ͼ
	char					require_[256];
	game_room_inf()
	{
		memset(this, 0, sizeof(*this));
	}
};

//��Ϸ������Ϣ��ƽ̨����ʱ��������
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
	int						id_;							//��Ϸid	
	int						type_;						//0 �ͻ�����Ϸ��1 flash��Ϸ
	char					dir_[64];					//Ŀ¼��ֻ������һ��Ŀ¼������������·��
	char					exe_[64];					//��ִ�г�����
	char					update_url_[128];	//����·��
	char					help_url_[128];		//����·��
	char					game_name_[128];	//��Ϸ����
	char					thumbnail_[128];	//����ͼ
	char					solution_[16];		//��Ϸ����Ʒֱ���,���Ϊnoembed,��ʾ��Ƕ��ƽ̨����
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
	int				id_;						//����id
	int				match_type_;		//�������� 0-��ʱ��, 1-���˴�����,2-��̭��
	int				game_id_;				//��Ϸid����Ӧgame_info����Ϸid
	std::string		match_name_;		//��������
	std::string		thumbnail_;			//����ͼ
	std::string		help_url_;			//��������	
	std::string		prize_desc_;		//��������
	int				start_type_	;		//����ʱ��,0��ʱ����,1�������� 2�汨�濪
	int				require_count_;	//�����������Ҫ��
	int				start_stripe_;	//start_type_ = 0ʱ������ֶ���Ч, 0 ���ʱ�俪, 1 �̶�ʱ�俪
	union 
	{
		int			wait_register_; //start_strip_ = 0 ʱ�� �ȴ�ע��ʱ��,��Ϊ��λ
		struct											//start_strip_ = 1 ʱ��
		{
			int		day, h, m, s;		//�������, ������� ʱ���֣���
		};
	};

	int						need_players_per_game_;					//ÿ����Ϸ��Ҫ���˲���
	int						eliminate_phase1_basic_score_;	//��̭����ѡ�׶γ�ʼ��
	int						eliminate_phase1_inc;						//��̭����ѡ�׶λ���������(���ڻ����ֵı���̭,�Է���Ϊ��λ��)
	int						elininate_phase2_count_;				//��̭����������׶�����

	int						end_type_;		//����ʱ�� 0��ʱ������1 ����ʤ��
	int						end_data_;		//���� end_type_ = 0, �����ǽ���ʱ��, = 1���Ǿ���ʤ����

	std::vector<std::vector<std::pair<int, int>>>	prizes_;		//����(��������Ĵ���)����Ʒid,������
	std::map<int, int>	cost_;										//������ ��Ʒid,������
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
	int		cat_;		//��Ʒ���:0-������Ʒ� 1-ͷ�����Ʒ� 2-ͷ����Ʒ�
	char	name_[64];
	char	desc_[256];
	char	ico_[64];
	int		vip_level_;

	std::vector<item_unit>		price_;	//�۸�
	std::vector<item_unit>		item_;	//��ֵ
	int		is_delay_;			//�Ƿ��ӳٵ���
	int		min_buy_num_;		//һ����С��������
	int		max_buy_num_;		//һ�����������
	int		buy_record_sec;		//�����¼��������
	int		forbid_buy_;		//�Ƿ��ֹ����
	int		day_limit_;			//ÿ�����ƹ������
};

struct	item_info 
{
	int		item_id_;
	int		type_;				//�������0-�������㣬1-��ʱ����
	char	name_[64];
	char	desc_[256];
	char	ico_[64];

	int		is_use_;			//�Ƿ��ʹ��

	std::vector<std::pair<int, int>>	to_item_;	//����ʹ�ü�ֵ

	int		is_send_;			//�Ƿ������
	int		send_vip_level_;	//��������VIP�ȼ�
	int		min_send_num_;		//����������
	int		max_send_num_;		//���������
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
	std::string		instance_;	//ʵ��id
	int				roomid_start_;	//�Զ��巿�����ʼλ��
};

class koko_player;
typedef boost::shared_ptr<koko_player> player_ptr;
struct priv_game_channel
{
	int game_id_;
	std::vector<player_ptr> players_;		//����Щ���
	std::map<int, priv_room_ptr> rooms_;	//����Щ����
};

struct	activity_info
{
	int		activity_id_;		//�ID
	int		cat_;				//���Ʒ���:0 - ������Ʒ� 1 - ͷ�����Ʒ� 2 - ͷ����Ʒ�
	char	name_[64];
	char	desc_[256];

	std::vector<item_unit>		price_;	//�۸�
	std::vector<item_unit>		item_;	//��ֵ
	int		min_buy_num_;		//һ����С��������
	int		max_buy_num_;		//һ�����������
	__int64	begin_time_;		//���ʼʱ���
	__int64 end_time_;			//�����ʱ���
};

struct	lucky_dial_info
{
	int		lucky_id_;			//����ID
	int		probability_;		//����
	int		prize_id_;			//��ƷID
	int		prize_num_;			//��Ʒ����
	int		limit_sys_num_;		//ÿ��ϵͳ����
	int		limit_user_num_;	//ÿ������н�����
	int		limit_second_;		//�н����
	int		max_lucky_value_;	//�������ֵ����󽱲����ã�
};

struct user_privilege
{
	int		vip_level_;					//�ȼ�
	int		vip_value_;					//�ȼ�����ɳ�ֵ
	int		login_prize_times_;			//ÿ��ǩ����������
	int		lucky_dial_times_;			//ÿ������ת����Ѵ���
	int		send_present_limit_;		//ÿ��������������
};

struct system_prize
{
	std::string prize_name_;
	int key_;
	int pid_;		//���ID
	std::vector<item_unit> prize_;		//�����Ʒ
};

struct user_mail
{
	std::string uid_;
	int			mail_type_;		//�ʼ����ͣ�0-ϵͳ�ʼ� 1-�����ʼ� 2-�����ʼ�
	int			title_type_;	//�������ͣ�0~5:ϵͳ�ʼ���ϵͳ�����¡���¼�����������������6:�����ʼ������7:�����ʼ������ѣ�
	std::string title_;
	std::string content_;
	std::string hyperlink_;
	int			state_;
	int			attach_state_;
	std::string	attach_;		//����
	int			save_days_;

	user_mail()
	{
	}
};