package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.io.Gzip
import org.basaltmc.io.NativeMutex
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.structure.BlockTagJson
import org.basaltmc.world.structure.StructureTemplate
import org.basaltmc.world.structure.StructureTemplatePool
import org.basaltmc.world.structure.TemplateLoader
import org.basaltmc.world.structure.VillagePoolJson

/**
 * Resolves structure pool/piece names (`"minecraft:village/plains/town_centers"`,
 * `"minecraft:bastion/starts"`) the way vanilla's `StructureTemplateManager`/registry lookups do:
 * read the real
 * `data/minecraft/worldgen/template_pool/<path>.json` and gzipped
 * `data/minecraft/structure/<path>.nbt` entries straight from the operator's own downloaded
 * [serverJar] (never embedded in this repo, see [ServerJarAssets]'s own KDoc), the same source
 * [RealNoiseParameters] already reads real worldgen data from. Results are cached: a jigsaw
 * assembly run references the same pool many times over (once per attempted connection). Chunks
 * (and so village assemblies) now generate on more than one thread at once
 * ([WorldState][org.basaltmc.server.world.WorldState]'s own KDoc explains why), so every cache
 * here is guarded the same way: read the jar/parse outside the lock (that work can take real
 * time and must not block every other in-flight lookup), then insert under a brief lock,
 * discarding a duplicate if another thread already won the race for the same key.
 */
public class VillageDataLoader(
    private val serverJar: ServerJarAssets,
) {
    private val cacheMutex = NativeMutex()
    private val templateCache = mutableMapOf<String, StructureTemplate?>()
    private val poolCache = mutableMapOf<String, StructureTemplatePool?>()
    private val processorTextCache = mutableMapOf<String, String?>()
    private val tagCache = mutableMapOf<String, Set<Int>?>()

    public val templateLoader: TemplateLoader = { location -> loadTemplate(location) }

    public val poolLookup: (String) -> StructureTemplatePool? = { name -> loadPool(name) }

    /** Resolves a block tag name to its member state ids, expanding nested `#tag` references. */
    public val tagLookup: (String) -> Set<Int>? = { name -> loadBlockTag(name) }

    private fun loadTemplate(location: String): StructureTemplate? =
        cachedOrCompute(templateCache, location) {
            val bytes =
                serverJar.readEntry("data/minecraft/structure/${stripNamespace(location)}.nbt")
                    ?: return@cachedOrCompute null
            val nbt = NbtIO.readCompressed(bytes, Gzip::decompress).tag as? NbtCompound ?: return@cachedOrCompute null
            StructureTemplate().apply { load(nbt) }
        }

    private fun loadPool(name: String): StructureTemplatePool? =
        cachedOrCompute(poolCache, name) {
            val path = "data/minecraft/worldgen/template_pool/${stripNamespace(name)}.json"
            val json = serverJar.readEntry(path)?.decodeToString() ?: return@cachedOrCompute null
            VillagePoolJson.parsePool(json, tagLookup, ::loadProcessorListText)
        }

    private fun loadBlockTag(name: String): Set<Int>? =
        cachedOrCompute(tagCache, name) { expandBlockTag(name, mutableSetOf()) }

    private fun expandBlockTag(
        name: String,
        seen: MutableSet<String>,
    ): Set<Int>? {
        if (!seen.add(name)) return emptySet()
        val path = "data/minecraft/tags/block/${stripNamespace(name)}.json"
        val json = serverJar.readEntry(path)?.decodeToString() ?: return null
        val result = mutableSetOf<Int>()
        for (entry in BlockTagJson.parse(json)) {
            if (entry.startsWith("#")) {
                expandBlockTag(entry.substring(1), seen)?.let(result::addAll)
            } else {
                Blocks.REGISTRY.byIdentifier(Identifier.parse(entry))?.let { result.add(it.stateId) }
            }
        }
        return result
    }

    private fun loadProcessorListText(name: String): String? =
        cachedOrCompute(processorTextCache, name) {
            val path = "data/minecraft/worldgen/processor_list/${stripNamespace(name)}.json"
            serverJar.readEntry(path)?.decodeToString()
        }

    private inline fun <T> cachedOrCompute(
        cache: MutableMap<String, T>,
        key: String,
        compute: () -> T,
    ): T {
        cacheMutex.withLock { cache[key] }?.let { return it }
        // `T` here is always itself nullable (`StructureTemplate?`/`StructureTemplatePool?`/
        // `String?`), so a genuine cache miss and a "resolved to null" result are indistinguishable
        // via a plain map lookup; recomputing an already-known-absent entry is harmless (it can
        // only re-confirm absence), unlike `MutableMap.getOrPut`'s own inability to tell them apart.
        val computed = compute()
        return cacheMutex.withLock {
            if (key in cache) cache.getValue(key) else computed.also { cache[key] = it }
        }
    }

    private fun stripNamespace(id: String): String = id.substringAfter(':')
}
