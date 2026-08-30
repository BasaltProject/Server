package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.world.structure.BlockTagJson

/**
 * Biome tags from the operator's own `server.jar`, expanded through their nested references.
 * Never committed: the tag is Mojang's data and is read at runtime like every other piece of it.
 */
public class BiomeTagLoader(
    private val serverJar: ServerJarAssets,
) {
    private val cache = mutableMapOf<String, Set<String>>()

    /** The biome ids in [name], or an empty set when the jar has no such tag. */
    public fun biomesIn(name: String): Set<String> = cache.getOrPut(name) { expand(name, mutableSetOf()) }

    private fun expand(
        name: String,
        seen: MutableSet<String>,
    ): Set<String> {
        if (!seen.add(name)) return emptySet()
        val path = "data/minecraft/tags/worldgen/biome/${name.substringAfter(':')}.json"
        val json = serverJar.readEntry(path)?.decodeToString() ?: return emptySet()
        val result = mutableSetOf<String>()
        for (entry in BlockTagJson.parse(json)) {
            if (entry.startsWith("#")) {
                result += expand(entry.substring(1), seen)
            } else {
                result += entry
            }
        }
        return result
    }
}
