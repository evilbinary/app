/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo                           *
 *   massimiliano.torromeo@gmail.com                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef UTILITIES_H
#define UTILITIES_H

#include <string>
#include <vector>
#include <unordered_map>

#include "inputmanager.h"

using ConfStrHash = std::unordered_map<std::string, std::string>;
using ConfIntHash = std::unordered_map<std::string, int>;

class case_less {
public:
	bool operator()(const std::string &left, const std::string &right) const;
	static std::string to_lower(std::string input);
};

inline bool isUTF8Starter(char c) {
	return (c & 0xC0) != 0x80;
}

/** Returns the string with whitespace stripped from both ends. */
std::string trim(const std::string& s);
/** Returns the string with whitespace stripped from the start. */
std::string ltrim(const std::string& s);
/** Returns the string with whitespace stripped from the end. */
std::string rtrim(const std::string& s);

/** Returns the contents of the given file as a string. */
std::string readFileAsString(std::string const& filename);

/**
 * Writes the given string to a file.
 * The update is done atomically but not durably; if you need durability
 * when fsync() the parent directory afterwards.
 * @return True iff the file was written successfully.
 */
bool writeStringToFile(std::string const& filename, std::string const& data);

/**
 * Tells the file system to commit the given directory to disk.
 * @return True iff the sync was successful.
 */
bool syncDir(std::string const& dirname);

std::string strreplace(std::string orig, const std::string &search, const std::string &replace);
std::string cmdclean(std::string cmdline);

/**
 * Returns the parent directory of the given directory path, or "/" if there is
 * no parent.
 * This function does not check the file system: it is only string manipulation.
 * @return The parent directory path, including a trailing '/'.
 */
std::string parentDir(std::string const& dir);

inline std::string trimExtension(std::string const& filename) {
	return filename.substr(0, filename.rfind('.'));
}

bool fileExists(const std::string &file);

/**
 * Constructs a non-existing path in a given directory based on the given name.
 */
std::string uniquePath(std::string const& dir, std::string const& name);

int constrain(int x, int imin, int imax);

int evalIntConf(ConfIntHash& hash, const std::string &key, int def, int imin, int imax);

/**
 * Splits the given string on the given delimiter, returning the split elements
 * in the given vector.
 * A delimiter can be a string of multiple characters, in which case that
 * entire delimiter string acts as a single delimiter.
 * If the delimiter is empty, the entire string is returned as a single element.
 * Any previous contents of the vector are discarded.
 */
void split(std::vector<std::string>& vec, std::string const& str,
		std::string const& delim);

int intTransition(int from, int to, long int tickStart, long duration=500,
		long tickNow=-1);

void request_repaint();

#endif // UTILITIES_H
