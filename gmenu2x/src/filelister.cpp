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

#include "filelister.h"

#include "buildopts.h"
#include "debug.h"
#include "utilities.h"

//for browsing the filesystem
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <set>

using namespace std;

FileLister::FileLister()
	: showDirectories(true)
	, showUpdir(true)
	, showFiles(true)
{
}

void FileLister::setFilter(const string &filter)
{
	if (filter.empty() || filter == "*") {
		this->filter.clear();
	} else {
		split(this->filter, filter, ",");
	}
}

static void moveNames(set<string, case_less>&& from, vector<string>& to)
{
	to.reserve(from.size());
	for (string const& name : from) {
		// Casting away the const is necessary to make the move work.
		// It will leave the set in an invalid state where it contains multiple
		// empty strings, but since the set is an rvalue we don't care.
		to.emplace_back(move(const_cast<string&>(name)));
	}
	to.shrink_to_fit();
}

bool FileLister::browse(const string& path, bool clean)
{
	if (clean) {
		directories.clear();
		files.clear();
	}

	string slashedPath = path;
	if (!path.empty() && path[path.length() - 1] != '/') {
		slashedPath.push_back('/');
	}

	DIR *dirp;
	if ((dirp = opendir(slashedPath.c_str())) == NULL) {
		if (errno != ENOENT) {
			ERROR("Unable to open directory: %s\n", slashedPath.c_str());
		}
		return false;
	}

	set<string, case_less> directorySet;
	set<string, case_less> fileSet;

	while (struct dirent *dptr = readdir(dirp)) {
		// Ignore hidden files and optionally "..".
		if (dptr->d_name[0] == '.') {
			if (!(dptr->d_name[1] == '.' && showUpdir
			      && slashedPath != GMENU2X_CARD_ROOT "/")) {
				continue;
			}
		}

		bool isDir, isFile;
#ifdef _DIRENT_HAVE_D_TYPE
		if (dptr->d_type != DT_UNKNOWN && dptr->d_type != DT_LNK) {
			isDir = dptr->d_type == DT_DIR;
			isFile = dptr->d_type == DT_REG;
		} else
#endif
		{
			string filepath = slashedPath + dptr->d_name;
			struct stat st;
			int statRet = stat(filepath.c_str(), &st);
			if (statRet == -1) {
				ERROR("Stat failed on '%s' with error '%s'\n", filepath.c_str(), strerror(errno));
				continue;
			}
			isDir = S_ISDIR(st.st_mode);
			isFile = S_ISREG(st.st_mode);
		}

		if (isDir) {
			if (!showDirectories)
				continue;

			directorySet.insert(string(dptr->d_name));
		} else if (isFile) {
			if (!showFiles)
				continue;

			if (filter.empty()) {
				fileSet.insert(string(dptr->d_name));
				continue;
			}

			// Determine file extension.
			const char *ext = strrchr(dptr->d_name, '.');
			if (ext) ext++; else ext = "";

			for (auto& filterExt : filter) {
				// Note: this won't work with UTF8 characters but there shouldn't
				// be any 
				if (case_less::to_lower(ext) == case_less::to_lower(filterExt)) {
					fileSet.insert(string(dptr->d_name));
					break;
				}
			}
		}
	}

	closedir(dirp);

	if (!directorySet.empty()) {
		for (string& dir : directories) {
			directorySet.emplace(move(dir));
		}
		directories.clear();
		moveNames(move(directorySet), directories);
	}

	if (!fileSet.empty()) {
		for (string& file : files) {
			fileSet.emplace(move(file));
		}
		files.clear();
		moveNames(move(fileSet), files);
	}

	return true;
}

string FileLister::operator[](size_t x)
{
	const auto dirCount = directories.size();
	return x < dirCount ? directories[x] : files[x - dirCount];
}
