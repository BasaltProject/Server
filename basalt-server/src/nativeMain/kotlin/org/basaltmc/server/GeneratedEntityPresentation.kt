package org.basaltmc.server

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtFloat
import org.basaltmc.protocol.play.SetEntityDataPacket
import org.basaltmc.protocol.play.SetEquipmentPacket

/** Metadata needed to render immutable entities emitted by structure templates. */
internal fun generatedEntityMetadata(
    type: String,
    data: NbtCompound,
): List<SetEntityDataPacket.Entry> =
    buildList {
        var sharedFlags = 0
        if (data.getBoolean("Invisible")) sharedFlags = sharedFlags or 0x20
        if (sharedFlags != 0) add(SetEntityDataPacket.Entry.Bytes(0, sharedFlags.toByte()))
        if (data.getBoolean("NoGravity")) add(SetEntityDataPacket.Entry.Booleans(5, true))

        if ("Health" in data) add(SetEntityDataPacket.Entry.Floats(9, data.getFloat("Health")))

        if (type in AGEABLE_TYPES && data.getInt("Age") < 0) {
            add(SetEntityDataPacket.Entry.Booleans(16, true))
        }

        when (type) {
            "armor_stand" -> addArmorStandMetadata(data)
            "cat" -> addCatMetadata(data)
            "horse" ->
                data.getInt("Variant").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(19, it))
                }
            "sheep" -> {
                val wool = (data.getByte("Color").toInt() and 0x0F) or if (data.getBoolean("Sheared")) 0x10 else 0
                if (wool != 0) add(SetEntityDataPacket.Entry.Bytes(18, wool.toByte()))
            }
            "villager" -> addVillagerMetadata(data, dataIndex = 19, finalizedIndex = 20)
            "zombie_villager" -> {
                if (data.getBoolean("IsBaby")) add(SetEntityDataPacket.Entry.Booleans(16, true))
                addVillagerMetadata(data, dataIndex = 20, finalizedIndex = 21)
            }
        }
    }

private fun MutableList<SetEntityDataPacket.Entry>.addArmorStandMetadata(data: NbtCompound) {
    var flags = 0
    if (data.getBoolean("Small")) flags = flags or 1
    if (data.getBoolean("ShowArms")) flags = flags or 4
    if (data.getBoolean("NoBasePlate")) flags = flags or 8
    if (data.getBoolean("Marker")) flags = flags or 16
    if (flags != 0) add(SetEntityDataPacket.Entry.Bytes(15, flags.toByte()))

    val pose = data.getCompound("Pose")
    ARMOR_STAND_POSES.forEach { (name, index) ->
        val values = pose.getList(name) ?: return@forEach
        if (values.size != 3) return@forEach
        add(
            SetEntityDataPacket.Entry.Rotations(
                index,
                (values[0] as? NbtFloat)?.value ?: return@forEach,
                (values[1] as? NbtFloat)?.value ?: return@forEach,
                (values[2] as? NbtFloat)?.value ?: return@forEach,
            ),
        )
    }
}

private fun MutableList<SetEntityDataPacket.Entry>.addCatMetadata(data: NbtCompound) {
    val variant = CAT_VARIANT_IDS[data.getString("variant")]
    // Black and red are the exact 26.2 defaults, so they are absent from vanilla's initial
    // non-default metadata packet.
    if (variant != null && variant != 1) add(SetEntityDataPacket.Entry.RegistryId(20, variant))
    val collarColor = data.getByte("CollarColor", 14).toInt() and 0xFF
    if (collarColor != 14) add(SetEntityDataPacket.Entry.Ints(23, collarColor))
}

private fun MutableList<SetEntityDataPacket.Entry>.addVillagerMetadata(
    data: NbtCompound,
    dataIndex: Int,
    finalizedIndex: Int,
) {
    val villager = data.getCompound("VillagerData")
    if (villager.size == 0) return
    add(
        SetEntityDataPacket.Entry.VillagerData(
            index = dataIndex,
            typeId = VILLAGER_TYPE_IDS[villager.getString("type")] ?: 2,
            professionId = VILLAGER_PROFESSION_IDS[villager.getString("profession")] ?: 0,
            level = villager.getInt("level", 1),
        ),
    )
    // Loading explicit VillagerData also flips DATA_VILLAGER_DATA_FINALIZED in both entity classes.
    add(SetEntityDataPacket.Entry.Booleans(finalizedIndex, true))
}

internal data class GeneratedEquipment(
    val slot: SetEquipmentPacket.Type,
    val item: String,
    val count: Int,
)

internal fun generatedEntityEquipment(data: NbtCompound): List<GeneratedEquipment> {
    val equipment = data.getCompound("equipment")
    return buildList {
        EQUIPMENT_SLOTS.forEach { (name, slot) ->
            val stack = equipment.getCompound(name)
            val item = stack.getString("id")
            val count = stack.getInt("count")
            if (item.isNotEmpty() && count > 0) add(GeneratedEquipment(slot, item, count))
        }
    }
}

private val AGEABLE_TYPES = setOf("camel", "cat", "cow", "horse", "pig", "sheep", "villager")

private val ARMOR_STAND_POSES =
    listOf(
        "Head" to 16,
        "Body" to 17,
        "LeftArm" to 18,
        "RightArm" to 19,
        "LeftLeg" to 20,
        "RightLeg" to 21,
    )

private val EQUIPMENT_SLOTS =
    listOf(
        "mainhand" to SetEquipmentPacket.Type.MAINHAND,
        "offhand" to SetEquipmentPacket.Type.OFFHAND,
        "feet" to SetEquipmentPacket.Type.FEET,
        "legs" to SetEquipmentPacket.Type.LEGS,
        "chest" to SetEquipmentPacket.Type.CHEST,
        "head" to SetEquipmentPacket.Type.HEAD,
        "body" to SetEquipmentPacket.Type.BODY,
        "saddle" to SetEquipmentPacket.Type.SADDLE,
    )

private val CAT_VARIANT_IDS =
    mapOf(
        "minecraft:jellie" to 0,
        "minecraft:black" to 1,
        "minecraft:tabby" to 2,
        "minecraft:british_shorthair" to 3,
        "minecraft:all_black" to 4,
        "minecraft:ragdoll" to 5,
        "minecraft:white" to 6,
        "minecraft:siamese" to 7,
        "minecraft:calico" to 8,
        "minecraft:persian" to 9,
        "minecraft:red" to 10,
    )

private val VILLAGER_TYPE_IDS =
    mapOf(
        "minecraft:desert" to 0,
        "minecraft:jungle" to 1,
        "minecraft:plains" to 2,
        "minecraft:savanna" to 3,
        "minecraft:snow" to 4,
        "minecraft:swamp" to 5,
        "minecraft:taiga" to 6,
    )

private val VILLAGER_PROFESSION_IDS =
    mapOf(
        "minecraft:none" to 0,
        "minecraft:nitwit" to 11,
    )
