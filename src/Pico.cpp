/* nanotts.cpp
 *
 * Copyright (C) 2014 Greg Naughton <greg@naughton.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *    Convert text to .wav using svox text-to-speech system.
 *    Rewrite of pico2wave.c
 *
 */

#include <cstring>

#include "Pico.hpp"
#include "Listener.hpp"

pads_t Boilerplate::pads[] = {
    {"speed", "<speed level=\"%d\">", "</speed>", 0},
    {"pitch", "<pitch level=\"%d\">", "</pitch>", 0},
    {"volume", "<volume level=\"%d\">", "</volume>", 0}};


Pico::Pico() {
    picoSystem              = 0;
    picoTaResource          = 0;
    picoSgResource          = 0;
    picoEngine              = 0;
    sdOutFile               = 0;
    picoLingwarePath        = 0;
    out_filename            = 0;

    strcpy( picoVoiceName, "PicoVoice" );

    total_text_length       = 0;
    text_remaining          = 0;
    listener                = 0;
    modifiers               = 0;

    picoMemArea             = 0;
    picoTaFileName          = 0;
    picoSgFileName          = 0;
    picoTaResourceName      = 0;
    picoSgResourceName      = 0;

    pico_writeWavPcm        = false;
}

Pico::~Pico() {
    if ( picoLingwarePath ) {
        delete[] picoLingwarePath;
    }

    cleanup();

    if ( picoMemArea )
        free( picoMemArea );
    if ( picoTaFileName )
        free( picoTaFileName );
    if ( picoSgFileName )
        free( picoSgFileName );
    if ( picoTaResourceName )
        free( picoTaResourceName );
    if ( picoSgResourceName )
        free( picoSgResourceName );
}

void Pico::setLangFilePath( const char * path ) {
    unsigned int len = strlen( path ) + 1;
    picoLingwarePath = new char[ len ];
    strcpy( picoLingwarePath, path );
}

int Pico::initializeSystem()
{
    const int       PICO_MEM_SIZE           = 2500000;
    pico_Retstring  outMessage;
    int             ret;

    picoMemArea = malloc( PICO_MEM_SIZE );

    if ( (ret = pico_initialize( picoMemArea, PICO_MEM_SIZE, &picoSystem )) ) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot initialize pico (%i): %s\n", ret, outMessage );

        pico_terminate(&picoSystem);
        picoSystem = 0;
        return -1;
    }

    /* Load the text analysis Lingware resource file.   */
    picoTaFileName = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );

    // path
    if ( !picoLingwarePath )
        setLangFilePath();
    strcpy((char *) picoTaFileName, picoLingwarePath);

    // check for connecting slash
    unsigned int len = strlen( (const char*)picoTaFileName );
    if ( picoTaFileName[len-1] != '/' )
        strcat((char*) picoTaFileName, "/");

    // langfile name
    strcat( (char *) picoTaFileName, voices.getTaName() );

    // attempt to load it
    if ( (ret = pico_loadResource(picoSystem, picoTaFileName, &picoTaResource)) ) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot load text analysis resource file (%i): %s\n", ret, outMessage );
        goto unloadTaResource;
    }

    /* Load the signal generation Lingware resource file.   */
    picoSgFileName = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );

    strcpy((char *) picoSgFileName,   picoLingwarePath );
    if ( picoSgFileName[len-1] != '/' )
        strcat((char*) picoSgFileName, "/");
    strcat((char *) picoSgFileName,   voices.getSgName() );

    if ( (ret = pico_loadResource(picoSystem, picoSgFileName, &picoSgResource)) ) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot load signal generation Lingware resource file (%i): %s\n", ret, outMessage );
        goto unloadSgResource;
    }

    /* Get the text analysis resource name.     */
    picoTaResourceName = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
    if((ret = pico_getResourceName( picoSystem, picoTaResource, (char *) picoTaResourceName ))) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot get the text analysis resource name (%i): %s\n", ret, outMessage );
        goto unloadSgResource;
    }

    /* Get the signal generation resource name. */
    picoSgResourceName = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
    if((ret = pico_getResourceName( picoSystem, picoSgResource, (char *) picoSgResourceName ))) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot get the signal generation resource name (%i): %s\n", ret, outMessage );
        goto unloadSgResource;
    }

    /* Create a voice definition.   */
    if((ret = pico_createVoiceDefinition( picoSystem, (const pico_Char *) picoVoiceName ))) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot create voice definition (%i): %s\n", ret, outMessage );
        goto unloadSgResource;
    }

    /* Add the text analysis resource to the voice. */
    if((ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) picoVoiceName, picoTaResourceName ))) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot add the text analysis resource to the voice (%i): %s\n", ret, outMessage );
        goto unloadSgResource;
    }

    /* Add the signal generation resource to the voice. */
    if((ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) picoVoiceName, picoSgResourceName ))) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot add the signal generation resource to the voice (%i): %s\n", ret, outMessage );
        goto unloadSgResource;
    }

    /* Create a new Pico engine. */
    if((ret = pico_newEngine( picoSystem, (const pico_Char *) picoVoiceName, &picoEngine ))) {
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        fprintf( stderr, "Cannot create a new pico engine (%i): %s\n", ret, outMessage );
        goto disposeEngine;
    }

    /* success */
    return 0;


    //
    // partial shutdowns below this line
    //  for pico cleanup in case of startup abort
    //
