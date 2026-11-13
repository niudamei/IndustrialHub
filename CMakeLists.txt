cmake_minimum_required(VERSION 3.19)
project(IndustrialHub VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network Charts Concurrent OPTIONAL_COMPONENTS SerialPort)
qt_standard_project_setup()
include(CTest)

qt_add_executable(hhh
    WIN32 MACOSX_BUNDLE
    app/main.cpp
    CustomFolder/custombutton.h CustomFolder/custombutton.cpp
    CustomFolder/customprogressbar.h CustomFolder/customprogressbar.cpp
    CustomFolder/customlineedit.h CustomFolder/customlineedit.cpp
    CustomFolder/customcheckbox.h CustomFolder/customcheckbox.cpp
    CustomFolder/customradiobutton.h CustomFolder/customradiobutton.cpp
    CustomFolder/customslider.h CustomFolder/customslider.cpp
    CustomFolder/customcombobox.h CustomFolder/customcombobox.cpp
    CustomFolder/customlabel.h CustomFolder/customlabel.cpp
    CustomFolder/customspinbox.h CustomFolder/customspinbox.cpp
    CustomFolder/customdoublespinbox.h CustomFolder/customdoublespinbox.cpp
    CustomFolder/customtablewidget.h CustomFolder/customtablewidget.cpp
    CustomFolder/customtreewidget.h CustomFolder/customtreewidget.cpp
    CustomFolder/customtabwidget.h CustomFolder/customtabwidget.cpp
    CustomFolder/customtextedit.h CustomFolder/customtextedit.cpp
    CustomFolder/customgraphicsview.h CustomFolder/customgraphicsview.cpp
    CustomFolder/customscrollarea.h CustomFolder/customscrollarea.cpp
    CustomFolder/customsplitter.h CustomFolder/customsplitter.cpp
    CustomFolder/customtoolbutton.h CustomFolder/customtoolbutton.cpp
    CustomFolder/customlistview.h CustomFolder/customlistview.cpp
    CustomFolder/customtreeview.h CustomFolder/customtreeview.cpp
    CustomFolder/customtableview.h CustomFolder/customtableview.cpp
    CustomFolder/customlistwidget.h CustomFolder/customlistwidget.cpp
    src/pages/AlarmPage.h src/pages/AlarmPage.cpp src/pages/AlarmPage.ui
    src/pages/ConfigPage.h src/pages/ConfigPage.cpp src/pages/ConfigPage.ui
    src/pages/DashboardPage.h src/pages/DashboardPage.cpp src/pages/DashboardPage.ui
    src/pages/devicepage.h src/pages/DevicePage.cpp src/pages/DevicePage.ui
    src/pages/devicedialog.h src/pages/devicedialog.cpp src/pages/devicedialog.ui
    src/pages/deviceinfo.h
    src/pages/devicemanager.h src/pages/devicemanager.cpp
    src/database/devicedatabase.h src/database/devicedatabase.cpp
    src/core/datamanager.h src/core/datamanager.cpp
    src/core/devicehealthmonitor.h src/core/devicehealthmonitor.cpp
    src/core/businessdatamanager.h src/core/businessdatamanager.cpp
    src/core/alarmmanager.h src/core/alarmmanager.cpp
    src/protocol/businessdatapoint.h
    src/protocol/iprotocolparser.h
    src/protocol/jsonprotocolparser.h src/protocol/jsonprotocolparser.cpp
    src/protocol/protocolmanager.h src/protocol/protocolmanager.cpp
    src/communication/icommunication.h src/communication/icommunication.cpp
    src/communication/simulatedcommunication.h src/communication/simulatedcommunication.cpp
    src/communication/tcpcommunication.h src/communication/tcpcommunication.cpp
    src/communication/udpcommunication.h src/communication/udpcommunication.cpp
    src/communication/communicationmanager.h src/communication/communicationmanager.cpp
    src/communication/communicationworker.h src/communication/communicationworker.cpp
    src/communication/slcanframecodec.h src/communication/slcanframecodec.cpp
    src/pages/HistoryPage.h src/pages/HistoryPage.cpp src/pages/HistoryPage.ui
    src/pages/MonitorPage.h src/pages/MonitorPage.cpp src/pages/MonitorPage.ui
    src/pages/TrendPage.h src/pages/TrendPage.cpp src/pages/TrendPage.ui
    src/pages/SettingsPage.h src/pages/SettingsPage.cpp src/pages/SettingsPage.ui
    src/ui/MainWindow/MainWindow.h
    src/ui/MainWindow/MainWindow.cpp
    src/ui/MainWindow/MainWindow.ui
)

