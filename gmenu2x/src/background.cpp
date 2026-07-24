// Various authors.
// License: GPL version 2 or later.

#include "background.h"

#include "gmenu2x.h"


Background::Background(GMenu2X& gmenu2x)
	: gmenu2x(gmenu2x)
	, battery(gmenu2x)
{
}

void Background::paint(Surface& s) {
	auto& font = *gmenu2x.font;
	const auto& bgmain = gmenu2x.bgmain;

	bgmain->blit(s, 0, 0);

#ifdef ENABLE_CLOCK
	font.write(s, clock.getTime(),
			s.width() / 2, gmenu2x.bottomBarTextY,
			Font::HAlignCenter, Font::VAlignMiddle);
#endif

	auto icon = battery.getIcon();
	if (icon) {
		icon->blit(s, s.width() - 19, gmenu2x.bottomBarIconY);
	}
}

bool Background::handleButtonPress(InputManager::Button button) {
	switch (button) {
		case InputManager::CANCEL:
			gmenu2x.showHelpPopup();
			return true;
		case InputManager::SETTINGS:
			gmenu2x.showSettings();
			return true;
		default:
			return false;
	}
}