disposeEngine:
    if (picoEngine) {
        pico_disposeEngine( picoSystem, &picoEngine );
        pico_releaseVoiceDefinition( picoSystem, (pico_Char *) picoVoiceName );
        picoEngine = 0;
    }
unloadSgResource:
    if (picoSgResource) {
        pico_unloadResource( picoSystem, &picoSgResource );
        picoSgResource = 0;
    }
unloadTaResource:
    if (picoTaResource) {
        pico_unloadResource( picoSystem, &picoTaResource );
        picoTaResource = 0;
    }

    if (picoSystem) {
        pico_terminate(&picoSystem);
        picoSystem = 0;
    }

    return -1;
}

void Pico::cleanup()
{
    if ( sdOutFile ) {
        picoos_Common common = (picoos_Common) pico_sysGetCommon(picoSystem);
        picoos_sdfCloseOut(common, &sdOutFile);
        sdOutFile = 0;
    }

    if (picoEngine) {
        pico_disposeEngine( picoSystem, &picoEngine );
        pico_releaseVoiceDefinition( picoSystem, (pico_Char *) picoVoiceName );
        picoEngine = 0;
    }

    if (picoSgResource) {
        pico_unloadResource( picoSystem, &picoSgResource );
        picoSgResource = 0;
    }

    if (picoTaResource) {
        pico_unloadResource( picoSystem, &picoTaResource );
        picoTaResource = 0;
    }

    if (picoSystem) {
        pico_terminate(&picoSystem);
        picoSystem = 0;
    }
}

void Pico::sendTextForProcessing( unsigned char * words, long long int word_len )
{
    local_text          = (pico_Char *) words;
    total_text_length   = word_len;
}

