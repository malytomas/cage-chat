#include "../common/common.h"
#include <cage-core/config.h>
#include <cage-core/debug.h>
#include <cage-core/ini.h>
#include <cage-core/logger.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/networkGinnel.h>
#include <cage-core/networkNatPunch.h>
#include <cage-core/serialization.h>
#include <cage-core/string.h>
#include <cage-engine/guiBuilder.h>
#include <cage-engine/window.h>
#include <cage-simple/engine.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

Holder<GinnelConnection> sc;
uint32 myName = 0;
uint32 requestListTimer = 0;

struct Peer
{
	Holder<NatPunch> a;
	Holder<GinnelConnection> c;
	String message;
	bool error = false;
};
std::unordered_map<uint32, Peer> peers;

void cleanup()
{
	std::erase_if(peers,
		[](const auto &p)
		{
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
			CAGE_LOG_DEBUG(SeverityEnum::Info, "chat", Stringizer() + "received description from peer: " + n);
			auto &a = peers[n].a;
			if (!a)
				a = newNatPunch({});
			a->setRemoteDescription(b);
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
				switch (peer.second.a->update())
				{
					case NatPunchStatusEnum::Synchronization:
					{
						CAGE_LOG_DEBUG(SeverityEnum::Info, "chat", Stringizer() + "sending description to peer: " + peer.first);
						MemoryBuffer b;
						Serializer s(b);
						s.writeLine("description");
						s.writeLine(Stringizer() + peer.first);
						s.write(peer.second.a->getLocalDescription());
						sc->write(b, 1, true);
						break;
					}
					case NatPunchStatusEnum::Connected:
					{
						CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "finished nat punch for peer: " + peer.first);
						peer.second.c = peer.second.a->makeGinnel();
						MemoryBuffer b;
						Serializer s(b);
						s.writeLine("whatsup");
						peer.second.c->write(b, 1, true);
						break;
					}
				}
			}
			else
			{
				CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "initiating nat punch with peer: " + peer.first);
				peer.second.a = newNatPunch({});
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
		g->label().text("nat punch");
		g->label().text("ginnel");
		g->label().text("message");
	}
	for (const auto &peer : peers)
	{
		g->label().text(Stringizer() + peer.first);
		g->label().text(peer.second.a ? String(natPunchStatusToString(peer.second.a->update())) : String());
		g->label().text(peer.second.c ? (peer.second.c->established() ? "connected" : "connecting") : "");
		g->label().text(peer.second.message);
	}
}

String randomMessage()
{
	switch (randomRange(0, 20))
	{
		case 0:
			return "hi";
		case 1:
			return "heya";
		case 2:
			return "morning";
		case 3:
			return "how are things?";
		case 4:
			return "what's new?";
		case 5:
			return "it's good to see you";
		case 6:
			return "g'day";
		case 7:
			return "howdy";
		case 8:
			return "what's up?";
		case 9:
			return "how's it going?";
		case 10:
			return "what's happening?";
		case 11:
			return "what's the story?";
		case 12:
			return "yo";
		case 13:
			return "hello";
		case 14:
			return "hi there";
		case 15:
			return "good morning";
		case 16:
			return "good afternoon";
		case 17:
			return "good evening";
		case 18:
			return "it's nice to meet you";
		case 19:
			return "it's a pleasure to meet you";
		default:
			return "";
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
		CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "connecting to signaling server at: " + (String)confServerHost);
		sc = newGinnelConnection(confServerHost, ServerListenPort, 0);
	}

	engineInitialize(EngineCreateConfig());

	const auto updateListener = controlThread().update.listen(&update);
	const auto closeListener = engineWindow()->events.listen(inputListener<InputClassEnum::WindowClose, InputWindow>([](auto) { engineStop(); }));

	engineWindow()->title("nat punch test client");
	engineWindow()->setWindowed();
	initializeGui();

	engineRun();
	engineFinalize();

	return 0;
}
