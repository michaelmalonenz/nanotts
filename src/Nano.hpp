#ifndef _NANO_HPP_
#define _NANO_HPP_

#include <string>
#include "Listener.hpp"
#include "Boilerplate.hpp"
#include "StreamHandler.h"
#include "mmfile.h"

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

    std::string voice;
    std::string langfiledir;
    std::string prefix;
    char suffix[100];
    std::string out_filename;
    std::string in_filename;
    std::string words;
    FILE *in_fp;
    FILE *out_fp;

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

    int parse_commandline_arguments();
    int setup_input_output();
    int verify_input_output();

    int ProduceInput(unsigned char **data, unsigned int *bytes);
    int playOutput();

    const std::string& getVoice();
    const std::string& getLangFilePath();

    const std::string& outFilename() const { return out_filename; }

    Listener<short> *getListener();

    Boilerplate *getModifiers();

    void SetListenerStdout();
    void SetListenerPlayback();
    void SetListenerPlaybackAndStdout();

    bool writingWaveFile() { return (out_mode & OUT_SINGLE_FILE) == OUT_SINGLE_FILE; }
};

#endif
