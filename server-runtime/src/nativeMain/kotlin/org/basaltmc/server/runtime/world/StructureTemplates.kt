package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.Gzip
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIO
import kotlin.concurrent.Volatile

/**
 * Structure files out of the operator's own `server.jar`, by id.
 *
 * The same files world generation already reads for villages and bastions; this is what lets
 * `/place template` put one down on demand. Loaded lazily and cached, because there are hundreds
 * and a server that never runs the command should not pay for any of them.
 */
public class StructureTemplates(
    private val serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Structures"),
) {
    @Volatile
    private var cache: Map<Identifier, StructureTemplate?> = emptyMap()

    @Volatile
    private var listings: Map<String, List<Identifier>> = emptyMap()

    public operator fun get(id: Identifier): StructureTemplate? {
        cache[id]?.let { return it }
        if (cache.containsKey(id)) return null
        val jar = serverJar ?: return null
        val loaded =
            runCatching {
                val bytes =
                    jar.readEntry("data/${id.namespace}/structure/${id.path}.nbt")
                        ?: error("no such structure file")
                val root = NbtIO.readCompressed(bytes, Gzip::decompress).tag as? NbtCompound ?: error("not a compound")
                StructureTemplate().also { it.load(root) }
            }.onFailure { logger.debug { "No structure '$id': ${it.message}" } }
                .getOrNull()
        cache = cache + (id to loaded)
        return loaded
    }

    /** Every structure file the jar ships. parity: `PlaceCommand.SUGGEST_TEMPLATES`. */
    public fun templateIds(): List<Identifier> = idsUnder("structure", ".nbt")

    /** Every template pool the jar ships, which is what `/place jigsaw` grows one of. */
    public fun poolIds(): List<Identifier> = idsUnder("worldgen/template_pool", ".json")

    /** Listing the jar is a directory walk, so each answer is kept rather than walked again. */
    private fun idsUnder(
        directory: String,
        extension: String,
    ): List<Identifier> {
        listings[directory]?.let { return it }
        val ids = serverJar?.dataIds(directory, extension).orEmpty()
        listings = listings + (directory to ids)
        return ids
    }
}
