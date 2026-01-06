all: out/stretch

out/stretch: main.cpp ../signalsmith-stretch.h util/*.h util/*.hxx
	mkdir -p out
	g++ -std=c++11 -O3 -g \
		-Wall -Wextra -Wfatal-errors -Wpedantic -pedantic-errors \
		-framework Accelerate -DSIGNALSMITH_USE_ACCELERATE \
		-I ../include \
		main.cpp -o out/stretch

# Uses input files from: https://signalsmith-audio.co.uk/code/stretch/inputs.zip
examples: out/stretch
	mkdir -p out/examples
	inputs/run-all.sh out/examples/u2- out/stretch --semitones=2
	inputs/run-all.sh out/examples/d2- out/stretch --semitones=-2
	inputs/run-all.sh out/examples/u4- out/stretch --semitones=4
	inputs/run-all.sh out/examples/d4- out/stretch --semitones=-4
	inputs/run-all.sh out/examples/u8- out/stretch --semitones=8
	inputs/run-all.sh out/examples/d8- out/stretch --semitones=-8
	inputs/run-all.sh out/examples/u16- out/stretch --semitones=16
	inputs/run-all.sh out/examples/d16- out/stretch --semitones=-16
	inputs/run-all.sh out/examples/t_8- out/stretch --time=0.8
	inputs/run-all.sh out/examples/t1_2- out/stretch --time=1.2
	inputs/run-all.sh out/examples/t1_5- out/stretch --time=1.5
	inputs/run-all.sh out/examples/t2- out/stretch --time=2
	inputs/run-all.sh out/examples/t4- out/stretch --time=4

TEST_WAV ?= "inputs/voice.wav"

dev: out/stretch
	out/stretch --time=0.8 --semitones=10 $(TEST_WAV) out/shift.wav
	out/stretch --time=0.8 --semitones=10 --formant-comp $(TEST_WAV) out/shift-fc.wav
	out/stretch --time=0.8 --semitones=10 --formant-comp --formant=3 $(TEST_WAV) out/shift-fc-f3.wav
	out/stretch --time=0.8 --semitones=10 --formant-comp --formant=3 --formant-base=500 $(TEST_WAV) out/shift-fc-f3-fb500.wav
	out/stretch --time=0.8 --semitones=10 --formant-comp --formant=2 --formant-base=100 $(TEST_WAV) out/shift-fc-f2-fb100.wav

clean:
	rm -rf out

### Example use of CMake

cmake:
	# CMAKE_BUILD_TYPE is needed for single-config generators (e.g. Makefiles)
	cmake -B out/build -DCMAKE_BUILD_TYPE=Release
	cmake --build out/build --config Release
