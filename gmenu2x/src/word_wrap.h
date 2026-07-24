#ifndef _WORD_WRAP_H_
#define _WORD_WRAP_H_

#include <string>

class FontStack;

std::string wordWrap(const FontStack &font, const std::string &text, int width);

#endif  // _WORD_WRAP_H_
