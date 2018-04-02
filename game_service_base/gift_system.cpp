#include "game_config.h"
#include "gift_system.h"
#include "gift_dispatcher.h"
#include "msg_server_common.h"

#pragma region 每日签到系统
GiftSystemEverydaySign* CreateGiftEverydaySign()
{
    return new GiftSystemEverydaySign();
}

GiftSystemEverydaySign::GiftSystemEverydaySign()
{

}

GiftSystemEverydaySign::~GiftSystemEverydaySign()
{
    Clear();
}

bool GiftSystemEverydaySign::Refresh()
{
    Clear();

    bool r = true;

    string sql;
    Query q(*the_service.gamedb_);
    sql = "SELECT `day`,`reward_id`,`item_id`,`content` FROM `server_parameters_everyday_sign`";
    if (q.get_result(sql))
    {
        while (q.fetch_row())
        {
            int day = q.getval();
            int reward_id = q.getval();
            int item_id = q.getval();
            const char* content = q.getstr();

            if (day <= 0)
            {
                r = false;
                break;
            }

            if (groups_.size() < day)
                groups_.resize(day);

            auto& group = groups_[day - 1];
            if (dispatchers_.find(reward_id) == dispatchers_.end())
            {
                auto p = the_service.create_gift_dispatcher(reward_id);
                if (!p)
                {
                    r = false;
                    break;
                }

                dispatchers_[reward_id] = p;
            }

            group.push_back(GiftData());
            GiftData& data = group.back();
            data.reward_id = reward_id;
            data.item_id = item_id;
            data.content = content;
        }
    }
    else
    {
        r = false;
    }
    q.free_result();

    if (!r)
        Clear();
    return r;
}

void GiftSystemEverydaySign::Clear()
{
    groups_.clear();
    for (auto ite : dispatchers_)
    {
        ite.second->Release();
    }
    dispatchers_.clear();
}

const std::vector<GiftGroup>& GiftSystemEverydaySign::GetConfig()
{
    return groups_;
}

bool GiftSystemEverydaySign::IsTodaySign(player_ptr player)
{
    __int64 last_get_tick = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_EVERYDAY_SIGN_TICK, the_service.the_config_.game_id);

	auto dat1 = boost::date_time::day_clock<date>::local_day();
	auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    return last_get_tick == this_day_tick;
}

void GiftSystemEverydaySign::SetTodaySign(player_ptr player)
{
	auto dat1 = boost::date_time::day_clock<date>::local_day();
	auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    the_service.cache_helper_.set_var(player->uid_ + KEY_EVERYDAY_SIGN_TICK, the_service.the_config_.game_id, this_day_tick);
}

int GiftSystemEverydaySign::GetSignDays(player_ptr player)
{
    return the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_EVERYDAY_SIGN_DAYS, the_service.the_config_.game_id);
}

void GiftSystemEverydaySign::AddSignDays(player_ptr player)
{
    the_service.cache_helper_.add_var(player->uid_ + KEY_EVERYDAY_SIGN_DAYS, the_service.the_config_.game_id, __int64(1));
}

bool GiftSystemEverydaySign::DispatchReward(player_ptr player, const GiftGroup& group)
{
    for (auto ite : group)
    {
        auto ite2 = dispatchers_.find(ite.reward_id);
        if (ite2 == dispatchers_.end())
            return false;

        auto dispatcher = ite2->second;
        if (!dispatcher->DispatchGift(gift_system_everyday_sign, player, ite.item_id, ite.content))
            return false;
    }

    return true;
}
#pragma endregion 每日签到系统

#pragma region 幸运转盘系统
GiftSystemLuckWheel* CreateGiftLuckWheel()
{
    return new GiftSystemLuckWheel();
}

GiftSystemLuckWheel::GiftSystemLuckWheel()
{
}

GiftSystemLuckWheel::~GiftSystemLuckWheel()
{
    Clear();
}

bool GiftSystemLuckWheel::Refresh()
{
    Clear();

    everyday_free_count_ = the_service.the_config_.get_global_config<int>("gs_luckwheel_free_count", 1);
    time_delay_ = the_service.the_config_.get_global_config<int>("gs_luckwheel_time_delay", 4000);
    diamond_price_ = the_service.the_config_.get_global_config<int>("gs_luckwheel_diamond_price", 1);

    bool r = true;

    string sql;
    Query q(*the_service.gamedb_);
    sql = "SELECT `id`,`reward_id`,`item_id`,`content`,`weight` FROM `server_parameters_luck_wheel`";
    if (q.get_result(sql))
    {
        total_weight_ = 0;
        while (q.fetch_row())
        {
            int id = q.getval();
            int reward_id = q.getval();
            int item_id = q.getval();
            const char* content = q.getstr();
            int weight = q.getval();

            if (gifts_.size() < id)
                gifts_.resize(id);

            if (dispatchers_.find(reward_id) == dispatchers_.end())
            {
                auto p = the_service.create_gift_dispatcher(reward_id);
                if (!p)
                {
                    r = false;
                    break;
                }

                dispatchers_[reward_id] = p;
            }

            auto& gift = gifts_[id - 1];
            gift.reward_id = reward_id;
            gift.item_id = item_id;
            gift.content = content;

            total_weight_ += weight;
            weight_refer_.push_back(total_weight_);
        }
    }
    else
    {
        r = false;
    }
    q.free_result();

    if (!r)
        Clear();
    return r;
}

