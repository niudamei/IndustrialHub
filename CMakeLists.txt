cmake_minimum_required(VERSION 3.19)
project(IndustrialHub VERSION 0.14.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network Charts Concurrent OPTIONAL_COMPONENTS SerialPort SerialBus)
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
    src/core/historymaintenance.h src/core/historymaintenance.cpp
    src/core/devicehealthmonitor.h src/core/devicehealthmonitor.cpp
    src/core/communicationdiagnostics.h src/core/communicationdiagnostics.cpp
    src/core/businessdatamanager.h src/core/businessdatamanager.cpp
    src/core/alarmmanager.h src/core/alarmmanager.cpp
    src/protocol/businessdatapoint.h
    src/protocol/iprotocolparser.h
    src/protocol/jsonprotocolparser.h src/protocol/jsonprotocolparser.cpp
    src/protocol/protocolmanager.h src/protocol/protocolmanager.cpp
    src/protocol/modbuscodec.h src/protocol/modbuscodec.cpp
    src/protocol/modbusmaster.h src/protocol/modbusmaster.cpp
    src/protocol/modbusregistermap.h src/protocol/modbusregistermap.cpp
    src/protocol/modbuspoller.h src/protocol/modbuspoller.cpp
    src/security/authmanager.h src/security/authmanager.cpp
    src/security/logindialog.h src/security/logindialog.cpp
    src/security/usermanagementpage.h src/security/usermanagementpage.cpp
    src/remote/mqttclient.h src/remote/mqttclient.cpp
    src/remote/websocketclient.h src/remote/websocketclient.cpp
    src/remote/httpclient.h src/remote/httpclient.cpp
    src/remote/remotetelemetryparser.h src/remote/remotetelemetryparser.cpp
    src/remote/remoteconfigstore.h src/remote/remoteconfigstore.cpp
    src/remote/remotepage.h src/remote/remotepage.cpp
    src/plugins/iplugin.h
    src/plugins/pluginmanager.h src/plugins/pluginmanager.cpp
    src/plugins/pluginpage.h src/plugins/pluginpage.cpp
    src/communication/icommunication.h src/communication/icommunication.cpp
    src/communication/simulatedcommunication.h src/communication/simulatedcommunication.cpp
    src/communication/simulatedmodbuscommunication.h src/communication/simulatedmodbuscommunication.cpp
    src/communication/tcpcommunication.h src/communication/tcpcommunication.cpp
    src/communication/udpcommunication.h src/communication/udpcommunication.cpp
    src/communication/communicationmanager.h src/communication/communicationmanager.cpp
    src/communication/communicationworker.h src/communication/communicationworker.cpp
    src/communication/slcanframecodec.h src/communication/slcanframecodec.cpp
    src/communication/virtualcancommunication.h src/communication/virtualcancommunication.cpp
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
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
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
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
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

    add_executable(communication_diagnostics_test
        tests/communication_diagnostics_test.cpp
        src/core/communicationdiagnostics.h
        src/core/communicationdiagnostics.cpp
        src/communication/communicationmanager.h
        src/communication/communicationmanager.cpp
        src/communication/communicationworker.h
        src/communication/communicationworker.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(communication_diagnostics_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(communication_diagnostics_test PRIVATE Qt6::Core Qt6::Sql Qt6::Network)
    add_test(NAME communication_diagnostics_test COMMAND communication_diagnostics_test)

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
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
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

    add_executable(history_maintenance_test
        tests/history_maintenance_test.cpp
        src/core/historymaintenance.h
        src/core/historymaintenance.cpp
    )
    target_include_directories(history_maintenance_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(history_maintenance_test PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME history_maintenance_test COMMAND history_maintenance_test)

    add_executable(modbus_master_test
        tests/modbus_master_test.cpp
        src/protocol/modbusmaster.h
        src/protocol/modbusmaster.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
        src/communication/communicationmanager.h
        src/communication/communicationmanager.cpp
        src/communication/communicationworker.h
        src/communication/communicationworker.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(modbus_master_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(modbus_master_test PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME modbus_master_test COMMAND modbus_master_test)

    add_executable(modbus_register_map_test
        tests/modbus_register_map_test.cpp
        src/protocol/modbusregistermap.h
        src/protocol/modbusregistermap.cpp
        src/protocol/businessdatapoint.h
    )
    target_include_directories(modbus_register_map_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(modbus_register_map_test PRIVATE Qt6::Core)
    add_test(NAME modbus_register_map_test COMMAND modbus_register_map_test)

    add_executable(modbus_poller_test
        tests/modbus_poller_test.cpp
        src/protocol/modbuspoller.h
        src/protocol/modbuspoller.cpp
        src/protocol/modbusmaster.h
        src/protocol/modbusmaster.cpp
        src/protocol/modbusregistermap.h
        src/protocol/modbusregistermap.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
        src/communication/communicationmanager.h
        src/communication/communicationmanager.cpp
        src/communication/communicationworker.h
        src/communication/communicationworker.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/pages/deviceinfo.h
        src/core/businessdatamanager.h
        src/core/businessdatamanager.cpp
        src/protocol/businessdatapoint.h
    )
    target_include_directories(modbus_poller_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(modbus_poller_test PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME modbus_poller_test COMMAND modbus_poller_test)

    add_executable(modbus_codec_test
        tests/modbus_codec_test.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
    )
    target_include_directories(modbus_codec_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(modbus_codec_test PRIVATE Qt6::Core)
    add_test(NAME modbus_codec_test COMMAND modbus_codec_test)

    add_executable(auth_manager_test
        tests/auth_manager_test.cpp
        src/security/authmanager.h
        src/security/authmanager.cpp
    )
    target_include_directories(auth_manager_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(auth_manager_test PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME auth_manager_test COMMAND auth_manager_test)

    add_executable(remote_client_test
        tests/remote_client_test.cpp
        src/remote/mqttclient.h
        src/remote/mqttclient.cpp
        src/remote/websocketclient.h
        src/remote/websocketclient.cpp
        src/remote/httpclient.h
        src/remote/httpclient.cpp
        src/remote/remotetelemetryparser.h
        src/remote/remotetelemetryparser.cpp
    )
    target_include_directories(remote_client_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(remote_client_test PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME remote_client_test COMMAND remote_client_test)

    add_executable(remote_config_store_test
        tests/remote_config_store_test.cpp
        src/remote/remoteconfigstore.h
        src/remote/remoteconfigstore.cpp
        src/remote/remotetelemetryparser.h
        src/remote/remotetelemetryparser.cpp
    )
    target_include_directories(remote_config_store_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(remote_config_store_test PRIVATE Qt6::Core)
    add_test(NAME remote_config_store_test COMMAND remote_config_store_test)

    add_library(sample_industrial_plugin MODULE
        tests/plugins/sampleplugin.cpp
        tests/plugins/sampleplugin.json
        src/plugins/iplugin.h
        src/communication/icommunication.h
        src/communication/icommunication.cpp
    )
    target_include_directories(sample_industrial_plugin PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(sample_industrial_plugin PRIVATE Qt6::Core)
    set_target_properties(sample_industrial_plugin PROPERTIES PREFIX "")

    add_executable(plugin_manager_test
        tests/plugin_manager_test.cpp
        src/plugins/iplugin.h
        src/plugins/pluginmanager.h
        src/plugins/pluginmanager.cpp
        src/communication/icommunication.h
        src/communication/icommunication.cpp
        src/communication/communicationmanager.h
        src/communication/communicationmanager.cpp
        src/communication/communicationworker.h
        src/communication/communicationworker.cpp
        src/communication/simulatedcommunication.h
        src/communication/simulatedcommunication.cpp
        src/communication/simulatedmodbuscommunication.h
        src/communication/simulatedmodbuscommunication.cpp
        src/protocol/modbuscodec.h
        src/protocol/modbuscodec.cpp
        src/communication/tcpcommunication.h
        src/communication/tcpcommunication.cpp
        src/communication/udpcommunication.h
        src/communication/udpcommunication.cpp
        src/pages/deviceinfo.h
    )
    target_include_directories(plugin_manager_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(plugin_manager_test PRIVATE Qt6::Core Qt6::Network)
    add_dependencies(plugin_manager_test sample_industrial_plugin)
    add_test(NAME plugin_manager_test
        COMMAND plugin_manager_test "$<TARGET_FILE:sample_industrial_plugin>")

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

if(TARGET Qt6::SerialBus)
    target_link_libraries(hhh PRIVATE Qt6::SerialBus)
    target_compile_definitions(hhh PRIVATE INDUSTRIALHUB_HAS_SERIALBUS)
    message(STATUS "Qt SerialBus found: VirtualCAN/VIRTUALCANFD drivers enabled.")
else()
    message(STATUS "Qt SerialBus not found: VirtualCAN drivers will be disabled; other communication drivers remain available.")
endif()

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


# Portable ZIP package. Qt runtime deployment is generated above by Qt's deploy script.
set(CPACK_PACKAGE_NAME "IndustrialHub")
set(CPACK_PACKAGE_VENDOR "IndustrialHub")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Industrial equipment monitoring and intelligent maintenance platform")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "IndustrialHub")
set(CPACK_PACKAGE_FILE_NAME "IndustrialHub-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
