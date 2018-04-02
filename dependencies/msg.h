#pragma once
#include "boost/smart_ptr.hpp"
#include "boost/property_tree/ptree.hpp"
#include <vector>
#include "json_helper.h"
#include "utility.h"
#include "sending_buffer.h"

#define GET_CLSID(m) m##_id

#define DECLARE_MSG_CREATE()\
	msg_ptr ret_ptr;\
	switch (cmd)\
{

#define END_MSG_CREATE()\
}\

#define REGISTER_CLS_CREATE(cls_)\
	case GET_CLSID(cls_):\
	ret_ptr.reset(new cls_);\
	break

#define MSG_CONSTRUCT(msg)\
	msg()\
{\
	head_.cmd_ = msg##_id;\
	init();\
}

#define		max_name  64
#define		max_guid  64

struct  msg_head
{
  unsigned short    len_;
  unsigned short    cmd_;
};

template<class T> T swap_byte_order(T d)
{ 
char* p = (char*)&d;
#ifndef _NO_SWAP_ORDER
  int s = sizeof(T);
  if (s <= 1) return d;
  char t;
  for (int i = 0; i < (s >> 1); i++)
  {
    t = p[i];
    p[i] = p[s - i - 1];
    p[s - i - 1] = t;
  }
#endif
  return d;
}

struct stream_base : public boost::enable_shared_from_this<stream_base>
{
public:
	stream_base()
	{
		rd_begin_ = NULL;
		rd_buff_len_ = 0;
		rd_fail_ = false;
		wt_begin_ = NULL;
		wt_fail_ = false;
		wt_buff_len_ = 0;
	}

	virtual int		read (boost::property_tree::ptree& jsval) { return 0; }

	virtual int		write(boost::property_tree::ptree& jsval) { return 0; }

	virtual int		write(char*& data_s, unsigned int  l)
	{
		wt_begin_ = (char*)data_s;
		wt_buff_len_ = l;
		wt_fail_ = false;
		return 0;
	}

	virtual int		read	(const char*& data_s, unsigned int l)
	{
		rd_begin_ = (char*)data_s;
		rd_buff_len_ = l;
		rd_fail_ = false;
		return 0;
	}

	virtual ~stream_base(){}

	template<class T>
	T				read_data(const char*&data_s, bool swap_order = true)
	{
		if (rd_fail_) return;

		if ((rd_buff_len_ - ((char*)data_s - rd_begin_)) < sizeof(T)) {
			rd_fail_ = true;
			return T(0);
		}

		T t = *(T*)data_s;
		if (swap_order)	{
			t = swap_byte_order<T>(t);
		}
		data_s += sizeof(T);

		return t;
	}

	template<class T>
	void			read_data(T& t, const char*&data_s, bool swap_order = true)
	{
		if (rd_fail_) return;

		if((rd_buff_len_ - ((char*)data_s - rd_begin_)) < sizeof(T)){
					rd_fail_ = true;
					return;
		}

		t = *(T*)data_s;
		if (swap_order)	{
			t = swap_byte_order<T>(t);
		}
		data_s += sizeof(T);
	}

	template<class T>
	void			read_data(T* v, int element_count, const char*&data_s, bool swap_order = false)
	{
		if (rd_fail_) return;

		if(rd_buff_len_ - (data_s - rd_begin_) < int(sizeof(T)) * element_count){
			rd_fail_ = true;
			return;
		}

		T* vv = (T*)v;
		for (int i = 0 ; i < element_count; i++)
		{	
			T t = *(T*)data_s;
			if (swap_order)	{
				t = swap_byte_order<T>(t);
			}
			data_s += sizeof(T);
			*vv = t;
			vv++;
		}
	}

	inline int	string_len(const char* data_s) 
	{
		int len = 0;
		unsigned char len1 = *(unsigned char*)data_s;
		data_s++;
		if (len1 == 0xFF) {
			len = int((unsigned char)(*data_s) << 8);
			data_s++;
			len |= (unsigned char)(*data_s);
		}
		else {
			len = len1;
		}
		return len;
	}

	template<> 
	void	read_data<char>(char* v, int element_count, const char*&data_s, bool swap_order)
	{
		if (rd_fail_) return;
		memset(v, 0, element_count);
		
		int buff_left = rd_buff_len_ - (data_s - rd_begin_);
		if (buff_left <= 0)	{
			rd_fail_ = true;
			return;
		}

		int len = string_len(data_s);
		if (len > 0xFF){
			data_s += 2;
		}
		else {
			data_s++;
		}

		buff_left = rd_buff_len_ - (data_s - rd_begin_);

		if (buff_left < len){
			rd_fail_ = true;
			return;
		}

		if(len == 0) return;

		memcpy(v, data_s, len);

		//添加终结符.以防攻击
		if(len < element_count){
			v[len] = 0;
			data_s += len + 1;
		}
		else {
			v[element_count - 1] = 0;
			data_s += element_count;
		}
	}

