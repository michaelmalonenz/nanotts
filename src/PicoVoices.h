#ifndef __PICO_VOICES__
#define __PICO_VOICES__ 1

#include <stdio.h>
#include <string.h>

class PicoVoices_t {
private:
    int voice;
    
    char ** picoSupportedLangIso3;
    char ** picoSupportedCountryIso3;
    char ** picoSupportedLang;
    char ** picoInternalLang;
    char ** picoInternalTaLingware;
    char ** picoInternalSgLingware;
    char ** picoInternalUtppLingware;

public:
    PicoVoices_t();
    ~PicoVoices_t();
    int setVoice( int );
    int setVoice( const char * );
    const char * getTaName() ;
    const char * getSgName() ;
    const char * getVoice() ;
};

#endif /* __PICO_VOICES__ */
