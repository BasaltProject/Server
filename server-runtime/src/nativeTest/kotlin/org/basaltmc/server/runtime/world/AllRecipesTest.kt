package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.CraftingRemainders
import org.basaltmc.server.registry.DyeColours
import org.basaltmc.server.registry.TrimMaterials
import org.basaltmc.server.world.crafting.CraftingInput
import org.basaltmc.server.world.crafting.Ingredient
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.crafting.RecipeJson
import org.basaltmc.server.world.crafting.SpecialCrafting
import org.basaltmc.server.world.crafting.SpecialRecipes
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The recipe families that are not a fixed list of ingredients: what each one accepts and what it
 * carries over. parity: the `crafting_special_*` classes, `DyeRecipe`, `ImbueRecipe` and the two
 * smithing ones.
 */
class AllRecipesTest {
    @BeforeTest
    fun wireUpTheTables() {
        SpecialCrafting.dyeColourOf = { DyeColours.of(it) }
        SpecialCrafting.maxDamageOf = { if (it.path == "iron_pickaxe") IRON_PICKAXE_DURABILITY else 0 }
        SpecialRecipes.trimMaterialOf = { TrimMaterials.of(it) }
    }

    private val ironPickaxe = Identifier.vanilla("iron_pickaxe")

    /** parity: `RepairItemRecipe`: the two bars plus five per cent, and never past full. */
    @Test
    fun twoDamagedToolsRepairIntoOne() {
        val recipe = Recipe.Special("", ItemStack(Identifier.vanilla("air"), 1), Recipe.Special.Kind.REPAIR_ITEM)
        val half = damaged(ironPickaxe, IRON_PICKAXE_DURABILITY / 2)

        val repaired = assertNotNull(SpecialCrafting.assemble(recipe, grid(2, 1, listOf(half, half))))

        assertEquals(ironPickaxe, repaired.item)
        // Two halves plus the bonus is a whole bar, so nothing is left damaged.
        assertEquals(0, SpecialRecipes.damageOf(repaired))
    }

    /** Two different tools are not a repair, whatever their damage. */
    @Test
    fun twoDifferentToolsAreNotARepair() {
        val recipe = Recipe.Special("", ItemStack(Identifier.vanilla("air"), 1), Recipe.Special.Kind.REPAIR_ITEM)
        val pickaxe = damaged(ironPickaxe, 10)
        val axe = damaged(Identifier.vanilla("iron_axe"), 10)

        assertNull(SpecialCrafting.assemble(recipe, grid(2, 1, listOf(pickaxe, axe))))
    }

    /** parity: `DyeRecipe`: one thing to colour, any number of dyes, and the average of them all. */
    @Test
    fun leatherTakesTheColourOfItsDyes() {
        val recipe =
            Recipe.Special(
                group = "",
                result = ItemStack(Identifier.vanilla("leather_helmet"), 1),
                kind = Recipe.Special.Kind.DYE,
                target = Ingredient(setOf(Identifier.vanilla("leather_helmet"))),
                material = Ingredient(setOf(Identifier.vanilla("red_dye"))),
            )
        val helmet = ItemStack(Identifier.vanilla("leather_helmet"), 1)
        val dye = ItemStack(Identifier.vanilla("red_dye"), 1)

        val dyed = assertNotNull(SpecialCrafting.assemble(recipe, grid(2, 1, listOf(helmet, dye))))

        assertEquals(
            checkNotNull(DyeColours.of(Identifier.vanilla("red_dye"))),
            (dyed.components[SpecialRecipes.DYED_COLOR] as? NbtInt)?.value,
        )
    }

    /** parity: `BookCloningRecipe`: the copies carry the book, and there is one per blank. */
    @Test
    fun aBookIsCopiedOntoEveryBlank() {
        val recipe =
            Recipe.Special(
                group = "",
                result = ItemStack(Identifier.vanilla("written_book"), 1),
                kind = Recipe.Special.Kind.BOOK_CLONING,
                target = Ingredient(setOf(Identifier.vanilla("written_book"))),
                material = Ingredient(setOf(Identifier.vanilla("writable_book"))),
            )
        val written =
            ItemStack(Identifier.vanilla("written_book"), 1, NbtCompound().also { it.putString("t", "hello") })
        val blank = ItemStack(Identifier.vanilla("writable_book"), 1)

        val copies = assertNotNull(SpecialCrafting.assemble(recipe, grid(3, 1, listOf(written, blank, blank))))

        assertEquals(2, copies.count, "one per blank book")
        assertEquals("hello", (copies.components["t"] as? org.basaltmc.wire.nbt.NbtString)?.value)
    }

