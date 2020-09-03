# Ppeureka

*Version 0.0.2*

A C++ client library for Spring Cloud Eureka.

Note that this project is under development and doesn't promise a stable interface.

I do not known the version of Eureka, 

The library is written in C++11 and requires a quite modern compiler. Currently it's compiled with:
* Windows: Visual Studio 2017

The library depends on:
* [libCURL](http://curl.haxx.se/libcurl/)

The library includes code of the following 3rd party libraries (check `ext` directory):
* [json11](https://github.com/dropbox/json11) library to deal with JSON.
* [libb64](http://libb64.sourceforge.net/) library for base64 decoding.


Unit test is TODO.

## Examples

Usage of Eureka rest api:


```cpp
#include <iostream>
#include <ppeureka/ppeureka.h>
#include <ppeureka/eureka_agent.h>
#include <thread>

using namespace ppeureka;

int main()
{
	try
	{
		ppeureka::agent::EurekaConnect conn;
		conn.setEndpoints({ "127.0.0.1:8700" });
		conn.start();

		auto inses = conn.queryInsAll();

		inses = conn.queryInsByAppId("EUREKA-SERVER");

		//inses = conn.queryInsByAppIdInsId("EUREKA-SERVER", "xxx:eureka-server:8700");

		inses = conn.queryInsByVip("eureka-server");

		inses = conn.queryInsBySVip("eureka-server");


		auto newIns = conn.getEmptyIns("demo-order2", "demo-order2:1234", 1234, "192.168.11.49");
		conn.registerIns(newIns);

		conn.statusOutOfService("demo-order2", "demo-order2:1234");
		conn.statusUp("demo-order2", "demo-order2:1234");

		conn.unregisterIns("demo-order2", "demo-order2:1234");

		ppeureka::agent::EurekaAgent agent(conn);
		agent.start();

		agent.registerIns("demo-order3", "www.baidu.com", 80);
		auto insId = agent.makeInsId("demo-order3", "www.baidu.com", 80);

		int tryCount = 0;
		while (tryCount++ < 3)
		{
			std::cout << "run: " << tryCount << std::endl;

			try
			{
				auto cfg_str = agent.callHttpConfigServer("demo-order3", "");
				std::cout << "call http cfg: " << cfg_str.length() << std::endl;
			}
			catch (const ppeureka::Error &e)
			{
				std::cout << "call http cfg fail: " << e.what() << std::endl;
			}

			try
			{
				auto cli = agent.getHttpClient("demo-order3");
				auto cfg_str = cli->requestRespData(ppeureka::agent::HttpMethod::METHOD_GET, "/", "");
				std::cout << "demo-order3: " << cfg_str.length() << std::endl;
			}
			catch (const ppeureka::Error &e)
			{
				std::cout << "call demo-order3 fail: " << e.what() << std::endl;
			}

			ppeureka::agent::AgentSnap snap;
			agent.getSnap(snap);
			std::cout << "snap: " << std::endl;
			std::cout << "  regs: " << snap.regs.size() << std::endl;
			for (auto &&stReg : snap.regs)
			{
				std::cout << "    " << stReg.first << ":";
				auto &regSnap = stReg.second;
				std::cout << " tm[" << regSnap.lastHeartTime.time_since_epoch().count() << "]";
				std::cout << " suc[" << regSnap.heartSucCount << "]";
				std::cout << " err[" << regSnap.heartErrCount << "]";
				std::cout << std::endl;
			}
			std::cout << "  apps: " << snap.apps.size() << std::endl;
			for (auto &&stApp : snap.apps)
			{
				std::cout << "    " << stApp.first << ": " << stApp.second.size() << std::endl;
				for (auto &&stIns : stApp.second)
				{
					std::cout << "      " << stIns.first << ":";
					auto &insSnap = stIns.second;
					std::cout << "       ep[" << insSnap.endpoint << "]" << std::endl;

					ppeureka::agent::EurekaAgent::CheckInsStatistics::SumAvg statSuc, statErr;
					if (!insSnap.statis.respSucTimeMicroSec.empty())
						statSuc = insSnap.statis.respSucTimeMicroSec.back();
					if (!insSnap.statis.respErrTimeMicroSec.empty())
						statErr = insSnap.statis.respErrTimeMicroSec.back();
					std::cout << "       stat[" << insSnap.statis.requestCountAll;
					std::cout << " suc(" << statSuc.count << "," << statSuc.avg() << ")";
					std::cout << " err(" << statErr.count << "," << statErr.avg() << ")";
					std::cout << "]" << std::endl;

					const auto &errSta = insSnap.errState;
					std::cout << "       err[" 
						<< " errStep:" << errSta.errStep
						<< " errTime:" << errSta.errTime.time_since_epoch().count()
						<< " inChoos:" << errSta.inChoosingCount
						<< " goodCnt:" << errSta.goodCount
						<< " errCnt:" << errSta.errorCount
						<< " errCntPrv:" << errSta.errorCountPrev
						<< "]";
					std::cout << std::endl;
				}
			}

			std::cout << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds{ 3 });
		}

		agent.unregisterIns("demo-order3", insId);

		std::cout << "agent stoping..." << std::endl;
		agent.stop();

		std::cout << "conn stoping..." << std::endl;
		conn.stop();
	}
	catch (const ppeureka::Error &e)
	{
		int iii = 0;
		iii = 1;
		std::cout << "error : " << e.what() << std::endl;
	}

	std::cout << "stoped." << std::endl;
    return 0;
}
```


## Documentation
TBD

## How To Build

### Get Dependencies
* Get C++11 compatible compiler. See above for the list of supported compilers.
* Install [CMake](http://www.cmake.org/) 3.1 or above.
* Install [libCURL](http://curl.haxx.se/libcurl/) (any version should be fine).

### Build

Prepare project:
```bash
mkdir workspace
cd workspace
cmake ..
```


Build:
```bash
cmake --build . --config Release
```

If Makefile generator was used then you can also do:
```bash
make
```

## How to Install

Build it first as described above then run
```bash
cmake --build . --config Release --target install
```

If Makefile generator was used then you can also do:
```bash
make install
```
## How to Use

Build and install it first as described above.

When installed, the library can be simply used in any CMake-based project as following:

```
find_package(ppeureka)
add_executable(<your_app> ...)
target_link_libraries(<your_app> ppeureka)
```

## License
The library released under [Boost Software License v1.0](http://www.boost.org/LICENSE_1_0.txt).
