cmake_minimum_required(VERSION 3.12...3.31)
project(apn_client CXX)

set(APN_SRC
    include/apn/types/credentials.hpp
    include/apn/types/notification.hpp
    include/apn/types/result.hpp

    include/apn/components/client.hpp

    include/apn/handlers/debug_send.hpp

    src/apn/types/credentials.cpp

    src/apn/jwt/token.hpp
    src/apn/jwt/token.cpp

    src/apn/components/client.cpp

    src/apn/handlers/debug_send.cpp
)

add_library(apn_client OBJECT ${APN_SRC})
target_link_libraries(apn_client PUBLIC userver::core)
target_include_directories(apn_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(apn_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(APN_BUILD_TESTS)
    add_executable(apn_unittest
        tests/jwt_test.cpp
        tests/notification_test.cpp
    )
    target_link_libraries(apn_unittest PRIVATE apn_client userver::utest)
    target_include_directories(apn_unittest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_google_tests(apn_unittest)
endif()
