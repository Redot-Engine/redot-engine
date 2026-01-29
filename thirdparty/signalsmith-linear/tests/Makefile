main: out/tests
	@cd out && ./tests

dev: out-cmake/tests
	cd out-cmake && ./tests benchmark 0.001 fft

benchmark: out/tests
	@cd out && ./tests benchmark

benchmark-fast: out/tests
	@cd out && ./tests benchmark 0.001

# MAC config
out/tests: *.cpp *.h ../*.h ../platform/*.h
	mkdir -p out
	g++ -std=c++11 \
		-O3 -ffast-math -DSIGNALSMITH_IGNORE_BROKEN_APPLECLANG \
		-Wall -Wextra -Wfatal-errors -Wpedantic -pedantic-errors \
		-framework Accelerate -DSIGNALSMITH_USE_ACCELERATE -DACCELERATE_NEW_LAPACK \
		main.cpp -o out/tests

out-cmake/tests: CMakeLists.txt *.cpp *.h ../*.h ../platform/*.h
	cmake -B build -G Xcode
	cmake --build build --config Release
	mkdir -p out-cmake
	cp build/Release/tests out-cmake

cmake: out-cmake/tests
	cd out-cmake && ./tests

cmake-benchmark: out-cmake/tests
	cd out-cmake && ./tests benchmark

cmake-benchmark-fast: out-cmake/tests
	cd out-cmake && ./tests benchmark 0.001

clean:
	rm -rf out
	rm -rf out-cmake
	rm -rf build
