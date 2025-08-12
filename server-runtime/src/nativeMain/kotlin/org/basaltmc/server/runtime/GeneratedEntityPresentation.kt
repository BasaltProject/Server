package org.basaltmc.server.runtime

import org.basaltmc.server.runtime.entity.AgeableMobs
import org.basaltmc.server.runtime.entity.CubeMobs
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket

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
        if ("Air" in data && data.getShort("Air").toInt() != DEFAULT_AIR_SUPPLY) {
            add(SetEntityDataPacket.Entry.Ints(AIR_SUPPLY_DATA_INDEX, data.getShort("Air").toInt()))
        }

        if ("Health" in data) add(SetEntityDataPacket.Entry.Floats(9, data.getFloat("Health")))

        if (Mobs.isMob(Identifier.vanilla(type))) {
            var mobFlags = 0
            if (data.getBoolean("NoAI")) mobFlags = mobFlags or 1
            if (data.getBoolean("LeftHanded")) mobFlags = mobFlags or 2
            if (mobFlags != 0) add(SetEntityDataPacket.Entry.Bytes(MOB_FLAGS_DATA_INDEX, mobFlags.toByte()))
        }

        if (AgeableMobs.isAgeable(type) && data.getInt("Age") < 0) {
            add(SetEntityDataPacket.Entry.Booleans(AgeableMobs.BABY_DATA_INDEX, true))
        }
        if (CubeMobs.isCube(type) && data.getInt("Size") != 0) {
            add(SetEntityDataPacket.Entry.Ints(CUBE_SIZE_DATA_INDEX, data.getInt("Size") + 1))
        }

        when (type) {
            "armor_stand" -> addArmorStandMetadata(data)
            "axolotl" ->
                data.getInt("Variant").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(17, it))
                }
            "cat" -> addCatMetadata(data)
            "chicken" -> {
                addRegistryMetadata(
                    data,
                    "variant",
                    18,
                    CHICKEN_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.CHICKEN_VARIANT,
                )
                addRegistryMetadata(
                    data,
                    "sound_variant",
                    19,
                    CHICKEN_SOUND_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.CHICKEN_SOUND_VARIANT,
                )
            }
            "cow" -> {
                addRegistryMetadata(
                    data,
                    "variant",
                    18,
                    COW_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.COW_VARIANT,
                )
                addRegistryMetadata(
                    data,
                    "sound_variant",
                    19,
                    COW_SOUND_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.COW_SOUND_VARIANT,
                )
            }
            "fox" ->
                if (data.getString("Type") == "snow") {
                    add(SetEntityDataPacket.Entry.Ints(19, 1))
                }
            "frog" ->
                addRegistryMetadata(
                    data,
                    "variant",
                    18,
                    FROG_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.FROG_VARIANT,
                )
            "goat" -> {
                if (data.getBoolean("IsScreamingGoat")) {
                    add(SetEntityDataPacket.Entry.Booleans(18, true))
                }
                if (!data.getBoolean("HasLeftHorn", true)) {
                    add(SetEntityDataPacket.Entry.Booleans(19, false))
                }
                if (!data.getBoolean("HasRightHorn", true)) {
                    add(SetEntityDataPacket.Entry.Booleans(20, false))
                }
            }
            "horse" ->
                data.getInt("Variant").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(19, it))
                }
            "llama", "trader_llama" -> {
                data.getInt("Strength").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(20, it))
                }
                data.getInt("Variant").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(21, it))
                }
            }
            "parrot" ->
                data.getInt("Variant").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(20, it))
                }
            "panda" -> {
                PANDA_GENE_IDS[data.getString("MainGene")]?.takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Bytes(19, it.toByte()))
                }
                PANDA_GENE_IDS[data.getString("HiddenGene")]?.takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Bytes(20, it.toByte()))
                }
            }
            "pig" -> {
                addRegistryMetadata(
                    data,
                    "variant",
                    19,
                    PIG_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.PIG_VARIANT,
                )
                addRegistryMetadata(
                    data,
                    "sound_variant",
                    20,
                    PIG_SOUND_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.PIG_SOUND_VARIANT,
                )
            }
            "rabbit" ->
                data.getInt("RabbitType").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(18, it))
                }
            "salmon" -> {
                val variant = SALMON_VARIANTS[data.getString("type")] ?: SALMON_MEDIUM
                if (variant != SALMON_MEDIUM) add(SetEntityDataPacket.Entry.Ints(17, variant))
            }
            "sheep" -> {
                val wool = (data.getByte("Color").toInt() and 0x0F) or if (data.getBoolean("Sheared")) 0x10 else 0
                if (wool != 0) add(SetEntityDataPacket.Entry.Bytes(18, wool.toByte()))
            }
            "tropical_fish" ->
                data.getInt("Variant").takeIf { it != 0 }?.let {
                    add(SetEntityDataPacket.Entry.Ints(17, it))
                }
            "villager" -> addVillagerMetadata(data, dataIndex = 19, finalizedIndex = 20)
            "zombie_villager" -> {
                if (data.getBoolean("IsBaby")) add(SetEntityDataPacket.Entry.Booleans(16, true))
                addVillagerMetadata(data, dataIndex = 20, finalizedIndex = 21)
            }
            "wolf" -> {
                addRegistryMetadata(
                    data,
                    "variant",
                    23,
                    WOLF_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.WOLF_VARIANT,
                )
                addRegistryMetadata(
                    data,
                    "sound_variant",
                    24,
                    WOLF_SOUND_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.WOLF_SOUND_VARIANT,
                )
            }
            "zombie_nautilus" ->
                addRegistryMetadata(
                    data,
                    "variant",
                    21,
                    ZOMBIE_NAUTILUS_VARIANT_IDS,
                    SetEntityDataPacket.RegistryHolder.ZOMBIE_NAUTILUS_VARIANT,
                )
        }
    }

