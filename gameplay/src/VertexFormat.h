#ifndef VERTEXFORMAT_H_
#define VERTEXFORMAT_H_

namespace vkcore
{


class VertexFormat
{
public:

    enum Usage
    {
        POSITION = 1,
        NORMAL = 2,
        COLOR = 3,
        TANGENT = 4,
        BINORMAL = 5,
        BLENDWEIGHTS = 6,
        BLENDINDICES = 7,
        TEXCOORD0 = 8,
        TEXCOORD1 = 9,
        TEXCOORD2 = 10,
        TEXCOORD3 = 11,
        TEXCOORD4 = 12,
        TEXCOORD5 = 13,
        TEXCOORD6 = 14,
        TEXCOORD7 = 15
    };

    class Element
    {
    public:
        Usage usage;
        unsigned int size;
        Element();

        Element(Usage usage, unsigned int size);
        bool operator == (const Element& e) const;
        bool operator != (const Element& e) const;
    };

   
    VertexFormat(const Element* elements, unsigned int elementCount);
    ~VertexFormat();
   
    const Element& getElement(unsigned int index) const;

    unsigned int getElementCount() const;

    unsigned int getVertexSize() const;

    bool operator == (const VertexFormat& f) const;

    bool operator != (const VertexFormat& f) const;

    static const char* toString(Usage usage);

private:

    std::vector<Element> _elements;
    unsigned int _vertexSize;
};

}

#endif
