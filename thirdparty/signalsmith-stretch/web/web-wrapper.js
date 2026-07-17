function registerWorkletProcessor(Module, audioNodeKey) {
	class WasmProcessor extends AudioWorkletProcessor {
		constructor(options) {
			super(options);
			this.wasmReady = false;
			this.wasmModule = null;
			this.channels = 0;
			this.buffersIn = [];
			this.buffersOut = [];
			
			this.audioBuffers = []; // list of (multi-channel) audio buffers
			this.audioBuffersStart = 0; // time-stamp for the first audio buffer
			this.audioBuffersEnd = 0; // just to be helpful
			
			this.timeIntervalSamples = sampleRate*0.1;
			this.timeIntervalCounter = 0;
			
			this.timeMap = [{
				active: false,
				input: 0,
				output: 0,
				rate: 1,
				semitones: 0,
				tonalityHz: 8000,
				formantSemitones: 0,
				formantCompensation: false,
				formantBaseHz: 0, /* 0 = attempt to detect */
				loopStart: 0,
				loopEnd: 0
			}];
			
			let remoteMethods = {
				configure: config => {
					Object.assign(this.config, config);
					this.configure();
				},
				latency: _ => {
					return this.inputLatencySeconds + this.outputLatencySeconds;
				},
				setUpdateInterval: seconds => {
					this.timeIntervalSamples = sampleRate*seconds;
				},
				stop: when => {
					if (typeof when !== 'number') when = currentTime;
					return remoteMethods.schedule({active: false, output: when});
				},
				start: (when, offset, duration, rate, semitones) => {
					if (typeof when === 'object') {
						if (!('active' in when)) when.active = true;
						return remoteMethods.schedule(when);
					}
					
					let obj = {active: true, input: 0, output: currentTime + this.outputLatencySeconds};
					if (typeof when === 'number') obj.output = when;
					if (typeof offset === 'number') obj.input = offset;
					if (typeof rate === 'number') obj.rate = rate;
					if (typeof semitones === 'number') obj.semitones = semitones;
					let result = remoteMethods.schedule(obj);
					if (typeof duration === 'number') {
						remoteMethods.stop(obj.output + duration);
						obj.output += duration;
						obj.active = false;
						remoteMethods.schedule(obj);
					}
					return result;
				},
				schedule: (objIn, adjustPrevious) => {
					let outputTime = ('outputTime' in objIn) ? objIn.outputTime : currentTime;

					let latestSegment = this.timeMap[this.timeMap.length - 1];
					while (this.timeMap.length && this.timeMap[this.timeMap.length - 1].output >= outputTime) {
						latestSegment = this.timeMap.pop();
					}

					let obj = Object.assign({}, latestSegment);
					Object.assign(obj, {
						input: null,
						output: outputTime,
					});
					Object.assign(obj, objIn);
					if (obj.input === null) {
						let rate = (latestSegment.active ? latestSegment.rate : 0);
						obj.input = latestSegment.input + (obj.output - latestSegment.output)*rate;
					}
					this.timeMap.push(obj);

					if (adjustPrevious && this.timeMap.length > 1) {
						let previous = this.timeMap[this.timeMap.length - 2];
						if (previous.output < currentTime) {
							let rate = (previous.active ? previous.rate : 0);
							previous.input += (currentTime - previous.output)*rate;
							previous.output = currentTime;
						}
						previous.rate = (obj.input - previous.input)/(obj.output - previous.output);
					}
	
					let currentMapSegment = this.timeMap[0];
					while (this.timeMap.length > 1 && this.timeMap[1].output <= outputTime) {
						this.timeMap.shift();
						currentMapSegment = this.timeMap[0];
					}
					let rate = (currentMapSegment.active ? currentMapSegment.rate : 0);
					let inputTime = currentMapSegment.input + (outputTime - currentMapSegment.output)*rate;
					this.timeIntervalCounter = this.timeIntervalSamples;
					this.port.postMessage(['time', inputTime]);
					
					return obj;
				},
				dropBuffers: toSeconds => {
					if (typeof toSeconds !== 'number') {
						let buffers = this.audioBuffers.flat(1).map(b => b.buffer);
						this.audioBuffers = [];
						this.audioBuffersStart = this.audioBuffersEnd = 0;
						return {
							value: {start: 0, end: 0},
							transfer: buffers
						};
					}
					let transfer = [];
					while (this.audioBuffers.length) {
						let first = this.audioBuffers[0];
						let length = first[0].length;
						let endSamples = this.audioBuffersStart + length;
						let endSeconds = endSamples/sampleRate;
						if (endSeconds > toSeconds) break;

						this.audioBuffers.shift().forEach(b => transfer.push(b.buffer));
						this.audioBuffersStart += length;
					}
					return {
						value: {
							start: this.audioBuffersStart/sampleRate,
							end: this.audioBuffersEnd/sampleRate
						},
						transfer: transfer
					};
				},
				addBuffers: sampleBuffers => {
					sampleBuffers = [].concat(sampleBuffers);
					this.audioBuffers.push(sampleBuffers);
					let length = sampleBuffers[0].length;
					this.audioBuffersEnd += length;
					return this.audioBuffersEnd/sampleRate;
				}
			};

			let pendingMessages = [];
			this.port.onmessage = event => pendingMessages.push(event);

			Module().then(wasmModule => {
				this.wasmModule = wasmModule;
				this.wasmReady = true;

				wasmModule._main();

				this.channels = options.numberOfOutputs ? options.outputChannelCount[0] : 2; // stereo by default
				this.configure();

				this.port.onmessage = event => {
					let data = event.data;
					let messageId = data.shift();
					let method = data.shift();
					let result = remoteMethods[method](...data);
					if (result?.transfer) {
						this.port.postMessage([messageId, result.value], result.transfer);
					} else {
						this.port.postMessage([messageId, result]);
					}
				};
				let methodArgCounts = {};
				for (let key in remoteMethods) {
					methodArgCounts[key] = remoteMethods[key].length;
				}
				this.port.postMessage(['ready', methodArgCounts]);
				pendingMessages.forEach(this.port.onmessage);
				pendingMessages = null;
			});
		}
		
		config = {
			preset: 'default'
		};
		configure() {
			if (this.config.blockMs) {
				let blockSamples = Math.round(this.config.blockMs/1000*sampleRate);
				let intervalSamples = Math.round((this.config.intervalMs || this.config.blockMs*0.25)/1000*sampleRate);
				let splitComputation = this.config.splitComputation;
				this.wasmModule._configure(this.channels, blockSamples, intervalSamples, splitComputation);
				this.wasmModule._reset();
			} else if (this.config.preset == 'cheaper') {
				this.wasmModule._presetCheaper(this.channels, sampleRate);
			} else {
				this.wasmModule._presetDefault(this.channels, sampleRate);
			}
			this.updateBuffers();
			this.inputLatencySeconds = this.wasmModule._inputLatency()/sampleRate;
			this.outputLatencySeconds = this.wasmModule._outputLatency()/sampleRate;
		}
		
		updateBuffers() {
			let wasmModule = this.wasmModule;
			// longer than one STFT block, so we can seek smoothly
			this.bufferLength = (wasmModule._inputLatency() + wasmModule._outputLatency());
			
			let lengthBytes = this.bufferLength*4;
			let bufferPointer = wasmModule._setBuffers(this.channels, this.bufferLength);
			this.buffersIn = [];
			this.buffersOut = [];
			for (let c = 0; c < this.channels; ++c) {
				this.buffersIn.push(bufferPointer + lengthBytes*c);
				this.buffersOut.push(bufferPointer + lengthBytes*(c + this.channels));
			}
		}

		process(inputList, outputList, parameters) {
			if (!this.wasmReady) {
				outputList.forEach(output => {
					output.forEach(channel => {
						channel.fill(0);
					});
				});
				return true;
			}
			if (!outputList[0]?.length) return false;

			let outputTime = currentTime + this.outputLatencySeconds;
			while (this.timeMap.length > 1 && this.timeMap[1].output <= outputTime) {
				this.timeMap.shift();
			}
			let currentMapSegment = this.timeMap[0];

			let wasmModule = this.wasmModule;
			wasmModule._setTransposeSemitones(currentMapSegment.semitones, currentMapSegment.tonalityHz/sampleRate);
			wasmModule._setFormantSemitones(currentMapSegment.formantSemitones, currentMapSegment.formantCompensation);
			wasmModule._setFormantBase(currentMapSegment.formantBaseHz/sampleRate);

			// Check the input/output channel counts
			if (outputList[0].length != this.channels) {
				this.channels = outputList[0]?.length || 0;
				configure();
			}
			let outputBlockSize = outputList[0][0].length;

			let memory = wasmModule.exports ? wasmModule.exports.memory.buffer : wasmModule.HEAP8.buffer;
			// Buffer list (one per channel)
			let inputs = inputList[0];
			if (!currentMapSegment.active) {
				outputList[0].forEach((_, c) => {
					let channelBuffer = inputs[c%inputs.length];
					let buffer = new Float32Array(memory, this.buffersIn[c], outputBlockSize);
					buffer.fill(0);
				});
				// Should detect silent input and skip processing
				wasmModule._process(outputBlockSize, outputBlockSize);
			} else if (inputs?.length) {
				// Live input
				outputList[0].forEach((_, c) => {
					let channelBuffer = inputs[c%inputs.length];
					let buffer = new Float32Array(memory, this.buffersIn[c], outputBlockSize);
					if (channelBuffer) {
						buffer.set(channelBuffer);
					} else {
						buffer.fill(0);
					}
				})
				wasmModule._process(outputBlockSize, outputBlockSize);
			} else {
				let inputTime = currentMapSegment.input + (outputTime - currentMapSegment.output)*currentMapSegment.rate;
				let loopLength = currentMapSegment.loopEnd - currentMapSegment.loopStart;
				if (loopLength > 0 && inputTime >= currentMapSegment.loopEnd) {
					currentMapSegment.input -= loopLength;
					inputTime -= loopLength;
				}
				
				inputTime += this.inputLatencySeconds;
				let inputSamplesEnd = Math.round(inputTime*sampleRate);

				// Fill the buffer with previous input
				let buffers = outputList[0].map((_, c) => new Float32Array(memory, this.buffersIn[c], this.bufferLength));

				let blockSamples = 0; // current write position in the temporary input buffer
				let audioBufferIndex = 0;
				let audioSamples = this.audioBuffersStart; // start of current audio buffer
				// zero-pad until the start of the audio data
				let inputSamples = inputSamplesEnd - this.bufferLength;
				if (inputSamples < audioSamples) {
					blockSamples = audioSamples - inputSamples;
					buffers.forEach(b => b.fill(0, 0, blockSamples));
					inputSamples = audioSamples;
				}
				while (audioBufferIndex < this.audioBuffers.length && audioSamples < inputSamplesEnd) {
					let audioBuffer = this.audioBuffers[audioBufferIndex];
					let startIndex = inputSamples - audioSamples; // start index within the audio buffer
					let bufferEnd = audioSamples + audioBuffer[0].length;
					// how many samples to copy: min(how many left in the buffer, how many more we need)
					let count = Math.min(audioBuffer[0].length - startIndex, inputSamplesEnd - inputSamples);
					if (count > 0) {
						buffers.forEach((buffer, c) => {
							let channelBuffer = audioBuffer[c%audioBuffer.length];
							buffer.subarray(blockSamples).set(channelBuffer.subarray(startIndex, startIndex + count));
						});
						audioSamples += count;
						blockSamples += count;
					} else { // we're already past this buffer - skip it
						audioSamples += audioBuffer[0].length;
					}
					++audioBufferIndex;
				}
				if (blockSamples < this.bufferLength) {
					buffers.forEach(buffer => buffer.subarray(blockSamples).fill(0));
				}

				// constantly seeking, so we don't have to worry about the input buffers needing to be a rate-dependent size
				wasmModule._seek(this.bufferLength, currentMapSegment.rate);
				wasmModule._process(0, outputBlockSize);

				this.timeIntervalCounter -= outputBlockSize;
				if (this.timeIntervalCounter <= 0) {
					this.timeIntervalCounter = this.timeIntervalSamples;
					this.port.postMessage(['time', inputTime]);
				}
			}
			
			// Re-fetch in case the memory changed (even though there *shouldn't* be any allocations)
			memory = wasmModule.exports ? wasmModule.exports.memory.buffer : wasmModule.HEAP8.buffer;
			outputList[0].forEach((channelBuffer, c) => {
				let buffer = new Float32Array(memory, this.buffersOut[c], outputBlockSize);
				channelBuffer.set(buffer);
			});
			
			return true;
		}
	}

	registerProcessor(audioNodeKey, WasmProcessor);
}

