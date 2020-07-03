
set(SOURCES
    lowest_file_number.cpp
    main.cpp
    mmfile.cpp
    Player_Alsa.cpp
    StreamHandler.cpp
    wav.cpp
)

add_executable(nanotts ${SOURCES})

target_include_directories(nanotts PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")

target_link_libraries(nanotts PUBLIC ttspico asound)