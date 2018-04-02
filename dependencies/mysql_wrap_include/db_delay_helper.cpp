#include "db_delay_helper.h"
#include "i_log_system.h"

bool db_delay_helper::pop_and_execute()
{
	std::unordered_multimap<std::string, delay_info> copy_l;
	{
		write_lock l(val_lst_mutex_);
		copy_l = sql_exe_lst_;
		sql_exe_lst_.clear();
	}
	if (copy_l.size() > 10000) {
		i_log_system::get_instance()->write_log(loglv_warning, "db is busy, the pending sql execution is %d.", copy_l.size());
	}
	
	Database::OPENDB* db_cnn = db_->grabdb();
	if (!db_cnn){
		i_log_system::get_instance()->write_log(loglv_error, "db is gone.");
		return false;
	}
	auto it_copy = copy_l.begin();
	while (it_copy != copy_l.end()) {
		delay_info dt = it_copy->second; it_copy++;
		db_cnn->last_sql_ = dt.sql_content_;
		sql_query_result qr;
		qr.cb = dt.callback_;
		qr.result_ = false;

		if (db_cnn && !dt.sql_content_.empty()) {
			int ret = mysql_query(&db_cnn->mysql, dt.sql_content_.c_str());
			if (ret != 0) {
				i_log_system::get_instance()->write_log(loglv_error, "sql execute err : %s \r\n, SQL = %s",
					mysql_error(&db_cnn->mysql),
					dt.sql_content_.c_str());
			}
			else {
				qr.result_ = true;
				if (dt.isqurey_) {
					fetch_result(&dt, &db_cnn->mysql, qr.results);
				}
				else {
					int st = -1;
					do {
						auto r = mysql_store_result(&db_cnn->mysql);
						if (r) {
							mysql_free_result(r);
						}
						st = mysql_next_result(&db_cnn->mysql);
					} while (st == 0);
				}
			}
		}
		else {
			i_log_system::get_instance()->write_log(loglv_error, "db_delay_helper::pop_and_execute() db_->grabdb() == null.");
		}

		results_.push_back(qr);
	}
	return !copy_l.empty();
}

void db_delay_helper::poll()
{
	sql_query_result qr;
	while (results_.pop_front(qr)) {
		if (qr.cb){
			qr.cb(qr.result_, qr.results);
		}
		qr.results.clear();
	}
}

void db_delay_helper::get_result(string sql, std::function<void(bool result, const std::vector<result_set_ptr>&)> cb)
{
	delay_info e;
	e.isqurey_ = true;
	e.sql_content_ = sql;
	e.callback_ = cb;

	write_lock l(val_lst_mutex_);
	sql_exe_lst_.insert(std::make_pair(sql, e));
}

void db_delay_helper::push_sql_exe(std::string k, std::string o, string sql, bool replace_previous)
{
	//如果(k+o)是空的,则不会替换
	if (replace_previous) {
		write_lock l(val_lst_mutex_);
		auto it = sql_exe_lst_.find(k + o);
		if (it != sql_exe_lst_.end()) {
			sql_exe_lst_.erase(it);
		}
	}

	delay_info e;
	e.id_ = k;
	e.isqurey_ = false;
	e.op_code_ = o;
	e.sql_content_ = sql;
	{
		write_lock l(val_lst_mutex_);
		if (replace_previous){
			sql_exe_lst_.insert(std::make_pair(k + o, e));
		}
		//避免在有相同key的情况下,不替换的操作被替换操作移除,加一个后辍以区分
		else {
			sql_exe_lst_.insert(std::make_pair(k + o + "r", e));
		}
	}
}


void db_delay_helper::set_db_ptr(db_ptr db)
{
	db_ = db;
}

void db_delay_helper::fetch_fields(MYSQL_RES * result, sql_query_result_set* ret)
{
	int i = 0;
	MYSQL_FIELD* pf = mysql_fetch_field(result);
	while (pf)
	{
		ret->fields_[pf->name] = i++;
		pf = mysql_fetch_field(result);
	}
}

void db_delay_helper::fetch_rows(MYSQL_RES * result, sql_query_result_set* ret)
{
	MYSQL_ROW row = mysql_fetch_row(result);
	int	irow = 0;
	while (row)
	{
		std::vector<std::string> v;
		for (unsigned i = 0; i < ret->fields_.size(); i++)
		{
			if (row[i]){
				v.push_back(row[i]);
			}
			else {
				v.push_back("");
			}
		}
		ret->rows_.push_back(v);
		row = mysql_fetch_row(result);
		irow++;
	}
}

void db_delay_helper::fetch_result(delay_info* dt, MYSQL *mysql, std::vector<result_set_ptr>& results)
{
	int st = -1;
	do {
		MYSQL_RES * result = mysql_store_result(mysql);
		if (result) {
			sql_query_result_set* ret = new sql_query_result_set();
			fetch_fields(result, ret);
			fetch_rows(result, ret);
			results.push_back(result_set_ptr(ret));
			mysql_free_result(result);
		}
		st = mysql_next_result(mysql);
	} while (st == 0);
}

query_result_reader::query_result_reader(result_set_ptr r)
{
	result_ = r;
	currow_ = -1;
	curcol_ = 0;
}

__int64 query_result_reader::getval()
{
	auto& v = result_->rows_[currow_];
	if (curcol_ >= v.size()) {
		return 0;
	}
	return  s2i<__int64>(result_->rows_[currow_][curcol_++]);
}

__int64 query_result_reader::getbigint()
{
	return getval();
}

const char* query_result_reader::getstr()
{
	auto& v = result_->rows_[currow_];
	if (curcol_ >= v.size()) {
		return "";
	}
	return result_->rows_[currow_][curcol_++].c_str();
}

double query_result_reader::getdouble()
{
	auto& v = result_->rows_[currow_];
	if (curcol_ >= v.size()) {
		return 0.0;
	}
	return  s2i<double>(v[curcol_++]);
}

bool query_result_reader::fetch_row()
{
	if (result_->rows_.empty()) {
		return false;
	}

	currow_++;
	curcol_ = 0;

	if (currow_ >= result_->rows_.size()) {
		return false;
	}
	return true;
}
