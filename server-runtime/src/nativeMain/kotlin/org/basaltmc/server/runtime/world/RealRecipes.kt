package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.Items
import org.basaltmc.server.registry.tags.TagFile
import org.basaltmc.server.registry.tags.TagFileJson
import org.basaltmc.server.registry.tags.TagSet
import org.basaltmc.server.world.crafting.CraftingInput
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.crafting.RecipeJson
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * Every crafting-table recipe the operator's own `server.jar` carries, read once at startup, like
 * [RealLootTables] and [RealNoiseParameters]: datapack content is never committed here.
 *
 * Item tags load first, because ingredients reference them and [RecipeJson] flattens a tag at
 * parse time. Recipe types this build has no model for are skipped rather than failing the load:
 * refusing to start over an uncraftable firework would be the worse answer.
 */
public class RealRecipes(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Recipes"),
) {
    private val recipes: List<Recipe> = serverJar?.let(::load).orEmpty()

    /** Every cooking recipe, keyed by what cooks it. parity: `RecipeManager`'s own index by type. */
    private val cooking: Map<Recipe.Cooker, List<Recipe.Cooking>> by lazy {
        recipes.filterIsInstance<Recipe.Cooking>().groupBy { it.cooker }
    }

    /**
     * What [cooker] makes of [stack], or null when it makes nothing.
     *
     * parity: `RecipeManager.getRecipeFor`, which walks the recipes of that type and takes the
     * first that accepts what is in the slot.
     */
    public fun cookingFor(
        cooker: Recipe.Cooker,
        stack: ItemStack,
    ): Recipe.Cooking? = cooking[cooker]?.firstOrNull { it.accepts(stack) }

    /**
     * Every stonecutting recipe, in the order they were read.
     *
     * The order is the contract with the client: it filters this same list by what is in the input
     * slot and sends back an index into what is left, so both sides have to agree on it.
     */
    public val stonecutting: List<Recipe.Stonecutting> by lazy { recipes.filterIsInstance<Recipe.Stonecutting>() }

    /** What a stonecutter can make of [stack], in the order the client will draw the buttons. */
    public fun stonecuttingFor(stack: ItemStack): List<Recipe.Stonecutting> = stonecutting.filter { it.accepts(stack) }

    /** The id each parsed recipe came from, in the same order, so a match can be named. */
    private var parsedIds: List<Identifier> = emptyList()

    /**
     * The recipe behind an id, for the book entries `/recipe give` sends.
     *
     * Only the recipes this build parsed have one: a recipe type it skipped can be *named* by
     * `/recipe` (its file is there) but cannot be drawn, and is left out of the book rather than
     * drawn wrong.
     */
    public fun recipeFor(id: Identifier): Recipe? {
        val index = parsedIds.indexOf(id)
        return if (index < 0) null else recipes[index]
    }

    /** How many crafting recipes were understood, for the startup line that reports it. */
    public val size: Int get() = recipes.size

    /** What this grid layout crafts right now, or `null` if it is not a recipe. */
    public fun resultFor(input: CraftingInput): ItemStack? = matchFor(input)?.second

    /**
     * The recipe this grid matches, as its id and its result.
     *
     * The id is what `limited_crafting` checks against the player's recipe book, which is the whole
     * reason a match needs a name rather than just a result.
     */
    public fun matchFor(input: CraftingInput): Pair<Identifier?, ItemStack>? {
        val match = recipeMatching(input) ?: return null
        // parity: `Recipe.assemble`, not `getResultItem`: a dye, a copy or a repair is defined by
        // what it carries over from the grid, so its result cannot be a fixed stack.
        return match.first to match.second.assemble(input)
    }

    /** The recipe this grid matches, with its id: what a caller needs to ask what it leaves behind. */
    public fun recipeMatching(input: CraftingInput): Pair<Identifier?, Recipe>? {
        val index = recipes.indexOfFirst { it.matches(input) }
        if (index < 0) return null
        return parsedIds.getOrNull(index) to recipes[index]
    }

    /** Every smithing recipe, which the smithing table walks in order like the stonecutter does. */
    public val smithing: List<Recipe.Smithing> by lazy { recipes.filterIsInstance<Recipe.Smithing>() }

    /** What a smithing table makes of these three slots, or null when they are not a recipe. */
    public fun smithingFor(
        template: ItemStack?,
        base: ItemStack?,
        addition: ItemStack?,
    ): Recipe.Smithing? = smithing.firstOrNull { it.accepts(template, base, addition) }

    /** Every recipe file's id, whether or not this build could parse it: what `/recipe *` names. */
    public var ids: Set<Identifier> = emptySet()
        private set

    private fun load(serverJar: ServerJarAssets): List<Recipe> {
        val tags = loadItemTags(serverJar)
        val parsedIdList = mutableListOf<Identifier>()
        ids =
            serverJar
                .entriesUnder(RECIPE_PREFIX)
                .filter { it.endsWith(".json") }
                .map { Identifier.vanilla(it.removePrefix(RECIPE_PREFIX).removeSuffix(".json")) }
                .toSet()
        val parsed = mutableListOf<Recipe>()
        var skipped = 0
        for (entry in serverJar.entriesUnder(RECIPE_PREFIX)) {
            if (!entry.endsWith(".json")) continue
            val recipe =
                runCatching { RecipeJson.parse(serverJar.readText(entry)) { tags[it].orEmpty() } }
                    .getOrElse { error ->
                        // A type with no model here, or a file this build could not read: both are
                        // named, because a silent skip is how a whole family of recipes went missing
                        // without anyone noticing.
                        logger.debug { "Skipped $entry: ${error.message}" }
                        skipped++
                        null
                    }
            if (recipe != null) {
                parsed += recipe
                parsedIdList += Identifier.vanilla(entry.removePrefix(RECIPE_PREFIX).removeSuffix(".json"))
            }
        }
        parsedIds = parsedIdList
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
