package org.basaltmc.server.runtime.player

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class FoodDataTest {
    private fun tick(
        food: FoodData,
        times: Int,
        difficulty: Difficulty = Difficulty.NORMAL,
        health: () -> Float,
        heal: (Float) -> Unit = {},
        hurt: (Float) -> Unit = {},
    ) {
        repeat(times) {
            food.tick(difficulty, health(), PlayerSession.MAX_HEALTH, heal, hurt)
        }
    }

    @Test
    fun exhaustionSpendsSaturationBeforeItSpendsFood() {
        val food = FoodData()
        food.addExhaustion(4.5f)

        tick(food, 1, health = { PlayerSession.MAX_HEALTH })

        assertEquals(4f, food.saturationLevel)
        assertEquals(20, food.foodLevel)
        assertEquals(0.5f, food.exhaustionLevel)
    }

    @Test
    fun foodOnlyDropsOnceSaturationIsGone() {
        val food = FoodData()
        food.set(20, 0f)
        food.addExhaustion(4.5f)

        tick(food, 1, health = { PlayerSession.MAX_HEALTH })

        assertEquals(19, food.foodLevel)
    }

    @Test
    fun regenerationTakes80TicksAtOrAbove18Food() {
        val food = FoodData()
        food.set(18, 0f)
        var health = 10f

        tick(food, 79, health = { health }, heal = { health += it })
        assertEquals(10f, health)

        tick(food, 1, health = { health }, heal = { health += it })
        assertEquals(11f, health)
        // parity: healing one heart costs 6 exhaustion, which is what makes regenerating expensive.
        assertEquals(6f, food.exhaustionLevel)
    }

    @Test
    fun anEmptyFoodBarStarvesEvery80TicksOnNormalButStopsAtOneHeart() {
        val food = FoodData()
        food.set(0, 0f)
        var health = 4f
        var hits = 0

        tick(food, 80, difficulty = Difficulty.NORMAL, health = { health }, hurt = {
            health -= it
            hits++
        })
        assertEquals(1, hits)
        assertEquals(3f, health)

        health = 1f
        tick(food, 80, difficulty = Difficulty.NORMAL, health = { health }, hurt = {
            health -= it
            hits++
        })
        assertEquals(1, hits)
    }

    @Test
    fun peacefulNeverEmptiesTheFoodBar() {
        val food = FoodData()
        food.set(20, 0f)
        food.addExhaustion(4.5f)

        tick(food, 1, difficulty = Difficulty.PEACEFUL, health = { PlayerSession.MAX_HEALTH })

        assertEquals(20, food.foodLevel)
    }

    @Test
    fun onlySprintingOnTheGroundCostsExhaustion() {
        assertEquals(0f, walked(1.0, sprinting = false, onGround = true))
        assertEquals(0f, walked(1.0, sprinting = true, onGround = false))
        // 0.1 per block, computed through vanilla's own round-to-centimetres path.
        assertEquals(0.1f, walked(1.0, sprinting = true, onGround = true), 1e-6f)
        assertEquals(0.05f, walked(0.5, sprinting = true, onGround = true), 1e-6f)
    }

    /** parity: swimming costs a tenth of sprinting, and counts the vertical distance too. */
    @Test
    fun swimmingCostsItsOwnRate() {
        assertEquals(0.01f, walked(1.0, sprinting = false, onGround = false, inWater = true), 1e-6f)
        // sqrt(3) blocks, rounded to centimetres the way vanilla rounds it.
        assertEquals(
            0.0173f,
            FoodData.movementExhaustion(1.0, 1.0, 1.0, sprinting = false, onGround = false, inWater = true),
            1e-6f,
        )
    }

    /** parity: `ServerPlayer.jumpFromGround`, 0.05 and 0.2 while sprinting. */
    @Test
    fun jumpingCostsMoreWhenSprinting() {
        assertEquals(0.05f, FoodData.jumpExhaustion(sprinting = false))
        assertEquals(0.2f, FoodData.jumpExhaustion(sprinting = true))
    }

    private fun walked(
        distance: Double,
        sprinting: Boolean,
        onGround: Boolean,
        inWater: Boolean = false,
    ): Float = FoodData.movementExhaustion(distance, 0.0, 0.0, sprinting, onGround, inWater)
}

class ExperienceDataTest {
    @Test
    fun theFirstLevelsCostWhatVanillaChargesForThem() {
        val experience = ExperienceData()
        assertEquals(7, experience.xpNeededForNextLevel())

        experience.givePoints(7)
        assertEquals(1, experience.level)
        assertEquals(9, experience.xpNeededForNextLevel())

        experience.setLevels(15)
        assertEquals(37, experience.xpNeededForNextLevel())
        experience.setLevels(30)
        assertEquals(112, experience.xpNeededForNextLevel())
    }

    @Test
    fun pointsRollOverIntoLevelsAndBackOutAgain() {
        val experience = ExperienceData()
        experience.givePoints(100)
        val reached = experience.level
        assertTrue(reached > 0)
        assertEquals(100, experience.total)

        experience.givePoints(-100)
        assertEquals(0, experience.level)
        assertEquals(0, experience.total)
    }

    @Test
    fun settingPointsKeepsTheLevelAndNeverRollsOver() {
        val experience = ExperienceData()
        experience.setLevels(1)
        experience.setPoints(8)

        assertEquals(1, experience.level)
        assertTrue(experience.progress < 1f)
    }
}
