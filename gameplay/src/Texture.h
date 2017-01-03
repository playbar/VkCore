#ifndef TEXTURE_H_
#define TEXTURE_H_

#include "Ref.h"
#include "Stream.h"
#include "vulkan/vulkan.h"

namespace vkcore
{

class Image;

/**
 * Defines a standard texture.
 */
class Texture : public Ref
{
    friend class Sampler;

public:

    enum Format
    {
        UNKNOWN = 0,
        RGB,
        RGB888 = RGB,
        RGB565,
        RGBA,
        RGBA8888 = RGBA,
        RGBA4444,
        RGBA5551,
        ALPHA,
        DEPTH,
    };

    enum Filter
    {
        NEAREST = GL_NEAREST,
        LINEAR = GL_LINEAR,
        NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST,
        LINEAR_MIPMAP_NEAREST = GL_LINEAR_MIPMAP_NEAREST,
        NEAREST_MIPMAP_LINEAR = GL_NEAREST_MIPMAP_LINEAR,
        LINEAR_MIPMAP_LINEAR = GL_LINEAR_MIPMAP_LINEAR
    };

    enum Wrap
    {
        REPEAT = GL_REPEAT,
        CLAMP = GL_CLAMP_TO_EDGE
    };

    enum Type
    {
        TEXTURE_2D = GL_TEXTURE_2D,
        TEXTURE_CUBE = GL_TEXTURE_CUBE_MAP
    };

    enum CubeFace
    {
        POSITIVE_X,
        NEGATIVE_X,
        POSITIVE_Y,
        NEGATIVE_Y,
        POSITIVE_Z,
        NEGATIVE_Z
    };
    
    /**
     * Defines a texture sampler.
     *
     * A texture sampler is basically an instance of a texture that can be
     * used to sample a texture from a material. In addition to the texture
     * itself, a sampler stores per-instance texture state information, such
     * as wrap and filter modes.
     */
    class Sampler : public Ref
    {
        friend class Texture;

    public:

        virtual ~Sampler();

        static Sampler* create(Texture* texture);

        static Sampler* create(const char* path, bool generateMipmaps = false);

        void setWrapMode(Wrap wrapS, Wrap wrapT, Wrap wrapR = REPEAT);

        void setFilterMode(Filter minificationFilter, Filter magnificationFilter);

        Texture* getTexture() const;

        void bind();

    private:

        Sampler(Texture* texture);

        Sampler& operator=(const Sampler&);

        Texture* _texture;
        Wrap _wrapS;
        Wrap _wrapT;
        Wrap _wrapR;
        Filter _minFilter;
        Filter _magFilter;
    };

    static Texture* create(const char* path, bool generateMipmaps = false);

    static Texture* create(Image* image, bool generateMipmaps = false);

    static Texture* create(Format format, unsigned int width, unsigned int height, const unsigned char* data, bool generateMipmaps = false, Type type = TEXTURE_2D);

    static Texture* create(TextureHandle handle, int width, int height, Format format = UNKNOWN);

    void setData(const unsigned char* data);

    const char* getPath() const;

    Format getFormat() const;

    Type getType() const;

    unsigned int getWidth() const;

    unsigned int getHeight() const;

    void generateMipmaps();

    bool isMipmapped() const;

    bool isCompressed() const;

    TextureHandle getHandle() const;

private:

    Texture();

    Texture(const Texture& copy);

    virtual ~Texture();

    Texture& operator=(const Texture&);

	static Texture* createKTX(const char* path);

    static Texture* createCompressedPVRTC(const char* path);

    static Texture* createCompressedDDS(const char* path);

    static GLubyte* readCompressedPVRTC(const char* path, Stream* stream, GLsizei* width, GLsizei* height, GLenum* format, unsigned int* mipMapCount, unsigned int* faceCount, GLenum faces[6]);

    static GLubyte* readCompressedPVRTCLegacy(const char* path, Stream* stream, GLsizei* width, GLsizei* height, GLenum* format, unsigned int* mipMapCount, unsigned int* faceCount, GLenum faces[6]);

    static int getMaskByteIndex(unsigned int mask);
    static GLint getFormatInternal(Format format);
    static GLenum getFormatTexel(Format format);
    static size_t getFormatBPP(Format format);

    std::string _path;
    TextureHandle _handle;
    Format _format;
    Type _type;
    unsigned int _width;
    unsigned int _height;
    bool _mipmapped;
    bool _cached;
    bool _compressed;
    Wrap _wrapS;
    Wrap _wrapT;
    Wrap _wrapR;
    Filter _minFilter;
    Filter _magFilter;

    GLint _internalFormat;
    GLenum _texelType;
    size_t _bpp;

	//////////////
	VkSampler sampler;
	VkImage image;
	VkImageLayout imageLayout;
	VkDeviceMemory deviceMemory;
	VkImageView imageView;
	VkDescriptorImageInfo descriptor;
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels;
};

}

#endif
