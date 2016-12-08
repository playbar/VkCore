#ifndef REF_H_
#define REF_H_

namespace vkcore
{

class Ref
{
public:

    void addRef();

    void release();

    unsigned int getRefCount() const;

protected:

    Ref();
    Ref(const Ref& copy);
    virtual ~Ref();

private:

    unsigned int _refCount;

    // Memory leak diagnostic data (only included when GP_USE_MEM_LEAK_DETECTION is defined)
#ifdef GP_USE_MEM_LEAK_DETECTION
    friend class Game;
    static void printLeaks();
    void* __record;
#endif
};

}

#endif
