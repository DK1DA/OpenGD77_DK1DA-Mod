/*
 * Copyright (C) 2019 Kai Ludwig, DG4KLU
 *           (C) 2020-2025 Roger Clark, VK3KYY / G4KYF
 *
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. Use of this source code or binary releases for commercial purposes is strictly forbidden. This includes, without limitation,
 *    incorporation in a commercial product or incorporation into a product or project which allows commercial use.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "dmr_codec/codec.h"
#include "functions/voicePrompts.h"
#include <stdint.h>
#include <string.h>

static uint16_t bitbuffer_encode[72];

/*
 * The AMBE codec entry points live inside the proprietary codec blob that the
 * firmware loader patches into flash at 0x0807537C. They are Thumb functions,
 * so a call through a pointer requires bit 0 (the Thumb bit) to be set.
 *
 * These used to be called with hand-written inline assembler that did
 * "BL <absolute address>". Newer GNU toolchains no longer accept a direct
 * branch to an absolute address (it fails to link), and even where it links the
 * bare asm (no operands / no clobber list) miscompiles and hard-faults on the
 * DMR path. Following the official R20260504 rework, the codec is now invoked
 * through C function pointers instead.
 *
 * The parameter lists below reproduce, exactly, the register/stack state the
 * original assembler set up (AAPCS: args 1-4 -> r0-r3, args 5+ -> stack).
 */
typedef void (*ambeDecodeFn_t)(void *waveOut, int n, void *bitBufIn, int a4, int a5, int halfFrame, void *ambeState);
typedef void (*ambeEncodeFn_t)(void *bitBufOut, int a2, void *waveIn, int n, int a5, int halfFrame, int a7, void *ambeState);
typedef void (*ambeEncodeEccFn_t)(void *bitBuf0, void *bitBuf1, int a3, void *eccState);

#define AMBE_DECODE_FN      ((ambeDecodeFn_t)(uintptr_t)(AMBE_DECODE | 1u))
#define AMBE_ENCODE_FN      ((ambeEncodeFn_t)(uintptr_t)(AMBE_ENCODE | 1u))
#define AMBE_ENCODE_ECC_FN  ((ambeEncodeEccFn_t)(uintptr_t)(AMBE_ENCODE_ECC | 1u))

void codecDecode(uint8_t *indata_ptr, int numbBlocks)
{
	uint16_t bitbuffer_decode[49];

	for (int idx = 0; idx < numbBlocks; idx++)
	{
		initFrame(indata_ptr, bitbuffer_decode);
		indata_ptr += 9;

		// First half-frame
		soundSetupBuffer();// sets currentWaveBuffer (the decoded-audio output buffer)
		AMBE_DECODE_FN((void *)currentWaveBuffer, 80, (void *)bitbuffer_decode, 0, 0, 0, (void *)ambebuffer_decode);
		soundStoreBuffer();

		// Second half-frame
		soundSetupBuffer();
		AMBE_DECODE_FN((void *)currentWaveBuffer, 80, (void *)bitbuffer_decode, 0, 0, 1, (void *)ambebuffer_decode);
		soundStoreBuffer();
	}
}

void codecEncodeBlock(uint8_t *outdata_ptr)
{
	memset((uint8_t *)outdata_ptr, 0, 9);// fills with zeros
	memset(bitbuffer_encode, 0, sizeof(bitbuffer_encode));

	// First half-frame
	soundRetrieveBuffer();// sets currentWaveBuffer (the audio input buffer)
	AMBE_ENCODE_FN((void *)bitbuffer_encode, 0, (void *)currentWaveBuffer, 80, 0x1840, 0, 0x2000, (void *)ambebuffer_encode);

	// Second half-frame
	soundRetrieveBuffer();
	AMBE_ENCODE_FN((void *)bitbuffer_encode, 0, (void *)currentWaveBuffer, 80, 0x0800, 1, 0x2000, (void *)ambebuffer_encode);

	// ECC pass
	AMBE_ENCODE_ECC_FN((void *)bitbuffer_encode, (void *)bitbuffer_encode, 0, (void *)ambebuffer_encode_ecc);

	for (int i = 0; i < 72; i++)
	{
		if (bitbuffer_encode[i] & 1)
		{
			outdata_ptr[i >> 3] |= 128 >> (i & 7);
		}
	}
}
