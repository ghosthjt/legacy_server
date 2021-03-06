#pragma once

struct  scene_set;
template<class inherit>
class		logic_base : public boost::enable_shared_from_this<inherit>
{
public:
	int							id_;							//第几个游戏
	std::string					strid_;
	player_ptr					is_playing_[MAX_SEATS];			//游戏者
	bool						is_waiting_config_;
	scene_set*					scene_set_;
	int							is_match_game_;					//是否为比赛场
	bool						is_private_game_;				//是否为私密游戏
	int							config_index_;
	logic_base()
	{
		id_ = 0;
		is_match_game_ = 0;
		is_private_game_ = false;
		scene_set_= nullptr;
		config_index_ = 0;
	}
	int									get_playing_count(int get_type = 0)
	{
		int ret = 0;
		for (int i = 0; i < MAX_SEATS; i++) {
			if (is_playing_[i].get()) {
				if (get_type == 2) {
					if (is_playing_[i]->is_bot_) {
						ret++;
					}
				}
				else if (get_type == 1) {
					if (!is_playing_[i]->is_bot_) {
						ret++;
					}
				}
				else
					ret++;
			}
		}
		return ret;
	}
	int					get_free_count()
	{
		return MAX_SEATS - get_playing_count(0);
	}
	virtual int		update(float dt) { return 0; };
	int		sync_info_to_world();
	int		sync_info_to_client(std::string _uid);
	virtual void sync_player_info(std::string uid_) {};
	virtual ~logic_base() {}
private:
	void  sync_broadcast_msg(msg_base<none_socket>& msg);
};
