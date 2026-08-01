#pragma once

#include <string>

namespace love
{
namespace system
{
namespace uwp
{

bool pickFile(const char *kind);
bool takePickedFile(std::string &path);
bool takePickError(std::string &error);

} // uwp
} // system
} // love
