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
#include "Nano.hpp"
#include <fmt/format.h>
#include "cxxopts.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _USE_ALSA
#include "Player_Alsa.h"
#endif

int GetNextLowestFilenameNumber(const char *prefix, const char *suffix, int zeropad);

#define PICO_DEFAULT_SPEED 0.88f
#define PICO_DEFAULT_PITCH 1.05f
#define PICO_DEFAULT_VOLUME 1.00f

#define FILE_OUTPUT_SUFFIX ".wav"
#define FILENAME_NUMBERING_LEADING_ZEROS 4

// software version information
#define CANONICAL_NAME "nanotts"
#define CONFIG_DIR_NAME ".nanotts"
#define VERSION_MAJOR "0"
#define CON1 "."
#define VERSION_MINOR "9"
#define CON2 "-"
// a = alpha, b = beta, rc = release-candidate, r = release
#define RELEASE_TYPE "a"
#include "release_version.h" // RELEASE_VERSION, increments every build
#define SOFTWARE_VERSION VERSION_MAJOR CON1 VERSION_MINOR CON2 RELEASE_TYPE RELEASE_VERSION
#ifdef _USE_ALSA
#define VERSIONED_NAME CANONICAL_NAME "-" SOFTWARE_VERSION "-alsa"
#else
#define VERSIONED_NAME CANONICAL_NAME "-" SOFTWARE_VERSION
#endif

Nano::Nano(int i, char **v) : my_argc(i),
                              my_argv(v),
                              voice(),
                              langfiledir(),
                              prefix(),
                              out_filename(),
                              in_filename(),
                              words(),
                              listener(this)
{
    sprintf(suffix, FILE_OUTPUT_SUFFIX);
    in_fp = 0;
    out_fp = 0;
    input_buffer = 0;
    input_size = 0;

    silence_output = true;
}

Nano::~Nano()
{
    if (input_buffer)
    {
        switch (in_mode)
        {
        case IN_STDIN:
            delete[] input_buffer;
            break;
        case IN_SINGLE_FILE:
            delete mmfile;
            mmfile = 0;
        default:
            break;
        }
        input_buffer = 0;
    }

    if (in_fp != 0 && in_fp != stdin)
    {
        fclose(in_fp);
        in_fp = 0;
    }
    if (out_fp != 0 && out_fp != stdout)
    {
        fclose(out_fp);
        out_fp = 0;
    }
}