void GiftSystemLuckWheel::Clear()
{
    everyday_free_count_ = 0;
    time_delay_ = 0;
    diamond_price_ = 0;
    total_weight_ = 0;
    weight_refer_.clear();
    gifts_.clear();
    for (auto ite : dispatchers_)
    {
        ite.second->Release();
    }
    dispatchers_.clear();
    //records_.clear();
}

int GiftSystemLuckWheel::GetFreeCount(player_ptr player)
{
    __int64 used_count = 0;

    auto dat1 = boost::date_time::day_clock<date>::local_day();
    auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    __int64 tick = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_LUCK_WHEEL_TICK, the_service.the_config_.game_id);
    if (this_day_tick == tick)
    {
        used_count = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_LUCK_WHEEL_USED, the_service.the_config_.game_id);
    }

    return everyday_free_count_ - used_count;
}

void GiftSystemLuckWheel::ReduceFreeCount(player_ptr player)
{
	auto dat1 = boost::date_time::day_clock<date>::local_day();
	auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    __int64 tick = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_LUCK_WHEEL_TICK, the_service.the_config_.game_id);
    if (tick != this_day_tick)
    {
        the_service.cache_helper_.set_var(player->uid_ + KEY_LUCK_WHEEL_TICK, the_service.the_config_.game_id, this_day_tick);
        the_service.cache_helper_.set_var(player->uid_ + KEY_LUCK_WHEEL_USED, the_service.the_config_.game_id, __int64(1));
    }
    else
    {
        the_service.cache_helper_.add_var(player->uid_ + KEY_LUCK_WHEEL_USED, the_service.the_config_.game_id, __int64(1));
    }
}

int GiftSystemLuckWheel::GetDelayTime()
{
    return time_delay_;
}

int GiftSystemLuckWheel::GenGiftIndex()
{
    int idx = -1;
    int n = rand_r(0, total_weight_ - 1);
    for (int i = 0; i < weight_refer_.size(); ++i)
    {
        if (n < weight_refer_[i])
        {
            idx = i;
            break;
        }
    }

    return idx;
}

void GiftSystemLuckWheel::AddRecord(const std::string& player_name, int reward_index, const std::string time)
{
    records_.push_back(LuckWheelRecord());
    auto& r = records_.back();
    r.player_name_ = player_name;
    r.reward_index_ = reward_index;
    r.time_ = time;

    if (records_.size() > 10)
    {
        records_.pop_front();
    }
}

const std::list<LuckWheelRecord>& GiftSystemLuckWheel::GetRecords()
{
    return records_;
}

bool GiftSystemLuckWheel::DispatchReward(player_ptr player, int reward_index)
{
    if (reward_index < 0 || reward_index >= gifts_.size())
        return false;

    auto& gift = gifts_[reward_index];
    auto ite = dispatchers_.find(gift.reward_id);
    if (ite == dispatchers_.end())
        return false;

    if (!(*ite).second->DispatchGift(gift_system_luck_wheel, player, gift.item_id, gift.content))
        return false;

    return true;
}

int GiftLuckWheelAwardTask::routine()
{
    auto glw = the_service.gift_luck_wheel_;
    glw->DispatchReward(pp, reward_index);

    std::string cur_time = boost::posix_time::to_iso_string(
        boost::posix_time::second_clock::local_time());
    glw->AddRecord(pp->name_, reward_index, cur_time);

    msg_luck_wheel_info msg;
    msg.count_ = glw->GetFreeCount(pp);
    msg.reward_index_ = reward_index;
    msg.show_reward_ = 1;
    pp->send_msg_to_client(msg);

    return routine_ret_break;
}
#pragma endregion 幸运转盘系统

#pragma region 破产补助系统
GiftSystemSubsidy* CreateGiftSubsidy()
{
    return new GiftSystemSubsidy();
}

GiftSystemSubsidy::GiftSystemSubsidy()
{
    dispatcher_ = nullptr;
}

GiftSystemSubsidy::~GiftSystemSubsidy()
{
    Clear();
}

