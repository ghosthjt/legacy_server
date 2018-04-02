cmake_minimum_required(VERSION 2.6)
project(mysqlwrapped)

include_directories( ../dependencies/ ../dependencies/mysql_wrap_include $(BOOST_INCLUDE) ../dependencies/mysql_win_include)

set(LIBRARY_OUTPUT_PATH ${mysqlwrapped_SOURCE_DIR}/../libs)
aux_source_directory(. src_sqlwrap)
add_library(mysqlwrapped STATIC ${src_sqlwrap})
