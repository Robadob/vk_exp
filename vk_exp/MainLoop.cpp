#include "MainLoop.h"

MainLoop::MainLoop()
	: loopContinue(false)
	, loopThread(nullptr)
{
	
}
MainLoop::~MainLoop()
{
	waitForStop();
	//Bin context
	if(ctxt.ready())
	ctxt.destroy();
}

bool MainLoop::init()
{
	try
	{
		ctxt.init();
	}
	catch(...)
	{
		fprintf(stderr, "ctxt init failed\n");
	}
	return ctxt.ready();
}
void MainLoop::loop()
{
	loopContinue.store(true);
	do
	{
		//Poll Events
		// Handle continuous key presses (movement)
		const Uint8 *state = SDL_GetKeyboardState(NULL);
		// handle each event on the queue
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0) {
			switch (e.type) {
				case SDL_QUIT:
					loopContinue.store(false);
					break;
				case SDL_KEYDOWN:
				{
					int x = 0;
					int y = 0;
					SDL_GetMouseState(&x, &y);
					handleKeypress(e.key.keysym.sym, x, y);
				}
				break;
				//case SDL_MOUSEWHEEL:
				//break;
				case SDL_MOUSEMOTION:
				{
					//handleMouseMove(e.motion.xrel, e.motion.yrel);
					break;
				}
				case SDL_MOUSEBUTTONDOWN:
				{
					//toggleMouseMode();
					break;
				}
				case SDL_WINDOWEVENT:
				{
					if (e.window.event == SDL_WINDOWEVENT_RESIZED)
					{
						ctxt.rebuildSwapChain();
					}
				}
			}
		}
		//Draw Frame
		drawFrame();
	} while (loopContinue.load(std::memory_order_relaxed));
	//Kill context, close window
	ctxt.destroy();
}
void MainLoop::loopAsync()
{
	if (ctxt.ready())
	{
		fprintf(stderr, "Error: Context should not be active when async thread started!\n");
		return;
	}
	if (!init())
		return;
	loop();
}

void MainLoop::startAsync()
{
	waitForStop();
	if(ctxt.ready())
	{//Context must be created in the thread it will be polled!
		ctxt.destroy();
	}
	loopThread = new std::thread(&MainLoop::loopAsync, this);
}
void MainLoop::start()
{
	waitForStop();
	if (!ctxt.ready())
		ctxt.init();
	if (!init())
		return;
	//Enter main loop;
	loop();
}
void MainLoop::stop()
{
	loopContinue.store(false);
}
void MainLoop::waitForStop()
{
	if(loopThread)
	{
		stop();
		loopThread->join();
		loopThread = nullptr;
	}
}
bool MainLoop::isRunning() const
{
	return loopContinue.load()&&ctxt.ready();
}
void MainLoop::drawFrame()
{
	ctxt.updateUniformBuffer();
	ctxt.getNextImage();
}
void MainLoop::handleKeypress(SDL_Keycode keycode, int x, int y) {
	switch (keycode) {
	case SDLK_ESCAPE:
		stop();
		break;
	case SDLK_F11:
		ctxt.toggleFullScreen();
		break;
	case SDLK_F10:
		//this->setMSAA(!this->msaaState);
		break;
	default:
		// Do nothing?
		break;
	}
}