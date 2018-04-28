#ifndef __StringUtils_h__
#define __StringUtils_h__
#include <algorithm>
#include <string>
namespace su
{	
	inline std::string toLower(const std::string &in)
	{
		std::string out(in.size(), '\0');//Create empty string of null terminating
		std::transform(in.begin(), in.end(), out.begin(), ::tolower);
		return out;
	}

	//https://stackoverflow.com/a/874160/1646387
	inline bool endsWith(std::string const &fullString, std::string const &ending) {
		if (fullString.length() >= ending.length()) {
			return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
		}
		else {
			return false;
		}
	}
}
#endif //__StringUtils_h__