/**
	Creates a Stretch node
	@async
	@function SignalsmithStretch
	@param {AudioContext} audioContext
	@param {Object} options - channel configuration (as per [options]{@link https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletNode/AudioWorkletNode#options})
	@returns {Promise<StretchNode>}
*/
SignalsmithStretch = ((Module, audioNodeKey) => {
	if (typeof AudioWorkletProcessor === "function" && typeof registerProcessor === "function") {
		// AudioWorklet side
		registerWorkletProcessor(Module, audioNodeKey);
		return {};
	}
	let promiseKey = Symbol();
	let createNode = async function(audioContext, options) {
		/**
			@classdesc An `AudioWorkletNode` with Signalsmith Stretch extensions
			@name StretchNode
			@augments AudioWorkletNode
			@property {number} inputTime - the current playback (in seconds) within the input audio stored by the node
		 */
		let audioNode;
		options = options || {
			numberOfInputs: 1,
			numberOfOutputs: 1,
			outputChannelCount: [2]
		};
		try {
			audioNode = new AudioWorkletNode(audioContext, audioNodeKey, options);
		} catch (e) {
			if (!audioContext[promiseKey]) {
				let moduleUrl = createNode.moduleUrl;
				if (!moduleUrl) {
					let moduleCode = `(${registerWorkletProcessor})((_scriptName=>${Module})(),${JSON.stringify(audioNodeKey)})`;
					moduleUrl = URL.createObjectURL(new Blob([moduleCode], {type: 'text/javascript'}));
				}
				audioContext[promiseKey] = audioContext.audioWorklet.addModule(moduleUrl);
			}
			await audioContext[promiseKey];
			audioNode = new AudioWorkletNode(audioContext, audioNodeKey, options);
		}

		// messages with Promise responses
		let requestMap = {};
		let idCounter = 0;
		let timeUpdateCallback = null;
		let post = (transfer, ...data) => {
			let id = idCounter++;
			return new Promise(resolve => {
				requestMap[id] = resolve;
				audioNode.port.postMessage([id].concat(data), transfer);
			});
		};
		audioNode.inputTime = 0;
		audioNode.port.onmessage = (event) => {
			let data = event.data;
			let id = data[0], value = data[1];
			if (id == 'time') {
				audioNode.inputTime = value;
				if (timeUpdateCallback) timeUpdateCallback(value);
			}
			if (id in requestMap) {
				requestMap[id](value);
				delete requestMap[id];
			}
		};
		
		return new Promise(resolve => {
			requestMap['ready'] = remoteMethodKeys => {
				Object.keys(remoteMethodKeys).forEach(key => {
					let argCount = remoteMethodKeys[key];
					audioNode[key] = (...args) => {
						let transfer = null;
						if (args.length > argCount) {
							transfer = args.pop();
						}
						return post(transfer, key, ...args);
					}
				});
				/** @lends StretchNode.prototype
					@method setUpdateInterval
				*/
				audioNode.setUpdateInterval = (seconds, callback) => {
					timeUpdateCallback = callback;
					return post(null, 'setUpdateInterval', seconds);
				}
				resolve(audioNode);
			}
		});
	};
	return createNode;
})(SignalsmithStretch, "signalsmith-stretch");
// register as a CommonJS/AMD module
if (typeof exports === 'object' && typeof module === 'object') {
	module.exports = SignalsmithStretch;
} else if (typeof define === 'function' && define['amd']) {
	define([], () => SignalsmithStretch);
}
