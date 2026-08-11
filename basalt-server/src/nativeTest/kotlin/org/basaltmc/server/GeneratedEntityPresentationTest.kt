package org.basaltmc.server

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtFloat
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.protocol.play.SetEntityDataPacket
import org.basaltmc.protocol.play.SetEquipmentPacket
import kotlin.test.Test
import kotlin.test.assertEquals

class GeneratedEntityPresentationTest {
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
}
