#pragma once

class Nano;

/*
================================================
Listener

stream class, for exchanging byte-streams between producer/consumer
================================================
*/
template <typename type>
class Listener
{
    type *data;
    unsigned int read_p;
    void (Nano::*consume)(short *, unsigned int);
    Nano *nano_class;

public:
    Listener() : data(0), read_p(0), consume(0), nano_class(0)
    {
    }
    Listener(Nano *n) : data(0), read_p(0), consume(0), nano_class(n)
    {
    }
    virtual ~Listener()
    {
    }

    void writeData(type *data, unsigned int byte_size);
    void setCallback(void (Nano::*con_f)(short *, unsigned int), Nano * = 0);
    bool hasConsumer();
};

template <typename type>
void Listener<type>::writeData(type *data, unsigned int byte_size)
{
    if (this->consume)
    {
        void (Nano::*pointer)(short *, unsigned int) = this->consume;
        (*nano_class.*pointer)(data, byte_size);
    }
}

template <typename type>
void Listener<type>::setCallback(void (Nano::*con_f)(short *, unsigned int), Nano *nano_class)
{
    this->consume = con_f;
    if (nano_class)
        this->nano_class = nano_class;
}

template <typename type>
bool Listener<type>::hasConsumer()
{
    return consume != 0;
}
