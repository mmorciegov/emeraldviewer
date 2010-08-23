# -*- cmake -*-
include(Prebuilt)

set(Boost_FIND_QUIETLY ON)
set(Boost_FIND_REQUIRED ON)

if (STANDALONE)
  find_package(Boost COMPONENTS system filesystem iostreams python regex signals thread wave program_options date_time REQUIRED)
else (STANDALONE)
  use_prebuilt_binary(boost)
  set(Boost_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)

  if (WINDOWS)
    set(BOOST_VERSION 1_39)
    if (MSVC80)
      set(Boost_PROGRAM_OPTIONS_LIBRARY 
          optimized libboost_program_options-vc80-mt-${BOOST_VERSION}
          debug libboost_program_options-vc80-mt-gd-${BOOST_VERSION})
      set(Boost_REGEX_LIBRARY
          optimized libboost_regex-vc80-mt-${BOOST_VERSION}
          debug libboost_regex-vc80-mt-gd-${BOOST_VERSION})
      set(Boost_SIGNALS_LIBRARY 
          optimized libboost_signals-vc80-mt-${BOOST_VERSION}
          debug libboost_signals-vc80-mt-gd-${BOOST_VERSION})
    else (MSVC90)
      set(Boost_PROGRAM_OPTIONS_LIBRARY 
          optimized libboost_program_options-vc90-mt-${BOOST_VERSION}
          debug libboost_program_options-vc90-mt-gd-${BOOST_VERSION})
      set(Boost_REGEX_LIBRARY
          optimized libboost_regex-vc90-mt-${BOOST_VERSION}
          debug libboost_regex-vc90-mt-gd-${BOOST_VERSION})
      set(Boost_SIGNALS_LIBRARY 
          optimized libboost_signals-vc90-mt-${BOOST_VERSION}
          debug libboost_signals-vc90-mt-gd-${BOOST_VERSION})
    endif (MSVC80)
  elseif (LINUX)
  	set(Boost_PROGRAM_OPTIONS_LIBRARY boost_program_options-mt)  	
  	set(Boost_REGEX_LIBRARY boost_regex-mt)
  	set(Boost_PYTHON_LIBRARY boost_python-mt)
  	set(Boost_SIGNALS_LIBRARY boost_signals-mt)
  	set(Boost_WAVE_LIBRARY boost_wave-mt)
  	set(Boost_SYSTEM_LIBRARY boost_system-mt)
  	set(Boost_FILESYSTEM_LIBRARY boost_filesystem-mt)
  	set(Boost_IOSTREAMS_LIBRARY boost_iostreams-mt)
  	set(Boost_DATE_TIME_LIBRARY boost_date_time-mt)
  	set(Boost_THREAD_LIBRARY boost_thread-mt)
  else (WINDOWS)
  	find_library(PROGRAM_LIB NAMES boost_program_options-mt boost_program_options PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(REGEX_LIB NAMES boost_regex-mt boost_regex PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(PYTHON_LIB NAMES boost_python-mt boost_python PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(SIGNALS_LIB NAMES boost_signals-mt boost_signals PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(WAVE_LIB NAMES boost_wave-mt boost_wave PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(SYSTEM_LIB NAMES boost_system-mt boost_system PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(FILESYSTEM_LIB NAMES boost_filesystem-mt boost_filesystem PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(IOSTREAMS_LIB NAMES boost_iostreams-mt boost_iostreams PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(DATE_LIB NAMES boost_date_time-mt boost_date_time PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)
  	find_library(THREAD_LIB NAMES boost_thread-mt boost_thread PATHS ${ARCH_PREBUILT_DIRS_RELEASE} NO_DEFAULT_PATH)

  	set(Boost_PROGRAM_OPTIONS_LIBRARY ${PROGRAM_LIB})  	
  	set(Boost_REGEX_LIBRARY ${REGEX_LIB})
  	set(Boost_PYTHON_LIBRARY ${PYTHON_LIB})
  	set(Boost_SIGNALS_LIBRARY ${SIGNALS_LIB})
  	set(Boost_WAVE_LIBRARY ${WAVE_LIB})
  	set(Boost_SYSTEM_LIBRARY ${SYSTEM_LIB})
  	set(Boost_FILESYSTEM_LIBRARY ${FILESYSTEM_LIB})
  	set(Boost_IOSTREAMS_LIBRARY ${IOSTREAMS_LIB})
  	set(Boost_DATE_TIME_LIBRARY ${DATE_LIB})
  	set(Boost_THREAD_LIBRARY ${THREAD_LIB})
  endif (WINDOWS)
endif (STANDALONE)
