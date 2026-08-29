package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** Durable `removeWhenFarAway` regressions across Animal and its explicit exceptions. */
@OptIn(ExperimentalUuidApi::class)
class AnimalDespawnTest {
    @Test
    fun ordinaryAnimalsIgnoreBothGenericDistanceRules() {
        for (type in listOf("cow", "strider", "skeleton_horse", "happy_ghast")) {
            val animal = mob(type).also { it.idleTicks = 10_000 }
            assertFalse(MobDespawn.shouldDespawn(animal, MobCategory.CREATURE, 200.0, LegacyRandom(1L)), type)
        }
    }

    @Test
    fun explicitAnimalExceptionsKeepTheirOwnAnswers() {
        for ((type, category) in EXPLICIT_TRUE_OVERRIDES) {
            assertTrue(MobDespawn.shouldDespawn(mob(type), category, 200.0, LegacyRandom(1L)), type)
        }

        val bucketAxolotl =
            mob("axolotl").also {
                it.sourceNbt = NbtCompound().apply { putBoolean("FromBucket", true) }
            }
        assertFalse(MobDespawn.shouldDespawn(bucketAxolotl, MobCategory.AXOLOTLS, 200.0, LegacyRandom(1L)))

        val ocelot = mob("ocelot").also { it.tickCount = 2_400 }
        assertFalse(MobDespawn.shouldDespawn(ocelot, MobCategory.CREATURE, 200.0, LegacyRandom(1L)))
        ocelot.tickCount++
        assertTrue(MobDespawn.shouldDespawn(ocelot, MobCategory.CREATURE, 200.0, LegacyRandom(1L)))
        ocelot.sourceNbt = NbtCompound().apply { putBoolean("Trusting", true) }
        assertFalse(MobDespawn.shouldDespawn(ocelot, MobCategory.CREATURE, 200.0, LegacyRandom(1L)))
    }

    @Test
    fun onlyChickenJockeysUseGenericDistanceDespawn() {
        val chicken = mob("chicken")
        assertFalse(MobPersistence.requiresCustomPersistence(chicken))
        assertFalse(MobDespawn.shouldDespawn(chicken, MobCategory.CREATURE, 200.0, LegacyRandom(1L)))

        chicken.sourceNbt = NbtCompound().apply { putBoolean("IsChickenJockey", true) }
        assertFalse(MobPersistence.requiresCustomPersistence(chicken))
        assertTrue(MobDespawn.shouldDespawn(chicken, MobCategory.CREATURE, 200.0, LegacyRandom(1L)))
    }

    private fun mob(type: String): ServerEntity =
        ServerEntity(
            entityId = 1,
            uuid = Uuid.random(),
            type = Identifier.vanilla(type),
            protocolType = 1,
            x = 0.0,
            y = 64.0,
            z = 0.0,
        )

    private companion object {
        val EXPLICIT_TRUE_OVERRIDES =
            listOf(
                "axolotl" to MobCategory.AXOLOTLS,
                "cod" to MobCategory.WATER_AMBIENT,
                "nautilus" to MobCategory.UNDERGROUND_WATER_CREATURE,
                "zombie_horse" to MobCategory.CREATURE,
                "camel_husk" to MobCategory.CREATURE,
                "hoglin" to MobCategory.MONSTER,
            )
    }
}
