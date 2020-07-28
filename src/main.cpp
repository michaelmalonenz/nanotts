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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h> // mmap

#include "cxxopts.hpp"

extern "C"
{
#include "picoapi.h"
#include "picoapid.h"
#include "picoos.h"
}

#include "Boilerplate.hpp"
#include "Listener.hpp"
#include "Pico.hpp"
#include "PicoVoices.h"
#include "mmfile.h"
#include "StreamHandler.h"

#ifdef _USE_ALSA
#include "Player_Alsa.h"
#endif

#define PICO_DEFAULT_SPEED 0.88f
#define PICO_DEFAULT_PITCH 1.05f
#define PICO_DEFAULT_VOLUME 1.00f

// searching these paths
const char *lingware_paths[2] = {"./lang", "/usr/share/pico/lang"};

#define FILE_OUTPUT_PREFIX "nanotts-output-"
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

// forward declarations
int GetNextLowestFilenameNumber(const char *prefix, const char *suffix, int zeropad);

pads_t Boilerplate::pads[] = {
    {"speed", "<speed level=\"%d\">", "</speed>", 0},
    {"pitch", "<pitch level=\"%d\">", "</pitch>", 0},
    {"volume", "<volume level=\"%d\">", "</volume>", 0}};

/*
================================================
    Nano

    class to handle workings of this program
================================================
*/
class Nano
{
private:
    enum inputMode_t
    {
        IN_NOT_SET,
        IN_STDIN,
        IN_CMDLINE_ARG,
        IN_CMDLINE_TRAILING,
        IN_SINGLE_FILE,
        IN_MULTIPLE_FILES
    };

    enum outputMode_t
    {
        OUT_NOT_SET = 0,
        OUT_STDOUT = 1,
        OUT_SINGLE_FILE = 2,
        OUT_PLAYBACK = 4,
        OUT_MULTIPLE_FILES = 8
    };

    int in_mode;
    int out_mode;

    int my_argc;
    char **my_argv;
    const char *exename;

    char *voice;
    char *langfiledir;
    char prefix[256];
    char suffix[100];
    char *out_filename;
    char *in_filename;
    char *words;
    FILE *in_fp;
    FILE *out_fp;

    char *copy_arg(int);

    unsigned char *input_buffer;
    unsigned int input_size;

    mmfile_t *mmfile;

    Listener<short> listener;
    void write_short_to_stdout(short *, unsigned int);
    void write_short_to_playback(short *data, unsigned int shorts);
    void write_short_to_playback_and_stdout(short *data, unsigned int shorts);

    Boilerplate modifiers;
    StreamHandler streamHandler;

public:
    bool silence_output;

    Nano(int, char **);
    virtual ~Nano();

    void PrintUsage();
    int parse_commandline_arguments();
    int setup_input_output();
    int verify_input_output();

    int ProduceInput(unsigned char **data, unsigned int *bytes);
    int playOutput();

    const char *getVoice();
    const char *getLangFilePath();

    const char *outFilename() const { return out_filename; }

    Listener<short> *getListener();

    Boilerplate *getModifiers();

    void SetListenerStdout();
    void SetListenerPlayback();
    void SetListenerPlaybackAndStdout();

    bool writingWaveFile() { return (out_mode & OUT_SINGLE_FILE) == OUT_SINGLE_FILE; }
};

Nano::Nano(int i, char **v) : my_argc(i), my_argv(v), listener(this)
{
    voice = 0;
    langfiledir = 0;
    sprintf(prefix, FILE_OUTPUT_PREFIX);
    sprintf(suffix, FILE_OUTPUT_SUFFIX);
    out_filename = 0;
    in_filename = 0;
    words = 0;
    in_fp = 0;
    out_fp = 0;
    input_buffer = 0;
    input_size = 0;

    silence_output = true;
}

