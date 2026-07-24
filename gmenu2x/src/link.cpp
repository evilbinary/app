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

#include "link.h"

#include "gmenu2x.h"
#include "menu.h"
#include "selector.h"
#include "surface.h"
#include "utilities.h"

#include <fstream>
#include <sstream>

using namespace std;


Link::Link(GMenu2X& gmenu2x, Action action)
	: gmenu2x(gmenu2x)
	, action(action)
	, iconPath(gmenu2x.sc.getSkinFilePath("icons/generic.png"))
	, edited(false)
	, rect {
		0, 0,
		static_cast<decltype(SDL_Rect().w)>(gmenu2x.skinConfInt["linkWidth"]),
		static_cast<decltype(SDL_Rect().h)>(gmenu2x.skinConfInt["linkHeight"])
	}
{
	updateSurfaces();
}

void Link::updateTextSurfaces() {
	updateTitleSurface();
	updateDescriptionSurface();
}

void Link::updateTitleSurface() {
	if (!title.empty()) {
		titleSurface = gmenu2x.font->render(title);
	} else {
		titleSurface = nullptr;
	}
}

void Link::updateDescriptionSurface() {
	if (!description.empty()) {
		descriptionSurface = gmenu2x.font->render(description);
	} else {
		descriptionSurface = nullptr;
	}
}

void Link::paint() {
	Surface& s = *gmenu2x.s;

	if (iconSurface) {
		iconSurface->blit(s, iconX, rect.y+padding, 32,32);
	}

	SDL_Rect coords = {
		static_cast<Sint16>(iconX + 16),
		static_cast<Sint16>(rect.y + gmenu2x.skinConfInt["linkHeight"] - padding),
		0, 0
	};

	titleSurface->blit(s, coords, Font::HAlignCenter, Font::VAlignBottom);
}

void Link::paintHover() {
	Surface& s = *gmenu2x.s;

	if (gmenu2x.useSelectionPng)
		gmenu2x.sc["imgs/selection.png"]->blit(s, rect, Font::HAlignCenter, Font::VAlignMiddle);
	else
		s.box(rect.x, rect.y, rect.w, rect.h, gmenu2x.skinConfColors[COLOR_SELECTION_BG]);
}

void Link::paintDescription(int center_x, int center_y)
{
	SDL_Rect coords = {
		static_cast<Sint16>(center_x),
		static_cast<Sint16>(center_y),
		0, 0
	};

	if (descriptionSurface != nullptr) {
		descriptionSurface->blit(*gmenu2x.s, coords,
				  Font::HAlignCenter, Font::VAlignBottom);
	}
}

void Link::updateSurfaces()
{
	iconSurface = gmenu2x.sc[getIconPath()];
}

const string &Link::getTitle() const {
	return title;
}

void Link::setTitle(const string &title) {
	this->title = title;
	updateTitleSurface();
	edited = true;
}

const string &Link::getDescription() const {
	return description;
}

void Link::setDescription(const string &description) {
	this->description = description;
	updateDescriptionSurface();
	edited = true;
}

const string &Link::getLaunchMsg() {
	return launchMsg;
}

const string &Link::getIcon() {
	return icon;
}

void Link::loadIcon() {
	if (icon.compare(0, 5, "skin:") == 0) {
		setIconPath(gmenu2x.sc.getSkinFilePath(icon.substr(5)));
	}
}

void Link::setIcon(const string &icon) {
	this->icon = icon;

	if (icon.compare(0, 5, "skin:") == 0)
		this->iconPath = gmenu2x.sc.getSkinFilePath(
					icon.substr(5));
	else
		this->iconPath = icon;

	edited = true;
	updateSurfaces();
}

const string &Link::searchIcon() {
	iconPath = gmenu2x.sc.getSkinFilePath("icons/generic.png");
	return iconPath;
}

const string &Link::getIconPath() {
	if (iconPath.empty()) searchIcon();
	return iconPath;
}

void Link::setIconPath(const string &icon) {
	if (fileExists(icon))
		iconPath = icon;
	else
		iconPath = gmenu2x.sc.getSkinFilePath("icons/generic.png");
	updateSurfaces();
}

void Link::setSize(int w, int h) {
	rect.w = w;
	rect.h = h;
	recalcCoordinates();
}

void Link::setPosition(int x, int y) {
	rect.x = x;
	rect.y = y;
	recalcCoordinates();
}

void Link::recalcCoordinates() {
	iconX = rect.x+(rect.w-32)/2;
	padding = (gmenu2x.skinConfInt["linkHeight"] - 32 - gmenu2x.font->getLineSpacing()) / 3;
}

void Link::run() {
	this->action();
}
