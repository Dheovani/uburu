if(NOT DEFINED UBURU_FORMAT_SOURCE_LIST)
  message(FATAL_ERROR "UBURU_FORMAT_SOURCE_LIST is required")
endif()

file(STRINGS "${UBURU_FORMAT_SOURCE_LIST}" UBURU_FORMAT_SOURCES)

set(UBURU_STYLE_ERRORS "")

foreach(UBURU_SOURCE IN LISTS UBURU_FORMAT_SOURCES)
  if(NOT EXISTS "${UBURU_SOURCE}")
    continue()
  endif()

  file(READ "${UBURU_SOURCE}" UBURU_CONTENT)
  string(REPLACE ";" "\\;" UBURU_CONTENT "${UBURU_CONTENT}")
  string(REPLACE "\r\n" "\n" UBURU_CONTENT "${UBURU_CONTENT}")
  string(REPLACE "\r" "\n" UBURU_CONTENT "${UBURU_CONTENT}")
  string(REPLACE "\n" ";" UBURU_LINES "${UBURU_CONTENT}")
  set(UBURU_LINE_NUMBER 0)

  foreach(UBURU_LINE IN LISTS UBURU_LINES)
    math(EXPR UBURU_LINE_NUMBER "${UBURU_LINE_NUMBER} + 1")

    string(FIND "${UBURU_LINE}" "\t" UBURU_TAB_INDEX)
    if(NOT UBURU_TAB_INDEX EQUAL -1)
      string(APPEND UBURU_STYLE_ERRORS "${UBURU_SOURCE}:${UBURU_LINE_NUMBER}: tab character is not allowed\n")
    endif()

    if(UBURU_LINE MATCHES "[ \t]+$")
      string(APPEND UBURU_STYLE_ERRORS "${UBURU_SOURCE}:${UBURU_LINE_NUMBER}: trailing whitespace is not allowed\n")
    endif()
  endforeach()
endforeach()

if(NOT UBURU_STYLE_ERRORS STREQUAL "")
  message(FATAL_ERROR "${UBURU_STYLE_ERRORS}")
endif()
