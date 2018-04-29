#ifndef __StringUtils_h__
#define __StringUtils_h__
#include <algorithm>
#include <string>
namespace su
{
	namespace
	{
		//_WIN32 is defined for both x86 and x64
		//https://msdn.microsoft.com/en-us/library/b0084kay.aspx
#ifdef _WIN32 
		struct MatchPathSeparator
		{
			bool operator()(char ch) const
			{
				return ch == '\\' || ch == '/';
			}
		};
#else
		struct MatchPathSeparator
		{
			bool operator()(char ch) const
			{
				return ch == '/';
			}
		};
#endif
	}
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
	/**
	* Returns the filename from the provided file path
	* @param filePath A null terminated string holding a file path
	* @return The filename extracted from the string
	* @note This has operating system dependent behaviour, Linux consider \\ a valid path
	*/
	inline std::string filenameFromPath(const std::string &filePath)
	{
		std::string pathname(filePath);
		std::string result = std::string(
			std::find_if(pathname.rbegin(), pathname.rend(), MatchPathSeparator()).base(),
			pathname.end()
		);
		return result;
	}
	/**
	* Returns the folder path from the provided file path
	* @param filePath A null terminated string holding a file path
	* @return The folder path extracted from the string
	* @note This has operating system dependent behaviour, Linux consider \\ a valid path
	*/
	inline std::string folderFromPath(const std::string &filePath)
	{
		std::string pathname(filePath);
		std::string result = std::string(
			pathname.begin(),
			std::find_if(pathname.rbegin(), pathname.rend(), MatchPathSeparator()).base()
		);
		return result;
	}
	/**
	* Returns the filename sans extension from the provided file path
	* @param filename A null terminated string holding a file name
	* @return The filename sans extension
	*/
	inline std::string removeFileExt(const std::string &filename)
	{
		size_t lastdot = filename.find_last_of(".");
		if (lastdot == std::string::npos) return filename;
		return filename.substr(0, lastdot);
	}
}
#endif //__StringUtils_h__