if(BUILD_TESTING)

    add_executable(slcan_frame_codec_test
        tests/slcan_frame_codec_test.cpp
        src/communication/slcanframecodec.h
        src/communication/slcanframecodec.cpp
    )
    target_include_directories(slcan_frame_codec_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(slcan_frame_codec_test PRIVATE Qt6::Core)
    add_test(NAME slcan_frame_codec_test COMMAND slcan_frame_codec_test)

    add_executable(communication_smoke_test
        tests/communication_smoke_test.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(communication_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(communication_smoke_test PRIVATE Qt6::Core)
    add_test(NAME communication_smoke_test COMMAND communication_smoke_test)

    add_executable(tcp_communication_smoke_test
        tests/tcp_communication_smoke_test.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(tcp_communication_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(tcp_communication_smoke_test PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME tcp_communication_smoke_test COMMAND tcp_communication_smoke_test)


    add_executable(udp_communication_smoke_test
        tests/udp_communication_smoke_test.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(udp_communication_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(udp_communication_smoke_test PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME udp_communication_smoke_test COMMAND udp_communication_smoke_test)

    add_executable(communication_manager_smoke_test
        tests/communication_manager_smoke_test.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/communication/communicationmanager.h
        src/communication/communicationmanager.cpp
        src/communication/communicationworker.h
        src/communication/communicationworker.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(communication_manager_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(communication_manager_smoke_test PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME communication_manager_smoke_test COMMAND communication_manager_smoke_test)

    add_executable(business_data_manager_test
        tests/business_data_manager_test.cpp
        src/core/businessdatamanager.h
        src/core/businessdatamanager.cpp
        src/protocol/businessdatapoint.h
    )
    target_include_directories(business_data_manager_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(business_data_manager_test PRIVATE Qt6::Core)
    add_test(NAME business_data_manager_test COMMAND business_data_manager_test)

    add_executable(alarm_manager_test
        tests/alarm_manager_test.cpp
        src/core/alarmmanager.h
        src/core/alarmmanager.cpp
        src/protocol/businessdatapoint.h
    )
    target_include_directories(alarm_manager_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(alarm_manager_test PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME alarm_manager_test COMMAND alarm_manager_test)

    add_executable(json_protocol_parser_test
        tests/json_protocol_parser_test.cpp
        src/protocol/businessdatapoint.h
        src/protocol/iprotocolparser.h
        src/protocol/jsonprotocolparser.h
        src/protocol/jsonprotocolparser.cpp
        src/protocol/protocolmanager.h
        src/protocol/protocolmanager.cpp
    )
    target_include_directories(json_protocol_parser_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(json_protocol_parser_test PRIVATE Qt6::Core)
    add_test(NAME json_protocol_parser_test COMMAND json_protocol_parser_test)

    add_executable(device_database_test
        tests/device_database_test.cpp
        src/database/devicedatabase.h
        src/database/devicedatabase.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(device_database_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(device_database_test PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME device_database_test COMMAND device_database_test)

    add_executable(device_health_monitor_test
        tests/device_health_monitor_test.cpp
        src/core/devicehealthmonitor.h
        src/core/devicehealthmonitor.cpp
    )
    target_include_directories(device_health_monitor_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(device_health_monitor_test PRIVATE Qt6::Core)
    add_test(NAME device_health_monitor_test COMMAND device_health_monitor_test)

    add_executable(datamanager_history_test
        tests/datamanager_history_test.cpp
        src/core/datamanager.h
        src/core/datamanager.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/communication/communicationmanager.h
        src/communication/communicationmanager.cpp
        src/communication/communicationworker.h
        src/communication/communicationworker.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(datamanager_history_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(datamanager_history_test PRIVATE Qt6::Core Qt6::Sql Qt6::Network)
    add_test(NAME datamanager_history_test COMMAND datamanager_history_test)
endif()


# RS485 support is optional so the project still builds when Qt SerialPort
# was not selected in the local Qt installation.
if(TARGET Qt6::SerialPort)
    target_sources(hhh PRIVATE
        src/communication/rs485communication.h
        src/communication/rs485communication.cpp
        src/communication/slcancommunication.h
        src/communication/slcancommunication.cpp
    )
    target_link_libraries(hhh PRIVATE Qt6::SerialPort)
    target_compile_definitions(hhh PRIVATE INDUSTRIALHUB_HAS_SERIALPORT)

    if(BUILD_TESTING)
        add_executable(rs485_communication_smoke_test
            tests/rs485_communication_smoke_test.cpp
            src/communication/icommunication.h
            src/communication/icommunication.cpp
            src/communication/rs485communication.h
            src/communication/rs485communication.cpp
            src/pages/deviceinfo.h
        )
        target_include_directories(rs485_communication_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})
        target_link_libraries(rs485_communication_smoke_test PRIVATE Qt6::Core Qt6::SerialPort)
        add_test(NAME rs485_communication_smoke_test COMMAND rs485_communication_smoke_test)

        add_executable(slcan_communication_smoke_test
            tests/slcan_communication_smoke_test.cpp
            src/communication/icommunication.h
            src/communication/icommunication.cpp
            src/communication/slcancommunication.h
            src/communication/slcancommunication.cpp
            src/communication/slcanframecodec.h
            src/communication/slcanframecodec.cpp
            src/pages/deviceinfo.h
        )
        target_include_directories(slcan_communication_smoke_test PRIVATE ${CMAKE_SOURCE_DIR})
        target_link_libraries(slcan_communication_smoke_test PRIVATE Qt6::Core Qt6::SerialPort)
        add_test(NAME slcan_communication_smoke_test COMMAND slcan_communication_smoke_test)

        target_sources(communication_manager_smoke_test PRIVATE
            src/communication/rs485communication.h
            src/communication/rs485communication.cpp
            src/communication/slcancommunication.h
            src/communication/slcancommunication.cpp
            src/communication/slcanframecodec.h
            src/communication/slcanframecodec.cpp
        )
        target_link_libraries(communication_manager_smoke_test PRIVATE Qt6::SerialPort)
        target_compile_definitions(communication_manager_smoke_test PRIVATE INDUSTRIALHUB_HAS_SERIALPORT)
    endif()
else()
    message(STATUS "Qt SerialPort not found: RS485 driver will be disabled.")
endif()

target_include_directories(hhh PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(hhh PRIVATE Qt6::Core Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Charts Qt6::Concurrent)

install(TARGETS hhh
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
qt_generate_deploy_app_script(
    TARGET hhh
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
