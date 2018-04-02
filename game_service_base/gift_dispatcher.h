#pragma once


enum gift_dispatcher_type
{
    gift_dispatcher_unknown = 0,    //未知的礼包分发器
    gift_dispatcher_currency,       //K豆分发器
    gift_dispatcher_diamond,        //钻石分发器
    gift_dispatcher_item,           //物品分发器
};

class IGiftDispatcher
{
public:
    virtual void Release() = 0;
    virtual bool DispatchGift(int system_id, player_ptr& player, int item_id, const std::string& data) = 0;
};

extern IGiftDispatcher* CreateDefaultGiftDispatcherById(int id);

//K豆分发器
class DefaultCurrencyGiftDispatcher : public IGiftDispatcher
{
public:
    DefaultCurrencyGiftDispatcher();
    virtual void Release();
    virtual bool DispatchGift(int system_id, player_ptr& player, int item_id, const std::string& data);
};

