#pragma once

#include <cstdint>
#include <vector>

#pragma pack(push, 1)

struct ByteMessage
{
    uint8_t data[30];
    uint8_t checksum;

    const bool Valid() const
    {
        uint8_t sum = 0;

        for (size_t i = 0; i < std::size(data); i++)
            sum += data[i];

        return sum == checksum;
    }
};

struct ByteStream
{
    uint8_t id[2];

    ByteMessage messages[64];

    uint8_t zero;

    const bool Valid() const
    {
        if (id[0] != 0x42 || id[1] != 0x03 || zero)
            return false;

        for (size_t i = 0; i < std::size(messages); i++)
            if (!messages[i].Valid())
                return false;

        return true;
    }
};

#pragma pack(pop)

class All1Block
{
    const unsigned length;
    unsigned left;
public:
    All1Block(unsigned length) :
        length(length),
        left(length)
    {
    }

    bool ProcessByte(uint8_t byte)
    {
        if (*this)
            return true;

        if (byte == 0xff)
        {
            left--;
            return true;
        }
        else
        {
            left = length;
            return false;
        }
    }

    // whether the data matched this block
    operator bool() const
    {
        return !left;
    }
};

template <class T>
int sign(T value)
{
    return (T(0) < value) - (value < T(0));
}

template<class T>
class Decoder
{
    const unsigned rectangle;
    int current_sign = 0;

    unsigned sample_counter = 0;

    std::vector<T> window;
    unsigned window_pointer = 0;
    T window_sum = 0;

    All1Block leader;
    All1Block end;

    ByteStream stream;
    unsigned stream_pointer = 0;

    uint16_t data = 0;

    unsigned bits_read = 0;

    constexpr static uint16_t ByteMask =    0b11'0000'0000'1;
    constexpr static uint16_t BytePattern = 0b11'0000'0000'0;
    constexpr static unsigned ByteSize = 11;

    bool ProcessByte(uint8_t byte)
    {
        if (!leader)
            return leader.ProcessByte(byte);

        if (stream_pointer < sizeof(ByteStream))
        {
            *(reinterpret_cast<uint8_t*>(&stream) + stream_pointer) = byte;
            stream_pointer++;

            return true;
        }

        if (!end)
            return end.ProcessByte(byte);

        return true;
    }

    void ProcessBit(bool bit)
    {
        data >>= 1;
        data |= (unsigned)bit << 10;

        bits_read++;

        if (bits_read >= ByteSize)
            if ((data & ByteMask) == BytePattern)
            {
                if (ProcessByte(data >> 1))
                    bits_read = 0;
            }
    }
public:
    /*
    * rectangle - one signal length (in samples)
    * window - window size for taking the average value
    * leader - length of leader block
    * end - length of end block
    */
    Decoder(unsigned rectangle, unsigned window, unsigned leader, unsigned end) :
        rectangle(rectangle),
        window(window),

        leader(leader),
        end(end)
    {
    }

    void Decode(T sample)
    {
        // average the values over the window
        // it is not necessary for this task, but it will not be superfluous
        window_sum -= window[window_pointer];
        window_sum += (window[window_pointer] = sample);
        ++window_pointer %= window.size();

        sample_counter++;

        const int sample_sign = sign(window_sum);

        if (current_sign != sample_sign)
        {
            current_sign = sample_sign;

            // if this is a signal with a long period - it is zero
            if (sample_counter > rectangle * 3 / 2)
            {
                ProcessBit(false);
            }
            // ignore small incorrect signals
            else if (sample_counter > rectangle / 2)
            {
                ProcessBit(true);
            }

            sample_counter = 0;
        }
    }

    ByteStream& Stream()
    {
        return stream;
    }
};
