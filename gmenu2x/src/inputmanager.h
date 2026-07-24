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

#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <SDL.h>
#include <string>
#include <vector>
#include <array>

#define INPUT_KEY_REPEAT_DELAY 250

class GMenu2X;
class Menu;
class InputManager;

#ifndef SDL_JOYSTICK_DISABLED
#define AXIS_STATE_POSITIVE 0
#define AXIS_STATE_NEGATIVE 1
struct Joystick {
	SDL_Joystick *joystick;
	bool axisState[2][2];
	Uint8 hatState;
	SDL_TimerID timer;
	InputManager *inputManager;
};
#endif

class InputManager {
public:
	enum Button {
		UP, DOWN, LEFT, RIGHT,
		ACCEPT, CANCEL,
		ALTLEFT, ALTRIGHT,
		MENU, SETTINGS, HOME,
		// Events that are not actually buttons:
		// (not included in BUTTON_TYPE_SIZE)
		REPAINT, QUIT,
	};
	static constexpr size_t BUTTON_TYPE_SIZE = 13;

	InputManager(GMenu2X& gmenu2x);
	~InputManager();

	bool init(Menu *menu);
	Button waitForPressedButton();
	void repeatRateChanged();
	Uint32 joystickRepeatCallback(Uint32 timeout, struct Joystick *joystick);
	bool pollButton(Button *button);
	bool getButton(Button *button, bool wait);

private:
	bool readConfFile(const std::string &conffile);

	struct ButtonMapEntry {
		bool kb_mapped, js_mapped;
		unsigned int kb_code, js_code;
	};

	GMenu2X& gmenu2x;
	Menu *menu;

	std::array<ButtonMapEntry, BUTTON_TYPE_SIZE> buttonMap;
#ifndef SDL_JOYSTICK_DISABLED
	std::vector<Joystick> joysticks;

	void startTimer(struct Joystick *joystick);
	void stopTimer(struct Joystick *joystick);
#endif
};

#endif
