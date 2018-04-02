#pragma once


enum gift_dispatcher_type
{
    gift_dispatcher_unknown = 0,    //δ֪������ַ���
    gift_dispatcher_currency,       //K���ַ���
    gift_dispatcher_diamond,        //��ʯ�ַ���
    gift_dispatcher_item,           //��Ʒ�ַ���
};

class IGiftDispatcher
{
public:
    virtual void Release() = 0;
    virtual bool DispatchGift(int system_id, player_ptr& player, int item_id, const std::string& data) = 0;
};

extern IGiftDispatcher* CreateDefaultGiftDispatcherById(int id);

//K���ַ���
class DefaultCurrencyGiftDispatcher : public IGiftDispatcher
{
public:
    DefaultCurrencyGiftDispatcher();
    virtual void Release();
    virtual bool DispatchGift(int system_id, player_ptr& player, int item_id, const std::string& data);
};

