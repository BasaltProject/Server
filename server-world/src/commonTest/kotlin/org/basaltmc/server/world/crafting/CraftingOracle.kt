package org.basaltmc.server.world.crafting

import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.Snbt
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the recipe schema, the matcher and the special rules. */
class CraftingOracle {
    private fun id(name: String) = Identifier.parse(name)

    private val tags =
        mapOf(
            id("minecraft:planks") to setOf(id("minecraft:oak_planks"), id("minecraft:spruce_planks")),
            id("minecraft:logs") to setOf(id("minecraft:oak_log"), id("minecraft:spruce_log")),
            id("minecraft:banners") to setOf(id("minecraft:white_banner"), id("minecraft:red_banner")),
        )

    private val resolveTag: (Identifier) -> Set<Identifier> = { tags[it] ?: emptySet() }

    private val recipes =
        mapOf(
            // A blank column and a blank row the shrink has to trim, a tag and a two-item array.
            "pickaxe" to
                """{"type":"minecraft:crafting_shaped","group":"pickaxe","pattern":["### ","ABA "," X  "],
                   "key":{"#":"#minecraft:planks","A":"minecraft:stick","B":"minecraft:stick",
                          "X":["minecraft:flint","minecraft:coal"]},
                   "result":{"id":"minecraft:wooden_pickaxe","count":1}}""",
            "torch" to
                """{"type":"minecraft:crafting_shaped","pattern":["C","S"],
                   "key":{"C":"minecraft:coal","S":"minecraft:stick"},
                   "result":{"id":"minecraft:torch","count":4}}""",
            // Not symmetrical, so it is also tried mirrored.
            "banner" to
                """{"type":"minecraft:crafting_shaped","pattern":["WW ","W  "],
                   "key":{"W":"minecraft:white_wool"},
                   "result":{"id":"minecraft:white_banner","count":1}}""",
            "shapeless" to
                """{"type":"minecraft:crafting_shapeless","group":"planks",
                   "ingredients":["#minecraft:logs","minecraft:stick","minecraft:stick"],
                   "result":{"id":"minecraft:oak_planks","count":4}}""",
            "transmute" to
                """{"type":"minecraft:crafting_transmute","input":"minecraft:shulker_box",
                   "material":"minecraft:red_dye","result":{"id":"minecraft:red_shulker_box","count":1}}""",
            "smelting" to
                """{"type":"minecraft:smelting","ingredient":"minecraft:iron_ore","experience":0.7,
                   "cookingtime":180,"result":{"id":"minecraft:iron_ingot","count":1}}""",
            // No cookingtime, so the type's own default is what it gets.
            "blasting" to
                """{"type":"minecraft:blasting","ingredient":"minecraft:iron_ore",
                   "result":{"id":"minecraft:iron_ingot","count":1}}""",
            "smoking" to
                """{"type":"minecraft:smoking","ingredient":"minecraft:beef","experience":0.35,
                   "result":{"id":"minecraft:cooked_beef","count":1}}""",
            "campfire" to
                """{"type":"minecraft:campfire_cooking","ingredient":"minecraft:beef",
                   "result":{"id":"minecraft:cooked_beef","count":1}}""",
            // A blank first row, which only the shrink's own leading-row counter removes.
            "leading" to
                """{"type":"minecraft:crafting_shaped","pattern":["  ","CC","S "],
                   "key":{"C":"minecraft:coal","S":"minecraft:stick"},
                   "result":{"id":"minecraft:torch","count":4}}""",
            // No cookingtime, and smelting's default is not the one the other three share.
            "smeltingDefault" to
                """{"type":"minecraft:smelting","ingredient":"minecraft:iron_ore",
                   "result":{"id":"minecraft:iron_ingot","count":1}}""",
            "stonecutting" to
                """{"type":"minecraft:stonecutting","ingredient":"minecraft:stone",
                   "result":{"id":"minecraft:stone_brick_slab","count":2}}""",
            "transform" to
                """{"type":"minecraft:smithing_transform","template":"minecraft:netherite_upgrade",
                   "base":"minecraft:diamond_sword","addition":"minecraft:netherite_ingot",
                   "result":{"id":"minecraft:netherite_sword","count":1}}""",
            // No result of its own: what comes out is the base with the pattern written on it.
            "trim" to
                """{"type":"minecraft:smithing_trim","template":"minecraft:coast_armor_trim_smithing_template",
                   "base":"minecraft:iron_chestplate","addition":["minecraft:iron_ingot","minecraft:gold_ingot"],
                   "pattern":"minecraft:coast"}""",
            "dye" to
                """{"type":"minecraft:crafting_dye","target":"minecraft:leather_helmet",
                   "dye":["minecraft:red_dye","minecraft:blue_dye","minecraft:yellow_dye"],
                   "result":{"id":"minecraft:leather_helmet","count":1}}""",
            "imbue" to
                """{"type":"minecraft:crafting_imbue","source":"minecraft:lingering_potion",
                   "material":"minecraft:arrow","result":{"id":"minecraft:tipped_arrow","count":8}}""",
            "pot" to
                """{"type":"minecraft:crafting_decorated_pot","back":"minecraft:brick",
                   "result":{"id":"minecraft:decorated_pot"}}""",
            "bookcloning" to
                """{"type":"minecraft:crafting_special_bookcloning","source":"minecraft:written_book",
                   "material":"minecraft:writable_book","result":{"id":"minecraft:written_book","count":1}}""",
            "bannerduplicate" to
                """{"type":"minecraft:crafting_special_bannerduplicate","banner":"#minecraft:banners",
                   "result":{"id":"minecraft:white_banner","count":1}}""",
            "shielddecoration" to
                """{"type":"minecraft:crafting_special_shielddecoration","target":"minecraft:shield",
                   "banner":"#minecraft:banners","result":{"id":"minecraft:shield","count":1}}""",
            "mapextending" to
                """{"type":"minecraft:crafting_special_mapextending","map":"minecraft:filled_map",
                   "material":"minecraft:paper","result":{"id":"minecraft:filled_map","count":1}}""",
            "rocket" to
                """{"type":"minecraft:crafting_special_firework_rocket","shell":"minecraft:paper",
                   "fuel":"minecraft:gunpowder","star":"minecraft:firework_star",
                   "result":{"id":"minecraft:firework_rocket","count":3}}""",
            "star" to
                """{"type":"minecraft:crafting_special_firework_star","fuel":"minecraft:gunpowder",
                   "dye":["minecraft:red_dye","minecraft:blue_dye","minecraft:yellow_dye"],
                   "shapes":{"burst":"minecraft:feather","creeper":"minecraft:creeper_head",
                             "large_ball":"minecraft:fire_charge","star":"minecraft:gold_nugget"},
                   "trail":"minecraft:diamond","twinkle":"minecraft:glowstone_dust",
                   "result":{"id":"minecraft:firework_star","count":1}}""",
            "starfade" to
                """{"type":"minecraft:crafting_special_firework_star_fade","target":"minecraft:firework_star",
                   "dye":["minecraft:red_dye","minecraft:blue_dye"],
                   "result":{"id":"minecraft:firework_star","count":1}}""",
            "repair" to
                """{"type":"minecraft:crafting_special_repairitem",
                   "result":{"id":"minecraft:air","count":1}}""",
        )

