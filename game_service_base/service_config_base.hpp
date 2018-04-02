
void service_config_base::refresh()
{
	Query q(*the_service.gamedb_);
	std::string sql = "SELECT init_stock, force_set, lowcap, lowcap_max, upcap, upcap_max, decay_per_hour FROM server_parameters_stock_control";
	q.get_result(sql);
	int forece_set = 0;
	if (q.fetch_row()){
		longlong stock = q.getbigint();
		forece_set = q.getval();
		the_service.the_new_config_.stock_lowcap_ = q.getbigint();
		the_service.the_new_config_.stock_lowcap_max_ = q.getbigint();
		the_service.the_new_config_.stock_upcap_start_ = q.getbigint();
		the_service.the_new_config_.stock_upcap_max_ = q.getbigint();
		the_service.the_new_config_.stock_decay_ = q.getbigint();
		if (forece_set){
			the_service.add_stock(stock);
		}
		warden_config conf;
		conf.stock_lowcap_ = the_service.the_new_config_.stock_lowcap_;
		conf.stock_lowcap_max_ = the_service.the_new_config_.stock_lowcap_max_;
		conf.stock_upcap_max_ = the_service.the_new_config_.stock_upcap_max_;
		conf.stock_upcap_start_ = the_service.the_new_config_.stock_upcap_start_;
		conf.stock_decay_ = the_service.the_new_config_.stock_decay_;
		the_service.warden_.config(conf);
	}
	q.free_result();

	if (forece_set) {
		sql = "update server_parameters_stock_control set force_set = 0";
		q.execute(sql);
	}
	q.free_result();


	//更新玩家库存
	sql = "select rowid, uid, rate from server_parameters_blacklist where `state` = 0 order by rowid";
	q.get_result(sql);
	std::string rowid;
	while (q.fetch_row())
	{
		rowid = q.getstr();
		std::string uid = q.getstr();
		__int64 rate = q.getbigint();
		the_service.warden_.add_user_balance(uid, rate);
	}
	q.free_result();

	if (!rowid.empty()){
		sql = "update server_parameters_blacklist set `state` = 1  where rowid <=" + rowid;
		q.execute(sql);
	}

    //更新全局配置表
    global_config_.clear();
    sql = "select `key`,`value` from `server_parameters_global`;";
    q.get_result(sql);
    while (q.fetch_row())
    {
        const char* strkey = q.getstr();
        const char* strval = q.getstr();
        global_config_[strkey] = strval;
    }

    //更新礼包系统配置
    the_service.refresh_gift_system();
}