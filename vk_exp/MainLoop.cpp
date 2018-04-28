#include "MainLoop.h"

#define MOUSE_SPEED 0.001f
#define SHIFT_MULTIPLIER 5.0f
#define DELTA_ROLL 0.01f
#define DELTA_MOVE 0.005f
#define DELTA_STRAFE 0.005f
#define DELTA_ASCEND 0.005f

MainLoop::MainLoop()
	: loopContinue(false)
	, loopThread(nullptr)
	, m_camera(glm::vec3(2, 2, 2))
{
	ctxt.setViewMatPtr(m_camera.getViewMatPtr());
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
	Uint32 previousTime = 0;
	do
	{
		//Calc time since last frame
		Uint32 currentTime = SDL_GetTicks();
		Uint32 frameTime = currentTime>previousTime?currentTime - previousTime:0;
		previousTime = currentTime;

		//Poll Events
		// Handle continuous key presses (movement)
		handleKeyboardState(SDL_GetKeyboardState(NULL), frameTime);
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
					handleMouseMove(e.motion.xrel, e.motion.yrel);
					break;
				}
				case SDL_MOUSEBUTTONDOWN:
				{
					toggleMouseMode();
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

void MainLoop::handleMouseMove(int x, int y) {
	if (SDL_GetRelativeMouseMode()) {
		m_camera.turn(x * MOUSE_SPEED, y * MOUSE_SPEED);
	}
}
void MainLoop::handleKeyboardState(const Uint8 *state, unsigned int&frameTime)
{
	float turboMultiplier = state[SDL_SCANCODE_LSHIFT] ? SHIFT_MULTIPLIER : 1.0f;
	turboMultiplier*= frameTime;//Adjust motion based on frame rate
	if (state[SDL_SCANCODE_W]) {
		m_camera.move(DELTA_MOVE*turboMultiplier);
	}
	if (state[SDL_SCANCODE_A]) {
		m_camera.strafe(-DELTA_STRAFE*turboMultiplier);
	}
	if (state[SDL_SCANCODE_S]) {
		m_camera.move(-DELTA_MOVE*turboMultiplier);
	}
	if (state[SDL_SCANCODE_D]) {
		m_camera.strafe(DELTA_STRAFE*turboMultiplier);
	}
	if (state[SDL_SCANCODE_Q]) {
		m_camera.roll(-DELTA_ROLL);
	}
	if (state[SDL_SCANCODE_E]) {
		m_camera.roll(DELTA_ROLL);
	}
	if (state[SDL_SCANCODE_SPACE]) {
		m_camera.ascend(DELTA_ASCEND*turboMultiplier);
	}
	if (state[SDL_SCANCODE_LCTRL]) {
		m_camera.ascend(-DELTA_ASCEND*turboMultiplier);
	}
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

void MainLoop::toggleMouseMode() {
	if (SDL_GetRelativeMouseMode()) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
	else {
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
}