    /** parity: `SmithingTrimRecipe.applyTrim`, whose result is the armour with the pattern on it. */
    @Test
    fun aTrimIsTheArmourWithAPatternOnIt() {
        val recipe =
            Recipe.Smithing(
                group = "",
                result = ItemStack(Identifier.vanilla("air"), 1),
                template = Ingredient(setOf(Identifier.vanilla("bolt_armor_trim_smithing_template"))),
                base = Ingredient(setOf(Identifier.vanilla("iron_chestplate"))),
                addition = Ingredient(setOf(Identifier.vanilla("iron_ingot"))),
                trimPattern = Identifier.vanilla("bolt"),
            )
        val armour = ItemStack(Identifier.vanilla("iron_chestplate"), 1)
        val ingot = ItemStack(Identifier.vanilla("iron_ingot"), 1)

        val trimmed = assertNotNull(recipe.assemble(armour, ingot))

        assertEquals(Identifier.vanilla("iron_chestplate"), trimmed.item, "it is the same armour")
        val trim = assertNotNull(trimmed.components[SpecialRecipes.TRIM] as? NbtCompound)
        assertEquals("minecraft:bolt", (trim["pattern"] as? org.basaltmc.wire.nbt.NbtString)?.value)
        assertEquals("minecraft:iron", (trim["material"] as? org.basaltmc.wire.nbt.NbtString)?.value)
    }

    /** An item that provides no trim material cannot trim anything. */
    @Test
    fun somethingThatIsNotATrimMaterialTrimsNothing() {
        val recipe =
            Recipe.Smithing(
                group = "",
                result = ItemStack(Identifier.vanilla("air"), 1),
                template = Ingredient(setOf(Identifier.vanilla("bolt_armor_trim_smithing_template"))),
                base = Ingredient(setOf(Identifier.vanilla("iron_chestplate"))),
                addition = Ingredient(setOf(Identifier.vanilla("dirt"))),
                trimPattern = Identifier.vanilla("bolt"),
            )

        assertNull(
            recipe.assemble(
                ItemStack(Identifier.vanilla("iron_chestplate"), 1),
                ItemStack(Identifier.vanilla("dirt"), 1),
            ),
        )
    }

    /** parity: `SmithingTransformRecipe`, which keeps what the base carried. */
    @Test
    fun aTransformKeepsWhatTheBaseCarried() {
        val recipe =
            Recipe.Smithing(
                group = "",
                result = ItemStack(Identifier.vanilla("netherite_sword"), 1),
                template = Ingredient(setOf(Identifier.vanilla("netherite_upgrade_smithing_template"))),
                base = Ingredient(setOf(Identifier.vanilla("diamond_sword"))),
                addition = Ingredient(setOf(Identifier.vanilla("netherite_ingot"))),
            )
        val sword = damaged(Identifier.vanilla("diamond_sword"), 42)

        val upgraded =
            assertNotNull(recipe.assemble(sword, ItemStack(Identifier.vanilla("netherite_ingot"), 1)))

        assertEquals(Identifier.vanilla("netherite_sword"), upgraded.item)
        assertEquals(42, SpecialRecipes.damageOf(upgraded), "the damage came across")
    }

