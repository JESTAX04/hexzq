// engine_stub.cpp — preview-host backend for the red:: engine contract.
// No Lua VM here: it simulates attach + echoes executes into the console so the
// full UI path (queue → pump → console) is exercised identically to injected mode.
#include "engine.h"
#include <deque>
#include <mutex>

namespace red {

static std::mutex            g_mtx;
static std::deque<std::string> g_queue;
static std::deque<std::string> g_console;
static bool                  g_init = false;

static void PushConsoleLocked(const std::string& line)
{
	g_console.push_back(line);
	if (g_console.size() > 800)
		g_console.erase(g_console.begin(), g_console.begin() + 200);
}

void ConsoleLog(const std::string& line)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	PushConsoleLocked(line);
}

EngineStatus Status()
{
	EngineStatus s;
	s.attached = g_init;
	s.detail = g_init ? "PREVIEW BACKEND - no VM (echo mode)" : "engine offline";
	return s;
}

void EnqueueExecute(const std::string& code)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	g_queue.push_back(code);
}

bool PopConsoleLine(std::string& out)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	if (g_console.empty()) return false;
	out = std::move(g_console.front());
	g_console.pop_front();
	return true;
}

void InitEngine()
{
	std::lock_guard<std::mutex> lk(g_mtx);
	if (g_init) return;
	g_init = true;
	PushConsoleLocked("[red] preview backend ready - execution is simulated (echo mode)");
	PushConsoleLocked("[red] build the injected dll for real lua execution");
}

void PumpGameThread()
{
	std::string chunk;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		if (g_queue.empty()) return;
		chunk = std::move(g_queue.front());
		g_queue.pop_front();
	}

	// Echo simulation: count lines, detect nothing, report "success"
	size_t lines = 1;
	for (char c : chunk) if (c == '\n') ++lines;

	ConsoleLog("[echo] chunk accepted (" + std::to_string(chunk.size()) + " bytes, " + std::to_string(lines) + " lines)");
	// Fake a runtime print for any print("...") literal so the console feels alive
	size_t p = 0;
	int shown = 0;
	while ((p = chunk.find("print(", p)) != std::string::npos && shown < 5)
	{
		size_t q = chunk.find('"', p);
		size_t r = (q == std::string::npos) ? std::string::npos : chunk.find('"', q + 1);
		if (q == std::string::npos || r == std::string::npos) break;
		ConsoleLog("[echo][print] " + chunk.substr(q + 1, r - q - 1));
		p = r;
		++shown;
	}
	ConsoleLog("[echo] exit code 0 (simulated)");
}

} // namespace red
