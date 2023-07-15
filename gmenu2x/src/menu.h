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

#ifndef MENU_H
#define MENU_H

#include "iconbutton.h"
#include "layer.h"
#include "link.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class GMenu2X;
class IconButton;
class LinkApp;
class Monitor;


/**
Handles the menu structure

	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
class Menu : public Layer {
private:
	class Animation {
	public:
		Animation();
		bool isRunning() { return curr != 0; }
		int currentValue() { return curr; }
		void adjust(int delta);
		void step();
	private:
		int curr;
	};

	GMenu2X& gmenu2x;
	IconButton btnContextMenu;
	int iSection, iLink;
	uint32_t iFirstDispRow;
	std::vector<std::string> sections;
	std::vector<std::unique_ptr<OffscreenSurface>> section_text_surfaces;
	std::vector<std::vector<std::unique_ptr<Link>>> links;

	uint32_t linkColumns, linkRows;

	Animation sectionAnimation;

	/**
	 * Determine which section headers are visible.
	 * The output values are relative to the middle section at 0.
	 */
	void calcSectionRange(int &leftSection, int &rightSection);

	void readLinks();
	void freeLinks();

	// Load all the sections of the given "sections" directory.
	void readSections(std::string const& parentDir);

#ifdef HAVE_LIBOPK
	// Load all the .opk packages of the given directory
	bool readPackages(std::string const& parentDir);
#ifdef ENABLE_INOTIFY
	std::vector<std::unique_ptr<Monitor>> monitors;
#endif
#endif

	// Load all the links on the given section directory.
	void readLinksOfSection(std::vector<std::unique_ptr<Link>>& links,
							std::string const& path, bool deletable);

	/**
	 * Attempts to creates a section directory if it does not exist yet.
	 * @return The full path of the section directory, or the empty string
	 *         if the directory could not be created.
	 */
	std::string createSectionDir(std::string const& sectionName);

	void decSectionIndex();
	void incSectionIndex();
	void linkLeft();
	void linkRight();
	void linkUp();
	void linkDown();

	void updateSectionTextSurfaces();
public:
	typedef std::function<void(void)> Action;

	Menu(GMenu2X& gmenu2x);
	virtual ~Menu();

#ifdef HAVE_LIBOPK
	void openPackage(std::string const& path, bool order = true);
	void openPackagesFromDir(std::string const& path);
#ifdef ENABLE_INOTIFY
	void removePackageLink(std::string const& path);
#endif
#endif

	int selSectionIndex();
	const std::string &selSection();
	void setSectionIndex(int i);

	void addActionLink(uint32_t section, std::string const& title,
			Action action, std::string const& description="",
			std::string const& icon="");
	bool addLink(std::string const& path, std::string const& file);

	/**
	 * Looks up a section by name, adding it if it doesn't exist yet.
	 * @return The index of the section.
	 */
	int sectionNamed(const char *sectionName);
	/**
	 * Looks up a section by name, adding it if it doesn't exist yet.
	 * @return The index of the section.
	 */
	int sectionNamed(std::string const& sectionName) {
		return sectionNamed(sectionName.c_str());
	}

	void deleteSelectedLink();
	void deleteSelectedSection();

	bool moveSelectedLink(std::string const& newSection);

	void skinUpdated();

	// Called when the font has changed but not when it is loaded for the first time.
	void fontChanged();
	
	void orderLinks();

	// Layer implementation:
	virtual bool runAnimations();
	virtual void paint(Surface &s);
	virtual bool handleButtonPress(InputManager::Button button);

	int selLinkIndex();
	Link *selLink();
	LinkApp *selLinkApp();
	void setLinkIndex(int i);

	const std::vector<std::string> &getSections() { return sections; }
	std::vector<std::unique_ptr<Link>> *sectionLinks(int i = -1);
};

#endif // MENU_H
