package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.entity.AgeableMobs
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Official 26.2 vectors for the common `AgeableMob`/`AbstractCubeMob` finalizers. */
class MobSpawnFinalizationTest {
    @Test
    fun onlyLaterAcceptedPackMembersRollForBeingBabies() {
        val cow = Identifier.vanilla("cow")
        val untouched = LegacyRandom(0L)
        val firstMemberRandom = LegacyRandom(0L)

        assertEquals(
            0,
            MobSpawnFinalization
                .stateFor(cow, groupIndex = 0, random = firstMemberRandom, specialMultiplier = 0f)
                .age,
        )
        assertEquals(untouched.nextFloat(), firstMemberRandom.nextFloat(), "the first member consumes no baby roll")

        val babySeed = (0L..10_000L).first { LegacyRandom(it).nextFloat() <= 0.05f }
        assertEquals(
            AgeableMobs.BABY_START_AGE,
            MobSpawnFinalization
                .stateFor(cow, groupIndex = 1, random = LegacyRandom(babySeed), specialMultiplier = 0f)
                .age,
        )
        assertTrue(AgeableMobs.isAgeable("squid"), "26.2 moved squid under AgeableWaterCreature")
        assertTrue(AgeableMobs.isAgeable("slime"), "26.2 cube mobs inherit AgeableMob")
    }

    @Test
    fun cubeSizeUsesTheOfficialJavaRandomSequenceAndDifficultyPromotion() {
        val slime = Identifier.vanilla("slime")

        assertEquals(1, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(1L), 0f).cubeSize)
        assertEquals(2, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(1L), 1f).cubeSize)
        assertEquals(4, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(2L), 1f).cubeSize)
        assertEquals(4, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(3L), 0f).cubeSize)

        val generated = NbtCompound()
        MobSpawnFinalization.writeGenerated(generated, slime, 0, LegacyRandom(2L), 1f)
        assertEquals(3, generated.getInt("Size"), "cube NBT stores live size minus one")
    }

    @Test
    fun difficultyMultiplierSeparatesWorldAgeFromChunkInhabitedTime() {
        assertEquals(0f, MobSpawnFinalization.specialMultiplier("normal", 0L, 0L, 1f))
        assertEquals(0f, MobSpawnFinalization.specialMultiplier("normal", 1_512_000L, 0L, 0f))
        assertEquals(0.25f, MobSpawnFinalization.specialMultiplier("normal", 1_512_000L, 0L, 1f))
        assertEquals(0.5f, MobSpawnFinalization.specialMultiplier("hard", 1_512_000L, 0L, 0f))
        assertEquals(1f, MobSpawnFinalization.specialMultiplier("hard", 1_512_000L, 3_600_000L, 1f))
    }
}
