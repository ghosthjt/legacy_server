#include "player.h"
#include <map>
#include "utf8_db.h"
#include "Query.h"
#include "utility.h"
#include "db_delay_helper.h"
#include <unordered_map>
#include "error.h"

extern boost::shared_ptr<utf8_data_base>	db_;
extern db_delay_helper& get_delaydb();
extern player_ptr get_player(std::string uid);

extern std::map<int, user_privilege>	glb_vip_privilege;
koko_player::koko_player()
{
	is_connection_lost_ = 0;
	gold_ = 0;
	iid_ = 0;
	vip_value_ = 0;
	isbot_ = 0;
	isagent_ = 0;
	is_newbee_ = 0;
	from_devtype_ = "";
	last_game_id_ = -1;
}

void koko_player::connection_lost()
{
	{
		BEGIN_UPDATE_TABLE("user_account");
		SET_FINAL_VALUE("is_online", 0);
		WITH_END_CONDITION("where uid = '" + uid_ + "'")
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
	{
		BEGIN_INSERT_TABLE("log_user_loginout");
		SET_FIELD_VALUE("uid", uid_);
		SET_FINAL_VALUE("islogin", 0);
		EXECUTE_NOREPLACE_DELAYED("", get_delaydb());
	}
}

int koko_player::vip_level()
{
	auto conf = glb_vip_privilege;
	int vip_level = 0;

	for (auto it = conf.begin(); it != conf.end(); it++)
	{
		if (vip_value_ >= it->second.vip_value_) {
			vip_level = it->first;
		}
		else {
			break;
		}
	}

	if (vip_level > vip_limit_) {
		vip_level = vip_limit_;
	}

	return vip_level;
}