int Pico::process()
{
    const int       MAX_OUTBUF_SIZE     = 128;
    const int       PCM_BUFFER_SIZE     = 256;
    pico_Char *     inp                 = 0;
    pico_Int16      bytes_sent, bytes_recv, out_data_type;
    short           outbuf[MAX_OUTBUF_SIZE/2];
    pico_Retstring  outMessage;
    char            pcm_buffer[ PCM_BUFFER_SIZE ];
    int             ret, getstatus;
    picoos_bool     done                = TRUE;

    bool            do_startpad         = false;
    bool            do_endpad           = false;

    // pads are optional, but can be provided to set pico-modifiers
    if ( modifiers ) {
        do_startpad = true;
        do_endpad = true;
        unsigned int len;
        inp = (pico_Char *) modifiers->getOpener( &len );
        text_remaining = len;
        fprintf( stderr, "%s", modifiers->getStatusMessage() );
    } else {
        inp = (pico_Char *) local_text;
    }

    unsigned int bufused = 0;
    memset( pcm_buffer, 0, PCM_BUFFER_SIZE );

    // open output WAVE/PCM for writing
    if ( pico_writeWavPcm ) {
        picoos_Common common = (picoos_Common) pico_sysGetCommon(picoSystem);
        if ( TRUE != (done=picoos_sdfOpenOut(common, &sdOutFile, (picoos_char *)out_filename, SAMPLE_FREQ_16KHZ, PICOOS_ENC_LIN)) ) {
            fprintf( stderr, "Cannot open output wave file: %s\n", out_filename );
            return -1;
        }
    }

    long long int text_length = total_text_length;

    /* synthesis loop   */
    while(1)
    {
        //−32,768 to 32,767.
        if (text_remaining <= 0)
        {
            // text_remaining run-out; end pre-pad text
            if ( do_startpad ) {
                do_startpad = false;
                // start normal text
                inp = (pico_Char *) local_text;
                int increment = text_length >= 32767 ? 32767 : text_length;
                text_length -= increment;
                text_remaining = increment;
            }
            // main text ran out
            else if ( text_length <= 0 ) {
                // tack end_pad on the end
                if ( do_endpad ) {
                    do_endpad = false;
                    unsigned int len;
                    inp = (pico_Char *) modifiers->getCloser( &len );
                    text_remaining = len;
                } else {
                    break; /* done */
                }
            }
            // continue feed main text
            else {
                int increment = text_length >= 32767 ? 32767 : text_length;
                text_length -= increment;
                text_remaining = increment;
            }
        }

        /* Feed the text into the engine.   */
        if ( (ret = pico_putTextUtf8(picoEngine, inp, text_remaining, &bytes_sent)) ) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf( stderr, "Cannot put Text (%i): %s\n", ret, outMessage );
            return -2;
        }

        text_remaining -= bytes_sent;
        inp += bytes_sent;

        do {

            /* Retrieve the samples */
            getstatus = pico_getData( picoEngine, (void *) outbuf, MAX_OUTBUF_SIZE, &bytes_recv, &out_data_type );
            if ( (getstatus !=PICO_STEP_BUSY) && (getstatus !=PICO_STEP_IDLE) ) {
                pico_getSystemStatusMessage(picoSystem, getstatus, outMessage);
                fprintf( stderr, "Cannot get Data (%i): %s\n", getstatus, outMessage );
                return -4;
            }

            /* copy partial encoding and get more bytes */
            if ( bytes_recv > 0 )
            {
                if ( (bufused + bytes_recv) <= PCM_BUFFER_SIZE ) {
                    memcpy( pcm_buffer+bufused, (int8_t *)outbuf, bytes_recv );
                    bufused += bytes_recv;
                }

                /* or write the buffer to wavefile, and retrieve any leftover decoding bytes */
                else
                {
                    if ( pico_writeWavPcm ) {
                        done = picoos_sdfPutSamples( sdOutFile, bufused / 2, (picoos_int16*) pcm_buffer );
                    }

                    if ( listener ) {
                        listener->writeData( (short*)pcm_buffer, bufused/2 );
                    }

                    bufused = 0;
                    memcpy( pcm_buffer, (int8_t *)outbuf, bytes_recv );
                    bufused += bytes_recv;
                }
            }

        } while (PICO_STEP_BUSY == getstatus);

        /* This chunk of synthesis is finished; pass the remaining samples. */
        if ( pico_writeWavPcm ) {
            done = picoos_sdfPutSamples( sdOutFile, bufused / 2, (picoos_int16*) pcm_buffer );
        }

        if ( listener ) {
            listener->writeData( (short*)pcm_buffer, bufused/2 );
        }
    }

    // close output wave file, so it can be opened elsewhere
    if ( sdOutFile ) {
        picoos_Common common = (picoos_Common) pico_sysGetCommon(picoSystem);
        picoos_sdfCloseOut(common, &sdOutFile);
        sdOutFile = 0;

        // report
        int bytes = fileSize( out_filename );
        fprintf( stderr, "wrote \"%s\" (%d bytes)\n", out_filename, bytes );
    }

    return bufused;
}

int Pico::setVoice( const char * v ) {
    int r = voices.setVoice( v ) ;
    fprintf( stderr, "using lang: %s\n", voices.getVoice() );
    return r;
}

int Pico::fileSize( const char * filename ) {
    FILE * file_p = fopen( filename, "rb" );
    if ( !file_p )
        return -1;
    fseek( file_p, 0L, SEEK_SET );
    int beginning = ftell( file_p );
    fseek( file_p, 0L, SEEK_END );
    int end = ftell( file_p );
    return end - beginning;
}

void Pico::setListener( Listener<short> * listener ) {
    this->listener = listener;
}

void Pico::addModifiers( Boilerplate * modifiers ) {
    this->modifiers = modifiers;
}
//////////////////////////////////////////////////////////////////