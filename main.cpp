#include "decoder.h"
#include <cstdio>
#include <memory.h>

// encoded signal rate (1 / 320 microseconds)
constexpr unsigned SignalRate = 3125;

// from http://soundfile.sapp.org/doc/WaveFormat/
struct WavHeader
{
    uint8_t     ChunkID[4];
    uint32_t    ChunkSize;
    uint8_t     Format[4];

    uint8_t     Subchunk1ID[4];
    uint32_t    Subchunk1Size;
    uint16_t    AudioFormat;
    uint16_t    NumChannels;
    uint32_t    SampleRate;
    uint32_t    ByteRate;
    uint16_t    BlockAlign;
    uint16_t    BitsPerSample;

    uint8_t     Subchunk2ID[4];
    uint32_t    Subchunk2Size;
};

// file helper - automatically closes the file
class File
{
    FILE* stream;
public:
    File(const char* name, const char* mode) : stream(nullptr) { fopen_s(&stream, name, mode); }
    ~File() { if (stream)fclose(stream); }

    operator FILE* () const { return stream; }
};

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Usage: ParadoxCat.exe <input file> <output file>\n");
        return 0;
    }

    File istream(argv[1], "rb");

    if (!istream)
    {
        printf("Unable to open the file: %s\n", argv[1]);
        return -1;
    }

    File ostream(argv[2], "wb");

    if (!ostream)
    {
        printf("Unable to write the file: %s\n", argv[2]);
        return -2;
    }

    WavHeader header;

    if (!fread(&header, sizeof(header), 1, istream))
    {
        printf("Unable to read wav header\n");
        return -3;
    }

    if (memcmp(header.ChunkID, "RIFF", 4) ||
        memcmp(header.Format, "WAVE", 4) ||
        memcmp(header.Subchunk1ID, "fmt ", 4) ||
        memcmp(header.Subchunk2ID, "data", 4))
    {
        printf("Wav header is not valid\n");
        return -4;
    }

    if (header.AudioFormat != 1)
    {
        printf("The application only supports PCM audio format\n");
        return -5;
    }

    if (header.BitsPerSample != 16)
    {
        printf("Only 16 bits per sample are supported\n");
        return -6;
    }

    Decoder<int> decoder(header.SampleRate / SignalRate, 4, 652, 130);

    size_t left = header.Subchunk2Size;
    int16_t buffer[1024];

    while (left)
    {
        const size_t size = std::min(left, sizeof(buffer));

        if (!fread(buffer, size, 1, istream))
        {
            printf("Unable to read samples\n");
            return -7;
        }

        // assuming that the stereo channels reproduce the same signal
        for (int i = 0; i < size / 2; i += header.NumChannels)
        {
            int sample = 0;

            for (int c = 0; c < header.NumChannels; c++)
                sample += buffer[i + c];

            decoder.Decode(sample);
        }

        left -= size;
    }

    ByteStream& steam = decoder.Stream();

    if (!steam.Valid())
    {
        printf("Decoding result is invalid\n");
        return -8;
    }

    for (size_t i = 0; i < std::size(steam.messages); i++)
        fwrite(steam.messages[i].data, sizeof(steam.messages[i].data), 1, ostream);

    return 0;
}
