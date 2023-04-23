#include <cage-core/logger.h>
#include <cage-core/config.h>
#include <cage-core/debug.h>
#include <cage-core/ini.h>
#include <cage-core/networkGinnel.h>
#include <cage-core/networkIce.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/serialization.h>
#include <cage-core/string.h>
#include <cage-engine/guiBuilder.h>
#include <cage-engine/window.h>
#include <cage-simple/engine.h>
#include "../common/common.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

Holder<GinnelConnection> sc;
uint32 myName = 0;
uint32 requestListTimer = 0;

struct Peer
{
	Holder<IceAgent> a;
	Holder<GinnelConnection> c;
	String message;
	bool error = false;
};
std::unordered_map<uint32, Peer> peers;

void cleanup()
{
	std::erase_if(peers, [](const auto &p) {
		if (p.second.error)
		{
			CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "removing peer: " + p.first);
			return true;
		}
		return false;
	});
}

bool actionMessage(Entity *e)
{
	const String mymsg = e->value<GuiInputComponent>().value;
	MemoryBuffer b;
	Serializer s(b);
	s.writeLine("message");
	s.writeLine(mymsg);
	for (auto &peer : peers)
	{
		if (!peer.second.error && peer.second.c)
		{
			try
			{
				peer.second.c->write(b, 2, true);
			}
			catch (...)
			{
				CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "failed to send message to peer: " + peer.first);
				peer.second.error = true;
			}
		}
	}
	cleanup();
	return false;
}

void windowClose(InputWindow)
{
	engineStop();
}

String toString(IceStatus s)
{
	switch (s)
	{
	case IceStatus::Failed: return "failed";
	case IceStatus::Initialization: return "initialization";
	case IceStatus::Gathering: return "gathering";
	case IceStatus::Connecting: return "connecting";
	case IceStatus::Connected: return "connected";
	case IceStatus::Completed: return "completed";
	default: return "unknown";
	}
}

void update()
{
	sc->update();
	while (sc->available())
	{
		Holder<PointerRange<const char>> b = sc->read();
		const String cmd = split(b);
		if (cmd == "ping")
		{
			// do nothing
		}
		else if (cmd == "init")
		{
			myName = toUint32(split(b));
			engineGuiEntities()->get(10)->value<GuiTextComponent>().value = Stringizer() + myName;
		}
		else if (cmd == "peer")
		{
			const uint32 n = toUint32(split(b));
			CAGE_ASSERT(n != myName);
			peers[n];
		}
		else if (cmd == "description")
		{
			const uint32 n = toUint32(split(b));
			CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "received description from peer: " + n);
			auto &a = peers[n].a;
			if (!a)
				a = newIceAgent({});
			a->setRemoteDescription(b);
			if (myName > n)
			{
				CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "sending second description to peer: " + n);
				MemoryBuffer b;
				Serializer s(b);
				s.writeLine("description");
				s.writeLine(Stringizer() + n);
				s.write(a->getLocalDescription());
				sc->write(b, 1, true);
			}
		}
		else
		{
			CAGE_THROW_ERROR(Exception, "unknown command from server");
		}
	}
	if ((requestListTimer++ % 20) == 0)
	{
		MemoryBuffer b;
		Serializer s(b);
		s.writeLine("list");
		sc->write(b, 1, true);
	}

	for (auto &peer : peers)
	{
		try
		{
			detail::OverrideBreakpoint ob;
			if (peer.second.c)
			{
				peer.second.c->update();
				while (peer.second.c->available())
				{
					Holder<PointerRange<const char>> b = peer.second.c->read();
					const String cmd = split(b);
					CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "received command: '" + cmd + "', from peer: " + peer.first);
					if (cmd == "message")
						peer.second.message = String(b);
					else if (cmd == "whatsup")
					{
						const String mymsg = engineGuiEntities()->get(11)->value<GuiInputComponent>().value;
						MemoryBuffer b;
						Serializer s(b);
						s.writeLine("message");
						s.writeLine(mymsg);
						peer.second.c->write(b, 1, true);
					}
				}
			}
			else if (peer.second.a)
			{
				if (peer.second.a->checkStatus())
				{
					CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "finished ICE for peer: " + peer.first);
					const auto r = peer.second.a->getResult();
					peer.second.a.clear();
					const uint32 id = min(myName, peer.first) + 1000 * max(myName, peer.first);
					peer.second.c = newGinnelConnection(r.localAddress, r.localPort, r.remoteAddress, r.remotePort, id, 0);
					MemoryBuffer b;
					Serializer s(b);
					s.writeLine("whatsup");
					peer.second.c->write(b, 1, true);
				}
			}
			else
			{
				peer.second.a = newIceAgent({});
				if (myName < peer.first)
				{
					CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "sending first description to peer: " + peer.first);
					MemoryBuffer b;
					Serializer s(b);
					s.writeLine("description");
					s.writeLine(Stringizer() + peer.first);
					s.write(peer.second.a->getLocalDescription());
					sc->write(b, 1, true);
				}
			}
		}
		catch (...)
		{
			CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "failed to update peer: " + peer.first);
			peer.second.error = true;
		}
	}
	cleanup();

	detail::guiDestroyEntityRecursively(engineGuiEntities()->getOrCreate(2));
	Holder<GuiBuilder> g = newGuiBuilder(engineGuiEntities()->get(1));
	auto _1 = g->setNextName(2).empty();
	auto _2 = g->topColumn();
	auto _3 = g->verticalTable(4);
	{
		g->label().text("name");
		g->label().text("ice agent");
		g->label().text("ginnel");
		g->label().text("message");
	}
	for (const auto &peer : peers)
	{
		g->label().text(Stringizer() + peer.first);
		g->label().text(peer.second.a ? toString(peer.second.a->getStatus()) : "");
		g->label().text(peer.second.c ? (peer.second.c->established() ? "connected" : "connecting") : "");
		g->label().text(peer.second.message);
	}
}

