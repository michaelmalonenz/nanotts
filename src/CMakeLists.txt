
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

target_link_libraries(
    nanotts
    INTERFACE
        cxxopts::cxxopts
    PUBLIC
        ttspico
        asound
)