cmake_minimum_required(VERSION 3.19)
project(hhh LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

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
    src/pages/AlarmPage.h src/pages/ConfigPage.h src/pages/DashboardPage.h src/pages/DevicePage.h src/pages/HistoryPage.h src/pages/MonitorPage.h src/pages/SettingsPage.h
    src/pages/deviceinfo.h
    src/ui/MainWindow/MainWindow.ui
    src/ui/MainWindow/MainWindow.h
    src/ui/MainWindow/MainWindow.cpp
    src/pages/AlarmPage.cpp src/pages/ConfigPage.cpp src/pages/DashboardPage.cpp src/pages/DevicePage.cpp src/pages/HistoryPage.cpp src/pages/MonitorPage.cpp src/pages/SettingsPage.cpp
    src/pages/AlarmPage.ui src/pages/ConfigPage.ui src/pages/DashboardPage.ui src/pages/DevicePage.ui src/pages/HistoryPage.ui src/pages/MonitorPage.ui src/pages/SettingsPage.ui
    CustomFolder/customlistwidget.h CustomFolder/customlistwidget.cpp
    src/pages/devicedialog.h src/pages/devicedialog.cpp src/pages/devicedialog.ui


)
target_include_directories(hhh PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(hhh
    PRIVATE
        Qt::Core
        Qt::Widgets
)


install(TARGETS hhh
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET hhh
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