	template<class T>
	void			read_data(std::vector<T>& v, const char*& data_s, int element_count, bool swap_order = false)
	{
		if (rd_fail_) return;

		if(rd_buff_len_ - (data_s - rd_begin_) < int(sizeof(T) * v.size())){
			rd_fail_ = true;
			return;
		}

		for (unsigned int i = 0 ; i < element_count; i++)
		{	
			T t = *(T*)data_s;
			if (swap_order)	{
				t = swap_byte_order<T>(t);
			}
			data_s += sizeof(T);
			v.push_back(t);
		}
	}

	template<class T>
	void			write_data(T v, char*& data_s, bool swap_order = true)
	{
		if (wt_fail_) return;
		if(wt_buff_len_ - (data_s - wt_begin_) < int(sizeof(T))){
			wt_fail_ = true;
			return;
		}
		if (swap_order)	{
				v = swap_byte_order<T>(v);
		}
     
		memcpy(data_s, &v, sizeof(T));
		data_s += sizeof(T);
	}

	template<class T>
	void			write_data(T* v, int element_count, char*& data_s, bool swap_order = false)
	{
		if(wt_fail_) return;
		if(wt_buff_len_ - (data_s - wt_begin_) < int(sizeof(T)) * element_count){
			wt_fail_ = true;
			return;
		}
		T* vv = (T*)v;
		for (int i = 0 ; i < element_count; i++)
		{
			T t = *vv;
			if (swap_order)	{
			t = swap_byte_order<T>(t);
			}
			memcpy(data_s, &t, sizeof(T));
			data_s += sizeof(T);
			vv++;
		}
	}

	template<> 
	void	write_data<char>(char* v, int element_count, char*& data_s, bool swap_order)
	{
		if (wt_fail_) return;
		char* vv = v;
		//找出字符串长度
		int len = 0;
		for (;len < element_count; len++, vv++)
		{
			if(*vv == 0){	break; }
		}

		int buff_left = wt_buff_len_ - (data_s - wt_begin_);
		if (buff_left <= 0){
			wt_fail_ = true;
			return;
		}

		if (len > 0xFF){
			*(unsigned char*)data_s = 0xFF; data_s++;
			*data_s = ((len >> 8) & 0xFF); data_s++;
		}
		*(unsigned char*)data_s = (len & 0xFF); data_s++;

		if(len == 0) return;		//没内容

		buff_left = wt_buff_len_ - (data_s - wt_begin_);
		if (buff_left < len){
			wt_fail_ = true;
			return;
		}

		memcpy(data_s, v, len);
		//整个长度内未包含终结符，则强制最后一个字节为终结符
		if(len < element_count){ 
			data_s[len] = 0;
			data_s += len + 1;
		}
		//添加一个终结符
		else{
			data_s[element_count - 1] = 0;
			data_s += element_count;
		}
	}

	template<class T>
	void			write_data(std::vector<T>& v, char*& data_s, bool swap_order = false)
	{
		if (wt_fail_) return;
		if(wt_buff_len_ - (data_s - wt_begin_) < int(sizeof(T) * v.size())){
			wt_fail_ = true;
			return;
		}
		for (unsigned int i = 0 ; i < v.size(); i++)
		{
			T t = v[i];
			if (swap_order)	{
				t = swap_byte_order<T>(t);
			}
			memcpy(data_s, &t, sizeof(T));
			data_s += sizeof(T);
		}
	}
	bool			read_successful()	{return !rd_fail_;}
	bool			write_successful()	{return !wt_fail_;}
protected:
	char*			rd_begin_;	//开始读取位置,用来检测越界读
	int				rd_buff_len_;
	bool			rd_fail_;
	char*			wt_begin_;	//开始读取位置,用来检测越界读
	int				wt_buff_len_;
	bool			wt_fail_;
};

template<class remote_t>
class msg_base : public stream_base
{
public:
	typedef boost::shared_ptr<msg_base<remote_t>> msg_ptr;
	msg_head		head_;
	remote_t		from_sock_;
	
	virtual stream_buffer	alloc_io_buffer() 
	{
		boost::shared_array<char> buff(new char[1024]);
		return stream_buffer(buff, 0, 1024);
	}

	int				read(boost::property_tree::ptree& jsval) { return 0; }

	int				write(boost::property_tree::ptree& jsval) { return 0; }

	int				read(const char*& data_s, unsigned int l)
	{
		stream_base::read(data_s, l);
		read_data(head_.len_, data_s);
		read_data(head_.cmd_, data_s);
		return 0;
	}

