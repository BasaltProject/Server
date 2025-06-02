package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.common.logging.Logger
import org.basaltmc.registry.Identifier
import org.basaltmc.registry.Items
import org.basaltmc.registry.tags.TagFile
import org.basaltmc.registry.tags.TagFileJson
import org.basaltmc.registry.tags.TagSet
import org.basaltmc.world.crafting.CraftingInput
import org.basaltmc.world.crafting.Recipe
import org.basaltmc.world.crafting.RecipeJson
import org.basaltmc.world.crafting.RecipeParseException
import org.basaltmc.world.loot.ItemStack

/**
 * Every crafting-table recipe the operator's own `server.jar` carries, read once at startup.
 *
 * Same arrangement as [RealLootTables] and [RealNoiseParameters], and for the same reason: recipes
 * are Mojang's datapack content, never committed here, so they are read from the download the
 * operator already has and has already had verified by SHA-1 (see [ServerJarAssets]).
 *
 * Item tags are loaded first because ingredients reference them (`#minecraft:planks`) and
 * [RecipeJson] flattens a tag into its members at parse time rather than looking it up on every
 * match. Recipes this build has no model for (smelting, stonecutting, smithing, and the special
 * hard-coded ones) are skipped rather than failing the load: they are a documented gap, and a
 * server that refused to start because it cannot craft a firework would be a worse answer.
 */
public class RealRecipes(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Recipes"),
) {
    private val recipes: List<Recipe> = serverJar?.let(::load).orEmpty()

    /** How many crafting recipes were understood, for the startup line that reports it. */
    public val size: Int get() = recipes.size

    /** What this grid layout crafts right now, or `null` if it is not a recipe. */
    public fun resultFor(input: CraftingInput): ItemStack? = recipes.firstOrNull { it.matches(input) }?.result

    private fun load(serverJar: ServerJarAssets): List<Recipe> {
        val tags = loadItemTags(serverJar)
        val parsed = mutableListOf<Recipe>()
        var skipped = 0
        for (entry in serverJar.entriesUnder(RECIPE_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val recipe =
                runCatching { RecipeJson.parse(serverJar.readText(entry)) { tags[it].orEmpty() } }
                    .getOrElse { error ->
                        // Every non-crafting recipe type lands here, which is most of the folder.
                        if (error !is RecipeParseException) logger.debug { "Skipped $entry: ${error.message}" }
                        skipped++
                        null
                    }
            if (recipe != null) parsed += recipe
        }
        logger.info { "Loaded ${parsed.size} crafting recipe(s) ($skipped other recipe type(s) skipped)." }
        return parsed
    }

    /**
     * The item tag graph, flattened. A tag whose members this build's registry does not know is
     * dropped rather than fatal: the registry is generated from the same jar, so a mismatch there
     * means something else is already wrong, and refusing to craft anything is not the way to say so.
     */
    private fun loadItemTags(serverJar: ServerJarAssets): Map<Identifier, Set<Identifier>> {
        val files = mutableMapOf<Identifier, TagFile>()
        for (entry in serverJar.entriesUnder(ITEM_TAG_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(ITEM_TAG_PREFIX).removeSuffix(".json")
            runCatching { files[Identifier.vanilla(path)] = TagFileJson.parse(serverJar.readText(entry)) }
                .onFailure { logger.debug { "Skipped item tag $entry: ${it.message}" } }
        }
        return runCatching {
            TagSet.resolve(files) { Items.REGISTRY.byIdentifier(it) != null }
        }.getOrElse {
            logger.warn { "Item tags could not be resolved, so tag-based recipes will not match: ${it.message}" }
            emptyMap()
        }
    }

    private companion object {
        const val RECIPE_PREFIX = "data/minecraft/recipe/"
        const val ITEM_TAG_PREFIX = "data/minecraft/tags/item/"
    }
}
