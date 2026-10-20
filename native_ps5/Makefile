CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
all: scanner-host launch-test-cli state-test-cli cover-test-cli roots-test-cli ftp-test-cli emulator-cover-test-cli
scanner-host: host_main.cpp library_core.cpp library_core.hpp
	$(CXX) $(CXXFLAGS) host_main.cpp library_core.cpp -o scanner-host
launch-test-cli: launch_test_cli.cpp launcher_client.cpp launcher_client.hpp
	$(CXX) $(CXXFLAGS) launch_test_cli.cpp launcher_client.cpp -o launch-test-cli
state-test-cli: state_test_cli.cpp library_core.cpp state_store.cpp state_store.hpp
	$(CXX) $(CXXFLAGS) state_test_cli.cpp library_core.cpp state_store.cpp -o state-test-cli
roots-test-cli: root_test_cli.cpp library_core.cpp root_manager.cpp ftp_readonly.cpp library_core.hpp root_manager.hpp
	$(CXX) $(CXXFLAGS) root_test_cli.cpp library_core.cpp root_manager.cpp ftp_readonly.cpp -o roots-test-cli
ftp-test-cli: ftp_test_cli.cpp ftp_readonly.cpp ftp_scan.cpp library_core.cpp
	$(CXX) $(CXXFLAGS) ftp_test_cli.cpp ftp_readonly.cpp ftp_scan.cpp library_core.cpp -o ftp-test-cli
clean:
	rm -f scanner-host launch-test-cli state-test-cli cover-test-cli roots-test-cli ftp-test-cli emulator-cover-test-cli
cover-test-cli: cover_test_cli.cpp cover_png.cpp cover_png.hpp
	$(CXX) $(CXXFLAGS) cover_test_cli.cpp cover_png.cpp -lz -o cover-test-cli

emulator-cover-test-cli: emulator_cover_test_cli.cpp emulator_covers.cpp ftp_readonly.cpp
	$(CXX) $(CXXFLAGS) emulator_cover_test_cli.cpp emulator_covers.cpp ftp_readonly.cpp -o emulator-cover-test-cli
