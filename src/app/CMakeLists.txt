add_executable(satgraf main.cpp resources.qrc)
target_link_libraries(satgraf PRIVATE satgraf_core satgraf_gui CLI11::CLI11)

if(APPLE)
    set(MACOS_ICON "${CMAKE_SOURCE_DIR}/icon.icns")
    if(EXISTS "${MACOS_ICON}")
        target_sources(satgraf PRIVATE "${MACOS_ICON}")
        set_source_files_properties("${MACOS_ICON}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    endif()

    set_target_properties(satgraf PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "satgraf"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.satgraf.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "MIT License"
        MACOSX_BUNDLE_ICON_FILE "icon.icns")

    install(TARGETS satgraf
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin)

    option(SATGRAF_MACOS_RUN_MACDEPLOYQT
        "Run Qt deployment (macdeployqt) during install/package."
        ON)
    if(SATGRAF_MACOS_RUN_MACDEPLOYQT)
        if(COMMAND qt_generate_deploy_app_script)
            qt_generate_deploy_app_script(
                TARGET satgraf
                OUTPUT_SCRIPT SATGRAF_QT_DEPLOY_SCRIPT
                NO_UNSUPPORTED_PLATFORM_ERROR)
            install(SCRIPT "${SATGRAF_QT_DEPLOY_SCRIPT}")
        else()
            install(CODE [[
                find_program(MACDEPLOYQT_EXECUTABLE
                    NAMES macdeployqt
                    HINTS
                        "$ENV{Qt6_DIR}/../../../bin"
                        "$ENV{Qt_DIR}/bin"
                        "/opt/homebrew/bin"
                        "/usr/local/bin")
                if(NOT MACDEPLOYQT_EXECUTABLE)
                    message(FATAL_ERROR "qt_generate_deploy_app_script unavailable and macdeployqt not found.")
                endif()

                set(APP_BUNDLE "${CMAKE_INSTALL_PREFIX}/satgraf.app")
                execute_process(
                    COMMAND "${MACDEPLOYQT_EXECUTABLE}" "${APP_BUNDLE}" -always-overwrite
                    RESULT_VARIABLE MACDEPLOYQT_RESULT)
                if(NOT MACDEPLOYQT_RESULT EQUAL 0)
                    message(FATAL_ERROR "macdeployqt failed with code ${MACDEPLOYQT_RESULT}")
                endif()
            ]])
        endif()
    endif()
else()
    install(TARGETS satgraf RUNTIME DESTINATION bin)
endif()
