#include "word_wrap.h"

#include <algorithm>

#include "font_stack.h"
#include "utilities.h"

namespace {

std::string wordWrapSingleLine(const FontStack &font, const std::string &text,
                               size_t start, size_t end, int width) {
	std::string result;
	result.reserve(end - start);

	while (start != end) {
		/* Clean the end of the string, allowing lines that are indented at
		 * the start to stay as such. */
		std::string run = rtrim(text.substr(start, end - start));
		int runWidth = font.getTextWidth(run);

		if (runWidth > width) {
			size_t fits = 0, doesntFit = run.length();
			/* First guess: width / runWidth approximates the proportion of
			 * the run that should fit. */
			size_t guess = std::min(run.length(),
			                        (size_t)(doesntFit * ((float)width / runWidth)));
			/* Adjust that to fully include any partial UTF-8 character. */
			while (guess < run.length() && !isUTF8Starter(run[guess])) {
				guess++;
			}

			if (font.getTextWidth(run.substr(0, guess)) <= width) {
				fits = guess;
				doesntFit = fits;
				/* Prime doesntFit, which should be closer to 2 * fits than
				 * to run.length() / 2 if the run is long. */
				do {
					fits = doesntFit;  // determined to fit by a previous iteration
					doesntFit = std::min(2 * fits, run.length());
					while (doesntFit < run.length() && !isUTF8Starter(run[doesntFit])) {
						doesntFit++;
					}
				} while (doesntFit < run.length() &&
				         font.getTextWidth(run.substr(0, doesntFit)) <= width);
			} else {
				doesntFit = guess;
			}

			/* End this loop when N full characters fit but N + 1 don't. */
			while (fits + 1 < doesntFit) {
				size_t guess = fits + (doesntFit - fits) / 2;
				if (!isUTF8Starter(run[guess])) {
					size_t oldGuess = guess;
					/* Adjust the guess to fully include a UTF-8 character. */
					for (size_t offset = 1; offset < (doesntFit - fits) / 2 - 1;
					     offset++) {
						if (isUTF8Starter(run[guess - offset])) {
							guess -= offset;
							break;
						} else if (isUTF8Starter(run[guess + offset])) {
							guess += offset;
							break;
						}
					}
					/* If there's no such character, exit early. */
					if (guess == oldGuess) {
						break;
					}
				}
				if (font.getTextWidth(run.substr(0, guess)) <= width) {
					fits = guess;
				} else {
					doesntFit = guess;
				}
			}

			/* The run shall be split at the last space-separated word that
			 * fully fits, or otherwise at the last character that fits. */
			size_t lastSpace = run.find_last_of(" \t\r", fits);
			if (lastSpace != std::string::npos) {
				fits = lastSpace;
			}

			/* If 0 characters fit, we'll have to make 1 fit anyway, otherwise
			 * we're in for an infinite loop. This can happen if the font size
			 * is large. */
			if (fits == 0) {
				fits = 1;
				while (fits < run.length() && !isUTF8Starter(run[fits])) {
					fits++;
				}
			}

			result.append(rtrim(run.substr(0, fits))).append("\n");
			start = std::min(end, text.find_first_not_of(" \t\r", start + fits));
		} else {
			result.append(rtrim(run));
			start = end;
		}
	}

	return result;
}

}  // namespace

std::string wordWrap(const FontStack &font, const std::string &text,
                     int width) {
	const size_t len = text.length();
	std::string result;
	result.reserve(len);

	size_t start = 0;
	while (true) {
		size_t end = std::min(text.find('\n', start), len);
		result.append(wordWrapSingleLine(font, text, start, end, width));
		start = end + 1;
		if (start >= len) {
			break;
		}
		result.push_back('\n');
	}

	return result;
}
