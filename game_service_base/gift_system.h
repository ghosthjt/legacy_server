#pragma once

#include "schedule_task.h"

//用于描述一项礼物的模版信息
struct GiftData
{
    int         reward_id;  //奖励ID      see gift_dispatcher_type
    int         item_id;    //奖励物品ID   reward_id为gift_dispatcher_item时有效
    std::string content;    //奖励内容
};
typedef std::list<GiftData>                     GiftGroup;

class IGiftDispatcher;

enum gift_system_type
{
    gift_system_unknown = 0,        //未知系统
    gift_system_everyday_sign,      //每日签到系统
    gift_system_luck_wheel,         //幸运转盘系统
    gift_system_subsidy,            //破产补助系统
};

#pragma region 每日签到系统
class GiftSystemEverydaySign
{
public:
    GiftSystemEverydaySign();
    ~GiftSystemEverydaySign();

    bool Refresh();
    void Clear();

    const std::vector<GiftGroup>& GetConfig();

    bool IsTodaySign(player_ptr player);
    void SetTodaySign(player_ptr player);

    int GetSignDays(player_ptr player);
    void AddSignDays(player_ptr player);

    bool DispatchReward(player_ptr player, const GiftGroup& group);

protected:
    std::vector<GiftGroup>                      groups_;
    std::map<int, IGiftDispatcher*>             dispatchers_;
};
extern GiftSystemEverydaySign* CreateGiftEverydaySign();
#pragma endregion 每日签到系统

#pragma region 幸运转盘系统
class LuckWheelRecord       //幸运转盘抽奖记录
{
public:
    std::string     player_name_;
    int             reward_index_;
    std::string     time_;
};

class GiftSystemLuckWheel
{
public:
    GiftSystemLuckWheel();
    ~GiftSystemLuckWheel();

    bool Refresh();
    void Clear();

    int GetFreeCount(player_ptr player);
    void ReduceFreeCount(player_ptr player);

    int GetDelayTime();

    int GenGiftIndex();

    void AddRecord(const std::string& player_name, int reward_index, const std::string time);
    const std::list<LuckWheelRecord>& GetRecords();

    bool DispatchReward(player_ptr player, int reward_index);

public:
    int                                         everyday_free_count_;   //每日赠送的抽奖次数
    int                                         time_delay_;            //从随机开始到获得奖励的延迟，单位毫秒
    int                                         diamond_price_;         //付费抽奖消耗钻石数量
    int                                         total_weight_;          //权重总和
    std::vector<int>                            weight_refer_;          //权重参照数据
    std::vector<GiftData>                       gifts_;                 //抽奖奖励数据
    std::map<int, IGiftDispatcher*>             dispatchers_;           //奖励分发管理器
    std::list<LuckWheelRecord>                  records_;               //全局抽奖记录
};
extern GiftSystemLuckWheel* CreateGiftLuckWheel();

class GiftLuckWheelAwardTask : public task_info
{
public:
    player_ptr	pp;
    int reward_index;

    GiftLuckWheelAwardTask(io_service& ios) : task_info(ios) {}
    virtual	int	routine();
};
#pragma endregion 幸运转盘系统

#pragma region 破产补助系统
class GiftSystemSubsidy
{
public:
    GiftSystemSubsidy();
    ~GiftSystemSubsidy();

    bool Refresh();
    void Clear();

    int GetFreeCount(player_ptr player);
    void ReduceFreeCount(player_ptr player);
    int GetObtainCount(player_ptr player);
    bool CanObtainReward(player_ptr player);

    int GetTotalCount();
    int GetDelayTime();
    int GenRewardIndex();
    const std::vector<__int64>& GetRewards();

    bool DispatchReward(player_ptr player, int reward_index);

public:
    int                     total_count_;       //每日允许的破产补助次数
    __int64                max_currency_;      //最大拥有K豆数量，补助条件一
    __int64                min_cost_ratio_;    //最小游戏输掉K豆系数，补助条件二
    int                     time_delay_;        //从随机开始到获得奖励的延迟，单位毫秒
    std::vector<__int64>   rewards_;           //随机奖励K豆数量
    IGiftDispatcher*        dispatcher_;        //奖励分发器
};
extern GiftSystemSubsidy* CreateGiftSubsidy();

class GiftSubsidyAwardTask : public task_info
{
public:
    player_ptr	pp;
    int reward_index;

    GiftSubsidyAwardTask(io_service& ios) : task_info(ios) {}
    virtual	int	routine();
};
#pragma endregion 破产补助系统
