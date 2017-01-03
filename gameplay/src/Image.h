#ifndef IMAGE_H__
#define IMAGE_H__

#include "Ref.h"

namespace vkcore
{

class Image : public Ref
{
public:

    enum Format
    {
        RGB,
        RGBA
    };

    static Image* create(const char* path);

    static Image* create(unsigned int width, unsigned int height, Format format, unsigned char* data = NULL);

    inline unsigned char* getData() const;

    inline Format getFormat() const;

    inline unsigned int getHeight() const;

    inline unsigned int getWidth() const;

private:

    Image();

    ~Image();

    Image& operator=(const Image&);

    unsigned char* _data;
    Format _format;
    unsigned int _width;
    unsigned int _height;
};

}

#include "Image.inl"

#endif