Nano::~Nano()
{
    if (voice)
        delete[] voice;
    if (langfiledir)
        delete[] langfiledir;
    if (out_filename)
        delete[] out_filename;
    if (in_filename)
        delete[] in_filename;
    if (words)
        delete[] words;

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

void Nano::PrintUsage()
{

    const char *program = strrchr(my_argv[0], '/');
    program = !program ? my_argv[0] : program + 1;
    this->exename = program;

    printf("usage: %s [options]\n", exename);

    char line1[100];
    char line2[100];
    char line3[100];
    char line4[100];
    memset(line1, 0, sizeof(line1));
    memset(line2, 0, sizeof(line2));
    memset(line3, 0, sizeof(line3));
    memset(line4, 0, sizeof(line4));
    sprintf(line1, "   %s -f ray_bradbury.txt -o ray_bradbury.wav", exename);
    sprintf(line2, "   echo \"Mary had a little lamb\" | %s --play", exename);
    sprintf(line3, "   %s -i \"Once upon a midnight dreary\" -v en-US --speed 0.8 --pitch 1.8 -w -p", exename);
    sprintf(line4, "   echo \"Brave Ulysses\" | %s -c | play -r 16k -L -t raw -e signed -b 16 -c 1 -", exename);

    struct help
    {
        const char *arg;
        const char *desc;
    } help_lines[] = {
        {"   -h, --help", "Displays this help. (overrides other input)"},
        {"   -v, --voice <voice>", "Select voice. (Default: en-GB)"},
        {"   -l <directory>", "Set Lingware voices directory. (defaults: \"./lang\", \"/usr/share/pico/lang/\")"},
        {"   -i <text>", "Input. (Text must be correctly quoted)"},
        {"   -f <filename>", "Filename to read input from"},
        {"   -o <filename>", "Write output to WAV/PCM file (enables WAV output)"},
        {"   -w, --wav ", "Write output to WAV file, will generate filename if '-o' option not provided"},
        {"   -p, --play ", "Play audio output"},
        {"   -m, --no-play", "do NOT play output on PC's soundcard"},
        {"   -c ", "Send raw PCM output to stdout"},
        {"   --prefix", "Set the file prefix (eg. \"MyRecording-\")."},
        {"", "Generated files will be auto-numbered."},
        {"", "Good for running multiple times with different inputs"},
        {"   --speed <0.2-5.0>", "change voice speed"},
        {"   --pitch <0.5-2.0>", "change voice pitch"},
        {"   --volume <0.0-5.0>", "change voice volume (>1.0 may result in degraded quality)"},
        {"   --version", "Displays version information about this program"},
        //        { "  --files", "set multiple input files" },
        {" ", " "},
        {" ", " "},
        {"Examples: ", " "},
        {line1, " "},
        {line2, " "},
        {line3, " "},
        {line4, " "},
        {" ", " "},
    };

    unsigned long long int size = *(&help_lines + 1) - help_lines;

    for (unsigned int i = 0; i < size; i++)
    {
        printf("%-24s%s\n", help_lines[i].arg, help_lines[i].desc);
    }
}

// get argument at index, make a copy and return it
char *Nano::copy_arg(int index)
{
    if (index >= my_argc)
        return 0;

    int len = strlen(my_argv[index]);
    char *buf = new char[len + 1];
    memset(buf, 0, len + 1);
    strcpy(buf, my_argv[index]);
    return buf;
}

int Nano::parse_commandline_arguments()
{

    cxxopts::Options options("NanoTTS", "A flexible text-to-speech engine");
    options.add_options()
        ("h,help", "Print this help")
        ("v,version", "Print the version")
        ("i,input", "input text argument", cxxopts::value<std::string>())
        ("f,file", "use the given text file as input", cxxopts::value<std::string>())
        ("o,output", "Write output to WAV/PCM file (enables WAV output)", cxxopts::value<std::string>())
        ("w,wav", "Write output to WAV file, will generate filename if '-o' option not provided")
        ("p,play", "Play audio output")
        ("m,no-play", "do NOT play output on PC's soundcard")
        ("c,stdout", "Send raw PCM output to stdout")
        ("x,prefix", "Set the file prefix (e.g. \"MyRecording-\").")
        // {"", "Generated files will be auto-numbered."},
        // {"", "Good for running multiple times with different inputs"},
        ("speed", "change voice speed <0.2-5.0>", cxxopts::value<float>())
        ("pitch", "change the voice pitch <0.5-2.0>", cxxopts::value<float>())
        ("volume", "change the voice volume <0.0-5.0> (>1.0 may result in degraded quality)", cxxopts::value<float>())
        // {"Possible Voices: ", " "},
        // {"   en-US, en-GB, de-DE, es-ES, fr-FR, it-IT", " "},
        ;
#define WARN_UNMATCHED_INPUTS()
    auto args = options.parse(my_argc, my_argv);
    
    if (args["h"].count() > 0)
    {
        std::cout << options.help() << std::endl;
        exit(-1);
    }

    if (args["v"].count() > 0)
    {
        std::cout << VERSIONED_NAME << std::endl;
        exit(-666);
    }
    // inputs and especially output are explicit
    in_mode = IN_NOT_SET;
    out_mode = OUT_NOT_SET;
    
    if (!isatty(fileno(stdin)))
    {
        in_mode = IN_STDIN;
    }

    for (int i = 1; i < my_argc; i++)
    {
        // INPUTS
        if (strcmp(my_argv[i], "-i") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (in_mode != IN_NOT_SET)
            {
                fprintf(stderr, " **error: multiple inputs\n\n");
                return -1;
            }
            in_mode = IN_CMDLINE_ARG;
            if ((words = copy_arg(i + 1)) == 0)
                return -1;
            ++i;
        }
        else if (strcmp(my_argv[i], "-f") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (in_mode != IN_NOT_SET)
            {
                fprintf(stderr, " **error: multiple inputs\n\n");
                return -1;
            }
            in_mode = IN_SINGLE_FILE;
            if ((in_filename = copy_arg(i + 1)) == 0)
                return -1;
            ++i;
        }
        else if (strcmp(my_argv[i], "--files") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (in_mode != IN_NOT_SET)
            {
                fprintf(stderr, " **error: multiple inputs\n\n");
                return -1;
            }
            in_mode = IN_MULTIPLE_FILES;
            // FIXME: get array of char*filename
            if ((in_filename = copy_arg(i + 1)) == 0)
                return -1;
            ++i;
        }
        else if (strcmp(my_argv[i], "-") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (in_mode != IN_NOT_SET && in_mode != IN_STDIN)
            {
                fprintf(stderr, " **error: multiple inputs\n\n");
                return -1;
            }
            in_mode = IN_STDIN;
            in_fp = stdin;
        }

        // OUTPUTS
        else if (strcmp(my_argv[i], "-o") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            out_mode |= OUT_SINGLE_FILE;
            if ((out_filename = copy_arg(i + 1)) == 0)
                return -1;
            ++i;
        }
        else if (strcmp(my_argv[i], "-c") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            out_mode |= OUT_STDOUT;
        }
        else if (strcmp(my_argv[i], "-w") == 0 || strcmp(my_argv[i], "--wav") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            out_mode |= OUT_SINGLE_FILE;
        }
        else if (strcmp(my_argv[i], "-m") == 0 || strcmp(my_argv[i], "--no-play") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            silence_output = true;
            out_mode &= ~OUT_PLAYBACK;
        }
        else if (strcmp(my_argv[i], "-p") == 0 || strcmp(my_argv[i], "--play") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            silence_output = false;
            out_mode |= OUT_PLAYBACK;
        }
        else if (strcmp(my_argv[i], "--prefix") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            out_mode |= OUT_SINGLE_FILE;
            if (i + 1 >= my_argc)
                return -1;
            strncpy(prefix, my_argv[i + 1], sizeof(prefix));
            ++i;
        }

        // SVOX
        else if (strcmp(my_argv[i], "-v") == 0 || strcmp(my_argv[i], "--voice") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if ((voice = copy_arg(i + 1)) == 0)
                return -1;
            ++i;
        }
        else if (strcmp(my_argv[i], "-l") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if ((langfiledir = copy_arg(i + 1)) == 0)
                return -1;
            fprintf(stderr, "Using Lingware directory: %s\n", langfiledir);
            ++i;
        }

        // OTHER
        else if (strcmp(my_argv[i], "--speed") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (i + 1 >= my_argc)
                return -1;
            modifiers.setSpeed(strtof(my_argv[i + 1], 0));
            ++i;
        }
        else if (strcmp(my_argv[i], "--pitch") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (i + 1 >= my_argc)
                return -1;
            modifiers.setPitch(strtof(my_argv[i + 1], 0));
            ++i;
        }
        else if (strcmp(my_argv[i], "--volume") == 0)
        {
            WARN_UNMATCHED_INPUTS();
            if (i + 1 >= my_argc)
                return -1;
            modifiers.setVolume(strtof(my_argv[i + 1], 0));
            ++i;
        }

        // doesn't match any expected arguments; therefor try to speak it
        else
        {
            if (in_mode != IN_NOT_SET && in_mode != IN_CMDLINE_TRAILING)
            {
                fprintf(stderr, " **error: trailing commandline arguments\n\n");
                return -4;
            }

            // TODO: collect the valid straggling terms and concat them together
            words = copy_arg(i);
            in_mode = IN_CMDLINE_TRAILING;
        }
    }

    if (verify_input_output() < 0)
    {
        return -3;
    }

    // DEFAULTS
    if (!voice)
    {
        voice = new char[6];
        strcpy(voice, "en-GB");
    }

    if (!langfiledir)
    {
        const char *path_p = 0;
        char test_file[128];
        for (unsigned int i = 0; i < sizeof(lingware_paths) / sizeof(lingware_paths[0]); ++i)
        {
            sprintf(test_file, "%s/%s", lingware_paths[i], "en-GB_ta.bin");
            struct stat ss;
            if (-1 != stat(lingware_paths[i], &ss))
            {
                if (S_ISDIR(ss.st_mode))
                {
                    if (-1 != stat(test_file, &ss))
                    {
                        if (S_ISREG(ss.st_mode))
                        {
                            path_p = lingware_paths[i];
                            break;
                        }
                    }
                }
            }
        }

        if (!path_p)
        {
            fprintf(stderr, " **error: Lang file path not found. Looking in: %s, %s\n\n", lingware_paths[0], lingware_paths[1]);
            return -8;
        }

        int len = strlen(path_p) + 1;
        langfiledir = new char[len];
        strcpy(langfiledir, path_p);
    }

    if (!out_filename)
    {
        int tmpsize = 100;
        out_filename = new char[tmpsize];
        memset(out_filename, 0, tmpsize);
        int next = GetNextLowestFilenameNumber(prefix, suffix, FILENAME_NUMBERING_LEADING_ZEROS);
        char fmt[32];
        sprintf(fmt, "%%s%%0%dd%%s", FILENAME_NUMBERING_LEADING_ZEROS);
        sprintf(out_filename, fmt, prefix, next, suffix);
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
            if (!words)
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
        mmfile = new mmfile_t(in_filename);
        *data = mmfile->data;
        *bytes = mmfile->size + 1; /* 1 additional for terminating '\0' */
        fprintf(stderr, "read: %u bytes from \"%s\"\n", mmfile->size, in_filename);
        break;
    case IN_CMDLINE_ARG:
    case IN_CMDLINE_TRAILING:
        *data = (unsigned char *)words;
        *bytes = strlen(words) + 1; /* 1 additional for terminating '\0' */
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

const char *Nano::getVoice()
{
    return voice;
}

const char *Nano::getLangFilePath()
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
//////////////////////////////////////////////////////////////////

/*
================================================
    NanoSingleton

    singleton subclass
================================================
*/
class NanoSingleton : public Nano
{
public:
    /**
     * only method to get singleton handle to this object
     */
    static NanoSingleton &instance();

    /**
     * explicit destruction
     */
    static void destroy();

    /**
     * utility method to pass-through arguments
     */
    static void setArgs(int i, char **v);

private:
    /**
     * prevent outside construction
     */
    NanoSingleton(int, char **);

    /**
     * prevent compile-time deletion
     */
    virtual ~NanoSingleton();

    /**
     * prevent copy-constructor
     */
    NanoSingleton(const NanoSingleton &);

    /**
     * prevent assignment operator
     */
    NanoSingleton &operator=(const NanoSingleton &);

    /**
     * sole instance of this object; static assures 0 initialization for free
     */
    static NanoSingleton *single_instance;
    static int my_argc;
    static char **my_argv;
};

NanoSingleton *NanoSingleton::single_instance;
int NanoSingleton::my_argc;
char **NanoSingleton::my_argv;

// implementations
NanoSingleton &NanoSingleton::instance()
{
    if (0 == single_instance)
    {
        single_instance = new NanoSingleton(my_argc, my_argv);
    }
    return *single_instance;
}

void NanoSingleton::destroy()
{
    if (single_instance)
    {
        delete single_instance;
        single_instance = 0;
    }
}

NanoSingleton::NanoSingleton(int i, char **v) : Nano(i, v)
{
}

NanoSingleton::~NanoSingleton()
{
}

void NanoSingleton::setArgs(int i, char **v)
{
    my_argc = i;
    my_argv = v;
}
//////////////////////////////////////////////////////////////////

/**
 * Pico wrapped with singleton
 */
class PicoSingleton : public Pico
{
public:
    /**
     * only method to get singleton handle to this object
     */
    static PicoSingleton &instance();

    /**
     * explicit destruction
     */
    static void destroy();

private:
    /**
     * prevent outside construction
     */
    PicoSingleton();

    /**
     * prevent compile-time deletion
     */
    virtual ~PicoSingleton();

    /**
     * prevent copy-constructor
     */
    PicoSingleton(const PicoSingleton &);

    /**
     * prevent assignment operator
     */
    PicoSingleton &operator=(const PicoSingleton &);

    /**
     * sole instance of this object; static assures 0 initialization for free
     */
    static PicoSingleton *single_instance;
};

PicoSingleton &PicoSingleton::instance()
{
    if (0 == single_instance)
    {
        single_instance = new PicoSingleton();
    }
    return *single_instance;
}

void PicoSingleton::destroy()
{
    if (single_instance)
    {
        delete single_instance;
        single_instance = 0;
    }
}

PicoSingleton::PicoSingleton()
{
}

PicoSingleton::~PicoSingleton()
{
}

PicoSingleton *PicoSingleton::single_instance;
//////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    NanoSingleton::setArgs(argc, argv);

    NanoSingleton &nano = NanoSingleton::instance();

    int res;

    //
    if ((res = nano.parse_commandline_arguments()) < 0)
    {
        nano.destroy();
        if (res == -666)
        {
            return 0;
        }
        nano.PrintUsage();
        return 127; // command not found
    }

    //
    unsigned char *words = 0;
    unsigned int length = 0;
    if (nano.ProduceInput(&words, &length) < 0)
    {
        nano.destroy();
        return 65; // data format error
    }

    //
    PicoSingleton &pico = PicoSingleton::instance();
    pico.setLangFilePath(nano.getLangFilePath());
    pico.setOutFilename(nano.outFilename());

    if (pico.setVoice(nano.getVoice()) < 0)
    {
        fprintf(stderr, "set voice failed, with: \"%s\n\"", nano.getVoice());
        pico.destroy();
        nano.destroy();
        return 127; // command not found
    }

    if (nano.writingWaveFile())
    {
        pico.writeWavePcm();
    }
    pico.setListener(nano.getListener());
    pico.addModifiers(nano.getModifiers());

    //
    if (pico.initializeSystem() < 0)
    {
        fprintf(stderr, " * problem initializing Svox Pico\n");
        pico.destroy();
        nano.destroy();
        return 126; // command found but not executable
    }

    //
    pico.sendTextForProcessing(words, length);

    //
    pico.process();

    //
    pico.cleanup();

    //
    pico.destroy();
    nano.destroy();
    return 0;
}
