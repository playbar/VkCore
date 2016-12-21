#ifndef SAMPLESGAME_H_
#define SAMPLESGAME_H_

#include "gameplay.h"
#include "Sample.h"

using namespace vkcore;


#define ADD_SAMPLE(cateogry, title, className, order) \
    static className* _createSample() \
    { \
        return new className(); \
    } \
    struct _foo ## className \
    { \
        _foo ## className() \
        { \
            SamplesGame::addSample((cateogry), (title), (void*)&_createSample, order); \
        } \
    }; \
    static _foo ## className _f;

class SamplesGame : public Game, Control::Listener
{
public:

    SamplesGame();

    void resizeEvent(unsigned int width, unsigned int height);
    
	void keyEvent(Keyboard::KeyEvent evt, int key);

    void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

    bool mouseEvent(Mouse::MouseEvent evt, int x, int y, int wheelDelta);

    void menuEvent();

    void gestureSwipeEvent(int x, int y, int direction);
    
    void gesturePinchEvent(int x, int y, float scale);
    
    void gestureTapEvent(int x, int y);

	void gestureLongTapEvent(int x, int y, float duration);

	void gestureDragEvent(int x, int y);

	void gestureDropEvent(int x, int y);

	void controlEvent(Control* control, EventType evt);

    void gamepadEvent(Gamepad::GamepadEvent evt, Gamepad* gamepad, unsigned int analogIndex = 0);

    static void addSample(const char* category, const char* title, void* func, unsigned int order);

    static SamplesGame* getInstance();
    
protected:

    void initialize();

    void finalize();

    void update(float elapsedTime) override;
    void render(float elapsedTime) override;
	void prepare() override;

private:

    typedef void*(*SampleGameCreatePtr)();

    void runSample(void* func);

    void exitActiveSample();

private:

    struct SampleRecord
    {
        std::string title;
        void* funcPtr;
        unsigned int order;

        SampleRecord() : funcPtr(NULL), order(0) { }
        SampleRecord(std::string title, void* funcPtr, unsigned int order) : title(title), funcPtr(funcPtr), order(order) { }

        SampleRecord& operator = (const SampleRecord& copy)
        {
            title = copy.title;
            funcPtr = copy.funcPtr;
            order = copy.order;
            return *this;
        }

        bool operator<(const SampleRecord& v) const
        {
            return order < v.order;
        }
    };

    static std::vector<std::string>* _categories;

    typedef std::vector<SampleRecord> SampleRecordList;
    static std::vector<SampleRecordList>* _samples;

    Sample* _activeSample;
    Font* _font;
    Form* _sampleSelectForm;
};

#endif
