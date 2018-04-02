#include "log.h"
#include "service.h"

#include "boost/atomic.hpp"
#include "game_service_base.hpp"
 #include "http_request.cpp"
 #include "simple_xml_parse.cpp"
 #include "md5.cpp"
 #include "json_helper.cpp"

#include "dice_controller.cpp"
#include "i_dice_controller.cpp"
// #include "net_socket_native.cpp"
// #include "dice_controller_slot.cpp"
// #include "base64Con.cpp"

boost::atomic<bool> glb_exit;
extern log_file<cout_and_file_output> glb_http_log;

bool algorithm_test = false;

#include <DbgHelp.h>
#pragma auto_inline (off)
#pragma comment(lib, "dbghelp.lib")
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	switch(CEvent)
	{
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		the_service.the_config_.shut_down_ = 1;
	}
	while (!glb_exit.load())
	{
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
	}
	return TRUE;
}

int		dump = 0;
LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	std::string fname = "crash" + boost::lexical_cast<std::string>(dump++) + ".dmp";
	HANDLE lhDumpFile = CreateFileA(fname.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL ,NULL);

	MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
	loExceptionInfo.ExceptionPointers = ExceptionInfo;
	loExceptionInfo.ThreadId = GetCurrentThreadId();
	loExceptionInfo.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), lhDumpFile, MiniDumpWithFullMemory, &loExceptionInfo, NULL, NULL);

	CloseHandle(lhDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int arc, char* args[])
{
	const char* ar = args[1];
	algorithm_test = arc > 2 ? strstr(args[2], "-algtest") != nullptr : false; 
	bool nocatch = arc >1 ? strstr(args[1], "-nocatch") != nullptr : false; 

	glb_http_log.output_.fname_ = "game_runlog.log";
	glb_http_log.write_log("==========================service starting=======================");
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE){
		return -1;
	}
	srand(::time(nullptr));
	
	if (the_service.run() != ERROR_SUCCESS_0){
		glb_http_log.write_log("service run failed!");
		the_service.stop();
		glb_http_log.stop_log();
		glb_exit = true;
		::system("pause");
		return -1;
	}
	glb_http_log.write_log("service run successful!");
	if (nocatch){
		the_service.main_thread_update();
	}
	else{
		__try{
			the_service.main_thread_update();
		}
		__except(MyUnhandledExceptionFilter(GetExceptionInformation()),
			EXCEPTION_EXECUTE_HANDLER){
		}
	}
	the_service.stop();
	glb_http_log.stop_log();
	glb_http_log.stop_log();
	glb_http_log.write_log("==========================service close ok=======================");
	glb_http_log.sync();
	glb_exit = true;

	return 0;
}
