cmake_minimum_required(VERSION 3.19)
set(PROJECT_NAME "modbus-hmi")
project(${PROJECT_NAME} LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Xml Charts)

qt_standard_project_setup()

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
)

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
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
        Qt::Charts
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
# 目标：exe所在目录/language
set(LANG_TARGET_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language")

# 编译完成自动拷贝整个文件夹
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${LANG_SOURCE_DIR}
    ${LANG_TARGET_DIR}
)

