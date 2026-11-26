cmake_minimum_required(VERSION 3.19)
set(PROJECT_NAME "modbus-hmi")
project(${PROJECT_NAME} LANGUAGES CXX)

# Windows 资源编译（exe 图标）；其他平台跳过
if(WIN32)
    enable_language(RC)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml Charts sql serialport PrintSupport)

qt_standard_project_setup()

# C++17 标准（Qt 6 默认即为 17，这里显式声明，避免依赖生成器默认值）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 开发期告警：尽早暴露未使用变量、隐式转换、签名比较等问题（不中断构建）
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(SOURCE_FILES
    src/main.cpp
    src/mainwidget.cpp
    src/base/LogHandler.cpp
    src/base/BaseSystemTray.cpp
    src/base/BaseConfigSetting.cpp       
    src/base/BaseWidget.cpp
    src/base/Language.cpp
    src/base/Signals.cpp
    src/base/BaseStytle.cpp
    src/component/menuwidget.cpp
    src/component/operationwidget.cpp
    src/component/comsetwidget.cpp
    src/component/modbussetwidget.cpp
    src/component/channelsetwidget.cpp
    src/component/realdatawidget.cpp
    src/component/realcurvewidget.cpp
    src/component/collectionstatisticswodget.cpp
    src/component/alarmandfaultwidget.cpp
    src/component/datasavewidget.cpp
    src/component/channeleditdialog.cpp
    src/component/alarmruledialog.cpp
    src/component/historyquerydialog.cpp
    src/component/commlogdialog.cpp
    src/component/curvesettingsdialog.cpp
    src/component/writeregisterdialog.cpp
    src/component/alarmhistorydialog.cpp
    src/component/disclaimerdialog.cpp
    3rdparty/QCustomPlot/qcustomplot.cpp
    src/common/projectconfig.cpp
    src/common/serialworker.cpp
    src/common/modbusrtu.cpp
    src/common/channelmodel.cpp
    src/common/serialporthelper.cpp
    src/common/modbuspoller.cpp
    src/common/savedatasql.cpp
)

# Windows：把图标资源编进 exe（双击运行 / 资源管理器显示自定义图标）
if(WIN32)
    list(APPEND SOURCE_FILES "${CMAKE_SOURCE_DIR}/resource/win/app.rc")
endif()

set(HEADER_FILES
    include/mainwidget.h
    include/base/LogHandler.h
    include/base/BaseSystemTray.h
    include/base/BaseConfigSetting.h
    include/base/BaseStructData.h
    include/base/BaseWidget.h
    include/base/Language.h
    include/base/Signals.h
    include/base/BaseStytle.h
    include/component/menuwidget.h
    include/component/operationwidget.h
    include/component/comsetwidget.h
    include/component/modbussetwidget.h
    include/component/channelsetwidget.h
    include/component/realdatawidget.h
    include/component/realcurvewidget.h
    include/component/collectionstatisticswodget.h
    include/component/alarmandfaultwidget.h
    include/component/datasavewidget.h
    include/component/channeleditdialog.h
    include/component/alarmruledialog.h
    include/component/historyquerydialog.h
    include/component/commlogdialog.h
    include/component/curvesettingsdialog.h
    include/component/writeregisterdialog.h
    include/component/alarmhistorydialog.h
    include/component/disclaimerdialog.h
    3rdparty/QCustomPlot/qcustomplot.h
    include/common/projectconfig.h
    include/common/serialworker.h
    include/common/modbusrtu.h
    include/common/channelmodel.h
    include/common/serialporthelper.h
    include/common/modbuspoller.h
    include/common/savedatasql.h
)

set(UI_FILES
    forms/mainwidget.ui
    forms/component/menuwidget.ui
    forms/component/operationwidget.ui
    forms/component/comsetwidget.ui
    forms/component/modbussetwidget.ui
    forms/component/channelsetwidget.ui
    forms/component/realdatawidget.ui
    forms/component/realcurvewidget.ui
    forms/component/collectionstatisticswodget.ui
    forms/component/alarmandfaultwidget.ui
    forms/component/datasavewidget.ui
)

qt_add_resources(RESOURCES
    resource/resource.qrc
    resource/stytlefile.qrc
)

qt_add_executable(${PROJECT_NAME}
    WIN32 MACOSX_BUNDLE
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${UI_FILES}
    ${RESOURCES}
)


set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOUIC_SEARCH_PATHS
        "${PROJECT_SOURCE_DIR}/forms;${PROJECT_SOURCE_DIR}/forms/component"

)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/base
        ${PROJECT_SOURCE_DIR}/include/component
        ${PROJECT_SOURCE_DIR}/include/common
        ${PROJECT_SOURCE_DIR}/3rdparty/QCustomPlot
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
        Qt::Charts
        Qt::SerialPort
        Qt::Sql
        Qt::PrintSupport
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# 安装语言文件,把language目录拷贝到程序目录下
# ========== 下面添加拷贝文件夹代码 ==========
# 源：项目根目录/language
set(LANG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/language")
# 目标：exe所在目录/language（多配置生成器下 exe 位于 Debug/Release 子目录）
set(LANG_TARGET_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/language")

# 编译完成自动拷贝整个文件夹
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${LANG_SOURCE_DIR}
    ${LANG_TARGET_DIR}
)

# ========== 单元测试（Qt Test + CTest） ==========
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

