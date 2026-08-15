package org.basaltmc.server.storage

import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ProgressRepositoryTest {
    private val path = "build/test-data/progress_test.db"
    private val uuid = "11111111-2222-3333-4444-555555555555"

    @AfterTest
    fun cleanup() {
        for (suffix in listOf("", "-wal", "-shm")) platform.posix.remove("$path$suffix")
    }

    private fun <T> withStore(block: (PlayerStore) -> T): T = PlayerStore.open(path).use(block)

    @Test
    fun criteriaAndRecipesSurviveTheServerStopping() {
        withStore { store ->
            store.progress.grantCriterion(uuid, "minecraft:story/mine_stone", "get_stone")
            store.progress.grantCriterion(uuid, "minecraft:story/mine_stone", "second")
            store.progress.learnRecipe(uuid, "minecraft:stone_pickaxe")
        }

        withStore { store ->
            val stored = store.progress.load(uuid)
            assertEquals(setOf("get_stone", "second"), stored.criteria["minecraft:story/mine_stone"])
            assertEquals(setOf("minecraft:stone_pickaxe"), stored.recipes)
        }
    }

    @Test
    fun revokingAnAdvancementTakesEveryCriterionWithIt() {
        withStore { store ->
            store.progress.grantCriterion(uuid, "minecraft:story/mine_stone", "get_stone")
            store.progress.grantCriterion(uuid, "minecraft:story/root", "crafting_table")
            store.progress.revokeAdvancement(uuid, "minecraft:story/mine_stone")

            val stored = store.progress.load(uuid)
            assertTrue("minecraft:story/mine_stone" !in stored.criteria)
            assertEquals(setOf("crafting_table"), stored.criteria["minecraft:story/root"])
        }
    }

    @Test
    fun grantingTwiceIsNotAnError() {
        withStore { store ->
            store.progress.grantCriterion(uuid, "minecraft:story/root", "crafting_table")
            store.progress.grantCriterion(uuid, "minecraft:story/root", "crafting_table")
            assertEquals(
                1,
                store.progress
                    .load(uuid)
                    .criteria
                    .getValue("minecraft:story/root")
                    .size,
            )
        }
    }

    @Test
    fun forgettingARecipeRemovesOnlyThatOne() {
        withStore { store ->
            store.progress.learnRecipe(uuid, "minecraft:stone_pickaxe")
            store.progress.learnRecipe(uuid, "minecraft:torch")
            store.progress.forgetRecipe(uuid, "minecraft:torch")
            assertEquals(setOf("minecraft:stone_pickaxe"), store.progress.load(uuid).recipes)
        }
    }
}
