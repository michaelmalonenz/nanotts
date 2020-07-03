#include "PicoVoices.h"

PicoVoices_t::PicoVoices_t() {

    /* supported voices
       Pico does not seperately specify the voice and locale.   */
    const char * _picoSupportedLangIso3[]        = { "eng",              "eng",              "deu",              "spa",              "fra",              "ita" };
    const char * _picoSupportedCountryIso3[]     = { "USA",              "GBR",              "DEU",              "ESP",              "FRA",              "ITA" };
    const char * _picoSupportedLang[]            = { "en-US",            "en-GB",            "de-DE",            "es-ES",            "fr-FR",            "it-IT" };
    const char * _picoInternalLang[]             = { "en-US",            "en-GB",            "de-DE",            "es-ES",            "fr-FR",            "it-IT" };
    const char * _picoInternalTaLingware[]       = { "en-US_ta.bin",     "en-GB_ta.bin",     "de-DE_ta.bin",     "es-ES_ta.bin",     "fr-FR_ta.bin",     "it-IT_ta.bin" };
    const char * _picoInternalSgLingware[]       = { "en-US_lh0_sg.bin", "en-GB_kh0_sg.bin", "de-DE_gl0_sg.bin", "es-ES_zl0_sg.bin", "fr-FR_nk0_sg.bin", "it-IT_cm0_sg.bin" };
    const char * _picoInternalUtppLingware[]     = { "en-US_utpp.bin",   "en-GB_utpp.bin",   "de-DE_utpp.bin",   "es-ES_utpp.bin",   "fr-FR_utpp.bin",   "it-IT_utpp.bin" };

    picoSupportedLangIso3 = new char*[6];
    picoSupportedCountryIso3 = new char*[6];
    picoSupportedLang = new char*[6];
    picoInternalLang = new char*[6];
    picoInternalTaLingware = new char*[6];
    picoInternalSgLingware = new char*[6];
    picoInternalUtppLingware = new char*[6];

    char * memory = new char[140 * 7];
    memset( memory, 0, 7 * 140 );
    float f;

    for ( int i = 0; i < 6; i++ ) {
        f = i;
        picoSupportedLangIso3[i]    = &memory[ (int)(f / 7 * 140) + 140 * 0 ];
        picoSupportedCountryIso3[i] = &memory[ (int)(f / 7 * 140) + 140 * 1 ];
        picoSupportedLang[i]        = &memory[ (int)(f / 7 * 140) + 140 * 2 ];
        picoInternalLang[i]         = &memory[ (int)(f / 7 * 140) + 140 * 3 ];
        picoInternalTaLingware[i]   = &memory[ (int)(f / 7 * 140) + 140 * 4 ];
        picoInternalSgLingware[i]   = &memory[ (int)(f / 7 * 140) + 140 * 5 ];
        picoInternalUtppLingware[i] = &memory[ (int)(f / 7 * 140) + 140 * 6 ];

        strcpy( picoSupportedLangIso3[i] , _picoSupportedLangIso3[i] );
        strcpy( picoSupportedCountryIso3[i] , _picoSupportedCountryIso3[i] );
        strcpy( picoSupportedLang[i] , _picoSupportedLang[i] );
        strcpy( picoInternalLang[i] , _picoInternalLang[i] );
        strcpy( picoInternalTaLingware[i] , _picoInternalTaLingware[i]  );
        strcpy( picoInternalSgLingware[i] , _picoInternalSgLingware[i] );
        strcpy( picoInternalUtppLingware[i] , _picoInternalUtppLingware[i] );
    }
}

PicoVoices_t::~PicoVoices_t() {
    delete[] picoSupportedLangIso3[0];
    delete[] picoSupportedLangIso3;
    delete[] picoSupportedCountryIso3;
    delete[] picoSupportedLang;
    delete[] picoInternalLang;
    delete[] picoInternalTaLingware;
    delete[] picoInternalSgLingware;
    delete[] picoInternalUtppLingware;
}

int PicoVoices_t::setVoice( int i ) {
    if ( i >= 0 && i < 6 ) {
        voice = i;
        return 0;
    }
    return -1;
}

int PicoVoices_t::setVoice( const char * voc ) {
    char ** matchable[] = { picoSupportedLang, picoSupportedLangIso3, picoSupportedCountryIso3, 0 };
    for ( int i = 0 ; i < 6; i++ ) {
        int j = 0;
        while ( matchable[j] )
        {
            if ( strcmp( (matchable[j])[i], voc ) == 0 ) {
                voice = i;
                return i;
            }
            ++j;
        }
    }
    return -1;
}

const char * PicoVoices_t::getTaName() {
    return picoInternalTaLingware[ voice ];
}

const char * PicoVoices_t::getSgName() {
    return picoInternalSgLingware[ voice ];
}

const char * PicoVoices_t::getVoice() {
    return picoInternalLang[ voice ];
}