    /** Every type in the folder parses: a family nobody models is a family nobody notices. */
    @Test
    fun everyRecipeTypeParses() {
        val types =
            listOf(
                """{"type":"minecraft:crafting_special_repairitem"}""",
                """{"type":"minecraft:crafting_special_bannerduplicate","banner":"minecraft:white_banner",""" +
                    """"result":{"id":"minecraft:white_banner"}}""",
                """{"type":"minecraft:crafting_special_mapextending","map":"minecraft:filled_map",""" +
                    """"material":"minecraft:paper","result":{"id":"minecraft:filled_map"}}""",
                """{"type":"minecraft:crafting_special_firework_rocket","fuel":"minecraft:gunpowder",""" +
                    """"shell":"minecraft:paper","star":"minecraft:firework_star",""" +
                    """"result":{"count":3,"id":"minecraft:firework_rocket"}}""",
                """{"type":"minecraft:crafting_imbue","material":"minecraft:arrow",""" +
                    """"source":"minecraft:lingering_potion","result":{"count":8,"id":"minecraft:tipped_arrow"}}""",
                """{"type":"minecraft:smithing_trim","addition":"minecraft:iron_ingot",""" +
                    """"base":"minecraft:iron_chestplate","pattern":"minecraft:bolt",""" +
                    """"template":"minecraft:bolt_armor_trim_smithing_template"}""",
            )

        for (json in types) {
            val recipe = RecipeJson.parse(json) { emptySet() }
            assertTrue(recipe is Recipe.Special || recipe is Recipe.Smithing, json)
        }
    }

    /** parity: `ImbueRecipe`, the potion in the middle and eight arrows around it. */
    @Test
    fun aLingeringPotionImbuesEightArrows() {
        val recipe =
            assertIs<Recipe.Special>(
                RecipeJson.parse(
                    """{"type":"minecraft:crafting_imbue","material":"minecraft:arrow",""" +
                        """"source":"minecraft:lingering_potion",""" +
                        """"result":{"count":8,"id":"minecraft:tipped_arrow"}}""",
                ) { emptySet() },
            )
        val arrow = ItemStack(Identifier.vanilla("arrow"), 1)
        val potion =
            ItemStack(Identifier.vanilla("lingering_potion"), 1, NbtCompound().also { it.putString("p", "healing") })
        val cells = List(9) { if (it == 4) potion else arrow }

        val tipped = assertNotNull(SpecialCrafting.assemble(recipe, grid(3, 3, cells)))

        assertEquals(8, tipped.count)
        assertEquals("healing", (tipped.components["p"] as? org.basaltmc.wire.nbt.NbtString)?.value)
    }

    /** parity: `Recipe.getRemainingItems`, the empty bucket a cake leaves where the milk was. */
    @Test
    fun aBucketIsLeftBehindWhereTheMilkWas() {
        Recipe.remainderOf = { CraftingRemainders.of(it) }
        val recipe =
            Recipe.Shapeless(
                group = "",
                result = ItemStack(Identifier.vanilla("cake"), 1),
                ingredients = listOf(Ingredient(setOf(Identifier.vanilla("milk_bucket")))),
            )
        val milk = ItemStack(Identifier.vanilla("milk_bucket"), 1)

        val left = recipe.remainingItems(grid(1, 1, listOf(milk)))

        assertEquals(Identifier.vanilla("bucket"), assertNotNull(left.single()).item)
    }

    /** parity: `BookCloningRecipe.getRemainingItems`, which keeps the book being copied. */
    @Test
    fun theBookBeingCopiedStaysInTheGrid() {
        Recipe.remainderOf = { CraftingRemainders.of(it) }
        val recipe =
            Recipe.Special(
                group = "",
                result = ItemStack(Identifier.vanilla("written_book"), 1),
                kind = Recipe.Special.Kind.BOOK_CLONING,
                target = Ingredient(setOf(Identifier.vanilla("written_book"))),
                material = Ingredient(setOf(Identifier.vanilla("writable_book"))),
            )
        val written = ItemStack(Identifier.vanilla("written_book"), 1)
        val blank = ItemStack(Identifier.vanilla("writable_book"), 1)

        val left = recipe.remainingItems(grid(2, 1, listOf(written, blank)))

        assertEquals(Identifier.vanilla("written_book"), assertNotNull(left[0]).item)
        assertNull(left[1], "the blank one is used up")
    }

    private fun damaged(
        item: Identifier,
        damage: Int,
    ): ItemStack = ItemStack(item, 1, NbtCompound().also { it[SpecialRecipes.DAMAGE] = NbtInt(damage) })

    private fun grid(
        width: Int,
        height: Int,
        items: List<ItemStack?>,
    ): CraftingInput = CraftingInput.of(width, height, items)

    private companion object {
        const val IRON_PICKAXE_DURABILITY = 250
    }
}
