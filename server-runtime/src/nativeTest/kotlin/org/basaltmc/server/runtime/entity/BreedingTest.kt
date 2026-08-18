package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `Animal` and `BreedGoal`: what feeding two animals leads to. */
@OptIn(ExperimentalUuidApi::class)
class BreedingTest {
    private var nextId = 1

    private fun animal(
        type: String = "cow",
        x: Double = 0.0,
        z: Double = 0.0,
    ): ServerEntity = ServerEntity(nextId++, Uuid.random(), Identifier.vanilla(type), 1, x, 64.0, z)

    /** parity: each animal's own `isFood`. */
    @Test
    fun eachAnimalEatsItsOwnFood() {
        assertTrue(Breeding.isFood(Identifier.vanilla("cow"), Identifier.vanilla("wheat")))
        assertFalse(Breeding.isFood(Identifier.vanilla("cow"), Identifier.vanilla("carrot")))
        assertTrue(Breeding.isFood(Identifier.vanilla("pig"), Identifier.vanilla("carrot")))
        assertTrue(Breeding.isFood(Identifier.vanilla("chicken"), Identifier.vanilla("wheat_seeds")))
        assertFalse(Breeding.isFood(Identifier.vanilla("zombie"), Identifier.vanilla("wheat")), "not everything eats")
    }

    /** parity: `setInLove`, thirty seconds of it, and a baby that is never in love at all. */
    @Test
    fun feedingPutsItInLove() {
        val cow = animal()

        assertTrue(Breeding.canFallInLove(cow))
        Breeding.feed(cow)
        assertEquals(Breeding.LOVE_TICKS, cow.loveTicks)
        assertFalse(Breeding.canFallInLove(cow), "it is already in love")

        val calf = animal().also { it.age = Breeding.BABY_AGE }
        assertFalse(Breeding.canFallInLove(calf), "a calf is too young")
    }

    /** parity: `BreedGoal.tick`: sixty ticks together, and within three blocks. */
    @Test
    fun twoInLoveBreedAfterSixtyTicks() {
        val one = animal(x = 0.0)
        val two = animal(x = 1.0)
        Breeding.feed(one)
        Breeding.feed(two)

        repeat(Breeding.COURTSHIP_TICKS - 1) {
            assertNull(Breeding.tick(one, listOf(two)), "not yet")
        }

        val partner = assertNotNull(Breeding.tick(one, listOf(two)))
        assertEquals(two.entityId, partner.entityId)
    }

    /** parity: three blocks apart is close enough, eight is not. */
    @Test
    fun theyHaveToBeNextToEachOther() {
        val one = animal(x = 0.0)
        val far = animal(x = 6.0)
        Breeding.feed(one)
        Breeding.feed(far)

        repeat(Breeding.COURTSHIP_TICKS + 10) { Breeding.tick(one, listOf(far)) }

        assertNull(Breeding.tick(one, listOf(far)), "six blocks apart is too far to breed")
    }

    /** parity: `spawnChildFromBreeding`: both parents come out of love and wait five minutes. */
    @Test
    fun afterBreedingBothParentsWait() {
        val one = animal()
        val two = animal(x = 1.0)
        Breeding.feed(one)
        Breeding.feed(two)

        Breeding.afterBreeding(one, two)

        assertEquals(0, one.loveTicks)
        assertEquals(0, two.loveTicks)
        assertEquals(Breeding.PARENT_COOLDOWN, one.age)
        assertFalse(Breeding.canFallInLove(one), "still on cooldown")
    }

    /** parity: `AgeableMob.ageUp`, one tick at a time in both directions. */
    @Test
    fun ababyGrowsUpAndAParentCountsDown() {
        val calf = animal().also { it.age = -3 }
        repeat(3) { Breeding.growUp(calf) }
        assertEquals(0, calf.age, "grown at last")

        val parent = animal().also { it.age = 3 }
        repeat(3) { Breeding.growUp(parent) }
        assertEquals(0, parent.age, "and ready to breed again")
    }

    /** parity: `canMate`: the same kind, both in love, and never with itself. */
    @Test
    fun whoCanMateWithWhom() {
        val cow = animal("cow")
        val otherCow = animal("cow", x = 1.0)
        val pig = animal("pig", x = 1.0)
        Breeding.feed(cow)
        Breeding.feed(otherCow)
        Breeding.feed(pig)

        assertTrue(Breeding.canMate(cow, otherCow))
        assertFalse(Breeding.canMate(cow, pig), "a cow and a pig make nothing")
        assertFalse(Breeding.canMate(cow, cow), "nor a cow and itself")
    }
}
