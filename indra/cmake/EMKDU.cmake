# -*- cmake -*-
include(Prebuilt)


use_prebuilt_binary(emkdu)
  
set(EMKDU_LIBRARY emkdu)
set(EMKDU_STATIC_LIBRARY emkdu_static)
set(EMKDU_LIBRARIES ${EMKDU_LIBRARY})
set(EMKDU_STATIC_LIBRARIES ${EMKDU_STATIC_LIBRARY})