String randomMessage()
{
	switch (randomRange(0, 20))
	{
	case 0: return "hi";
	case 1: return "heya";
	case 2: return "morning";
	case 3: return "how are things?";
	case 4: return "what's new?";
	case 5: return "it's good to see you";
	case 6: return "g'day";
	case 7: return "howdy";
	case 8: return "what's up?";
	case 9: return "how's it going?";
	case 10: return "what's happening?";
	case 11: return "what's the story?";
	case 12: return "yo";
	case 13: return "hello";
	case 14: return "hi there";
	case 15: return "good morning";
	case 16: return "good afternoon";
	case 17: return "good evening";
	case 18: return "it's nice to meet you";
	case 19: return "it's a pleasure to meet you";
	default: return "";
	}
}

void initializeGui()
{
	Holder<GuiBuilder> g = newGuiBuilder(engineGuiEntities());
	auto _1 = g->topColumn();
	{
		auto _1 = g->row();
		g->setNextName(10).label().text("");
		g->setNextName(11).input(randomMessage()).text("your message").bind<&actionMessage>();
	}
	auto _2 = g->setNextName(1).empty();
}

int main(int argc, const char *args[])
{
	// log to console
	Holder<Logger> log1 = newLogger();
	log1->format.bind<logFormatConsole>();
	log1->output.bind<logOutputStdOut>();

	{
		ConfigString confServerHost("cagechat/server/host", "localhost");
		Holder<Ini> cmd = newIni();
		cmd->parseCmd(argc, args);
		confServerHost = cmd->cmdString('h', "host", confServerHost);
		cmd->checkUnusedWithHelp();
		sc = newGinnelConnection(confServerHost, ServerListenPort, 0);
	}

	engineInitialize(EngineCreateConfig());

	EventListener<void()> updateListener;
	updateListener.attach(controlThread().update);
	updateListener.bind<&update>();
	InputListener<InputClassEnum::WindowClose, InputWindow> closeListener;
	closeListener.attach(engineWindow()->events);
	closeListener.bind<&windowClose>();

	engineWindow()->title("cage chat - ICE test client");
	engineWindow()->setWindowed();
	initializeGui();

	engineStart();
	engineFinalize();

	return 0;
}
