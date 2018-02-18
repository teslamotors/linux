/*

Copyright (c) 2012, Dmitry Grinberg (dmitrygr@gmail.com / http://dmitrygr.com)
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _SBC_DEC_H_
#define _SBC_DEC_H_

#include <linux/types.h>

#define SBC_MAX_SAMPLES_PER_PACKET   128
#define SBC_MAX_PACKET_SIZE          262  /* 2 + 8 / 2 + 16 * 8 * 16 / 8 */

/**
 * Reset the SBC audio decoder state.
 */
void sbc_decoder_reset(void);

/**
 * Decode a packet of SBC audio data to PCM.
 */
uint32_t sbc_decode(uint8_t blocks_per_packet, uint8_t num_bits,
		    const uint8_t* buf, uint16_t len,
		    int16_t* outbuf);	/*return num SAMPLES produced */


#endif


