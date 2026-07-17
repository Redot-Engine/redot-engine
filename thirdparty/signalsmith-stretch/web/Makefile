release/SignalsmithStretch.mjs: release/SignalsmithStretch.js
	@ echo "let module = {}, exports = {};" > release/SignalsmithStretch.mjs
	@ cat release/SignalsmithStretch.js >> release/SignalsmithStretch.mjs
	@ echo "let _export=SignalsmithStretch;export default _export;" >> release/SignalsmithStretch.mjs

release/SignalsmithStretch.js: emscripten/main.js web-wrapper.js
	@ cp emscripten/main.js release/SignalsmithStretch.js
	@ cat web-wrapper.js >> release/SignalsmithStretch.js

emscripten/main.js: emscripten/main.cpp ../signalsmith-stretch.h
	@ emscripten/compile.sh emscripten/main.cpp emscripten/main.js SignalsmithStretch

## Development helpers

jsdoc: release/SignalsmithStretch.js release/SignalsmithStretch.mjs
	npx jsdoc release --verbose

server:
	python3 -m http.server

watch:
	# pip3 install watchdog
	watchmedo shell-command --patterns='*.js;Makefile' --command='make jsdoc;echo "rebuilt"' --drop