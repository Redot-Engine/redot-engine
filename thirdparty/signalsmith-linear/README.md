# Signalsmith Linear

Header-only C++11 wrappers for common things needed by our audio libraries.  It's designed for internal use, so [caveat developor](https://en.wikipedia.org/wiki/Caveat_emptor).  The goal is to wrap around Accelerate/IPP when available, but still work without it.

Everything is in `signalsmith::linear::` namespace.

## FFTs

```cpp
#include "signalsmith-linear/fft.h"
```

This provides real and complex FFTs.  They all have `.resize(size_t)`, and `.fft()`/`.ifft()` methods which can take either `std::complex<>` pointers, or a real/imaginary pair ("split-complex" form) for each complex argument.

The `Pow2FFT<>` and `Pow2RealFFT<>` templates wrap around fast implementations where available.

The main `FFT<>`, `RealFFT<>` and `ModifiedRealFFT<>` templates wrap around the `Pow2<>` implementations, to add support for multiples of 3 and 5.  They provide a static `.fastSizeAbove()` to find the next biggest size. 

### Chunked computation

The main FFT classes also provide `.steps()` method, and an optional `size_t` first argument to the `.fft()`/`.ifft()` methods, so that computation can be divided up into chunks.

The computation time for the chunks is not exactly equal, but when you're doing large FFTs periodically (instead of smaller ones regularly) it can help distribute the computation out, without using threads.

## STFTs

```cpp
#include "signalsmith-linear/stft.h"
```

This provides `DynamicSTFT<>` template, which is configured for any block length (zero padding to a fast size), and a (default!) interval between blocks.  It can have a different number of input/output channels, using `.spectrum(c)` to access both.

### Input and output

The `.synthesise()` method moves the output time forward, and adds in output block(s) generated from the spectrum.  The result can then be queried using `.readOutput()`.

Input is passed in using `.writeInput()`, but you must move the input time forward using `.moveInput()` before calling `.analyse()`.  By default, this will analyse the most recent block of input, but if you passed in a non-zero `extraInputHistory` when configuring, you can analyse input from some time in the past.

### Window shape and block interval

It has separate analysis/synthesis windows, which can be changed on-the-fly.  Both windows have an "offset" marking the centre of the window, to support asymmetrical setups (for reduced latency).

The "synthesis interval" (optionally passed to `.synthesise()`) can also change arbitrarily.  The output is normalised according to these gaps (and the analysis/synthesis windows) such that regardless of spacing or window shape, it would perfectly reconstruct the input.

The analysis should remain constant between analysis and synthesis, since it's used for normalising the output properly.  If you're synthesising signals from scratch (which have no inherent input windowing), then you should reflect that by setting the analysis window to all 1s.

### Chunked computation

Similar to the FFTs' `.steps()` methods, `DynamicSTFT` has `.analyseSteps()`/`.synthesiseSteps()` methods, and `.analyseStep()`/`.synthesiseStep()` to help you spread the computation out over time.

The window shapes/offsets, input/output and synthesis interval must stay the same until the analysis/synthesis is finished.

## Expressions

```cpp
#include "signalsmith-linear/linear.h"
```

The main `Linear` class provides expression templates, which wrap around three types of pointer: real, complex, and split-complex.  You can wrap these pointers (or `std::vector`s) into `Expression`s using the `()` operator:

```cpp
Linear linear;

float *a, *b;
std::complex<float> *c;
size_t size;

// Once the above variables are set up:
linear(a, size) = linear(b) + linear(c).abs();

// Pass in two real pointers to make a split-complex expression (which is often a bit faster).
linear(c, size) = linear(a, b).conj();
```

Implementations may use temporary internal storage.  This means it's not thread-safe, it should be a member of your processing class, and you should also call `.reserve???()` during configuration, with the longest vector length you expect to use.

## Building

### CMake

If you're using CMake, include this directory.  It will add a `signalsmith-linear` target which doesn't build anything, but linking to this "library" will add the include path.

By default, it will link to Accelerate on Mac.  If you don't want this, set the CMake option `SIGNALSMITH_USE_ACCELERATE` to `OFF`.

The similar option `SIGNALSMITH_USE_IPP` if `OFF` by default.  When enabled, it will link to IPP and set the `SIGNALSMITH_USE_IPP` preprocessor definition.

`SIGNALSMITH_USE_PFFFT` and `SIGNALSMITH_USE_PFFFT_DOUBLE` don't link to anything, because there are multiple versions.

### Other

To use Accelerate on Mac, link the framework and define `SIGNALSMITH_USE_ACCELERATE`:

```
g++ -framework Accelerate -DSIGNALSMITH_USE_ACCELERATE
```

Similarly, define `SIGNALSMITH_USE_IPP` (and link to `IPP::ippcore` and `IPP::ipps`) for IPP. 

Not all PFFFT versions support double-precision, so there are separate `SIGNALSMITH_USE_PFFFT`/`SIGNALSMITH_USE_PFFFT_DOUBLE` flags.