bool GiftSystemSubsidy::Refresh()
{
    Clear();

    dispatcher_ = the_service.create_gift_dispatcher(gift_dispatcher_currency);
    if (!dispatcher_)
        return false;

    total_count_ = the_service.the_config_.get_global_config<int>("gs_subsidy_count", 3);
    max_currency_ = the_service.the_config_.get_global_config<__int64>("gs_subsidy_max_gold", 1000);
    min_cost_ratio_ = the_service.the_config_.get_global_config<__int64>("gs_subsidy_min_cost_ratio", 500);
    time_delay_ = the_service.the_config_.get_global_config<int>("gs_subsidy_time_delay", 4000);
    std::string str_rewards = the_service.the_config_.get_global_config<std::string>("gs_subsidy_rewards", "2000,3000,4000");
    split_str(str_rewards, ",", rewards_, false);

    return true;
}

void GiftSystemSubsidy::Clear()
{
    total_count_ = 0;
    max_currency_ = 0;
    min_cost_ratio_ = 0;
    time_delay_ = 0;
    rewards_.clear();
    if (dispatcher_)
    {
        dispatcher_->Release();
        dispatcher_ = nullptr;
    }
}

int GiftSystemSubsidy::GetFreeCount(player_ptr player)
{
    __int64 used_count = 0;

    auto dat1 = boost::date_time::day_clock<date>::local_day();
    auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    __int64 tick = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_SUBSIDY_TICK, the_service.the_config_.game_id);
    if (this_day_tick == tick)
    {
        used_count = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_SUBSIDY_USED, the_service.the_config_.game_id);
    }

    return total_count_ - used_count;
}

void GiftSystemSubsidy::ReduceFreeCount(player_ptr player)
{
    auto dat1 = boost::date_time::day_clock<date>::local_day();
    auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    __int64 tick = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_SUBSIDY_TICK, the_service.the_config_.game_id);
    if (tick != this_day_tick)
    {
        the_service.cache_helper_.set_var(player->uid_ + KEY_SUBSIDY_TICK, the_service.the_config_.game_id, this_day_tick);
        the_service.cache_helper_.set_var(player->uid_ + KEY_SUBSIDY_USED, the_service.the_config_.game_id, __int64(1));
    }
    else
    {
        the_service.cache_helper_.add_var(player->uid_ + KEY_SUBSIDY_USED, the_service.the_config_.game_id, __int64(1));
    }
}

int GiftSystemSubsidy::GetObtainCount(player_ptr player)
{
	auto dat1 = boost::date_time::day_clock<date>::local_day();
	auto dat2 = from_string("1900-1-1");
    __int64 this_day_tick = (dat1 - dat2).days();

    __int64 tick = the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_SUBSIDY_TICK, the_service.the_config_.game_id);
    if (this_day_tick != tick)
        return 0;

    return the_service.cache_helper_.get_var<__int64>(player->uid_ + KEY_SUBSIDY_USED, the_service.the_config_.game_id);
}

bool GiftSystemSubsidy::CanObtainReward(player_ptr player)
{
    __int64 num = the_service.cache_helper_.get_currency(player->uid_);
    if (num > max_currency_)
        return false;

    int obtain_count = GetObtainCount(player);
    __int64 parameter = (obtain_count + 1) * min_cost_ratio_;
    if (the_service.get_player_today_win(player) > -parameter)
        return false;

    return true;
}

int GiftSystemSubsidy::GetTotalCount()
{
    return total_count_;
}

int GiftSystemSubsidy::GetDelayTime()
{
    return time_delay_;
}

int GiftSystemSubsidy::GenRewardIndex()
{
    return rand_r(0, rewards_.size() - 1);
}

const std::vector<__int64>& GiftSystemSubsidy::GetRewards()
{
    return rewards_;
}

bool GiftSystemSubsidy::DispatchReward(player_ptr player, int reward_index)
{
    if (reward_index < 0 || reward_index >= rewards_.size())
        return false;

    if (!dispatcher_)
        return false;

    if (!dispatcher_->DispatchGift(gift_system_subsidy, player, 0, boost::lexical_cast<std::string>(rewards_[reward_index])))
        return false;

    return true;
}

int	GiftSubsidyAwardTask::routine()
{
    auto gs = the_service.gift_subsidy_;
    gs->DispatchReward(pp, reward_index);

    auto& rewards = gs->GetRewards();
    if (reward_index >= 0 && reward_index < rewards.size())
    {
        msg_subsidy_result msg;
        msg.reward_num_ = rewards[reward_index];
        pp->send_msg_to_client(msg);
    }

    return routine_ret_break;
}
#pragma endregion 破产补助系统