int Nano::parse_commandline_arguments()
{

    cxxopts::Options options("NanoTTS", "A flexible text-to-speech engine");
    options.add_options()("h,help", "Print this help")("version", "Print the version")("i,input", "input text argument", cxxopts::value<std::string>())("f,file", "use the given text file as input", cxxopts::value<std::string>())("o,output", "Write output to WAV/PCM file (enables WAV output)", cxxopts::value<std::string>())("w,wav", "Write output to WAV file, will generate filename if '-o' option not provided")("p,play", "Play audio output")("m,no-play", "do NOT play output on PC's soundcard")("c,stdout", "Send raw PCM output to stdout")("x,prefix", "Set the file prefix (e.g. 'MyRecording'). Generated files will be auto-numbered. Good for running multiple times with different inputs", cxxopts::value<std::string>()->default_value("nanotts-output-"))("speed", "change voice speed <0.2-5.0>", cxxopts::value<float>()->default_value("0.88"))("pitch", "change the voice pitch <0.5-2.0>", cxxopts::value<float>()->default_value("1.05"))("volume", "change the voice volume <0.0-5.0> (>1.0 may result in degraded quality)", cxxopts::value<float>()->default_value("1.00"))("v,voice", "Set the voice to use.  Possible voices: en-US, en-GB, de-DE, es-ES, fr-FR, it-IT", cxxopts::value<std::string>()->default_value("en-GB"))("l,lang-file-dir", "the directory containing the language files", cxxopts::value<std::string>()->default_value("./lang"));
    auto args = options.parse(my_argc, my_argv);

    if (args["h"].count() > 0)
    {
        std::cout << options.help() << std::endl;
        exit(-1);
    }

    if (args["version"].count() > 0)
    {
        std::cout << VERSIONED_NAME << std::endl;
        exit(-666);
    }

    modifiers.setSpeed(args["speed"].as<float>());
    modifiers.setPitch(args["pitch"].as<float>());
    modifiers.setVolume(args["volume"].as<float>());

    // inputs and especially output are explicit
    in_mode = IN_NOT_SET;
    out_mode = OUT_NOT_SET;

    voice = args["voice"].as<std::string>();
    langfiledir = args["l"].as<std::string>();

    // need to validate the langfilefile dir.  Possibly use install dir for pico?
    // "/usr/share/pico/lang"

    if (args["f"].count() > 0)
    {
        if (in_mode != IN_NOT_SET)
        {
            fprintf(stderr, " **error: multiple inputs\n\n");
            return -1;
        }
        in_mode = IN_SINGLE_FILE;
        in_filename = args["f"].as<std::string>();
    }

    if (args["i"].count() > 0)
    {
        if (in_mode != IN_NOT_SET)
        {
            fprintf(stderr, " **error: multiple inputs\n\n");
            return -1;
        }
        in_mode = IN_CMDLINE_ARG;
        words = args["i"].as<std::string>();
    }

    if (args["o"].count() > 0)
    {
        out_mode |= OUT_SINGLE_FILE;
        out_filename = args["o"].as<std::string>();
    }

    if (!isatty(fileno(stdin)))
    {
        in_mode = IN_STDIN;
    }

    if (args["x"].count() > 0)
        out_mode |= OUT_SINGLE_FILE;
    prefix = args["x"].as<std::string>();

    // OUTPUTS
    if (args["p"].count() > 0)
    {
        silence_output = false;
        out_mode |= OUT_PLAYBACK;
    }

    if (args["c"].count() > 0)
        out_mode |= OUT_STDOUT;

    if (args["w"].count() > 0)
        out_mode |= OUT_SINGLE_FILE;

    if (args["m"].count() > 0)
    {
        silence_output = true;
        out_mode &= ~OUT_PLAYBACK;
    }

    for (int i = 1; i < my_argc; i++)
    {
        // INPUTS
        // else if (strcmp(my_argv[i], "--files") == 0)
        // {
        //     WARN_UNMATCHED_INPUTS();
        //     if (in_mode != IN_NOT_SET)
        //     {
        //         fprintf(stderr, " **error: multiple inputs\n\n");
        //         return -1;
        //     }
        //     in_mode = IN_MULTIPLE_FILES;
        //     // FIXME: get array of char*filename
        //     if ((in_filename = copy_arg(i + 1)) == 0)
        //         return -1;
        //     ++i;
        // }
        if (strcmp(my_argv[i], "-") == 0)
        {
            if (in_mode != IN_NOT_SET && in_mode != IN_STDIN)
            {
                fprintf(stderr, " **error: multiple inputs\n\n");
                return -1;
            }
            in_mode = IN_STDIN;
            in_fp = stdin;
        }
    }

    if (verify_input_output() < 0)
    {
        return -3;
    }

    if (out_filename.empty())
    {
        int next = GetNextLowestFilenameNumber(prefix.c_str(), suffix, FILENAME_NUMBERING_LEADING_ZEROS);
        out_filename = fmt::format("{}{:04d}{}", prefix, next, suffix);
    }

    //
    if (setup_input_output() < 0)
    {
        return -2;
    }

    return 0;
}

int Nano::setup_input_output()
{
#define __NOT_IMPL__                                  \
    do                                                \
    {                                                 \
        fprintf(stderr, " ** not implemented ** \n"); \
        return -1;                                    \
    } while (0);

    switch (in_mode)
    {
    case IN_STDIN:
        if (in_fp)
            break;
        // detect if stdin is coming from a pipe
        if (!isatty(fileno(stdin)))
        { // On windows prefix with underscores: _isatty, _fileno
            in_fp = stdin;
        }
        else
        {
            if (words.empty())
            {
                fprintf(stderr, " **error: reading from stdin.\n\n");
                return -1;
            }
        }
        break;
    case IN_SINGLE_FILE:
        // mmap elsewhere
        break;
    case IN_CMDLINE_ARG:
    case IN_CMDLINE_TRAILING:
        break;
    case IN_MULTIPLE_FILES:
        __NOT_IMPL__
    default:
        __NOT_IMPL__
        break;
    };

    int modes[] = {OUT_STDOUT, OUT_SINGLE_FILE, OUT_PLAYBACK, OUT_MULTIPLE_FILES};
    for (int i = 0; i < 4; ++i)
    {
        int test_mode = modes[i] & out_mode;
        switch (test_mode)
        {
        case OUT_SINGLE_FILE:
            break;
        case OUT_PLAYBACK:
            break;
        case OUT_STDOUT:
            out_fp = stdout;
            fprintf(stderr, "writing pcm stream to stdout\n");
            break;
        case OUT_MULTIPLE_FILES:
            __NOT_IMPL__
            break;
        default:
            break;
        }
    }

    if ((out_mode & (OUT_PLAYBACK | OUT_STDOUT)) == (OUT_PLAYBACK | OUT_STDOUT))
    {
        SetListenerPlaybackAndStdout();
    }
    else if (out_mode & OUT_PLAYBACK)
    {
        SetListenerPlayback();
    }
    else if (out_mode & OUT_STDOUT)
    {
        SetListenerStdout();
    }

#undef __NOT_IMPL__
    return 0;
}

