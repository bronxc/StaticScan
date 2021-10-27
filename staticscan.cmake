include_directories(${CMAKE_CURRENT_LIST_DIR})

include(${CMAKE_CURRENT_LIST_DIR}/../SpecAbstract/specabstract.cmake)

set(STATICSCAN_SOURCES
    ${SPECABSTRACT_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/staticscan.cpp
    ${CMAKE_CURRENT_LIST_DIR}/staticscanitem.cpp
    ${CMAKE_CURRENT_LIST_DIR}/staticscanitemmodel.cpp
)
