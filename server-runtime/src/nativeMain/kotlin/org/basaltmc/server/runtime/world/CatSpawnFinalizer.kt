package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.nbt.NbtCompound

/** World conditions consulted by `Cat.finalizeSpawn` after its superclass finalizer. */
internal data class CatSpawnContext(
    val moonBrightness: Float,
    val inBlackCatStructure: Boolean,
) {
    companion object {
        val DEFAULT = CatSpawnContext(moonBrightness = 0f, inBlackCatStructure = false)
    }
}

/** Minecraft 26.2's post-super cat variant and sound selection. */
internal object CatSpawnFinalizer {
    fun afterCommon(
        context: CatSpawnContext,
        random: BasaltRandom,
        data: NbtCompound,
    ) {
        val variant =
            if (context.inBlackCatStructure) {
                // PriorityProvider still samples the one-element highest-priority result.
                random.nextInt(1)
                ALL_BLACK
            } else {
                val candidateCount =
                    if (context.moonBrightness >= FULL_MOON_THRESHOLD) {
                        VARIANTS.size
                    } else {
                        ALL_BLACK_INDEX
                    }
                VARIANTS[random.nextInt(candidateCount)]
            }
        data.putString("variant", "minecraft:$variant")
        data.putString("sound_variant", "minecraft:${SOUND_VARIANTS[random.nextInt(SOUND_VARIANTS.size)]}")
    }

    private const val FULL_MOON_THRESHOLD = 0.9f
    private const val ALL_BLACK_INDEX = 10
    private const val ALL_BLACK = "all_black"
    private val VARIANTS =
        listOf(
            "tabby",
            "black",
            "red",
            "siamese",
            "british_shorthair",
            "calico",
            "persian",
            "ragdoll",
            "white",
            "jellie",
            ALL_BLACK,
        )
    private val SOUND_VARIANTS = listOf("classic", "royal")
}
