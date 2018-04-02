#pragma once
#include "logic_base.h"
#include "msg_server_common.h"

template<class inherit>
int logic_base<inherit>::sync_info_to_world()
{
	if(is_match_game_) return 0;
	{
		msg_private_room_sync msg;
		msg.room_id_ = id_;
		msg.player_count_ = get_playing_count(0);
		msg.config_index_ = config_index_;
		msg.seats_ = MAX_SEATS;
		if (the_service.world_connector_.get()){
			send_msg(the_service.world_connector_, msg);
		}
	}

	for (int i = 0; i < MAX_SEATS; i++)
	{
		if (!is_playing_[i].get()) continue;

		msg_private_room_psync msg;
		msg.room_id_ = id_;
		if (is_playing_[i]->is_bot_){
			strcpy(msg.uid_, is_playing_[i]->uid_.c_str());
		}
		else
			strcpy(msg.uid_, is_playing_[i]->platform_uid_.c_str());
		strcpy(msg.nick_name_, is_playing_[i]->name_.c_str());
		if (the_service.world_connector_.get()){
			send_msg(the_service.world_connector_, msg);
		}
	}
	return 0;
}

template<class inherit>
void logic_base<inherit>::sync_broadcast_msg(msg_base<none_socket>& msg)
{
	for (int i = 0; i < MAX_SEATS; i++)
	{
		if (!is_playing_[i].get()) continue;
		is_playing_[i]->send_msg_to_client(msg);
	}
}

template<class inherit>
int logic_base<inherit>::sync_info_to_client(std::string _uid)
{
	if(is_match_game_) return 0;
	for(int i = 0; i < MAX_SEATS; i++)
	{
		player_ptr pp = is_playing_[i];
		if(!pp.get())continue;
		if(pp->uid_ == _uid){
			msg_deposit_change2 msg;
			msg.pos_ = pp->pos_;
			msg.credits_ = (double)the_service.cache_helper_.get_currency(pp->uid_);
			msg.display_type_ = 0;
			sync_broadcast_msg(msg);
			break;
		}
	}
	return 0;
}