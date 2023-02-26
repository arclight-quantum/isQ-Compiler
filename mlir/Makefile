CMAKE_FLAGS := -DISQ_OPT_ENABLE_ASSERTIONS=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo
reconfigure: unconfigure configure
unconfigure:
	rm build/.configure_stamp
configure: build/.configure_stamp
build/.configure_stamp:
	mkdir -p build
	cd build && cmake -GNinja ${CMAKE_FLAGS} ..
	touch build/.configure_stamp
build: configure
	cd build && ninja
clean:
	rm -rf build
.PHONY: configure build clean unconfigure reconfigure

