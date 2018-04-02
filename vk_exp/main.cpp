#ifndef __main_cpp__
#define __main_cpp__

#include "Context.h"

int main(int argc, char *argv[])
{
	Context ctxt;
	ctxt.init();
	printf("Vulkan context init complete!\n");
	ctxt.destroy();
	printf("Vulkan context destroy complete!\n");
	getchar();
	return EXIT_SUCCESS;
}
#endif //__main_cpp__