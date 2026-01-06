// Adapted from the Emscripten error message when initialising std::random_device
var crypto = globalThis?.crypto || {
	getRandomValues: array => {
		// Cryptographically insecure, but fine for audio
		for (var i = 0; i < array.length; i++) array[i] = (Math.random()*256)|0;
	}
};
var performance = globalThis?.performance || {
	now: _ => Date.now()
};
