package org.basaltmc.auth

import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.AES
import dev.whyoleg.cryptography.operations.Cipher

/**
 * AES-128/CFB8, stateful across calls to [update] so it can encrypt or decrypt a socket's byte
 * stream as it arrives in arbitrary-sized chunks, exactly like the `CipherInputStream`/
 * `CipherOutputStream` vanilla wraps its channel in after the login key exchange.
 *
 * // parity: `net.minecraft.util.Crypt.getCipher`, `Cipher.getInstance("AES/CFB8/NoPadding")`
 * with the AES key's own bytes reused as the IV. `cryptography-kotlin`'s `AES.CFB8` only exposes
 * whole-message operations (a fresh random IV per call), which doesn't match this: CFB8 here is a
 * single continuous keystream for the entire connection lifetime, in each direction
 * independently. So this drives the block cipher itself, one call per byte, exactly as CFB8 is
 * defined (NIST SP 800-38A 6.3): encrypt the 16-byte shift register, XOR its first byte with the
 * input byte, then shift the *ciphertext* byte into the register, for both directions. This is
 * inherently one AES-128 block operation per byte; matching vanilla's own well-known CFB8 cost,
 * not a Basalt-specific inefficiency.
 *
 * Verified against real `javax.crypto.Cipher("AES/CFB8/NoPadding")` output, including chunked
 * calls with odd boundaries, to prove the streaming state is bit-exact (see [Cfb8CipherTest]).
 */
@OptIn(DelicateCryptographyApi::class)
public class Cfb8Cipher private constructor(
    private val blockCipher: Cipher,
    key: ByteArray,
) {
    private val register: ByteArray = key.copyOf()

    public fun update(
        input: ByteArray,
        offset: Int = 0,
        length: Int = input.size - offset,
        encrypting: Boolean,
    ): ByteArray {
        val output = ByteArray(length)
        for (i in 0 until length) {
            val keystreamByte = blockCipher.encryptBlocking(register)[0]
            val inputByte = input[offset + i]
            val outputByte = (inputByte.toInt() xor keystreamByte.toInt()).toByte()
            output[i] = outputByte
            val feedbackByte = if (encrypting) outputByte else inputByte
            register.copyInto(register, destinationOffset = 0, startIndex = 1, endIndex = register.size)
            register[register.size - 1] = feedbackByte
        }
        return output
    }

    public companion object {
        /** [key] is AES-128 (16 bytes); vanilla reuses it verbatim as the initial shift register. */
        public fun create(key: ByteArray): Cfb8Cipher {
            require(key.size == 16) { "AES-128 key must be 16 bytes, was ${key.size}" }
            val ecb = CryptographyProvider.Default.get(AES.ECB)
            val ecbKey = ecb.keyDecoder().decodeFromByteArrayBlocking(AES.Key.Format.RAW, key)
            val blockCipher = ecbKey.cipher(padding = false)
            return Cfb8Cipher(blockCipher, key)
        }
    }
}
