#pragma once

#if defined(__SSE__) || defined(_M_X64)
	class StopDenormals {
		unsigned int controlStatusRegister;
	public:
		StopDenormals() : controlStatusRegister(_mm_getcsr()) {
			_mm_setcsr(controlStatusRegister|0x8040); // Flush-to-Zero and Denormals-Are-Zero
		}
		~StopDenormals() {
			_mm_setcsr(controlStatusRegister);
		}
	};
#elif (defined (__ARM_NEON) || defined (__ARM_NEON__))
	class StopDenormals {
		uintptr_t status;
	public:
		StopDenormals() {
			uintptr_t asmStatus;
			asm volatile("mrs %0, fpcr" : "=r"(asmStatus));
			status = asmStatus = asmStatus|0x01000000U; // Flush to Zero
			asm volatile("msr fpcr, %0" : : "ri"(asmStatus));
		}
		~StopDenormals() {
			uintptr_t asmStatus = status;
			asm volatile("msr fpcr, %0" : : "ri"(asmStatus));
		}
	};
#else
#	if __cplusplus >= 202302L
# 		warning "The `StopDenormals` class doesn't do anything for this architecture"
#	endif
	class StopDenormals {}; // FIXME: add for other architectures
#endif