int Nano::verify_input_output()
{
    if (in_mode == IN_NOT_SET)
    {
        fprintf(stderr, " **error: no input\n\n");
        return -1;
    }

    if (out_mode == OUT_NOT_SET)
    {
        fprintf(stderr, " **error: no output mode selected\n\n");
        return -2;
    }

    return 0;
}

void Nano::SetListenerStdout()
{
    listener.setCallback(&Nano::write_short_to_stdout);
}
void Nano::SetListenerPlayback()
{
#ifdef _USE_ALSA
    streamHandler.player = new Player_Alsa();
#endif
    streamHandler.StreamOpen();
    listener.setCallback(&Nano::write_short_to_playback);
}
void Nano::SetListenerPlaybackAndStdout()
{
#ifdef _USE_ALSA
    streamHandler.player = new Player_Alsa();
#endif
    streamHandler.StreamOpen();
    listener.setCallback(&Nano::write_short_to_playback_and_stdout);
}

// puts input into *data, and number_bytes into bytes
// returns 0 on no more data
int Nano::ProduceInput(unsigned char **data, unsigned int *bytes)
{
    switch (in_mode)
    {
    case IN_STDIN:
        input_buffer = new unsigned char[1000000];
        memset(input_buffer, 0, 1000000);
        input_size = fread(input_buffer, 1, 1000000, stdin);
        *data = input_buffer;
        *bytes = input_size + 1; /* pico expects 1 for terminating '\0' */
        fprintf(stderr, "read: %u bytes from stdin\n", input_size);
        break;
    case IN_SINGLE_FILE:
        mmfile = new mmfile_t(in_filename.c_str());
        *data = mmfile->data;
        *bytes = mmfile->size + 1; /* 1 additional for terminating '\0' */
        fprintf(stderr, "read: %u bytes from \"%s\"\n", mmfile->size, in_filename.c_str());
        break;
    case IN_CMDLINE_ARG:
    case IN_CMDLINE_TRAILING:
        *data = (unsigned char *)words.c_str();
        *bytes = words.length() + 1; /* 1 additional for terminating '\0' */
        fprintf(stderr, "read: %u bytes from command line\n", *bytes);
        break;
    case IN_MULTIPLE_FILES:
        fprintf(stderr, "multiple files not supported\n");
        return -1;
    default:
        fprintf(stderr, "unknown input\n");
        return -1;
    }

    return 0;
}

//
int Nano::playOutput()
{
    if (silence_output)
        return 0;

    return 1;
}

const std::string &Nano::getVoice()
{
    return voice;
}

const std::string &Nano::getLangFilePath()
{
    return langfiledir;
}

void Nano::write_short_to_stdout(short *data, unsigned int shorts)
{
    if (out_mode & OUT_STDOUT)
        fwrite(data, 2, shorts, out_fp);
}

void Nano::write_short_to_playback(short *data, unsigned int shorts)
{
    if (out_mode & OUT_PLAYBACK)
        streamHandler.SubmitFrames((unsigned char *)data, shorts);
}

void Nano::write_short_to_playback_and_stdout(short *data, unsigned int shorts)
{
    if (out_mode & OUT_STDOUT)
        fwrite(data, 2, shorts, out_fp);
    if (out_mode & OUT_PLAYBACK)
        streamHandler.SubmitFrames((unsigned char *)data, shorts);
}

Listener<short> *Nano::getListener()
{
    if (!listener.hasConsumer())
        return 0;
    return &listener;
}

Boilerplate *Nano::getModifiers()
{
    if (modifiers.isChanged())
        return &modifiers;
    return 0;
}