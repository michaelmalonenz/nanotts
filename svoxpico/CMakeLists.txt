project(svoxpico)

set(SOURCES picoacph.c
            picoapi.c
            picobase.c
            picocep.c
            picoctrl.c
            picodata.c
            picodbg.c
            picoextapi.c
            picofftsg.c
            picokdbg.c
            picokdt.c
            picokfst.c
            picoklex.c
            picoknow.c
            picokpdf.c
            picokpr.c
            picoktab.c
            picoos.c
            picopal.c
            picopam.c
            picopr.c
            picorsrc.c
            picosa.c
            picosig.c
            picosig2.c
            picospho.c
            picotok.c
            picotrns.c
            picowa.c
)


add_library(ttspico STATIC ${SOURCES})

target_include_directories(ttspico INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
