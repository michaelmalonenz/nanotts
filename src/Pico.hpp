#pragma once

extern "C"
{
#include "picoapi.h"
#include "picoapid.h"
#include "picoos.h"
}
#include <string>
#include "Boilerplate.hpp"
#include "Listener.hpp"
#include "PicoVoices.h"

/*
================================================
Pico

class to encapsulate the workings of the SVox PicoTTS System
================================================
*/
class Pico
{
private:
    PicoVoices_t voices;

    pico_System picoSystem;
    pico_Resource picoTaResource;
    pico_Resource picoSgResource;
    pico_Engine picoEngine;
    picoos_SDFile sdOutFile;
    char *out_filename;

    pico_Char *local_text;
    pico_Int16 text_remaining;
    long long int total_text_length;
    std::string picoLingwarePath;

    char picoVoiceName[10];
    Listener<short> *listener;
    Boilerplate *modifiers;

    void *picoMemArea;
    pico_Char *picoTaFileName;
    pico_Char *picoSgFileName;
    pico_Char *picoTaResourceName;
    pico_Char *picoSgResourceName;
    bool pico_writeWavPcm;

public:
    Pico();
    virtual ~Pico();

    void setLangFilePath(const std::string& path);
    int initializeSystem();
    void cleanup();
    void sendTextForProcessing(unsigned char *, long long int);
    int process();

    int setVoice(const char *);
    void setOutFilename(const char *fn) { out_filename = const_cast<char *>(fn); }

    int fileSize(const char *filename);
    void setListener(Listener<short> *);
    void addModifiers(Boilerplate *);
    void writeWavePcm(bool new_setting = true) { pico_writeWavPcm = new_setting; }
};