	int				write(char*& data_s, unsigned int l)
	{
		stream_base::write(data_s, l);
		write_data(head_.len_, data_s);
		write_data(head_.cmd_, data_s);
		return 0;
	}
	virtual	int handle_this() {return 0;};
	void			init()
	{

	}
};


struct none_socket{};

template<class socket_t>
class msg_common_reply: public msg_base<socket_t>
{
public:
	int				rp_cmd_;
	int				err_;
	char			des_[128];
	msg_common_reply()
	{
		head_.cmd_ = 1001;
		memset(des_, 0, 128);
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(rp_cmd_, data_s);
		read_jvalue(err_, data_s);
		read_jstring(des_, 128, data_s);
		return 0;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(rp_cmd_, data_s);
		write_jvalue(err_, data_s);
		write_jvalue(des_, data_s);
		return 0;
	}

	int			read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data(rp_cmd_, data_s);
		read_data(err_, data_s);
		read_data<char>(des_, 128, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(rp_cmd_, data_s);
		write_data(err_, data_s);
		write_data<char>(des_, 128, data_s);
		return 0;
	}
};

template<class socket_t>
class msg_sequence_send_over : public msg_base<socket_t>
{
public:
	int				cmd_;
	msg_sequence_send_over()
	{
		head_.cmd_ = 997;
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(cmd_, data_s);
		return 0;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(cmd_, data_s);
		return 0;
	}

	int			read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data(cmd_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(cmd_, data_s);
		return 0;
	}
};

template<class socket_t>
class msg_common_reply_ex : public msg_base<socket_t>
{
public:
	int				cmd_;
	std::string		data_;
	msg_common_reply_ex()
	{
		head_.cmd_ = 995;
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(cmd_, data_s);
		read_jvalue(data_, data_s);
		return 0;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(cmd_, data_s);
		write_jvalue(data_, data_s);
		return 0;
	}

	int			read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data(cmd_, data_s);
		read_data(data_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(cmd_, data_s);
		write_data(data_, data_s);
		return 0;
	}
};

template<class socket_t>
class msg_server_time : public msg_base<socket_t>
{
public:
	__int64			timestamp_;
	msg_server_time()
	{
		head_.cmd_ = 996;
	}

	int			read(boost::property_tree::ptree& data_s)
	{
		msg_base::read(data_s);
		read_jvalue(timestamp_, data_s);
		return 0;
	}

	int			write(boost::property_tree::ptree& data_s)
	{
		msg_base::write(data_s);
		write_jvalue(timestamp_, data_s);
		return 0;
	}

	int			read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data(timestamp_, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(timestamp_, data_s);
		return 0;
	}
};

template<class socket_t>
class msg_common_reply_internal: public msg_common_reply<socket_t>
{
public:
	int		SN;
	msg_common_reply_internal()
	{
		head_.cmd_ = 6000;
    }

	int			read(const char*& data_s, unsigned int l)
	{
		msg_common_reply::read(data_s, l);
		read_data(SN, data_s);
		return 0;
	}
	int			write(char*& data_s, unsigned int l)
	{
		msg_common_reply::write(data_s, l);
		write_data(SN, data_s);
		return 0;
	}
};

template<class socket_t>
class msg_ping : public msg_base<socket_t>
{
public:
	msg_ping()
	{
		head_.cmd_ = 0xFFFF;
	}
};

template<class socket_t>
class msg_json_form : public msg_base<socket_t>
{
public:
    unsigned short  sub_cmd_;
	//重载缓冲分配
	stream_buffer	alloc_io_buffer()
	{
		boost::shared_array<char> buff(new char[content_.size() + 8]);
		return stream_buffer(buff, 0, content_.size() + 8); //8为content前面的数据长度.
	}

	void	set_content(std::string json_dat)
	{
		//如果数据长度大于1024,启用压缩
		if (json_dat.length() > 0x400){
			is_compressed_ = 1;
			content_ = gzip_gz(json_dat);
		}
		else {
			content_ = json_dat;
			is_compressed_ = 0;
		}
	}
	
	std::string content()
	{
		return content_;
	}

    msg_json_form()
    {
		head_.cmd_ = 0xCCCC;
    }

	int			read(const char*& data_s, unsigned int l)
	{
		msg_base::read(data_s, l);
		read_data(sub_cmd_, data_s);
		read_data(is_compressed_, data_s);

		int len = head_.len_ - 8;	//8为content前面的数据长度.
		if (len > 0xFFFF)	{
			rd_fail_ = true;
			return 0;
		}
		
		if (len < 0){
			rd_fail_ = true;
			return 0;
		}

		boost::shared_array<unsigned char> buff(new unsigned char[len]);
		if (buff == nullptr) {
			rd_fail_ = true;
			return 0;
		}

		read_data<unsigned char>(buff.get(), len, data_s);

		content_.insert(content_.end(), (char*)buff.get(), (char*)buff.get() + len);
		//如果压缩过,则解压
		if (is_compressed_){
			content_ = ungzip_gz(content_);
		}
		return 0;
	}

