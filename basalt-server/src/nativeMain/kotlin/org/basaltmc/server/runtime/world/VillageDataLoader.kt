package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.structure.BlockTagJson
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import org.basaltmc.server.world.structure.VillagePoolJson
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.Gzip
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIO

/**
 * Resolves structure pool and piece names the way vanilla's `StructureTemplateManager` does, from
 * the operator's own downloaded [serverJar] (never embedded here, see [ServerJarAssets]).
 *
 * Results are cached, since one jigsaw assembly asks for the same pool once per attempted
 * connection. Assemblies run on several threads, so each cache parses outside the lock and inserts
 * under a brief one, discarding a duplicate if another thread won the same key.
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
