default: clean build install

clean:
	@echo "clean..."
	rm -rf build

build:
	@echo "build..."
	mkdir build
	cd build; cmake -DCMAKE_MODULE_PATH=/usr/local/share/cmake-3.8/Modules/ ..

install:
	@echo "install..."
	cd build; make && make install
