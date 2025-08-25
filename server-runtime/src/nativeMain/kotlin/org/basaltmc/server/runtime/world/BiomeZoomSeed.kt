package org.basaltmc.server.runtime.world

import dev.whyoleg.cryptography.CryptographyProvider
import dev.whyoleg.cryptography.DelicateCryptographyApi
import dev.whyoleg.cryptography.algorithms.SHA256

/** // parity: BiomeManager.obfuscateSeed, including Guava's little-endian hashLong/asLong. */
@OptIn(DelicateCryptographyApi::class)
internal fun obfuscateBiomeSeed(seed: Long): Long {
    val input = ByteArray(Long.SIZE_BYTES) { index -> (seed ushr (index * 8)).toByte() }
    val digest =
        CryptographyProvider.Default
            .get(SHA256)
            .hasher()
            .hashBlocking(input)
    var result = 0L
    for (index in 0 until Long.SIZE_BYTES) {
        result = result or ((digest[index].toLong() and 0xFFL) shl (index * 8))
    }
    return result
}
