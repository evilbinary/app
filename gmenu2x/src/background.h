// Various authors.
// License: GPL version 2 or later.

#ifndef BACKGROUND_H
#define BACKGROUND_H

#include "battery.h"
#include "clock.h"
#include "layer.h"

class GMenu2X;


/**
 * The backmost layer.
 */
class Background : public Layer {
public:
	Background(GMenu2X& gmenu2x);

	// Layer implementation:
	virtual void paint(Surface& s);
	virtual bool handleButtonPress(InputManager::Button button);

private:
	GMenu2X& gmenu2x;
	Battery battery;
#ifdef ENABLE_CLOCK
	Clock clock;
#endif
};

#endif // BACKGROUND_H