    int			write(char*& data_s, unsigned int l)
    {
        msg_base::write(data_s, l);
        write_data(sub_cmd_, data_s);
		write_data(is_compressed_, data_s);
        write_data<unsigned char>((unsigned char*)content_.c_str(), content_.length(),  data_s);
        return 0;
    }

private:
	unsigned short  is_compressed_;		//尽量对齐字节为4的倍数
	std::string     content_;
};

template<class remote_t>
stream_buffer build_send_stream(msg_base<remote_t>* msg)
{
	stream_buffer stmb = msg->alloc_io_buffer();
	char* start = stmb.buffer();
	msg->write(start, stmb.buffer_left());
	unsigned short len = start - stmb.data();
	stmb.use_buffer(len);
	*(unsigned short*)stmb.data() = swap_byte_order(len);
	return stmb;
}

template<class remote_t>
void build_send_stream(msg_base<remote_t>* msg, stream_buffer& strm)
{
	char* pstrm = strm.buffer();
	char* pc = pstrm;
	msg->write(pstrm, strm.buffer_left());
	unsigned short len = pstrm - pc;
	strm.use_buffer(len);
	*(unsigned short*) pc = swap_byte_order(len);
}

#ifdef USE_JSON_PROTOCOL
    #define send_protocol   send_json
#else
    #define send_protocol   send_msg
#endif

template<class remote_t, class msg_t>
int			send_json(remote_t psock, msg_t& msg,  bool close_this = false, bool sync = false)
{
	msg_json_form<remote_t> ack;
	ack.sub_cmd_ = msg.head_.cmd_;
    std::string str = json_msg_helper::to_json(&msg);
	ack.set_content(str);
    return send_msg(psock, ack, close_this, sync);
}

template<class remote_t, class msg_t>
int			send_msg(remote_t psock, msg_t& msg,  bool close_this = false, bool sync = false)
{
	stream_buffer strm = build_send_stream(&msg);
	return send_msg(psock, strm, close_this, sync);
}

template<class remote_t>
int			send_msg(remote_t psock, stream_buffer strm, bool close_this = false, bool sync = false)
{
	if (!sync){
		if (psock->isworking()) {
			psock->add_to_send_queue(strm, close_this);
			return 0;
		}
		else {
			return -1;
		}
	}
	else{
		return psock->send_data(strm);
	}
}

template<class remote_t, class msg_creator_t>
boost::shared_ptr<msg_base<remote_t>>		pickup_msg_from_socket(remote_t psock, msg_creator_t creator, bool& got_msg)
{
	unsigned short n = 0;
	if (psock->pickup_data(&n, 2, false)){
		n = ntohs(n);
		char * c = new char[n];
		boost::shared_array<char> dat(c);
		if (!psock->pickup_data(c, n, true)) return boost::shared_ptr<msg_base<remote_t>>();
		
		got_msg = true;
		msg_head head;
		head = *(msg_head*) c;
		head.cmd_ = ntohs(head.cmd_);
		
		if (head.cmd_ == 0xCCCC){
			boost::shared_ptr<msg_json_form<remote_t>> msg_json(new msg_json_form<remote_t>());
			msg_json->read((const char*&)c, n);
			msg_json->from_sock_ = psock;
			if (msg_json->read_successful()){
				boost::shared_ptr<msg_base<remote_t>> msg = creator(msg_json->sub_cmd_);
				if (msg != nullptr && !msg_json->content().empty()){
					boost::property_tree::ptree jsn_obj;
					std::stringstream sstr;
					sstr << msg_json->content();
					try	{
						boost::property_tree::read_json(sstr, jsn_obj);
						msg->read(jsn_obj);
						msg->from_sock_ = psock;
						return msg;
					}
					catch (boost::property_tree::json_parser_error e){
						std::cout << "json msg read error: " << e.message() << std::endl;
					}
					return nullptr;
				}
// 				else if (!msg){
// 					std::cout << "can't create msg, cmd = " << msg_json->sub_cmd_ << std::endl;
// 				}
			}
			else {
				psock->close();
				std::cout << "json msg read error. " << std::endl;
			}
		}
		else {
			boost::shared_ptr<msg_base<remote_t>> msg = creator(head.cmd_);
			if (msg.get()) {
				msg->read((const char*&)c, n);
				msg->from_sock_ = psock;
				if (msg->read_successful()) {
					return msg;
				}
				else {
					psock->close();
					std::cout << "msg read failed, cmd = " << head.cmd_ << std::endl;
				}
			}
			else {
				std::cout << "can't create msg, cmd = " << head.cmd_ << std::endl;
			}
		}
	}

	return boost::shared_ptr<msg_base<remote_t>>();
}
