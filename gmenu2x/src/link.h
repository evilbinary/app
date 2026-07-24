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

#ifndef LINK_H
#define LINK_H

#include <SDL.h>

#include <memory>
#include <functional>
#include <string>

class GMenu2X;
class OffscreenSurface;


/**
Base class that represents a link on screen.

	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
class Link {
public:
	typedef std::function<void(void)> Action;

	Link(GMenu2X& gmenu2x, Action action);
	virtual ~Link() {};

	virtual void paint();
	void paintHover();
	void paintDescription(int center_x, int center_y);

	virtual void loadIcon();

	void setSize(int w, int h);
	void setPosition(int x, int y);

	const std::string &getTitle() const;
	void setTitle(const std::string &title);
	const std::string &getDescription() const;
	void setDescription(const std::string &description);
	const std::string &getLaunchMsg();
	const std::string &getIcon();
	void setIcon(const std::string &icon);
	const std::string &getIconPath();

	void updateTextSurfaces();

	void run();

protected:
	GMenu2X& gmenu2x;
	bool edited;
	std::string launchMsg, icon, iconPath;

	OffscreenSurface *iconSurface;
	std::unique_ptr<OffscreenSurface> titleSurface;
	std::unique_ptr<OffscreenSurface> descriptionSurface;

	virtual const std::string &searchIcon();
	void setIconPath(const std::string &icon);
	void updateSurfaces();

private:
	void recalcCoordinates();
	void updateTitleSurface();
	void updateDescriptionSurface();

	Action action;

	SDL_Rect rect;
	uint32_t iconX, padding;
	int lastTick;
	std::string title, description;
};

#endif
