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
    fun concreteGroupDataControlsBabyRolls() {
        assertEquals(null, MobSpawnFinalization.babySpawnChance("slime"))
        assertEquals(null, MobSpawnFinalization.babySpawnChance("wolf"))
        assertEquals(0.1f, MobSpawnFinalization.babySpawnChance("dolphin"))
        assertEquals(0.2f, MobSpawnFinalization.babySpawnChance("donkey"))
        assertEquals(1f, MobSpawnFinalization.babySpawnChance("rabbit"))

        val adultSlime = MobSpawnFinalization.stateFor(Identifier.vanilla("slime"), 3, LegacyRandom(4640L), 0f)
        assertEquals(0, adultSlime.age, "SlimeGroupData explicitly disables inherited baby rolls")

        val rabbit = MobSpawnFinalization.stateFor(Identifier.vanilla("rabbit"), 1, LegacyRandom(0L), 0f)
        assertEquals(AgeableMobs.BABY_START_AGE, rabbit.age)

        val foxSecond = MobSpawnFinalization.stateFor(Identifier.vanilla("fox"), 1, LegacyRandom(0L), 0f)
        val foxThird = MobSpawnFinalization.stateFor(Identifier.vanilla("fox"), 2, LegacyRandom(0L), 0f)
        assertEquals(0, foxSecond.age)
        assertEquals(AgeableMobs.BABY_START_AGE, foxThird.age)
    }

    @Test
    fun primitiveAnimalVariantsRunBeforeTheirCommonSuperclass() {
        val parrot = MobSpawnFinalization.stateFor(Identifier.vanilla("parrot"), 0, LegacyRandom(2L), 0f)
        assertEquals(3, parrot.typeData.getInt("Variant"))

        val llama = MobSpawnFinalization.stateFor(Identifier.vanilla("llama"), 0, LegacyRandom(1L), 0f)
        assertEquals(2, llama.typeData.getInt("Strength"))
        assertEquals(1, llama.typeData.getInt("Variant"))

        val group = MobSpawnGroupData()
        val rabbitRandom = LegacyRandom(1L)
        val firstRabbit =
            MobSpawnFinalization.stateFor(
                Identifier.vanilla("rabbit"),
                0,
                rabbitRandom,
                0f,
                group,
                Identifier.vanilla("plains"),
            )
        val secondRabbit =
            MobSpawnFinalization.stateFor(
                Identifier.vanilla("rabbit"),
                1,
                rabbitRandom,
                0f,
                group,
                Identifier.vanilla("frozen_peaks"),
            )
        assertEquals(5, firstRabbit.typeData.getInt("RabbitType"), "plains roll 85 selects salt")
        assertEquals(
            5,
            secondRabbit.typeData.getInt("RabbitType"),
            "the first rabbit's variant belongs to its whole pack",
        )
        assertEquals(AgeableMobs.BABY_START_AGE, secondRabbit.age)

        val salmon = MobSpawnFinalization.stateFor(Identifier.vanilla("salmon"), 0, LegacyRandom(0L), 0f)
        assertEquals("small", salmon.typeData.getString("type"))
        val warmSheep =
            MobSpawnFinalization.stateFor(
                Identifier.vanilla("sheep"),
                0,
                LegacyRandom(0L),
                0f,
                biome = Identifier.vanilla("savanna"),
            )
        assertEquals(12, warmSheep.typeData.getByte("Color").toInt())
    }

    @Test
    fun horseVariantsAndAttributesUseConcreteSuperclassOrder() {
        val group = MobSpawnGroupData()
        val first = MobSpawnFinalization.stateFor(Identifier.vanilla("horse"), 0, LegacyRandom(1L), 0f, group)

        assertEquals(772, first.typeData.getInt("Variant"), "variant 4 plus markings 3 in the high byte")
        assertEquals(24.0, first.attributeBases["minecraft:max_health"])
        assertEquals(0.22561408252176893, first.attributeBases["minecraft:movement_speed"]!!, 1e-15)
        assertEquals(0.7819374795746402, first.attributeBases["minecraft:jump_strength"]!!, 1e-15)

        val later = MobSpawnFinalization.stateFor(Identifier.vanilla("horse"), 1, LegacyRandom(2L), 0f, group)
        assertEquals(4, later.typeData.getInt("Variant") and 0xFF, "the coat variant belongs to the pack")

        val llama = MobSpawnFinalization.stateFor(Identifier.vanilla("llama"), 0, LegacyRandom(1L), 0f)
        assertEquals(26.0, llama.attributeBases["minecraft:max_health"])

        val generated = NbtCompound()
        MobSpawnFinalization.writeGenerated(generated, Identifier.vanilla("horse"), 0, LegacyRandom(1L))
        assertEquals(4, generated.getList("attributes")?.size, "three randomized bases plus follow range")
    }

    @Test
    fun axolotlPackKeepsItsTwoCommonVariantsAndForcesLaterBabies() {
        val group = MobSpawnGroupData()
        val first = MobSpawnFinalization.stateFor(Identifier.vanilla("axolotl"), 0, LegacyRandom(1L), 0f, group)
        assertEquals(2, group.variant)
        assertEquals(0, group.secondaryVariant)
        assertEquals(2, first.typeData.getInt("Variant"))

        val second = MobSpawnFinalization.stateFor(Identifier.vanilla("axolotl"), 1, LegacyRandom(2L), 0f, group)
        assertEquals(0, second.typeData.getInt("Variant"), "later members select only from the stored pair")
        assertEquals(0, second.age)

        val third = MobSpawnFinalization.stateFor(Identifier.vanilla("axolotl"), 2, LegacyRandom(2L), 0f, group)
        assertEquals(AgeableMobs.BABY_START_AGE, third.age)
    }

    @Test
    fun foxPackKeepsBiomeVariantAndRollsMouthEquipmentBeforeSuper() {
        val group = MobSpawnGroupData()
        val first =
            MobSpawnFinalization.stateFor(
                Identifier.vanilla("fox"),
                0,
                LegacyRandom(4097L),
                0f,
                group,
                Identifier.vanilla("snowy_taiga"),
            )
        assertEquals("snow", first.typeData.getString("Type"))
        assertEquals(
            "minecraft:egg",
            first.typeData
                .getCompound("equipment")
                .getCompound("mainhand")
                .getString("id"),
        )

        val later =
            MobSpawnFinalization.stateFor(
                Identifier.vanilla("fox"),
                2,
                LegacyRandom(0L),
                0f,
                group,
                Identifier.vanilla("plains"),
            )
        assertEquals("snow", later.typeData.getString("Type"), "the first fox determines the whole pack")
        assertEquals(AgeableMobs.BABY_START_AGE, later.age)
    }

    @Test
    fun pandaGenesApplyOnlyTheirExpressedAttributeOverride() {
        val weak = MobSpawnFinalization.stateFor(Identifier.vanilla("panda"), 0, LegacyRandom(6531L), 0f)
        assertEquals("weak", weak.typeData.getString("MainGene"))
        assertEquals("weak", weak.typeData.getString("HiddenGene"))
        assertEquals(10.0, weak.attributeBases["minecraft:max_health"])

        val lazy = MobSpawnFinalization.stateFor(Identifier.vanilla("panda"), 0, LegacyRandom(4608L), 0f)
        assertEquals("lazy", lazy.typeData.getString("MainGene"))
        assertEquals("brown", lazy.typeData.getString("HiddenGene"))
        assertEquals(0.07000000029802322, lazy.attributeBases["minecraft:movement_speed"])
    }

    @Test
    fun caveSpiderOverrideSkipsMobFinalizerAndItsRandomDraws() {
        val random = LegacyRandom(7L)
        val untouched = LegacyRandom(7L)
        val state = MobSpawnFinalization.stateFor(Identifier.vanilla("cave_spider"), 0, random, 1f)

        assertEquals(false, state.ranMobFinalizer)
        assertEquals(untouched.nextLong(), random.nextLong())
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
