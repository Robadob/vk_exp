#ifndef __main_cpp__
#define __main_cpp__

#include "MainLoop.h"

int main(int argc, char *argv[])
{
    MainLoop *ml = new MainLoop();
    ml->startAsync();
    using namespace std::chrono_literals;
    //for(unsigned int i = 0;i<1000;++i)
    while(ml->isRunning())
        std::this_thread::sleep_for(100ms);
    delete ml;
    return EXIT_SUCCESS;
}
#endif //__main_cpp__