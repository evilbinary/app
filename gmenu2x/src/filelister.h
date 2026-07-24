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

#ifndef FILELISTER_H
#define FILELISTER_H

#include <string>
#include <vector>

class FileLister {
private:
	std::vector<std::string> filter;
	bool showDirectories, showUpdir, showFiles;

	std::vector<std::string> directories, files;

public:
	FileLister();

	/**
	 * Scans the given directory.
	 * @param clean If true, start a new result set, if false add to the
	 *   results from the previous scan.
	 * @return True iff the given directory could be opened.
	 */
	bool browse(const std::string& path, bool clean = true);

	size_t size() const { return files.size() + directories.size(); }
	size_t dirCount() const { return directories.size(); }
	size_t fileCount() const { return files.size(); }

	std::string operator[](size_t);
	bool isFile(size_t x) const { return x >= directories.size(); }
	bool isDirectory(size_t x) const { return x < directories.size(); }

	void setFilter(const std::string &filter);

	void setShowDirectories(bool enabled) { showDirectories = enabled; }
	void setShowUpdir(bool enabled) { showUpdir = enabled; }
	void setShowFiles(bool enabled) { showFiles = enabled; }

	const std::vector<std::string> &getDirectories() const { return directories; }
	const std::vector<std::string> &getFiles() const { return files; }
};

#endif // FILELISTER_H
