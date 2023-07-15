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

#include "powersaver.h"
#include "utilities.h"

#include "debug.h"

#include <SDL.h>
#include "compat-algorithm.h"

//for browsing the filesystem
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <unistd.h>

using namespace std;

bool case_less::operator()(const string &left, const string &right) const {
	return std::lexicographical_compare(left.begin(), left.end(),
					    right.begin(), right.end());
}

std::string case_less::to_lower(std::string data)
{
	std::transform(data.begin(), data.end(), data.begin(), 
		[](std::string::value_type c) { return std::tolower(c); }
	);
	return data;
}

string trim(const string& s) {
	auto b = s.find_first_not_of(" \t\r");
	auto e = s.find_last_not_of(" \t\r");
	return b == string::npos ? "" : string(s, b, e + 1 - b);
}

string ltrim(const string& s) {
	auto b = s.find_first_not_of(" \t\r");
	return b == string::npos ? "" : string(s, b);
}

string rtrim(const string& s) {
	auto e = s.find_last_not_of(" \t\r");
	return e == string::npos ? "" : string(s, 0, e + 1);
}

// See this article for a performance comparison of different approaches:
//   http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
string readFileAsString(string const& filename) {
	ifstream in(filename, ios::in | ios::binary);
	if (!in) {
		return "<error opening " + string(filename) + ">";
	}

	// Get file size.
	in.seekg(0, ios::end);
	auto size = max(int(in.tellg()), 0); // tellg() returns -1 on errors
	in.seekg(0, ios::beg);

	string contents(size, '\0');
	in.read(&contents[0], contents.size());
	in.close();

	if (in.fail()) {
		return "<error reading " + string(filename) + ">";
	} else {
		contents.shrink_to_fit();
		return contents;
	}
}

constexpr int writeOpenFlags =
#ifdef O_CLOEXEC
		O_CLOEXEC | // Linux
#endif
		O_CREAT | O_WRONLY | O_TRUNC;

// Use C functions since STL doesn't seem to have any way of applying fsync().
bool writeStringToFile(string const& filename, string const& data) {
	// Open temporary file.
	string tempname = filename + '~';
	int fd = open(tempname.c_str(), writeOpenFlags, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		return false;
	}

	// Write temporary file.
	const char *bytes = data.c_str();
	size_t remaining = data.size();
	bool ok = true;
	while (remaining != 0) {
		ssize_t written = write(fd, bytes, remaining);
		if (written <= 0) {
			ok = false;
			break;
		} else {
			bytes += written;
			remaining -= written;
		}
	}
	if (ok) {
		ok = fsync(fd) == 0;
	}

	// Close temporary file.
	ok &= close(fd) == 0;

	// Replace actual output file with temporary file.
	if (ok) {
		ok = rename(tempname.c_str(), filename.c_str()) == 0;
	}

	return ok;
}

constexpr int dirOpenFlags =
#ifdef O_DIRECTORY
		O_DIRECTORY | // Linux
#endif
#ifdef O_CLOEXEC
		O_CLOEXEC | // Linux
#endif
		O_RDONLY;

bool syncDir(string const& dirname)
{
	int fd = open(dirname.c_str(), dirOpenFlags);
	if (fd < 0) {
		return false;
	}

	bool ok = fsync(fd) == 0;

	ok &= close(fd) == 0;

	return ok;
}

string parentDir(string const& dir) {
	// Note that size() is unsigned, so for short strings the '- 2' wraps
	// around and as a result the entire string is searched, which is fine.
	auto p = dir.rfind('/', dir.size() - 2);
	return p == string::npos ? "/" : dir.substr(0, p + 1);
}

bool fileExists(const string &file) {
	return access(file.c_str(), F_OK) == 0;
}

string uniquePath(string const& dir, string const& name)
{
	string path = dir + "/" + name;
	unsigned int x = 2;
	while (fileExists(path)) {
		stringstream ss;
		ss << dir << '/' << name << x;
		ss >> path;
		x++;
	}
	return path;
}

//Configuration parsing utilities
int evalIntConf (ConfIntHash& hash, const std::string &key, int def, int imin, int imax) {
	auto it = hash.find(key);
	if (it == hash.end()) {
		return hash[key] = def;
	} else {
		return it->second = compat::clamp(it->second, imin, imax);
	}
}

void split(vector<string>& vec, string const& str, string const& delim) {
	vec.clear();

	if (str.empty())
		return;

	if (delim.empty()) {
		vec.push_back(str);
		return;
	}

	string::size_type i = 0, j;
	while ((j = str.find(delim, i)) != string::npos) {
		vec.push_back(str.substr(i, j - i));
		i = j + delim.size();
	}
	vec.push_back(str.substr(i));
}

string strreplace (string orig, const string &search, const string &replace) {
	string::size_type pos = orig.find( search, 0 );
	while (pos != string::npos) {
		orig.replace(pos,search.length(),replace);
		pos = orig.find( search, pos+replace.length() );
	}
	return orig;
}

string cmdclean (string cmdline) {
	string spchars = "\\`$();|{}&'\"*?<>[]!^~-#\n\r ";
	for (size_t i=0; i<spchars.length(); i++) {
		string curchar = spchars.substr(i,1);
		cmdline = strreplace(cmdline, curchar, "\\"+curchar);
	}
	return cmdline;
}

int intTransition(int from, int to, long tickStart, long duration, long tickNow) {
	if (tickNow<0) tickNow = SDL_GetTicks();
	return compat::clamp((int)(((tickNow-tickStart) * (to-from)) / duration), from, to);
	//                    elapsed                 increments
}

void request_repaint()
{
	// if (!PowerSaver::getInstance()->getScreenState())
		// return;

	SDL_UserEvent e = { SDL_USEREVENT };

	/* Inject an user event, that will be handled as a "repaint"
	 * event by the InputManager */
	SDL_PushEvent((SDL_Event *) &e);
	DEBUG("Injecting event code %i\n", e.code);
}
