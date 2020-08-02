
set(SOURCES
    Pico.cpp
    PicoVoices.cpp
    lowest_file_number.cpp
    main.cpp
    mmfile.cpp
    Player_Alsa.cpp
    StreamHandler.cpp
    wav.cpp
)


add_executable(nanotts "${SOURCES}")

# for release_version.h
target_include_directories(nanotts PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")
set_property(TARGET nanotts PROPERTY CXX_STANDARD 20)

find_package(ALSA QUIET)
if (ALSA_FOUND)
    target_compile_definitions(nanotts PRIVATE -D_USE_ALSA)
endif()

target_link_libraries(
    nanotts
    INTERFACE
        cxxopts::cxxopts
    PUBLIC
        ttspico
        fmt
        ${ALSA_LIBRARIES}
)