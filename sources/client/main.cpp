#include <cage-core/logger.h>
#include <cage-engine/guiManager.h>
#include <cage-engine/window.h>
#include <cage-simple/engine.h>
#include "../common/common.h"

void windowClose(InputWindow)
{
	engineStop();
}

void update()
{

}

int main()
{
	// log to console
	Holder<Logger> log1 = newLogger();
	log1->format.bind<logFormatConsole>();
	log1->output.bind<logOutputStdOut>();

	engineInitialize(EngineCreateConfig());

	EventListener<void()> updateListener;
	updateListener.attach(controlThread().update);
	updateListener.bind<&update>();
	InputListener<InputClassEnum::WindowClose, InputWindow> closeListener;
	closeListener.attach(engineWindow()->events);
	closeListener.bind<&windowClose>();

	engineWindow()->setMaximized();

	engineStart();
	engineFinalize();

	return 0;
}
