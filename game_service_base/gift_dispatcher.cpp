#include "game_config.h"
#include "gift_dispatcher.h"
#include "gift_system.h"
#include "msg_server_common.h"

IGiftDispatcher* CreateDefaultGiftDispatcherById(int id)
{
    IGiftDispatcher* r = nullptr;
    switch (id)
    {
    case gift_dispatcher_currency:
        r = new DefaultCurrencyGiftDispatcher();
        break;
    }
    return r;
}

DefaultCurrencyGiftDispatcher::DefaultCurrencyGiftDispatcher()
{
}

void DefaultCurrencyGiftDispatcher::Release()
{
    delete this;
}

//数据库中log_send_gold_detail表reason字段的类型
//该表用于记录赠送玩家K豆情况
enum log_send_gold_detail_type
{
    log_send_gold_detail_level = 1,     //等级奖励
    log_send_gold_detail_online,        //在线奖励
    log_send_gold_detail_everyday,      //每日登陆奖励
    log_send_gold_detail_sign,          //签到奖励
    log_send_gold_detail_luck_wheel,    //幸运转盘奖励
    log_send_gold_detail_subsidy,       //破产补助奖励
};
bool DefaultCurrencyGiftDispatcher::DispatchGift(int system_id, player_ptr& player, int item_id, const std::string& data)
{
    int reason = -1;
    switch (system_id)
    {
    case gift_system_everyday_sign:
        reason = log_send_gold_detail_sign;
        break;
    case gift_system_luck_wheel:
        reason = log_send_gold_detail_luck_wheel;
        break;
    case gift_system_subsidy:
        reason = log_send_gold_detail_subsidy;
        break;
    }
    if (reason == -1)
        return false;

    __int64 val = boost::lexical_cast<__int64>(data);
    __int64 out_count = 0;
    int r = the_service.cache_helper_.give_currency(player->uid_, val, out_count, true);
    if (r == ERROR_SUCCESS_0)
    {
        the_service.add_stock(-val);
        Database& db = *the_service.gamedb_;
        BEGIN_INSERT_TABLE("log_send_gold_detail");
        SET_FIELD_VALUE("reason", reason);
        SET_FIELD_VALUE("uid", player->uid_);
        SET_FINAL_VALUE("val", (int)val);
        EXECUTE_IMMEDIATE();
        return true;
    }
    return false;
}

