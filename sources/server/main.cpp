#include <cage-core/logger.h>
#include <cage-core/networkGinnel.h>
#include <cage-core/concurrent.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/serialization.h>
#include <cage-core/string.h>
#include "../common/common.h"
#include <vector>
#include <string>
#include <algorithm>

struct Connection
{
	Holder<GinnelConnection> conn;
	uint32 name = 0;
};

Holder<GinnelServer> s = newGinnelServer(ServerListenPort);
std::vector<Connection> cons;
uint32 names = 1;
uint32 sendPingTimer = 0;

const Connection &find(uint32 name)
{
	for (const Connection &it : cons)
		if (it.name == name)
			return it;
	CAGE_THROW_ERROR(Exception, "name not found");
}

int main()
{
	// log to console
	Holder<Logger> log1 = newLogger();
	log1->format.bind<logFormatConsole>();
	log1->output.bind<logOutputStdOut>();

	while (true)
	{
		sendPingTimer++;
		try
		{
			if (auto a = s->accept())
			{
				Connection c;
				c.conn = std::move(a);
				c.name = names++;
				CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "accepted peer: " + c.name);
				{
					MemoryBuffer b;
					Serializer s(b);
					s.writeLine("init");
					s.writeLine(Stringizer() + c.name);
					c.conn->write(b, 1, true);
				}
				cons.push_back(std::move(c));
			}
		}
		catch (...)
		{
			// nothing
		}
		for (Connection &c : cons)
		{
			try
			{
				c.conn->update();
				while (c.conn->available())
				{
					Holder<PointerRange<const char>> b = c.conn->read();
					const String cmd = split(b);
					if (cmd == "ping")
					{
						// do nothing
					}
					else if (cmd == "list")
					{
						MemoryBuffer b;
						for (const Connection &cc : cons)
						{
							if (cc.name == c.name)
								continue;
							b.clear();
							Serializer s(b);
							s.writeLine("peer");
							s.writeLine(Stringizer() + cc.name);
							c.conn->write(b, 1, true);
						}
					}
					else if (cmd == "description")
					{
						const uint32 targetName = toUint32(split(b));
						CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "peer " + c.name + " is sending description to peer " + targetName);
						MemoryBuffer bb;
						Serializer s(bb);
						s.writeLine("description");
						s.writeLine(Stringizer() + c.name);
						s.write(b);
						find(targetName).conn->write(bb, 1, true);
					}
					else
					{
						CAGE_THROW_ERROR(Exception, "unknown command from client");
					}
				}
				if ((sendPingTimer % 100) == 0)
				{
					MemoryBuffer b;
					Serializer s(b);
					s.writeLine("ping");
					c.conn->write(b, 1, true);
				}
			}
			catch (...)
			{
				c.conn.clear();
			}
		}
		std::erase_if(cons, [](const Connection &c) {
			if (!c.conn)
			{
				CAGE_LOG(SeverityEnum::Info, "chat", Stringizer() + "removing peer: " + c.name);
				return true;
			}
			return false;
		});
		threadSleep(5000);
	}
}