    private fun stack(
        name: String,
        count: Int = 1,
        components: NbtCompound = NbtCompound(),
    ) = ItemStack(id(name), count, components)

    private fun withInt(
        key: String,
        value: Int,
    ) = NbtCompound().also { it[key] = NbtInt(value) }

    private fun patterned(): NbtCompound {
        val pattern = NbtCompound()
        pattern["pattern"] = NbtString("minecraft:stripe_bottom")
        pattern["color"] = NbtString("red")
        return NbtCompound().also { it[SpecialRecipes.BANNER_PATTERNS] = NbtList(NbtType.COMPOUND, listOf(pattern)) }
    }

    private val grids: Map<String, Triple<Int, Int, List<ItemStack?>>> =
        mapOf(
            // The pickaxe as authored, in the bottom-left of a 3x3 grid.
            "pickaxe" to
                Triple(
                    3,
                    3,
                    listOf(
                        stack("minecraft:oak_planks"),
                        stack("minecraft:spruce_planks"),
                        stack("minecraft:oak_planks"),
                        stack("minecraft:stick"),
                        stack("minecraft:stick"),
                        stack("minecraft:stick"),
                        null,
                        stack("minecraft:flint"),
                        null,
                    ),
                ),
            // The same shape one column right, which the trim makes the same input.
            "pickaxeShifted" to
                Triple(
                    3,
                    3,
                    listOf(
                        stack("minecraft:oak_planks"),
                        stack("minecraft:oak_planks"),
                        stack("minecraft:oak_planks"),
                        stack("minecraft:stick"),
                        stack("minecraft:stick"),
                        stack("minecraft:stick"),
                        stack("minecraft:coal"),
                        null,
                        null,
                    ),
                ),
            "torch" to Triple(1, 2, listOf(stack("minecraft:coal"), stack("minecraft:stick"))),
            // The same two, inset, so the trim has to move both the column and the row.
            "torchInset" to
                Triple(
                    3,
                    3,
                    listOf(
                        null,
                        null,
                        null,
                        null,
                        stack("minecraft:coal"),
                        null,
                        null,
                        stack("minecraft:stick"),
                        null,
                    ),
                ),
            "leading" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:coal"),
                        stack("minecraft:coal"),
                        stack("minecraft:stick"),
                        null,
                    ),
                ),
            "torchWrong" to Triple(1, 2, listOf(stack("minecraft:stick"), stack("minecraft:coal"))),
            // The banner shape mirrored: only the flip pass accepts it.
            "bannerMirror" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:white_wool"),
                        stack("minecraft:white_wool"),
                        null,
                        stack("minecraft:white_wool"),
                    ),
                ),
            // The banner shape as authored, which only the unflipped pass accepts.
            "bannerPlain" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:white_wool"),
                        stack("minecraft:white_wool"),
                        stack("minecraft:white_wool"),
                        null,
                    ),
                ),
            "shapeless" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:stick"),
                        stack("minecraft:spruce_log"),
                        stack("minecraft:stick"),
                        null,
                    ),
                ),
            // Three sticks and no log: the matching has to fail, not just count to three.
            "shapelessWrong" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:stick"),
                        stack("minecraft:stick"),
                        stack("minecraft:stick"),
                        null,
                    ),
                ),
            "transmute" to
                Triple(2, 1, listOf(stack("minecraft:red_dye"), stack("minecraft:shulker_box"))),
            "one" to Triple(1, 1, listOf(stack("minecraft:iron_ore"))),
            "oneBeef" to Triple(1, 1, listOf(stack("minecraft:beef"))),
            "oneStone" to Triple(1, 1, listOf(stack("minecraft:stone"))),
            // A bucket, whose emptied form is what the grid is left holding.
            "bucketed" to
                Triple(
                    2,
                    1,
                    listOf(stack("minecraft:water_bucket"), stack("minecraft:oak_log")),
                ),
            "dye" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:leather_helmet"),
                        stack("minecraft:red_dye"),
                        stack("minecraft:blue_dye"),
                        null,
                    ),
                ),
            // Already dyed, so the mix averages against what it had.
            "dyeAgain" to
                Triple(
                    2,
                    1,
                    listOf(
                        stack("minecraft:leather_helmet", 1, withInt(SpecialRecipes.DYED_COLOR, 0x102030)),
                        stack("minecraft:yellow_dye"),
                    ),
                ),
            // A dye with no colour behind it, which makes the whole thing not a recipe.
            "dyeUnknown" to
                Triple(
                    2,
                    1,
                    listOf(stack("minecraft:leather_helmet"), stack("minecraft:green_dye")),
                ),
            "imbue" to
                Triple(
                    3,
                    3,
                    listOf(
                        stack("minecraft:arrow"),
                        stack("minecraft:arrow"),
                        stack("minecraft:arrow"),
                        stack("minecraft:arrow"),
                        stack(
                            "minecraft:lingering_potion",
                            1,
                            NbtCompound().also { it[SpecialRecipes.POTION_CONTENTS] = NbtString("minecraft:strength") },
                        ),
                        stack("minecraft:arrow"),
                        stack("minecraft:arrow"),
                        stack("minecraft:arrow"),
                        stack("minecraft:arrow"),
                    ),
                ),
            "pot" to
                Triple(
                    3,
                    3,
                    listOf(
                        null,
                        stack("minecraft:brick"),
                        null,
                        stack("minecraft:brick"),
                        null,
                        stack("minecraft:brick"),
                        null,
                        stack("minecraft:brick"),
                        null,
                    ),
                ),
            "map" to
                Triple(
                    3,
                    3,
                    listOf(
                        stack("minecraft:paper"),
                        stack("minecraft:paper"),
                        stack("minecraft:paper"),
                        stack("minecraft:paper"),
                        stack("minecraft:filled_map", 1, withInt(SpecialRecipes.MAP_ID, 7)),
                        stack("minecraft:paper"),
                        stack("minecraft:paper"),
                        stack("minecraft:paper"),
                        stack("minecraft:paper"),
                    ),
                ),
            "bookcloning" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:writable_book", 2),
                        stack(
                            "minecraft:written_book",
                            1,
                            NbtCompound().also {
                                it[SpecialRecipes.WRITTEN_BOOK_CONTENT] = NbtString("a title")
                            },
                        ),
                        stack("minecraft:writable_book", 1),
                        null,
                    ),
                ),
            "bannerduplicate" to
                Triple(
                    2,
                    1,
                    listOf(stack("minecraft:white_banner"), stack("minecraft:red_banner", 1, patterned())),
                ),
            // Two blanks: nothing to copy from, so no recipe.
            "bannerBlanks" to
                Triple(2, 1, listOf(stack("minecraft:white_banner"), stack("minecraft:red_banner"))),
            "shield" to
                Triple(
                    2,
                    1,
                    listOf(stack("minecraft:shield"), stack("minecraft:red_banner", 1, patterned())),
                ),
            // A shield that already carries patterns is not a recipe.
            "shieldUsed" to
                Triple(
                    2,
                    1,
                    listOf(stack("minecraft:shield", 1, patterned()), stack("minecraft:red_banner", 1, patterned())),
                ),
            "rocket" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:paper"),
                        stack("minecraft:gunpowder"),
                        stack("minecraft:gunpowder"),
                        stack("minecraft:firework_star"),
                    ),
                ),
            // Four gunpowder is past the flight cap, which is one to three.
            "rocketOverfuelled" to
                Triple(
                    3,
                    2,
                    listOf(
                        stack("minecraft:paper"),
                        stack("minecraft:gunpowder"),
                        stack("minecraft:gunpowder"),
                        stack("minecraft:gunpowder"),
                        stack("minecraft:gunpowder"),
                        null,
                    ),
                ),
            "star" to
                Triple(
                    2,
                    2,
                    listOf(
                        stack("minecraft:gunpowder"),
                        stack("minecraft:red_dye"),
                        stack("minecraft:blue_dye"),
                        stack("minecraft:diamond"),
                    ),
                ),
            "starShaped" to
                Triple(
                    3,
                    2,
                    listOf(
                        stack("minecraft:gunpowder"),
                        stack("minecraft:yellow_dye"),
                        stack("minecraft:gold_nugget"),
                        stack("minecraft:glowstone_dust"),
                        null,
                        null,
                    ),
                ),
            // A star with no dye at all, which is not a recipe however much else is right.
            "starNoDye" to
                Triple(2, 1, listOf(stack("minecraft:gunpowder"), stack("minecraft:diamond"))),
            // Two shapes at once, which is not a recipe however many dyes there are.
            "starTwoShapes" to
                Triple(
                    3,
                    2,
                    listOf(
                        stack("minecraft:gunpowder"),
                        stack("minecraft:red_dye"),
                        stack("minecraft:gold_nugget"),
                        stack("minecraft:feather"),
                        null,
                        null,
                    ),
                ),
            "starfade" to
                Triple(
                    2,
                    1,
                    listOf(
                        stack("minecraft:firework_star", 1, withInt(SpecialRecipes.DYED_COLOR, 0xB02E26)),
                        stack("minecraft:blue_dye"),
                    ),
                ),
            "repair" to
                Triple(
                    2,
                    1,
                    listOf(
                        stack("minecraft:iron_pickaxe", 1, withInt(SpecialRecipes.DAMAGE, 200)),
                        stack("minecraft:iron_pickaxe", 1, withInt(SpecialRecipes.DAMAGE, 30)),
                    ),
                ),
            // Both worn far enough that the repair lands short of full, which is where the bonus shows.
            "repairWorn" to
                Triple(
                    2,
                    1,
                    listOf(
                        stack("minecraft:iron_pickaxe", 1, withInt(SpecialRecipes.DAMAGE, 200)),
                        stack("minecraft:iron_pickaxe", 1, withInt(SpecialRecipes.DAMAGE, 200)),
                    ),
                ),
            // Two of a kind that takes no damage at all is not a repair.
            "repairSticks" to
                Triple(2, 1, listOf(stack("minecraft:stick"), stack("minecraft:stick"))),
            // A stack of two never repairs, however damaged it is.
            "repairStacked" to
                Triple(
                    2,
                    1,
                    listOf(
                        stack("minecraft:iron_pickaxe", 2, withInt(SpecialRecipes.DAMAGE, 200)),
                        stack("minecraft:iron_pickaxe", 1),
                    ),
                ),
            "empty" to Triple(2, 2, listOf(null, null, null, null)),
            // One empty slot, whose bounding box is the one that does not collapse.
            "emptyOne" to Triple(1, 1, listOf(null)),
        )

    private fun checksum(text: String): Long {
        var checksum = -3750763034362895579L
        for (char in text) checksum = (checksum xor char.code.toLong()) * 1099511628211L
        return checksum
    }

    private fun describe(stack: ItemStack?): String =
        if (stack == null) "-" else "${stack.item}x${stack.count}${Snbt.stringify(stack.components)}"

    /** The parsed recipe itself, so the schema is checked apart from what the matcher does with it. */
    private fun shapeOf(recipe: Recipe): String =
        when (recipe) {
            is Recipe.Shaped ->
                "shaped/${recipe.width}x${recipe.height}/" +
                    recipe.ingredients.joinToString(";") {
                        it
                            ?.items
                            ?.map(Identifier::toString)
                            ?.sorted()
                            ?.joinToString(",")
                            ?: "-"
                    }
            is Recipe.Shapeless ->
                "shapeless/" +
                    recipe.ingredients.joinToString(
                        ";",
                    ) {
                        it.items
                            .map(Identifier::toString)
                            .sorted()
                            .joinToString(",")
                    }
            is Recipe.Cooking ->
                "cooking/${recipe.cooker}/${recipe.experience}/${recipe.cookingTime}/" +
                    recipe.ingredient.items
                        .map(Identifier::toString)
                        .sorted()
                        .joinToString(",")
            is Recipe.Stonecutting ->
                "stonecutting/" +
                    recipe.ingredient.items
                        .map(Identifier::toString)
                        .sorted()
                        .joinToString(",")
            is Recipe.Smithing ->
                "smithing/${recipe.trimPattern}/" +
                    listOf(recipe.template, recipe.base, recipe.addition)
                        .joinToString(";") {
                            it.items
                                .map(Identifier::toString)
                                .sorted()
                                .joinToString(",")
                        }
            is Recipe.Special ->
                "special/${recipe.kind}/" +
                    listOf(recipe.target, recipe.material, recipe.extra)
                        .joinToString(";") {
                            it
                                ?.items
                                ?.map(Identifier::toString)
                                ?.sorted()
                                ?.joinToString(",") ?: "-"
                        } +
                    "/" +
                    recipe.modifiers.joinToString(";") { (name, ingredient) ->
                        "$name=" +
                            ingredient.items
                                .map(Identifier::toString)
                                .sorted()
                                .joinToString(",")
                    }
        }

    private fun report(
        name: String,
        recipeName: String,
        gridName: String,
    ) {
        val recipe = RecipeJson.parse(recipes.getValue(recipeName), resolveTag)
        val (width, height, items) = grids.getValue(gridName)
        val input = CraftingInput.of(width, height, items)
        val matches = recipe.matches(input)
        val assembled = describe(recipe.assemble(input))
        val remaining = recipe.remainingItems(input).joinToString(",") { describe(it) }
        println(
            "ORACLE $name matches=$matches group=${recipe.group} shape=${shapeOf(recipe)} " +
                "result=$assembled remaining=$remaining " +
                "checksum=${checksum("$matches$assembled$remaining")}",
        )
    }

    /** The two questions a smithing table asks, which no crafting grid ever reaches. */
    private fun reportSmithing(
        name: String,
        recipeName: String,
        template: ItemStack?,
        base: ItemStack,
        addition: ItemStack,
    ) {
        val recipe = RecipeJson.parse(recipes.getValue(recipeName), resolveTag) as Recipe.Smithing
        val accepts = recipe.accepts(template, base, addition)
        val assembled = describe(recipe.assemble(base, addition))
        println("ORACLE $name accepts=$accepts shape=${shapeOf(recipe)} result=$assembled")
    }

    private fun reportRefused(
        name: String,
        json: String,
    ) {
        val message =
            try {
                RecipeJson.parse(json, resolveTag)
                "accepted"
            } catch (failure: RecipeParseException) {
                failure.message ?: "unknown"
            } catch (failure: IllegalArgumentException) {
                failure.message ?: "unknown"
            }
        println("ORACLE $name refused=${message.replace(' ', '_')}")
    }

    @Test
    fun dump() {
        Recipe.remainderOf = { item ->
            when (item.toString()) {
                "minecraft:water_bucket", "minecraft:lava_bucket" -> id("minecraft:bucket")
                "minecraft:honey_bottle" -> id("minecraft:glass_bottle")
                else -> null
            }
        }
        SpecialCrafting.maxDamageOf = { item ->
            when (item.toString()) {
                "minecraft:iron_pickaxe" -> 250
                "minecraft:diamond_sword" -> 1561
                else -> 0
            }
        }
        SpecialCrafting.dyeColourOf = { item ->
            when (item.toString()) {
                "minecraft:red_dye" -> 0xB02E26
                "minecraft:blue_dye" -> 0x3C44AA
                "minecraft:yellow_dye" -> 0xFED83D
                "minecraft:white_dye" -> 0xF9FFFE
                else -> null
            }
        }
        SpecialRecipes.trimMaterialOf = { item ->
            when (item.toString()) {
                "minecraft:iron_ingot" -> id("minecraft:iron")
                "minecraft:gold_ingot" -> id("minecraft:gold")
                else -> null
            }
        }

        report("pickaxe", "pickaxe", "pickaxe")
        report("pickaxeShifted", "pickaxe", "pickaxeShifted")
        report("pickaxeWrongGrid", "pickaxe", "torch")
        report("torch", "torch", "torch")
        report("torchWrong", "torch", "torchWrong")
        report("bannerMirror", "banner", "bannerMirror")
        report("bannerPlain", "banner", "bannerPlain")
        report("torchInset", "torch", "torchInset")
        report("leading", "leading", "leading")
        report("shapeless", "shapeless", "shapeless")
        report("shapelessWrong", "shapeless", "shapelessWrong")
        report("shapelessRemainder", "shapeless", "bucketed")
        report("transmute", "transmute", "transmute")
        report("smelting", "smelting", "one")
        report("blasting", "blasting", "one")
        report("smoking", "smoking", "oneBeef")
        report("campfire", "campfire", "oneBeef")
        report("smeltingWrong", "smelting", "oneBeef")
        report("smeltingDefault", "smeltingDefault", "one")
        report("stonecutting", "stonecutting", "oneStone")
        report("smithingInGrid", "transform", "one")
        report("dye", "dye", "dye")
        report("dyeAgain", "dye", "dyeAgain")
        report("dyeUnknown", "dye", "dyeUnknown")
        report("imbue", "imbue", "imbue")
        report("imbueWrong", "imbue", "map")
        report("pot", "pot", "pot")
        report("mapextending", "mapextending", "map")
        report("bookcloning", "bookcloning", "bookcloning")
        report("bannerduplicate", "bannerduplicate", "bannerduplicate")
        report("bannerBlanks", "bannerduplicate", "bannerBlanks")
        report("shield", "shielddecoration", "shield")
        report("shieldUsed", "shielddecoration", "shieldUsed")
        report("rocket", "rocket", "rocket")
        report("rocketOverfuelled", "rocket", "rocketOverfuelled")
        report("star", "star", "star")
        report("starShaped", "star", "starShaped")
        report("starNoDye", "star", "starNoDye")
        report("starTwoShapes", "star", "starTwoShapes")
        report("starfade", "starfade", "starfade")
        report("repair", "repair", "repair")
        report("repairWorn", "repair", "repairWorn")
        report("repairSticks", "repair", "repairSticks")
        report("repairStacked", "repair", "repairStacked")
        report("repairEmpty", "repair", "empty")
        report("repairEmptyOne", "repair", "emptyOne")

        reportSmithing(
            "transform",
            "transform",
            stack("minecraft:netherite_upgrade"),
            stack("minecraft:diamond_sword", 1, withInt(SpecialRecipes.DAMAGE, 12)),
            stack("minecraft:netherite_ingot"),
        )
        reportSmithing(
            "transformWrongBase",
            "transform",
            stack("minecraft:netherite_upgrade"),
            stack("minecraft:iron_sword"),
            stack("minecraft:netherite_ingot"),
        )
        reportSmithing(
            "trim",
            "trim",
            stack("minecraft:coast_armor_trim_smithing_template"),
            stack("minecraft:iron_chestplate"),
            stack("minecraft:gold_ingot"),
        )
        // The same trim it already wears is not a recipe.
        reportSmithing(
            "trimAgain",
            "trim",
            stack("minecraft:coast_armor_trim_smithing_template"),
            stack(
                "minecraft:iron_chestplate",
                1,
                NbtCompound().also {
                    it[SpecialRecipes.TRIM] =
                        NbtCompound().also { trim ->
                            trim["pattern"] = NbtString("minecraft:coast")
                            trim["material"] = NbtString("minecraft:gold")
                        }
                },
            ),
            stack("minecraft:gold_ingot"),
        )
        // An addition that provides no trim material at all.
        reportSmithing(
            "trimNoMaterial",
            "trim",
            stack("minecraft:coast_armor_trim_smithing_template"),
            stack("minecraft:iron_chestplate"),
            stack("minecraft:copper_ingot"),
        )

        reportRefused("unknownType", """{"type":"minecraft:crafting_magic","result":{"id":"minecraft:stone"}}""")
        reportRefused(
            "longSymbol",
            """{"type":"minecraft:crafting_shaped","pattern":["AB"],"key":{"AB":"minecraft:stone"},
               "result":{"id":"minecraft:stone"}}""",
        )
        reportRefused(
            "undefinedSymbol",
            """{"type":"minecraft:crafting_shaped","pattern":["AB"],"key":{"A":"minecraft:stone"},
               "result":{"id":"minecraft:stone"}}""",
        )
    }
}
