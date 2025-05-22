package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.io.Gzip
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import org.basaltmc.world.structure.StructureTemplate
import org.basaltmc.world.structure.StructureTemplatePool
import org.basaltmc.world.structure.TemplateLoader
import org.basaltmc.world.structure.VillagePoolJson

/**
 * Resolves village pool/piece names (`"minecraft:village/plains/town_centers"`,
 * `"minecraft:village/plains/town_centers/plains_fountain_01"`) the way vanilla's
 * `StructureTemplateManager`/registry lookups do: read the real
 * `data/minecraft/worldgen/template_pool/<path>.json` and gzipped
 * `data/minecraft/structure/<path>.nbt` entries straight from the operator's own downloaded
 * [serverJar] (never embedded in this repo, see [ServerJarAssets]'s own KDoc), the same source
 * [RealNoiseParameters] already reads real worldgen data from. Results are cached: a jigsaw
 * assembly run references the same pool many times over (once per attempted connection).
 */
public class VillageDataLoader(
    private val serverJar: ServerJarAssets,
) {
    private val templateCache = mutableMapOf<String, StructureTemplate?>()
    private val poolCache = mutableMapOf<String, StructureTemplatePool?>()
    private val processorTextCache = mutableMapOf<String, String?>()

    public val templateLoader: TemplateLoader = { location -> loadTemplate(location) }

    public val poolLookup: (String) -> StructureTemplatePool? = { name -> loadPool(name) }

    private fun loadTemplate(location: String): StructureTemplate? =
        templateCache.getOrPut(location) {
            val bytes =
                serverJar.readEntry("data/minecraft/structure/${stripNamespace(location)}.nbt") ?: return@getOrPut null
            val nbt = NbtIO.readCompressed(bytes, Gzip::decompress).tag as? NbtCompound ?: return@getOrPut null
            StructureTemplate().apply { load(nbt) }
        }

    private fun loadPool(name: String): StructureTemplatePool? =
        poolCache.getOrPut(name) {
            val path = "data/minecraft/worldgen/template_pool/${stripNamespace(name)}.json"
            val json = serverJar.readEntry(path)?.decodeToString() ?: return@getOrPut null
            VillagePoolJson.parsePool(json, ::loadProcessorListText)
        }

    private fun loadProcessorListText(name: String): String? =
        processorTextCache.getOrPut(name) {
            val path = "data/minecraft/worldgen/processor_list/${stripNamespace(name)}.json"
            serverJar.readEntry(path)?.decodeToString()
        }

    private fun stripNamespace(id: String): String = id.substringAfter(':')
}
