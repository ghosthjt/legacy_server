#include "boost/asio.hpp"
#include "boost/date_time.hpp"
#include "boost/atomic.hpp"

#include <unordered_map>
#include "schedule_task.h"
#include "simple_xml_parse.h"
#include <set>

#include "utf8_db.h"
#include "Query.h"
#include "db_delay_helper.h"
#include "utility.h"

#include "net_service.h"
#include "boost/atomic.hpp"
#include "msg_cache.h"
#include "log.h"
#include "memcache_defines.h"
#include <simple_xml_parse.cpp>
#include "db_delay_helper.cpp"
#include "i_log_system.cpp"
#include <DbgHelp.h>
#include "net_socket_basic.cpp"

#pragma auto_inline (off)
#pragma comment(lib, "dbghelp.lib")

#define SLAYVE_KEY "$"

/************************************************************************/
/*                                                                      */
/************************************************************************/

#define READ_XML_VALUE(p, n)\
	n = doc.get_value(p#n, n)

#define WRITE_XML_VALUE(p, n)\
	doc.add_node(p, #n, n)

using namespace nsp_simple_xml;


const int			db_hash_count = 1; //目前只能是1，不可多配
boost::shared_ptr<utf8_data_base>	db_;
db_delay_helper	db_delay_helper_[db_hash_count];
boost::shared_ptr<boost::asio::io_service> timer_srv;
const __int64 IID_GEN_START = 20000000;
task_ptr task;
std::vector<boost::shared_ptr<boost::thread>> cache_persistent_threads;

std::set<std::string>		failed_key;
boost::mutex		failed_key_mut;

struct table;
struct table_row;
extern void	insert_row(table& tb, const std::string& key, table_row& row);

struct none_logic {};
class mem_cache_socket : public remote_socket_impl<mem_cache_socket, none_logic>
{
public:
	mem_cache_socket(net_server<mem_cache_socket>& srv) :
		remote_socket_impl<mem_cache_socket, none_logic>(srv)
	{
	}
	void close(bool passive = false)
	{
		remote_socket_impl<mem_cache_socket, none_logic>::close(passive);
	}
};

std::unordered_map<unsigned int, task_wptr> task_info::glb_running_task_;

typedef boost::shared_ptr<mem_cache_socket> remote_socket_ptr;
boost::shared_ptr<net_server<mem_cache_socket>> the_net;

class service_config
{
public:
	std::string					accdb_host_;
	std::string					accdb_user_;
	std::string					accdb_pwd_;
	short								accdb_port_;
	std::string					accdb_name_;

	unsigned short			client_port_;
	unsigned short			sync_local_port_;	//同步本地侦听端口

	std::string					sync_remote_ip_;		//同步ip
	unsigned short			sync_remote_port_;	//同步本地侦听端口
	int									is_slave_;	//是否为备用cache
	service_config()
	{
		restore_default();
	}
	virtual ~service_config()
	{

	}
	virtual int				save_default()
	{
		return 0;
	}

	void			restore_default()
	{
		accdb_host_ = "192.168.17.230";
		accdb_user_ = "root";
		accdb_pwd_ = "123456";
		accdb_port_ = 3306;
		accdb_name_ = "account_server";
		client_port_ = 9999;
		sync_local_port_ = 8000;
		is_slave_ = 0;
		sync_remote_ip_ = "192.168.17.238";
		sync_remote_port_ = 8000;
	}

	int				load_from_file()
	{
		xml_doc doc;
		if (!doc.parse_from_file("memcache.xml")) {
			restore_default();
			save_default();
		}
		else {
			READ_XML_VALUE("config/", client_port_);
			READ_XML_VALUE("config/", accdb_port_);
			READ_XML_VALUE("config/", accdb_host_);
			READ_XML_VALUE("config/", accdb_user_);
			READ_XML_VALUE("config/", accdb_pwd_);
			READ_XML_VALUE("config/", accdb_name_);
			READ_XML_VALUE("config/", sync_local_port_);
			READ_XML_VALUE("config/", is_slave_);
			READ_XML_VALUE("config/", sync_remote_ip_);
			READ_XML_VALUE("config/", sync_remote_port_);
		}
		return 0;
	}
};

service_config the_config;

boost::atomic_uint	msg_recved(0);
boost::atomic_uint	msg_handled(0);
boost::atomic_uint	unsended_data(0);

std::list<msg_base<remote_socket_ptr>::msg_ptr>	glb_msg_list_;
std::vector<std::string> g_bot_names;

struct client_query
{
	boost::shared_ptr<msg_cache_cmd<remote_socket_ptr>> msg_cmd_;
};

template<class remote_t>
int				handle_msg_cache_cmd(msg_cache_cmd<remote_t>* cmd_msg, remote_t psock, bool iscallback);

db_delay_helper& get_delaydb()
{
	return db_delay_helper_[0];
}

void sql_trim(std::string& str)
{
	for (unsigned int i = 0; i < str.size(); i++)
	{
		if (str[i] == '\''){
			str[i] = ' ';
		}
	}
}

std::string trim_to_uid(const std::string& key)
{
	auto n = key.find_last_of('_');
	if (n == std::string::npos){
		return "";
	}
	else{
		return std::string(key.begin(), key.begin() + n);
	}
}

std::string trim_to_subkey(const std::string& key)
{
	auto n = key.find_last_of('_');
	if (n == std::string::npos){
		return "";
	}
	else{
		return std::string(key.begin() + n, key.end());
	}
}

struct variant
{
	int				type_;
	union
	{
		__int64		value_;			
		char		str_value_[64];
	};

	variant()
	{
		type_ = 0;
		memset(str_value_, 0, 64);
	}

	variant(__int64 val)
	{
		type_ = 0;
		value_ = val;
	}
	variant(std::string val)
	{
		type_ = 1;
		COPY_STR(str_value_, val.c_str());
	}

	void	reset()
	{
		type_ = 0;
		memset(str_value_, 0, 64);
	}
};

//行数据
struct table_row
{
	std::unordered_map<std::string, variant> key_value;		//键-值对
	boost::posix_time::ptime		last_use_;								//最后一次使用时间
};

struct feild
{
	int			data_type;
	std::string		name_;
	std::string		name_in_table_;
	bool		auto_increase_;	//自动增长
	bool		unique_;
	feild()
	{
		auto_increase_ = false;
		unique_ = false;
		data_type = 0;
	}
};

struct table
{
	virtual		int load_row(Database& db, const std::string, client_query qr) = 0;

	void	save_row(const std::string key, const std::string& subkey, variant& val)
	{
		std::string sval;
		if (val.type_ == 1){
			sval = val.str_value_;
			sql_trim(sval);
		}
		else{
			sval = lx2s(val.value_);
		}
		std::string subk = subkey;
		if (subkey == SLAYVE_KEY)
			subk = "";
		std::string sql;
		if (val.type_ == 0){
			sql = "call save_account_info('" + key + "','" + subk + "', '" + sval + "', '');";
		}
		else{
			sql = "call save_account_info('" + key + "','" + subk + "', '0', '" + sval + "');";
		}
		get_delaydb().push_sql_exe(key + subkey, "", sql, true);
	}

	void			load_uniques(Database& db)
	{
		std::string sql = "select ";
		auto it = field_infos_.begin();
		std::string fields;
		std::vector<feild>	vf;
		while (it != field_infos_.end())
		{
			if (it->second.unique_ == 1){
				if (fields.empty()){
					fields += "`" + it->second.name_in_table_ + "`";
					vf.push_back(it->second);
				}
				else{
					fields += ", `" + it->second.name_in_table_ + "`";
					vf.push_back(it->second);
				}
			}
			it++;
		}

		if (!fields.empty()){
			sql += fields;
			sql += " from ";
			sql += "`" + table_name_ + "`";

			Query q(db);
			if (!q.get_result(sql)) return;

			int ii = 0;
			while (q.fetch_row())
			{
				for (int i = 0; i < vf.size(); i++)
				{
					if (vf[i].data_type == 0){
						unique_int_[vf[i].name_].insert(q.getbigint());
					}
					else{
						unique_str_[vf[i].name_].insert(q.getstr());
					}
				}

				if ((ii + 1) % 1000 == 0){
					std::cout<< "loading uniques...." << ii + 1 << std::endl;
				}
				ii++;
			}
		}
	}
	
	//提取出1小时之前活跃的数据
	void	init_cache(Database& db)
	{
		std::string sql = "select uid, gold from account_summary where update_time > current_timestamp() - 3600";

		Query q(db);
		if (!q.get_result(sql)) return;

		int ii = 0;
		while (q.fetch_row())
		{
			std::string uid = q.getstr();
			__int64 gold = q.getbigint();
			rows_[uid].key_value.insert(std::make_pair(KEY_CUR_GOLD, gold));
			if ((ii + 1) % 1000 == 0) {
				std::cout << "loading cache...." << ii + 1 << std::endl;
			}
			ii++;
		}
	}

	std::string		table_name_;		//数据库表名
	std::unordered_map<std::string, table_row>	rows_;
	std::unordered_map<std::string, feild> field_infos_;
	std::unordered_map<std::string, std::set<__int64>> unique_int_;
	std::unordered_map<std::string, std::set<std::string>> unique_str_;

	table()
	{

	}
};

struct main_table : public table
{
	feild					main_field_;
	int load_row(Database& db, const std::string key, client_query qr) override
	{
		std::string sql = "select ";
		auto it = field_infos_.begin();
		std::string fields;
		std::vector<feild>	vf;
		while (it != field_infos_.end())
		{
			if (fields.empty()){
				fields += "`" + it->second.name_in_table_ + "`";
				vf.push_back(it->second);
			}
			else{
				fields += ", `" + it->second.name_in_table_ + "`";
				vf.push_back(it->second);
			}
			it++;
		}

		if (fields.empty()) return -1;
		
		sql += fields;
		sql += " from ";
		sql += "`" + table_name_ + "`";
		sql += " where `" + main_field_.name_in_table_ + "`";
		sql += " = '" + key + "'";

		auto fn = [vf, this, key, qr](bool result, const std::vector<result_set_ptr>& vp) {
			if (!result || vp.empty()){	return;}
			query_result_reader q(vp[0]);
			if (q.fetch_row()) {
				table_row tr;
				for (int i = 0; i < vf.size(); i++)
				{
					if (vf[i].data_type == 0) {
						replace_map_v(tr.key_value, std::make_pair(vf[i].name_, variant(q.getbigint())));
					}
					else {
						replace_map_v(tr.key_value, std::make_pair(vf[i].name_, variant(q.getstr())));
					}
				}
				insert_row(*this, key, tr);
			}
			else {
				table_row tr;
				insert_row(*this, key, tr);
			}
			handle_msg_cache_cmd(qr.msg_cmd_.get(), qr.msg_cmd_->from_sock_, true);
		};

		db_delay_helper_[0].get_result(sql, fn);
		return 0;
	}
};

struct slave_table : public table
{
	int load_row(Database& db, const std::string key, client_query qr) override
	{
		std::string sql = "select value_type, credits, str_value from `" + table_name_ + "` where uid = '" + key +"'";

		auto fn = [this, key, qr](bool result, const std::vector<result_set_ptr>& vp) {
			if (!result || vp.empty()) { return; }
			query_result_reader q(vp[0]);
			if (q.fetch_row()) {
				table_row tr;
				int tp = q.getval();
				if (tp == 0) {
					replace_map_v(tr.key_value, std::make_pair(SLAYVE_KEY, variant(q.getbigint())));
				}
				else {
					q.getbigint();
					replace_map_v(tr.key_value, std::make_pair(SLAYVE_KEY, variant(q.getstr())));
				}
				insert_row(*this, key, tr);
			}
			else {
				table_row tr;
				insert_row(*this, key, tr);
			}
			handle_msg_cache_cmd(qr.msg_cmd_.get(), qr.msg_cmd_->from_sock_, true);
		};
		db_delay_helper_[0].get_result(sql, fn);
		return 0;
	}
};

main_table		main_table_; //主要表
slave_table		slave_table_; //缓存表
enum
{
	E_CMD_ADD			= 1,
	E_CMD_COST		,
	E_CMD_SET			,
	E_CMD_COST_ALL,
	E_CMD_BIND		,
	E_CMD_GET			,
	E_ACC_UID		,
	E_IID_UID		,
	E_GOLD_RANK	,
};

inline int	get_optype(std::string& cmd)
{
	static std::unordered_map<std::string, int> m;
	if (m.empty()){
		m.insert(std::make_pair(CMD_ADD			, E_CMD_ADD			));
		m.insert(std::make_pair(CMD_COST		, E_CMD_COST		));
		m.insert(std::make_pair(CMD_SET			, E_CMD_SET			));
		m.insert(std::make_pair(CMD_COST_ALL, E_CMD_COST_ALL));
		m.insert(std::make_pair(CMD_BIND		, E_CMD_BIND		));
		m.insert(std::make_pair(CMD_GET			, E_CMD_GET			));
		m.insert(std::make_pair(ACC_UID			, E_ACC_UID		));
		m.insert(std::make_pair(IID_UID			, E_IID_UID		));
		m.insert(std::make_pair(GOLD_RANK		, E_GOLD_RANK		));
	}
	return m[cmd];
}

bool				load_table_define()
{
	nsp_simple_xml::xml_doc doc;
	if(!doc.parse_from_file("db_define.xml"))
		return false;

	if (!doc.is_parsed())
		return false;

	nsp_simple_xml::xml_node* main_tb = doc.get_node("config/main_table");
	
	if (!main_tb) return false;
	
	main_table_.field_infos_.clear();

	main_table_.main_field_.name_in_table_ = main_tb->get_node("keyfeild")->get_attr<std::string>("dbf");
	main_table_.main_field_.data_type =  main_tb->get_node("keyfeild")->get_attr<int>("type");

	main_table_.table_name_ = main_tb->get_value<std::string>("name", "");
	nsp_simple_xml::xml_node* fls = main_tb->get_node("feilds");
	for (int i = 0 ; i < fls->child_lst.size(); i++ )
	{
		feild fd;
		std::string		cmd = fls->child_lst[i].get_value<std::string>();
		fd.name_in_table_ = fls->child_lst[i].get_attr<std::string>("dbf");
		fd.data_type =  fls->child_lst[i].get_attr<int>("type");
		fd.unique_ = fls->child_lst[i].get_attr<int>("uniq");
		fd.name_ = cmd;
		main_table_.field_infos_.insert(std::make_pair(cmd, fd));
	}
	slave_table_.table_name_ = doc.get_node("config/cache_table/name")->get_value<std::string>();
	return true;
}

extern int		query_table(table& tb, const std::string& key, const std::string& sub_key, int optype, variant& val, bool iscallback);
extern int		update_table(table& tb, const std::string& key, const std::string& sub_key, int optype, variant& val, bool iscallback);

table*	determine_table(std::string& sub_key, variant& val)
{
	auto it = main_table_.field_infos_.find(sub_key);
	//如果主表里没有sub_key字段，
	if (it == main_table_.field_infos_.end()){
		return &slave_table_;
	}
	//如果数据属于主表
	else{
		val.type_ = it->second.data_type;
		return &main_table_;
	}
}

table_row*		find_row(table& tb, const std::string& key)
{
	table_row* tr_ret = nullptr;
	auto it = tb.rows_.find(key);
	if (it == tb.rows_.end()){
		return nullptr;
	}else{
		tr_ret = &it->second;
	}

	if (tr_ret){
		tr_ret->last_use_ = boost::posix_time::microsec_clock::local_time();
	}
	return tr_ret;
}

int		check_unique(table& tb, const std::string& key, std::string& sub_key, variant& val)
{
	auto itf = tb.field_infos_.find(sub_key);
	if (itf != tb.field_infos_.end()){
		feild& fd = itf->second;
		//如果要设置的字段需要查检唯一性
		if (fd.unique_){
			//如果数字型
			if (fd.data_type == 0){
				auto it_set = tb.unique_int_.find(sub_key);
				if (it_set != tb.unique_int_.end()){
					//如果有重复值
					if(it_set->second.find(val.value_) != it_set->second.end()){
						return 2;
					}
					else{
						it_set->second.insert(val.value_);
					}
				}
				else{
					std::set<__int64> st;
					st.insert(val.value_);
					tb.unique_int_.insert(std::make_pair(sub_key, st));
				}
				return 1;
			}
			else {
				auto it_set = tb.unique_str_.find(sub_key);
				if (it_set != tb.unique_str_.end()){
					//如果有重复值
					if(it_set->second.find(val.str_value_) != it_set->second.end()){
						return 2;
					}
					else{
						it_set->second.insert(val.str_value_);
					}
				}
				else{
					std::set<std::string> st;
					st.insert(val.str_value_);
					tb.unique_str_.insert(std::make_pair(sub_key, st));
				}
				return 1;
			}
		}
		else{
			return 0;
		}
	}
	else{
		return -1;
	}
}

int		handle_bind(const std::string& complex_key, variant& val)
{
	table_row* tr = find_row(main_table_, complex_key);
	if (!tr){
		return CACHE_RET_NO_REQ_DATA;
	}
	else{
		auto itiid = tr->key_value.find(KEY_USER_IID);
		if (itiid != tr->key_value.end()){
			if(itiid->second.value_ != 0){
				return CACHE_RET_DATA_EXISTS;
			}
			else{
				if (val.value_ == 0){
					table_row* idgen = find_row(slave_table_, std::string(KEY_GENUID));
					if (idgen){
						variant& vgen = (*idgen).key_value[SLAYVE_KEY];
						if (vgen.value_ < IID_GEN_START){
							vgen.value_ = IID_GEN_START;
						}
						replace_map_v(tr->key_value, std::make_pair(KEY_USER_IID, variant(vgen.value_)));
						main_table_.save_row(complex_key, std::string(KEY_USER_IID), variant(vgen.value_));
						vgen.value_++;
						slave_table_.save_row(KEY_GENUID, std::string(SLAYVE_KEY), vgen);
					}
				}
				else{
					int r = check_unique(main_table_, complex_key, std::string(KEY_USER_IID), variant(val.value_));
					if (r == 2){
						return CACHE_RET_DATA_EXISTS;
					}
					itiid->second.value_ = val.value_;
					main_table_.save_row(complex_key, std::string(KEY_USER_IID), variant(val.value_));
				}
			}
		}
		else{
			table_row* idgen = find_row(slave_table_, std::string(KEY_GENUID));
			if (idgen){
				variant& vgen = (*idgen).key_value[SLAYVE_KEY];
				if (vgen.value_ < IID_GEN_START){
					vgen.value_ = IID_GEN_START;
				}
				replace_map_v(tr->key_value, std::make_pair(KEY_USER_IID, variant(vgen.value_)));
				main_table_.save_row(complex_key, std::string(KEY_USER_IID), variant(vgen.value_));
				vgen.value_++;
				slave_table_.save_row(KEY_GENUID, std::string(SLAYVE_KEY), vgen);
			}
		}
	}
	return 0;
}

int		handle_operation_prepare(std::string complex_key, std::string cmd, variant& val, client_query qr)
{
	int optype = get_optype(cmd);
	int ret = CACHE_RET_UNKNOWN_CMD;
	switch (optype)
	{
	case E_CMD_ADD:
	case E_CMD_COST:
	case E_CMD_SET:
	case E_CMD_COST_ALL:
	case E_CMD_GET:
	{
		std::string key = trim_to_uid(complex_key);
		std::string sub_key = trim_to_subkey(complex_key);
		table*	using_table = determine_table(sub_key, val);

		if (using_table == &main_table_) {
			using_table->load_row(*db_, key, qr);
		}
		else {
			using_table->load_row(*db_, complex_key, qr);
		}
	}
	break;
	}
	return CACHE_RET_SUCCESS;
}

int		handle_operation(std::string complex_key, std::string cmd, variant& val)
{
	int optype = get_optype(cmd);
	int ret = CACHE_RET_UNKNOWN_CMD;
	switch (optype)
	{
	case E_CMD_ADD			:
	case E_CMD_COST			:
	case E_CMD_SET			:
	case E_CMD_COST_ALL	:
		{
			std::string key = trim_to_uid(complex_key);
			std::string sub_key = trim_to_subkey(complex_key);
			table*	using_table = determine_table(sub_key, val);

			if (using_table == &main_table_){
				ret = update_table(*using_table, key, sub_key, optype, val, false);
			}
			else{
				ret = update_table(*using_table, complex_key, std::string(SLAYVE_KEY), optype, val, false);
			}
		}
		break;
	case	E_ACC_UID:
	case	E_IID_UID:
	case	E_GOLD_RANK:
		{
			ret = CACHE_RET_INPROCESS;
		}
		break;
	case  E_CMD_BIND :
		{
			ret = handle_bind(complex_key, val);
		}
		break;
	case E_CMD_GET:
		{
			std::string key = trim_to_uid(complex_key);
			std::string sub_key = trim_to_subkey(complex_key);
			table*	using_table = determine_table(sub_key, val);

			if (using_table == &main_table_){
				ret = query_table(*using_table, key, sub_key, optype, val, false);
			}
			else{
				ret = query_table(*using_table, complex_key, std::string(SLAYVE_KEY), optype, val, false);
			}
		}
		break;
	}
	return ret;
}

//cache数据1小时过期
class check_timeout : public task_info
{
public:
	std::string key;
	check_timeout(boost::asio::io_service& isv) : task_info(isv)
	{

	}
	int routine() override
	{
		auto it = main_table_.rows_.find(key);
		if (it != main_table_.rows_.end()){
			if((boost::posix_time::microsec_clock::local_time() - it->second.last_use_).total_seconds() >= 7200){
				//数据全部都落地了，才移除缓存。
				boost::lock_guard<boost::mutex> lk(failed_key_mut);
				if (failed_key.find(key) == failed_key.end()){
					main_table_.rows_.erase(it);
					return task_info::routine_ret_break;
				}
			}
		}

		it = slave_table_.rows_.find(key);
		if (it != slave_table_.rows_.end()){
			if((boost::posix_time::microsec_clock::local_time() - it->second.last_use_).total_seconds() >= 7200){
				slave_table_.rows_.erase(it);
				return task_info::routine_ret_break;
			}
		}
		return task_info::routine_ret_continue;
	}
};

inline void		insert_row(table& tb, const std::string& key, table_row& row)
{
	replace_map_v(tb.rows_, std::make_pair(key, row));
	check_timeout* ptask = new check_timeout(*timer_srv);
	task_ptr task(ptask);
	ptask->key = key;
	ptask->schedule(boost::posix_time::seconds(7200).total_milliseconds());
}

int		update_table(table& tb, 
	const std::string& key, 
	const std::string& sub_key, 
	int optype, 
	variant& val,
	bool iscallback)
{
	auto pr = find_row(tb, key);
	if (!pr) {
		if (iscallback) {
			return CACHE_RET_NO_REQ_DATA;
		}
		else
			return CACHE_RET_INPROCESS;
	}

	bool will_save = false;
	table_row& row = *pr;
	variant	val_to_set = val;
	switch (optype)
	{
	case E_CMD_ADD:
		{
			__int64 old = val.value_;
			auto it = row.key_value.find(sub_key);
			if (it != row.key_value.end()){
				it->second.value_ += val.value_;
				val.value_ = it->second.value_;
				will_save = (old != val.value_);
			}
			else{
				variant v;
				v.type_ = 0;
				v.value_ = val.value_;
				replace_map_v(row.key_value, std::make_pair(sub_key, v));
				will_save = true;
			}
		}
		break;
	case E_CMD_COST	:
		{
			auto it = row.key_value.find(sub_key);
			if (it == row.key_value.end()){
				val.value_ = 0;
			}
			else{
				variant& v = it->second;
				if (v.value_ < val.value_){
					return CACHE_RET_LESS_THAN_COST;
				}
				__int64 old = val.value_;
				v.value_ -= val.value_;
				val.value_ = v.value_;		//返回剩余值
				will_save = (old != val.value_);
			}
		}
		break;
	case E_CMD_COST_ALL:
		{
			auto it = row.key_value.find(sub_key);
			if (it == row.key_value.end()){
				val.value_ = 0;
			}
			else{
				variant& v = it->second;
				if (v.value_ < val.value_){
					return CACHE_RET_LESS_THAN_COST;
				}

				val.value_ = v.value_;
				v.value_ = 0;
				will_save = true;
			}
		}
		break;
	case E_CMD_SET:
		{
			auto it = row.key_value.find(sub_key);
			if (it != row.key_value.end()){
				if (val.type_ == 0){
					will_save = (it->second.value_ != val.value_);
				}
				else {
					will_save = true;
				}
				it->second = val;
			}
			else{
				replace_map_v(row.key_value, std::make_pair(sub_key, val));
				will_save = true;
			}
		}
		break;
	}

	if (optype == E_CMD_COST_ALL){
		variant tv = val;		tv.value_ = 0;
		if (will_save){
			tb.save_row(key, sub_key, tv);
		}
	}
	else{
		if (will_save){
			tb.save_row(key, sub_key, val);
		}
	}
	return CACHE_RET_SUCCESS;
}

int		query_table(table& tb, const std::string& key, const std::string& sub_key, int optype, variant& val, bool iscallback)
{
	switch (optype)
	{
	case E_CMD_GET	 :
		{
			table_row* pr = find_row(tb, key);
			if (!pr){
				if (iscallback){
					val.reset();
					return CACHE_RET_SUCCESS;
				}
				else {
					return CACHE_RET_INPROCESS;
				}
			}

			table_row& row = *pr;
			auto itf = row.key_value.find(sub_key);
			if (itf == row.key_value.end()){
				val.reset();
				return CACHE_RET_SUCCESS;
			}
			val = itf->second;
		}
		break;
	default:
		return CACHE_RET_UNKNOWN_CMD;
	}
	return CACHE_RET_SUCCESS;
}

int		init_from_db()
{
	load_table_define();
	main_table_.load_uniques(*db_);
	main_table_.init_cache(*db_);
	return 0;
}

template<class remote_t>
boost::shared_ptr<msg_base<remote_t>> create_msg(unsigned short cmd)
{
	typedef boost::shared_ptr<msg_base<remote_t>> msg_ptr;

	DECLARE_MSG_CREATE()
	case GET_CLSID(msg_cache_cmd):
		ret_ptr.reset(new msg_cache_cmd<remote_t>());
		break;
	case GET_CLSID(msg_cache_cmd_ret):
		ret_ptr.reset(new msg_cache_cmd_ret<remote_t>());
		break;
	case GET_CLSID(msg_cache_updator_register):
		ret_ptr.reset(new msg_cache_updator_register<remote_t>());
		break;
	case GET_CLSID(msg_cache_ping):
		ret_ptr.reset(new msg_cache_ping<remote_t>());
		break;
		END_MSG_CREATE()
			return  ret_ptr;
}

bool glb_exit = false;
bool glb_exited = false;
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	switch(CEvent)
	{
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		glb_exit = true;
	} 
	while (!glb_exited)
	{
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
	}
	return TRUE;
}

class tast_on_5sec : public task_info
{
public:
	tast_on_5sec(boost::asio::io_service& ios):task_info(ios)
	{

	}
	int		routine();
};



void	pickup_msgs(bool& busy)
{
	std::vector<remote_socket_ptr>  remotes = the_net->get_remotes();
	for (unsigned int i = 0 ;i < remotes.size(); i++)
	{
		bool got_msg = false;
		do 
		{
			remote_socket_ptr psock = remotes[i];
			got_msg = false;
			boost::shared_ptr<msg_base<remote_socket_ptr>> pmsg = boost::dynamic_pointer_cast<msg_base<remote_socket_ptr>>(
				pickup_msg_from_socket(psock, create_msg<remote_socket_ptr>, got_msg));
			unsigned int pick_count = 0;
			if (pmsg.get())
			{
				busy = true;
				pick_count++;
				if (pmsg->head_.cmd_ == GET_CLSID(msg_cache_updator_register) ||
					psock->is_authorized()){
						if (pmsg->head_.cmd_ != GET_CLSID(msg_cache_ping)){
							pmsg->from_sock_ = psock;
							glb_msg_list_.push_back(pmsg);
						}
				}
				msg_recved++;
			}
		} while (got_msg);
	}
}

int			load_bot_names()
{
	FILE* fp = NULL;
	fopen_s(&fp, "bot_names.txt", "r");
	if(!fp){
		i_log_system::get_instance()->write_log(loglv_info, "bot_names.txt does not exists!");
		return -1;
	}
	vector<char> data_read;
	char c[128];
	while (!feof(fp))
	{
		int s = fread(c, sizeof(char), 128, fp);
		for (int i = 0 ; i < s; i++)
		{
			data_read.push_back(c[i]);
		}
	}
	fclose(fp);

	split_str(data_read.data() + 3, "\n", g_bot_names, true);
	if (g_bot_names.empty()){
		i_log_system::get_instance()->write_log(loglv_info, "bot names not valid!");
		return -1; 
	}
	return 0;
}

safe_list<client_query> query_lst_;
safe_list<client_query> result_lst_;

std::unordered_map<std::string, variant> reload_lst_;

void	sql_execute_cb(std::string& key, bool exe_result)
{
	boost::lock_guard<boost::mutex> lk(failed_key_mut);
	if (exe_result){
		failed_key.erase(key);
	}
	else{
		failed_key.insert(key);
	}
}

int db_thread_func(db_delay_helper* db_delay, int tid)
{
	unsigned int idle = 0;
	time_counter ts;
	ts.restart();
	while(!glb_exit)
	{
		bool busy = db_delay->pop_and_execute();

		if (ts.elapse() > 3000 && tid == 0){
			ts.restart();
			Query q(*db_delay->get_db_ptr());
			int rows = 0;
			std::string uids;
			std::string sql = "select `rowid`, `vkey`,`val_type`, `ival`, `strval`  from account_force_set where state = 1";
			q.get_result(sql);
			while (q.fetch_row() && rows < 10)
			{
				rows++;
				int rid = q.getval();
				std::string key = q.getstr();
				variant v;
				v.type_ = q.getval();
				if (v.type_ == 0) {
					v.value_ = q.getbigint();
				}
				else {
					q.getbigint();
					COPY_STR(v.str_value_, q.getstr());
				}

				{
					boost::lock_guard<boost::mutex> lk(failed_key_mut);
					reload_lst_.insert(std::make_pair(key, v));
				}

				if (uids.empty()) {
					uids += "" + lx2s(rid) + "";
				}
				else {
					uids += "," + lx2s(rid) + "";
				}
			}
			q.free_result();

			if (!uids.empty()) {
				sql = "update account_force_set set state = 0 where rowid in(" + uids + ")";
				q.execute(sql);
			}
		}

		if (!busy){
			//为了减轻数据库负担,故意设定成500毫秒保存一次数据.
			boost::posix_time::milliseconds ms(1);
			boost::this_thread::sleep(ms);
		}
	}
	return 0;
};

template<class remote_t>
int				handle_msg_cache_cmd(msg_cache_cmd<remote_t>* cmd_msg, remote_t psock, bool iscallback)
{
	msg_cache_cmd_ret<remote_t> msg_ret;
	COPY_STR(msg_ret.key_, cmd_msg->key_);
	COPY_STR(msg_ret.sequence_, cmd_msg->sequence_);

	variant val;
	val.type_ = cmd_msg->val_type_;
	if (val.type_ == 0){
		val.value_ = (__int64)cmd_msg->value_;
	}
	else{
		COPY_STR(val.str_value_, cmd_msg->str_value_);
	}

	msg_ret.return_code_ = handle_operation(std::string(cmd_msg->key_), std::string(cmd_msg->cache_cmd),  val);
	if (msg_ret.return_code_ == CACHE_RET_SUCCESS){
		msg_ret.val_type_ = val.type_;
		if (val.type_ == 0){
			msg_ret.value_ = val.value_;
		}
		else{
			COPY_STR(msg_ret.str_value_, val.str_value_);
		}
	}
	else if(msg_ret.return_code_ == CACHE_RET_INPROCESS && !iscallback){
		client_query qu;
		qu.msg_cmd_ = boost::dynamic_pointer_cast<msg_cache_cmd<remote_socket_ptr>>(cmd_msg->shared_from_this());
		handle_operation_prepare(qu.msg_cmd_->key_,	qu.msg_cmd_->cache_cmd, val, qu);
	}

	if (cmd_msg->head_.cmd_ == GET_CLSID(msg_cache_cmd) &&
		msg_ret.return_code_ != CACHE_RET_INPROCESS){
		send_msg(psock, msg_ret);
	}
	return 0;
}

int handle_cache_msgs()
{
	auto it_cache = glb_msg_list_.begin();
	while (it_cache != glb_msg_list_.end())
	{
		auto cmd = *it_cache;	it_cache++;
		if (cmd->head_.cmd_ == GET_CLSID(msg_cache_cmd)){
			msg_cache_cmd<remote_socket_ptr> * cmd_msg = (msg_cache_cmd<remote_socket_ptr> *) cmd.get();
			int r = handle_msg_cache_cmd(cmd_msg, cmd_msg->from_sock_, false);
		}
		else if (cmd->head_.cmd_ == GET_CLSID(msg_cache_updator_register)){
			msg_cache_updator_register<remote_socket_ptr> * cmd_msg = (msg_cache_updator_register<remote_socket_ptr> *) cmd.get();
			if(_stricmp(cmd_msg->key_, "FE77AC20-5911-4CCB-8369-27449BB6D806") == 0 ){
				cmd_msg->from_sock_->set_authorized();
			}
		}
		msg_handled++;
	}
	glb_msg_list_.clear();
	return 0;
}

int		start_network()
{
	the_net->add_acceptor(the_config.client_port_);

	if(the_net->run()){
		i_log_system::get_instance()->write_log(loglv_info, "net service start success!");
	}
	else{
		i_log_system::get_instance()->write_log(loglv_info, "!!!!!net service start failed!!!!");
		return -1;
	}
	return 0;
}

void clean()
{
	task.reset();

	for (unsigned int i = 0 ; i < cache_persistent_threads.size(); i++)
	{
		cache_persistent_threads[i]->join();
	}

	//继续执行没保存完的sql语句
	for (int i = 0 ; i < db_hash_count; i++)
	{
		db_delay_helper_[i].pop_and_execute();
	}

	timer_srv->stop();
	timer_srv.reset();

	the_net->stop();
	the_net.reset();
	i_log_system::get_instance()->stop();
}


int run()
{
	i_log_system::get_instance()->start();
	timer_srv.reset(new boost::asio::io_service());
	the_net.reset(new net_server<mem_cache_socket>());
	boost::system::error_code ec;
	the_config.load_from_file();
	db_.reset(new utf8_data_base(the_config.accdb_host_, the_config.accdb_user_,the_config.accdb_pwd_, the_config.accdb_name_));
	if(!db_->grabdb()){
		i_log_system::get_instance()->write_log(loglv_error, "database start failed!!!!");
		return -1;
	}
	for (int i = 0; i < db_hash_count; i++)
	{
		db_delay_helper_[i].set_db_ptr(db_);
	}

	if (load_bot_names() != 0){
		goto _EXIT;
	}

	init_from_db();
	if (start_network() != 0){
		goto _EXIT;
	}
	
	//这里只能有一个数据库线程，不可贪多，因为会导致数据库写入乱序出问题。
	//以后可以优化成读取线程多个，写入只有一个，目前就只一个线程吧
	for (int i = 0; i < 1; i++)
	{
		boost::shared_ptr<boost::thread> td(new boost::thread(boost::bind(db_thread_func, db_delay_helper_, i)));
		cache_persistent_threads.push_back(td);
	}
	int idle_count = 0;

	tast_on_5sec* ptask = new tast_on_5sec(*timer_srv);
	task.reset(ptask);
	ptask->schedule(5000);

	while(!glb_exit)
	{
		bool busy = false;
		timer_srv->reset();
		timer_srv->poll();
		the_net->ios_.reset();
		the_net->ios_.poll();

		db_delay_helper_[0].poll();

		pickup_msgs(busy);
		handle_cache_msgs();

		{
			boost::lock_guard<boost::mutex> lk(failed_key_mut);
			for (auto it = reload_lst_.begin(); it != reload_lst_.end(); it++) 
			{
				handle_operation(it->first, string("CMD_SET"), it->second);
			}
			reload_lst_.clear();
		}

		if (!busy){
			idle_count++;
			if (idle_count > 300){
				boost::posix_time::milliseconds ms(1);
				boost::this_thread::sleep(ms);
				idle_count = 0;
			}
		}
		else{
			idle_count = 0;
		}
	}
	cout << "service will stop." << endl;
_EXIT:
	clean();
	return 0;
}

LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	HANDLE lhDumpFile = CreateFileA("crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL ,NULL);

	MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
	loExceptionInfo.ExceptionPointers = ExceptionInfo;
	loExceptionInfo.ThreadId = GetCurrentThreadId();
	loExceptionInfo.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),lhDumpFile, MiniDumpNormal, &loExceptionInfo, NULL, NULL);

	CloseHandle(lhDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main()
{
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE)
	{
		return -1;
	}
	__try{
		if (run() == 0){
			cout <<"service has been stopped gracefully, you can close the window safety!"<< endl;
		}
		else
			cout <<"exited with error!"<< endl;
	}
	__except(MyUnhandledExceptionFilter(GetExceptionInformation()),
		EXCEPTION_EXECUTE_HANDLER){
		clean();
	}
	glb_exited = true;
	::system("pause");
	return 0;
}

int tast_on_5sec::routine()
{
	i_log_system::get_instance()->write_log(loglv_info, "game is running ok, this time msg recved:%d, handled:%d, remain sending data: %d", msg_recved.load(), msg_handled.load(), unsended_data.load());
	msg_handled = 0;
	msg_recved = 0;
	unsended_data = 0;

	return task_info::routine_ret_continue;
}