private fun MutableList<SetEntityDataPacket.Entry>.addRegistryMetadata(
    data: NbtCompound,
    key: String,
    index: Int,
    ids: Map<String, Int>,
    registry: SetEntityDataPacket.RegistryHolder,
) {
    val id = ids[data.getString(key)] ?: return
    if (id != 0) add(SetEntityDataPacket.Entry.RegistryId(index, id, registry))
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
    // Black is the defineSynchedData default, so vanilla's initial packet never carries it.
    if (variant != null && variant != CAT_VARIANT_BLACK) add(SetEntityDataPacket.Entry.RegistryId(20, variant))
    val collarColor = data.getByte("CollarColor", 14).toInt() and 0xFF
    if (collarColor != 14) add(SetEntityDataPacket.Entry.Ints(23, collarColor))
    addRegistryMetadata(
        data,
        "sound_variant",
        24,
        CAT_SOUND_VARIANT_IDS,
        SetEntityDataPacket.RegistryHolder.CAT_SOUND_VARIANT,
    )
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

/** The live synchronized fields this server mutates after an entity has spawned. */
internal data class RuntimeEntityMetadata(
    val airSupply: Int,
    val baby: Boolean?,
    val cubeSize: Int?,
) {
    /** Initial non-default fields, or every field whose live value changed since [previous]. */
    fun changesFrom(previous: RuntimeEntityMetadata?): List<SetEntityDataPacket.Entry> =
        buildList {
            if (previous?.airSupply != airSupply && (previous != null || airSupply != DEFAULT_AIR_SUPPLY)) {
                add(SetEntityDataPacket.Entry.Ints(AIR_SUPPLY_DATA_INDEX, airSupply))
            }
            if (baby != null && (previous?.baby != baby) && (previous != null || baby)) {
                add(SetEntityDataPacket.Entry.Booleans(AgeableMobs.BABY_DATA_INDEX, baby))
            }
            if (
                cubeSize != null &&
                previous?.cubeSize != cubeSize &&
                (previous != null || cubeSize != CubeMobs.DEFAULT_SIZE)
            ) {
                add(SetEntityDataPacket.Entry.Ints(CUBE_SIZE_DATA_INDEX, cubeSize))
            }
        }
}

internal fun runtimeEntityMetadata(entity: ServerEntity): RuntimeEntityMetadata =
    RuntimeEntityMetadata(
        airSupply = entity.airSupply,
        baby = (entity.age < 0).takeIf { AgeableMobs.isAgeable(entity.type) },
        cubeSize = entity.cubeSize.takeIf { CubeMobs.isCube(entity.type) },
    )

/** `AbstractCubeMob.ID_SIZE`: two `AgeableMob` fields follow the common index 16. */
private const val CUBE_SIZE_DATA_INDEX = 18
private const val MOB_FLAGS_DATA_INDEX = 15
private const val AIR_SUPPLY_DATA_INDEX = 1
private const val DEFAULT_AIR_SUPPLY = 300

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

/**
 * Indices into the `minecraft:cat_variant` registry exactly as this server syncs it (see
 * `VanillaConfigurationRegistries` and `VanillaRegistryData.catVariants`): the client
 * resolves the id against that packet's entry order, nothing else.
 */
private const val CAT_VARIANT_BLACK = 1

private val CAT_VARIANT_IDS =
    mapOf(
        "minecraft:tabby" to 0,
        "minecraft:black" to CAT_VARIANT_BLACK,
        "minecraft:red" to 2,
        "minecraft:siamese" to 3,
        "minecraft:british_shorthair" to 4,
        "minecraft:calico" to 5,
        "minecraft:persian" to 6,
        "minecraft:ragdoll" to 7,
        "minecraft:white" to 8,
        "minecraft:jellie" to 9,
        "minecraft:all_black" to 10,
    )
private val CAT_SOUND_VARIANT_IDS = registryIds("classic", "royal")

private val CHICKEN_VARIANT_IDS = registryIds("temperate", "warm", "cold")
private val CHICKEN_SOUND_VARIANT_IDS = registryIds("classic", "picky")
private val COW_VARIANT_IDS = registryIds("temperate", "warm", "cold")
private val COW_SOUND_VARIANT_IDS = registryIds("classic", "moody")
private val FROG_VARIANT_IDS = registryIds("temperate", "warm", "cold")
private val PIG_VARIANT_IDS = registryIds("temperate", "warm", "cold")
private val PIG_SOUND_VARIANT_IDS = registryIds("classic", "big", "mini")
private val WOLF_VARIANT_IDS =
    registryIds("pale", "spotted", "snowy", "black", "ashen", "rusty", "woods", "chestnut", "striped")
private val WOLF_SOUND_VARIANT_IDS = registryIds("classic", "puglin", "sad", "angry", "grumpy", "big", "cute")
private val ZOMBIE_NAUTILUS_VARIANT_IDS = registryIds("temperate", "warm")

private fun registryIds(vararg paths: String): Map<String, Int> =
    paths.mapIndexed { index, path -> "minecraft:$path" to index }.toMap()

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

private const val SALMON_MEDIUM = 1
private val SALMON_VARIANTS = mapOf("small" to 0, "medium" to SALMON_MEDIUM, "large" to 2)
private val PANDA_GENE_IDS =
    mapOf(
        "normal" to 0,
        "lazy" to 1,
        "worried" to 2,
        "playful" to 3,
        "brown" to 4,
        "weak" to 5,
        "aggressive" to 6,
    )
