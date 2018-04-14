#ifndef __MainLoop_h__
#define __MainLoop_h__
#include <thread>
#include <atomic>
#include "Context.h"

/**
 * This class controls the main loop of the application
 */
class MainLoop
{
public:
	MainLoop();
	~MainLoop();
	bool init();
	void start();
	void startAsync();
	void stop();
	void waitForStop();
	bool isRunning() const;
private:
	void loop();
	void loopAsync();
	void handleKeypress(SDL_Keycode keycode, int x, int y);
	void drawFrame();
	std::atomic<bool> loopContinue;
	std::thread *loopThread;
	Context ctxt;
};

#endif //__MainLoop_h__