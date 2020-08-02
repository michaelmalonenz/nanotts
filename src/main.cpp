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
#include <string.h>
#include <iostream>

#include "Nano.hpp"
#include "Pico.hpp"
#include "PicoVoices.h"

int main(int argc, char **argv)
{
    Nano nano(argc, argv);
    int res;

    //
    if ((res = nano.parse_commandline_arguments()) < 0)
    {
        if (res == -666)
        {
            return EXIT_SUCCESS;
        }
        return 127; // command not found
    }

    //
    unsigned char *words = 0;
    unsigned int length = 0;
    if (nano.ProduceInput(&words, &length) < 0)
    {
        return 65; // data format error
    }

    //
    Pico pico;
    pico.setLangFilePath(nano.getLangFilePath().c_str());
    pico.setOutFilename(nano.outFilename().c_str());

    if (pico.setVoice(nano.getVoice().c_str()) < 0)
    {
        std::cerr << "set voice failed, with: \"" << nano.getVoice() << "\"" << std::endl;
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
        std::cerr << " * problem initializing Svox Pico" << std::endl;
        return 126; // command found but not executable
    }

    //
    pico.sendTextForProcessing(words, length);

    //
    pico.process();

    //
    pico.cleanup();

    //
    return EXIT_SUCCESS;
}
