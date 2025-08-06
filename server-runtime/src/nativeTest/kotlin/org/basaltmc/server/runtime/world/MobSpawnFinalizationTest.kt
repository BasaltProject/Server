package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.entity.AgeableMobs
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Official 26.2 vectors for the common `AgeableMob`/`Mob`/`AbstractCubeMob` finalizers. */
class MobSpawnFinalizationTest {
    @Test
    fun onlyLaterAcceptedPackMembersRollForBeingBabies() {
        val cow = Identifier.vanilla("cow")
        val firstMemberRandom = LegacyRandom(0L)

        val first =
            MobSpawnFinalization.stateFor(
                cow,
                groupIndex = 0,
                random = firstMemberRandom,
                specialMultiplier = 0f,
            )
        assertEquals(0, first.age)
        assertEquals(0.056326043040338905, first.followRangeBonus, 1e-15)
        assertEquals(false, first.leftHanded)

        val babySeed = (0L..10_000L).first { LegacyRandom(it).nextFloat() <= 0.05f }
        val baby = MobSpawnFinalization.stateFor(cow, 1, LegacyRandom(babySeed), 0f)
        assertEquals(4_640L, babySeed)
        assertEquals(AgeableMobs.BABY_START_AGE, baby.age)
        assertEquals(0.012703759813200509, baby.followRangeBonus, 1e-15)
        assertTrue(AgeableMobs.isAgeable("squid"), "26.2 moved squid under AgeableWaterCreature")
        assertTrue(AgeableMobs.isAgeable("slime"), "26.2 cube mobs inherit AgeableMob")
    }

    @Test
    fun cubeSizeUsesTheOfficialJavaRandomSequenceAndDifficultyPromotion() {
        val slime = Identifier.vanilla("slime")

        assertEquals(2, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(1L), 0f).cubeSize)
        assertEquals(4, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(1L), 1f).cubeSize)
        assertEquals(1, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(2L), 1f).cubeSize)
        assertEquals(2, MobSpawnFinalization.stateFor(slime, 0, LegacyRandom(11L), 1f).cubeSize)

        val generated = NbtCompound()
        MobSpawnFinalization.writeGenerated(generated, slime, 0, LegacyRandom(1L), 1f)
        assertEquals(3, generated.getInt("Size"), "cube NBT stores live size minus one")
        assertEquals(1, generated.getList("attributes")?.size)

        val leftHanded = NbtCompound()
        MobSpawnFinalization.writeGenerated(leftHanded, Identifier.vanilla("zombie"), 0, LegacyRandom(38L))
        assertTrue(leftHanded.getBoolean("LeftHanded"))
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
