package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.uuid.Uuid

class GeneratedEntityPresentationTest {
    @Test
    fun mobFlagsPreserveNoAiAndLeftHandedState() {
        val data =
            NbtCompound().apply {
                putBoolean("NoAI", true)
                putBoolean("LeftHanded", true)
            }

        assertEquals(
            listOf(SetEntityDataPacket.Entry.Bytes(15, 3)),
            generatedEntityMetadata("zombie", data),
        )
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Bytes(15, 4)),
            generatedEntityMetadata(
                "armor_stand",
                NbtCompound().apply {
                    putBoolean("NoAI", true)
                    putBoolean("ShowArms", true)
                },
            ),
            "NoAI is not a mob flag on furniture whose index 15 belongs to another class",
        )
    }

    @Test
    fun materializesArmorStandFlagsPoseAndEquipment() {
        val pose = NbtCompound()
        pose["Head"] =
            NbtList(
                NbtType.FLOAT,
                listOf(NbtFloat(3.978817f), NbtFloat(1.5454245f), NbtFloat(0f)),
            )
        val head =
            NbtCompound().apply {
                putString("id", "minecraft:iron_helmet")
                putInt("count", 1)
            }
        val data =
            NbtCompound().apply {
                putBoolean("Invisible", true)
                putFloat("Health", 20f)
                putBoolean("ShowArms", true)
                putCompound("Pose", pose)
                putCompound("equipment", NbtCompound(mapOf("head" to head)))
            }

        assertEquals(
            listOf(
                SetEntityDataPacket.Entry.Bytes(0, 0x20),
                SetEntityDataPacket.Entry.Floats(9, 20f),
                SetEntityDataPacket.Entry.Bytes(15, 4),
                SetEntityDataPacket.Entry.Rotations(16, 3.978817f, 1.5454245f, 0f),
            ),
            generatedEntityMetadata("armor_stand", data),
        )
        assertEquals(
            listOf(GeneratedEquipment(SetEquipmentPacket.Type.HEAD, "minecraft:iron_helmet", 1)),
            generatedEntityEquipment(data),
        )
    }

    @Test
    fun materializesBabyVillagerDataAndFinalizedFlag() {
        val villagerData =
            NbtCompound().apply {
                putString("type", "minecraft:desert")
                putString("profession", "minecraft:nitwit")
                putInt("level", 1)
            }
        val data =
            NbtCompound().apply {
                putInt("Age", -21_359)
                putFloat("Health", 20f)
                putCompound("VillagerData", villagerData)
            }

        assertEquals(
            listOf(
                SetEntityDataPacket.Entry.Floats(9, 20f),
                SetEntityDataPacket.Entry.Booleans(16, true),
                SetEntityDataPacket.Entry.VillagerData(19, typeId = 0, professionId = 11, level = 1),
                SetEntityDataPacket.Entry.Booleans(20, true),
            ),
            generatedEntityMetadata("villager", data),
        )
    }

    @Test
    fun materializesHorseAndSheepVariants() {
        val horse = NbtCompound().apply { putInt("Variant", 261) }
        val sheep =
            NbtCompound().apply {
                putByte("Color", 15)
                putBoolean("Sheared", true)
            }

        assertEquals(
            listOf(SetEntityDataPacket.Entry.Ints(19, 261)),
            generatedEntityMetadata("horse", horse),
        )
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Bytes(18, 0x1F)),
            generatedEntityMetadata("sheep", sheep),
        )
    }

    @Test
    fun materializesPrimitiveNaturalAnimalVariants() {
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Ints(18, 5)),
            generatedEntityMetadata("rabbit", NbtCompound().apply { putInt("RabbitType", 5) }),
        )
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Ints(20, 2), SetEntityDataPacket.Entry.Ints(21, 3)),
            generatedEntityMetadata(
                "llama",
                NbtCompound().apply {
                    putInt("Strength", 2)
                    putInt("Variant", 3)
                },
            ),
        )
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Ints(20, 4)),
            generatedEntityMetadata("parrot", NbtCompound().apply { putInt("Variant", 4) }),
        )
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Ints(17, 2)),
            generatedEntityMetadata("salmon", NbtCompound().apply { putString("type", "large") }),
        )
    }

    @Test
    fun liveBabiesAndCubeSizesSendInitialAndBoundaryMetadata() {
        val slime = ServerEntity(1, Uuid.random(), Identifier.vanilla("slime"), 1, 0.0, 64.0, 0.0)
        slime.age = -24_000
        slime.cubeSize = 4
        val baby = runtimeEntityMetadata(slime)

        assertEquals(
            listOf(
                SetEntityDataPacket.Entry.Booleans(16, true),
                SetEntityDataPacket.Entry.Ints(18, 4),
            ),
            baby.changesFrom(null),
        )

        slime.age = 0
        assertEquals(
            listOf(SetEntityDataPacket.Entry.Booleans(16, false)),
            runtimeEntityMetadata(slime).changesFrom(baby),
        )
    }
}
