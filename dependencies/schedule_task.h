#pragma once
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>
#include "boost/enable_shared_from_this.hpp"
#include "boost/bind.hpp"
#include "boost/asio/placeholders.hpp"
#include "boost/date_time/posix_time/conversion.hpp"
#include <unordered_map>

using namespace boost::posix_time;
using namespace boost::gregorian;

struct date_info
{
	int			d,	h,	m,	s;
	date_info()
	{
		d = h = m = s = 0;
	}
};

class task_info;
typedef boost::weak_ptr<task_info> task_wptr;
class task_info : public boost::enable_shared_from_this<task_info>
{
public:
	enum{
		routine_ret_continue,
		routine_ret_break,
	};
	enum 
	{
		tt_delay,
		tt_specific_day,
		tt_specific_weekday,
	};

	static	std::unordered_map<unsigned int, task_wptr>	 glb_running_task_;
	boost::asio::deadline_timer timer_;
	task_info(boost::asio::io_service& ios):timer_(ios)
	{
		static unsigned int idgen = 1;
		delay_ = 0;
		iid_ = idgen++;
		time_type_ = tt_delay;
	}

	virtual ~task_info()
	{
		cancel();
	}

	void		schedule(unsigned int delay)
	{
		time_type_ = tt_delay;
		delay_ = delay;
		timer_.expires_from_now(boost::posix_time::milliseconds(delay_));
		timer_.async_wait(boost::bind(&task_info::handler, shared_from_this(), boost::asio::placeholders::error));
		glb_running_task_.insert(std::make_pair(iid_, shared_from_this()));
	}
	void		schedule_at_next_days(date_info& d)
	{
		time_type_ = tt_specific_day;
		dat_ = d;
		date dat = boost::date_time::day_clock<date>::local_day();
		ptime pt(dat, time_duration(dat_.h - 8, dat_.m, dat_.s)); //中国时区+8
		pt += days(dat_.d);
		timer_.expires_at(pt);
		timer_.async_wait(boost::bind(&task_info::handler, shared_from_this(), boost::asio::placeholders::error));
		glb_running_task_.insert(std::make_pair(iid_, shared_from_this()));
	}

	void		schedule_weekdays(date_info& d)
	{
		time_type_ = tt_specific_weekday;
		dat_ = d;
		
		boost::gregorian::date today = boost::date_time::day_clock<date>::local_day();
		boost::gregorian::day_iterator it(today);
		boost::gregorian::date next_wkday = boost::date_time::next_weekday(*it, boost::gregorian::greg_weekday(dat_.d));
		
		//如果今天正好是计划的周天,则看计划的时间有没有到
		if (today == next_wkday){
			boost::posix_time::ptime p1 = boost::posix_time::second_clock::local_time();
			boost::posix_time::ptime p2(next_wkday, time_duration(dat_.h, dat_.m, dat_.s));
			int msec = (p2 - p1).total_seconds();
			//如果离执行计划的时间<60秒,则计入下一天.
			if (msec < 60){
				next_wkday += days(7);
			}
		}

		ptime pt(next_wkday, time_duration(dat_.h - 8, dat_.m, dat_.s)); //中国时区+8
		timer_.expires_at(pt);
		timer_.async_wait(boost::bind(&task_info::handler, shared_from_this(), boost::asio::placeholders::error));
		glb_running_task_.insert(std::make_pair(iid_, shared_from_this()));
	}

	unsigned int	time_left()
	{
		auto left = timer_.expires_from_now();
		return left.total_seconds();
	}

	void		cancel()
	{
		boost::system::error_code ec;
		timer_.cancel(ec);
		auto it = glb_running_task_.find(iid_);
		if (it != glb_running_task_.end()){
			glb_running_task_.erase(it);
		}
	}

protected:
	int			time_type_;
	int			delay_;
	date_info	dat_;

	void		handler(boost::system::error_code ec)
	{
		if (ec.value() == 0){
			if(routine() == routine_ret_break){
				cancel();
			}
			else{
				if (time_type_ == tt_delay){
					schedule(delay_);
				}
				else if (time_type_ == tt_specific_weekday){
					schedule_weekdays(dat_);
				}
				else{
					schedule_at_next_days(dat_);
				}
			}
		}
		else{
			cancel();
		}
	}

	virtual int routine()
	{
		return routine_ret_break;
	}

private:
	unsigned	int iid_;
};

typedef boost::shared_ptr<task_info> task_ptr;
