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

//���ݿ���log_send_gold_detail��reason�ֶε�����
//�ñ����ڼ�¼�������K�����
enum log_send_gold_detail_type
{
    log_send_gold_detail_level = 1,     //�ȼ�����
    log_send_gold_detail_online,        //���߽���
    log_send_gold_detail_everyday,      //ÿ�յ�½����
    log_send_gold_detail_sign,          //ǩ������
    log_send_gold_detail_luck_wheel,    //����ת�̽���
    log_send_gold_detail_subsidy,       //�Ʋ���������
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

