#ifndef POWERSAVER_H
#define POWERSAVER_H

#include <memory>
#include <SDL.h>

class PowerSaver {
public:
	static std::shared_ptr<PowerSaver> getInstance();

	~PowerSaver();
	void resetScreenTimer();
	void setScreenTimeout(unsigned int seconds);

	bool getScreenState() const { return screenState; }

private:
	PowerSaver();

	void addScreenTimer();
	void removeScreenTimer();
	void setScreenBlanking(bool state);
	void enableScreen();
	void disableScreen();

	static std::shared_ptr<PowerSaver> instance;

	bool screenState;
	unsigned int screenTimeout;
	unsigned int timeout_startms;
	SDL_TimerID screenTimer;

	friend Uint32 screenTimerCallback(Uint32 timeout, void *d);
};

#endif
