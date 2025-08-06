package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.world.MobSpawnFinalization
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `Entity.saveWithoutId`, in the keys it shares with it. */
@OptIn(ExperimentalUuidApi::class)
class EntityNbtTest {
    private fun zombie(): ServerEntity =
        ServerEntity(
            entityId = 7,
            uuid = Uuid.parse("11111111-2222-3333-4444-555555555555"),
            type = Identifier.vanilla("zombie"),
            protocolType = 151,
            x = 8.5,
            y = 64.0,
            z = -3.25,
            yaw = 90f,
            pitch = -12f,
        )

    @Test
    fun anEntityComesBackAsItWent() {
        val zombie = zombie()
        zombie.health = 13.5
        zombie.age = 42
        zombie.velocityX = 0.25
        zombie.velocityZ = -0.5
        zombie.persistent = true
        zombie.customName = "Bob"
        zombie.tags += "friendly"

        val restored = assertNotNull(EntityNbt.decode(EntityNbt.encode(zombie), entityId = 9, protocolType = 151))
        assertEquals(9, restored.entityId)
        assertEquals(zombie.uuid, restored.uuid)
        assertEquals(zombie.type, restored.type)
        assertEquals(8.5, restored.x)
        assertEquals(-3.25, restored.z)
        assertEquals(90f, restored.yaw)
        assertEquals(13.5, restored.health)
        assertEquals(42, restored.age)
        assertEquals(0.25, restored.velocityX)
        assertTrue(restored.persistent)
        assertEquals("Bob", restored.customName)
        assertTrue("friendly" in restored.tags)
    }

    @Test
    fun cubeSizeUsesVanillasMinusOneNbtAndReturnsAsTheLiveSize() {
        val slime =
            ServerEntity(
                entityId = 8,
                uuid = Uuid.random(),
                type = Identifier.vanilla("slime"),
                protocolType = 1,
                x = 0.5,
                y = 32.0,
                z = 0.5,
            ).also { it.cubeSize = 4 }

        val tag = EntityNbt.encode(slime)
        assertEquals(3, tag.getInt("Size"))
        assertEquals(4, assertNotNull(EntityNbt.decode(tag, entityId = 9, protocolType = 1)).cubeSize)
    }

    @Test
    fun generatedMobAttributesUseAndReloadVanillasPackedShape() {
        val zombie = zombie()
        MobSpawnFinalization.apply(zombie, groupIndex = 0, LegacyRandom(0L), specialMultiplier = 0f)

        val tag = EntityNbt.encode(zombie)
        assertEquals(1, tag.getList("attributes")?.size)
        val restored = assertNotNull(EntityNbt.decode(tag, entityId = 9, protocolType = 151))
        val modifier =
            restored.attributeModifiers["minecraft:follow_range"]?.get("minecraft:random_spawn_bonus")
        assertEquals(0.056326043040338905, assertNotNull(modifier).amount, 1e-15)
        assertEquals(AttributeOperation.ADD_MULTIPLIED_BASE, modifier.operation)
    }

    @Test
    fun generatedHorseReloadsRandomBasesAndClampsConstructionHealth() {
        val data = NbtCompound()
        MobSpawnFinalization.writeGenerated(data, Identifier.vanilla("horse"), 0, LegacyRandom(1L))
        val descriptor =
            ChunkGeneratedEntity(
                Identifier.vanilla("horse"),
                0.5,
                64.0,
                0.5,
                0f,
                0f,
                data,
            )

        val restored = assertNotNull(EntityNbt.decodeGenerated(descriptor, entityId = 10, protocolType = 66))
        assertEquals(24.0, restored.attribute("minecraft:max_health"))
        assertEquals(24.0, restored.health, "the constructor's default 53 health must follow the randomized maximum")
        assertEquals(0.22561408252176893, restored.attribute("minecraft:movement_speed"), 1e-15)
        assertEquals(0.7819374795746402, restored.attribute("minecraft:jump_strength"), 1e-15)
    }

    @Test
    fun aVillagerKeepsItsTrade() {
        val villager = zombie()
        villager.profession = "librarian"
        villager.villagerLevel = 3
        villager.villagerExperience = 55

        val restored = assertNotNull(EntityNbt.decode(EntityNbt.encode(villager), entityId = 1, protocolType = 1))
        assertEquals("librarian", restored.profession)
        assertEquals(3, restored.villagerLevel)
        assertEquals(55, restored.villagerExperience)
    }

    @Test
    fun aTagWithoutAPositionIsNoEntity() {
        val tag = EntityNbt.encode(zombie())
        tag.remove("Pos")
        assertNull(EntityNbt.decode(tag, entityId = 1, protocolType = 1))
    }

    @Test
    fun aGeneratedVillagerBecomesALiveEntityWithoutLosingItsStructureNbt() {
        val villagerData =
            NbtCompound().apply {
                putString("type", "minecraft:taiga")
                putString("profession", "minecraft:nitwit")
                putInt("level", 2)
            }
        val data =
            NbtCompound().apply {
                putString("id", "minecraft:villager")
                putBoolean("NoAI", false)
                putInt("Age", -1200)
                this["VillagerData"] = villagerData
                putInt("Xp", 17)
            }
        val descriptor =
            ChunkGeneratedEntity(
                Identifier.vanilla("villager"),
                8.5,
                64.0,
                9.5,
                90f,
                5f,
                data,
            )

        val entity = assertNotNull(EntityNbt.decodeGenerated(descriptor, entityId = 44, protocolType = 140))

        assertEquals(Identifier.vanilla("villager"), entity.type)
        assertEquals(8.5, entity.x)
        assertEquals(90f, entity.yaw)
        assertEquals(-1200, entity.age)
        assertEquals("nitwit", entity.profession)
        assertEquals(2, entity.villagerLevel)
        assertEquals(17, entity.villagerExperience)
        assertEquals("minecraft:taiga", entity.sourceNbt?.getCompound("VillagerData")?.getString("type"))
        assertTrue(!entity.noAi, "the absence of NoAI=true must leave structure villagers able to think")
    }

    @Test
    fun anExplicitGeneratedNoAiFlagSurvivesMaterializationAndSave() {
        val descriptor =
            ChunkGeneratedEntity(
                Identifier.vanilla("villager"),
                0.5,
                64.0,
                0.5,
                0f,
                0f,
                NbtCompound().apply { putBoolean("NoAI", true) },
            )

        val entity = assertNotNull(EntityNbt.decodeGenerated(descriptor, entityId = 4, protocolType = 140))

        assertTrue(entity.noAi)
        assertTrue(EntityNbt.decode(EntityNbt.encode(entity), entityId = 5, protocolType = 140)!!.noAi)
    }
}
