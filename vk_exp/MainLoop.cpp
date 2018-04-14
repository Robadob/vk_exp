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
			if (e.type == SDL_QUIT)
				loopContinue.store(false);
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
	//Get Image from swapchain
	ctxt.getNextImage();
	//Execute command buffer with image as attachment to framebuffer
	//Return image to swapchain present
}
