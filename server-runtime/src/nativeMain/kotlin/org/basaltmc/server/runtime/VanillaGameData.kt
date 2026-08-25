package org.basaltmc.server.runtime

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.runtime.world.Jukeboxes
import org.basaltmc.server.runtime.world.RealAdvancements
import org.basaltmc.server.runtime.world.RealBiomeSpawns
import org.basaltmc.server.runtime.world.RealBlockTags
import org.basaltmc.server.runtime.world.RealDamageTypeTags
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealEntityTypeTags
import org.basaltmc.server.runtime.world.RealFluidTags
import org.basaltmc.server.runtime.world.RealItemTags
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.runtime.world.RealTestInstances
import org.basaltmc.server.runtime.world.RealTimelineTags
import org.basaltmc.server.runtime.world.RealTimelines
import org.basaltmc.server.runtime.world.RealVillagerTrades
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * Every read-only registry this server reads straight out of the operator's own `server.jar`.
 *
 * Built once, before anything that could depend on it: no chunk generates, no player loads and no
 * command registers before this exists. Where each table comes from (loot, tags, recipes...) is
 * exactly what vanilla's own `server.jar` holds, so a null [serverJar] (as in most tests, which
 * never break a block in survival) leaves every table empty rather than absent.
 */
public class VanillaGameData(
    serverJar: ServerJarAssets?,
    logger: Logger,
) {
    public val lootTables: RealLootTables = RealLootTables(serverJar)

    /** What each biome offers the spawner, out of the operator's own jar. */
    public val biomeSpawns: RealBiomeSpawns = RealBiomeSpawns(serverJar)

    /** What each villager offers, out of the same jar. */
    public val villagerTrades: RealVillagerTrades = RealVillagerTrades(serverJar)

    /** The song data a comparator reads off a playing jukebox. */
    public val jukeboxes: Jukeboxes = Jukeboxes(serverJar)
    public val recipes: RealRecipes = RealRecipes(serverJar)
    public val blockTags: RealBlockTags = RealBlockTags(serverJar)
    public val itemTags: RealItemTags = RealItemTags(serverJar)
    public val fluidTags: RealFluidTags = RealFluidTags(serverJar)
    public val timelineTags: RealTimelineTags = RealTimelineTags(serverJar)

    init {
        // Loud on purpose: a client whose overworld timeline tag is empty renders a sky that never
        // moves, and nothing else in the log would say why.
        val overworldTimelines = timelineTags.membersOf(Identifier.vanilla("in_overworld"))
        if (overworldTimelines.isEmpty()) {
            logger.warn {
                "No timelines are tagged for the overworld, so the sky will not move. " +
                    "The server jar's data is missing or could not be read."
            }
        } else {
            logger.debug { "Overworld timelines: ${overworldTimelines.joinToString()}." }
        }
    }

    public val damageTypeTags: RealDamageTypeTags = RealDamageTypeTags(serverJar)
    public val entityTags: RealEntityTypeTags = RealEntityTypeTags(serverJar)
    public val advancements: RealAdvancements = RealAdvancements(serverJar)
    public val enchantments: RealEnchantments = RealEnchantments(serverJar)
    public val dialogs: RealDialogs = RealDialogs(serverJar)
    public val timelines: RealTimelines = RealTimelines(serverJar)
    public val testInstances: RealTestInstances = RealTestInstances(serverJar)

    /** Structure files, loaded on demand: what `/place template` puts down. */
    public val structureTemplates: StructureTemplates = StructureTemplates(serverJar)
}
