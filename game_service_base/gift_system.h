#pragma once

#include "schedule_task.h"

//��������һ�������ģ����Ϣ
struct GiftData
{
    int         reward_id;  //����ID      see gift_dispatcher_type
    int         item_id;    //������ƷID   reward_idΪgift_dispatcher_itemʱ��Ч
    std::string content;    //��������
};
typedef std::list<GiftData>                     GiftGroup;

class IGiftDispatcher;

enum gift_system_type
{
    gift_system_unknown = 0,        //δ֪ϵͳ
    gift_system_everyday_sign,      //ÿ��ǩ��ϵͳ
    gift_system_luck_wheel,         //����ת��ϵͳ
    gift_system_subsidy,            //�Ʋ�����ϵͳ
};

#pragma region ÿ��ǩ��ϵͳ
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
#pragma endregion ÿ��ǩ��ϵͳ

#pragma region ����ת��ϵͳ
class LuckWheelRecord       //����ת�̳齱��¼
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
    int                                         everyday_free_count_;   //ÿ�����͵ĳ齱����
    int                                         time_delay_;            //�������ʼ����ý������ӳ٣���λ����
    int                                         diamond_price_;         //���ѳ齱������ʯ����
    int                                         total_weight_;          //Ȩ���ܺ�
    std::vector<int>                            weight_refer_;          //Ȩ�ز�������
    std::vector<GiftData>                       gifts_;                 //�齱��������
    std::map<int, IGiftDispatcher*>             dispatchers_;           //�����ַ�������
    std::list<LuckWheelRecord>                  records_;               //ȫ�ֳ齱��¼
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
#pragma endregion ����ת��ϵͳ

#pragma region �Ʋ�����ϵͳ
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
    int                     total_count_;       //ÿ��������Ʋ���������
    __int64                max_currency_;      //���ӵ��K����������������һ
    __int64                min_cost_ratio_;    //��С��Ϸ���K��ϵ��������������
    int                     time_delay_;        //�������ʼ����ý������ӳ٣���λ����
    std::vector<__int64>   rewards_;           //�������K������
    IGiftDispatcher*        dispatcher_;        //�����ַ���
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
#pragma endregion �Ʋ�����ϵͳ
