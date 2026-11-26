find_package(Qt6 REQUIRED COMPONENTS Test Gui SerialPort Sql Xml)

# 测试共用核心代码：协议、通道模型、数据库、语言包
set(HMI_TEST_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/modbusrtu.cpp
    ${CMAKE_SOURCE_DIR}/src/common/serialporthelper.cpp
    ${CMAKE_SOURCE_DIR}/src/common/modbuspoller.cpp
    ${CMAKE_SOURCE_DIR}/src/common/serialworker.cpp
    ${CMAKE_SOURCE_DIR}/src/common/channelmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/common/savedatasql.cpp
    ${CMAKE_SOURCE_DIR}/src/base/Language.cpp
    # Q_OBJECT 头文件显式加入，保证 AUTOMOC 生成元对象
    ${CMAKE_SOURCE_DIR}/include/common/modbusrtu.h
    ${CMAKE_SOURCE_DIR}/include/common/serialporthelper.h
    ${CMAKE_SOURCE_DIR}/include/common/modbuspoller.h
    ${CMAKE_SOURCE_DIR}/include/common/serialworker.h
    ${CMAKE_SOURCE_DIR}/include/common/polltask.h
    ${CMAKE_SOURCE_DIR}/include/common/channelmodel.h
    ${CMAKE_SOURCE_DIR}/include/common/savedatasql.h
    ${CMAKE_SOURCE_DIR}/include/base/Language.h
)

qt_add_library(hmi_test_core STATIC ${HMI_TEST_CORE_SOURCES})

target_include_directories(hmi_test_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/base
    ${CMAKE_SOURCE_DIR}/include/common
)

target_link_libraries(hmi_test_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
    Qt6::Sql
    Qt6::SerialPort
)

target_compile_definitions(hmi_test_core PUBLIC
    HMI_LANGUAGE_DIR="${CMAKE_SOURCE_DIR}/language"
)

# 每个测试文件生成一个可执行程序，注册到 CTest
set(HMI_TEST_NAMES
    test_modbusrtu
    test_modbuspoller
    test_channelmodel
    test_savedatasql
    test_serialworker
    test_languagekeys
)

foreach(test_name IN LISTS HMI_TEST_NAMES)
    qt_add_executable(${test_name} ${test_name}.cpp)
    # 控制台子系统，保证 ctest/Qt Test 输出可见
    set_target_properties(${test_name} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(${test_name} PRIVATE hmi_test_core Qt6::Test)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# 可选：Python 协议级测试（模拟从站，无需串口硬件与 com0com）
# 未安装 Python3 时自动跳过，不影响 C++ 测试
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME python_slave_sim
             COMMAND ${Python3_EXECUTABLE} -m unittest discover -s . -p "test_*.py" -v
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python)
    set_tests_properties(python_slave_sim PROPERTIES
        ENVIRONMENT "PYTHONIOENCODING=utf-8")
else()
    message(STATUS "Python3 not found, python_slave_sim test skipped")
endif()
