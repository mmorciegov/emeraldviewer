# -*- cmake -*-
include(Prebuilt)

use_prebuilt_binary(Growl)

if (DARWIN)
    set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
    set(GROWL_LIBRARY Growl)
elseif (WINDOWS)
    set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
    set(GROWL_LIBRARY lgggrowl++)
elseif (LINUX)
    # Everything glib-2.0 and GTK-specific is pulled in by UI.cmake..
    set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libnotify)
    set(GROWL_LIBRARY notify)
endif (DARWIN)
