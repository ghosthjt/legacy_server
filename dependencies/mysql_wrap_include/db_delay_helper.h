#pragma once
#include "Database.h"
#include "boost/shared_ptr.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <safe_list.h>

class Database;
typedef boost::shared_ptr<Database>	db_ptr;
using namespace std;

struct sql_query_result_set;
typedef boost::shared_ptr<sql_query_result_set>	result_set_ptr;

struct delay_info
{
	bool			isqurey_;
	std::string		id_;
	std::string		op_code_;
	std::string		sql_content_;
	std::function<void(bool result, const std::vector<result_set_ptr>&)> callback_;
};

struct sql_query_result_set 
{
	std::map<std::string, int> fields_;
	std::vector<std::vector<std::string>> rows_;
};

struct sql_query_result
{
	int		result_;
	std::function<void(bool result, const std::vector<result_set_ptr>&)> cb;
	std::vector<result_set_ptr> results;
};

class query_result_reader
{
public:
	query_result_reader(result_set_ptr r);

	__int64		getval();
	__int64		getbigint();
	const char*	getstr();
	double		getdouble();
	bool		fetch_row();

private:
	result_set_ptr result_;
	int		currow_;
	int		curcol_;
};

struct db_delay_helper
{
	typedef boost::lock_guard<boost::mutex> write_lock;
	bool			pop_and_execute();

	void			poll();

	void			get_result(string sql, std::function<void(bool result, const std::vector<result_set_ptr>&)> cb);
	void			push_sql_exe(std::string k, std::string o, string sql, bool replace_previous);

	void			set_db_ptr(db_ptr db);
	db_ptr			get_db_ptr() {return db_;}
private:
	std::unordered_multimap<std::string, delay_info>	sql_exe_lst_;
	boost::mutex	val_lst_mutex_;
	safe_list<sql_query_result>	results_;
	db_ptr db_;
	void			fetch_result(delay_info* dt, MYSQL *mysql, std::vector<result_set_ptr>& results);
	void			fetch_fields(MYSQL_RES * result, sql_query_result_set* ret);
	void			fetch_rows(MYSQL_RES * result, sql_query_result_set* ret);

};
