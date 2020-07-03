#pragma once
#include <cstring>
#include <cstdio>
#include <cmath>

struct pads_t
{
    const char *name;
    const char *ofmt;
    const char *cfmt;
    float *val;
};

/*
================================================
Boilerplate

adds padding text around input to set various parameters in pico
================================================
*/
class Boilerplate
{
    char plate_begin[100];
    char plate_end[50];

    float speed;
    float pitch;
    float volume;

    const unsigned int padslen;

    void setOne(const char *verb, float value)
    {
        char buf[100];

        // find the parm and set it
        for (unsigned int i = 0; i < padslen; i++)
        {
            if (strcmp(pads[i].name, verb) == 0)
            {
                *(pads[i].val) = value;
                break;
            }
        }

        // rebuild the plates
        memset(plate_begin, 0, 100);
        memset(plate_end, 0, 50);
        for (unsigned int i = 0; i < padslen; i++)
        {
            if (*(pads[i].val) != -1)
            {
                // begin plate
                int ivalue = ceilf(*(pads[i].val) * 100.0f);
                sprintf(buf, pads[i].ofmt, ivalue);
                strcat(plate_begin, buf);
                // end plate := reverse order to match tag order
                strcpy(buf, plate_end);
                sprintf(plate_end, "%s%s", pads[i].cfmt, buf);
                // </volume></pitch></speed>
            }
        }
    }

public:
    static pads_t pads[];

    Boilerplate() : padslen(3)
    {
        speed = -1.0f;
        pitch = -1.0f;
        volume = -1.0f;

        memset(plate_begin, 0, 100);
        memset(plate_end, 0, 50);

        pads[0].val = &speed;
        pads[1].val = &pitch;
        pads[2].val = &volume;
    }

    Boilerplate(float s, float p, float v) : padslen(3)
    {
        setSpeed(s);
        setPitch(p);
        setVolume(v);
    }

    bool isChanged()
    {
        return !((speed == -1.0f) && (pitch == -1.0f) && (volume == -1.0f));
    }

    char *getOpener(unsigned int *l)
    {
        *l = strlen(plate_begin);
        return plate_begin;
    }
    char *getCloser(unsigned int *l)
    {
        *l = strlen(plate_end);
        return plate_end;
    }

    void setSpeed(float f)
    {
        setOne("speed", f);
    }
    void setPitch(float f)
    {
        setOne("pitch", f);
    }
    void setVolume(float f)
    {
        setOne("volume", f);
    }

    const char *getStatusMessage()
    {
        static char buf[100];
        memset(buf, 0, 100);
        char tmp[40];

        for (unsigned int i = 0; i < padslen; i++)
        {
            if (*(pads[i].val) != -1)
            {
                pads_t *p = &pads[i];
                sprintf(tmp, "%s: %.2f\n", p->name, *p->val);
                strcat(buf, tmp);
            }
        }
        return buf;
    }
};
