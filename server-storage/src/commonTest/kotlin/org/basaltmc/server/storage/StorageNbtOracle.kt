package org.basaltmc.server.storage

import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtIO
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.Snbt
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the two stored blobs and the vanilla player import. */
class StorageNbtOracle {
    private fun checksum(bytes: ByteArray): Long {
        var checksum = -3750763034362895579L
        for (byte in bytes) checksum = (checksum xor (byte.toLong() and 0xFF)) * 1099511628211L
        return checksum
    }

    private fun describe(inventory: StoredInventory): String =
        inventory.slots
            .withIndex()
            .joinToString(";") { (index, stack) ->
                if (stack == null) "" else "$index=${stack.id}x${stack.count}${Snbt.stringify(stack.components)}"
            }.replace(Regex(";+"), ";")

    private fun components(vararg pairs: Pair<String, String>): NbtCompound =
        NbtCompound().also { for ((key, value) in pairs) it.putString(key, value) }

    private fun inventory(): StoredInventory {
        val slots = arrayOfNulls<StoredItemStack>(StoredInventory.PLAYER_SLOTS)
        slots[0] = StoredItemStack("minecraft:stone", 64)
        slots[8] = StoredItemStack("minecraft:diamond_sword", 1, components("minecraft:custom_name" to "Biter"))
        // The last main slot, the four armour slots and the offhand: the ends of every range.
        slots[35] = StoredItemStack("minecraft:torch", 12)
        slots[36] = StoredItemStack("minecraft:iron_helmet", 1)
        slots[39] = StoredItemStack("minecraft:iron_boots", 1)
        slots[40] = StoredItemStack("minecraft:shield", 1)
        return StoredInventory(slots.toList())
    }

    @Test
    fun dump() {
        val full = inventory()
        val encoded = InventoryNbt.encode(full)
        val roundTrip = InventoryNbt.decode(encoded)
        println("ORACLE inventory bytes=${encoded.size} checksum=${checksum(encoded)} slots=${describe(roundTrip)}")

        val bare = InventoryNbt.encode(StoredInventory.empty(StoredPlayer.ENDER_CHEST_SLOTS))
        println(
            "ORACLE inventoryEmpty bytes=${bare.size} checksum=${checksum(bare)}" +
                " size=${InventoryNbt.decode(bare).slots.size} isEmpty=${InventoryNbt.decode(bare).isEmpty}",
        )

        val stats =
            StoredStats.EMPTY
                .with(StoredStats.DEATHS, 3)
                .with(StoredStats.WALK_ONE_CM, 12_345_678_901L)
                .with(StoredStats.mined("minecraft:deepslate"), 91)
                // Added then taken back to zero, which is dropped rather than written.
                .with(StoredStats.JUMP, 7)
                .with(StoredStats.JUMP, -7)
        val statsBytes = StatsNbt.encode(stats)!!
        val statsBack = StatsNbt.decode(statsBytes)
        println(
            "ORACLE stats bytes=${statsBytes.size} checksum=${checksum(statsBytes)}" +
                " counters=${statsBack.counters.entries.sortedBy { it.key }.joinToString(
                    ",",
                ) { "${it.key}=${it.value}" }}" +
                " deaths=${statsBack[StoredStats.DEATHS]} jump=${statsBack[StoredStats.JUMP]}" +
                " missing=${statsBack["minecraft:custom/minecraft:nothing"]}",
        )
        println("ORACLE statsEmpty encoded=${StatsNbt.encode(StoredStats.EMPTY)}")

        for (
        (name, bytes) in
        listOf(
            "notNbt" to byteArrayOf(9, 9, 9),
            "wrongVersion" to
                NbtIO.write(
                    NbtCompound().also {
                        it.putInt("Version", 99)
                        it.putInt("Size", 4)
                    },
                ),
            "badSlot" to
                NbtIO.write(
                    NbtCompound().also {
                        it.putInt("Version", 1)
                        it.putInt("Size", 4)
                        it["Slots"] =
                            NbtList(
                                NbtType.COMPOUND,
                                listOf(NbtCompound().also { entry -> entry.putInt("Slot", 9) }),
                            )
                    },
                ),
            // A slot exactly at the size, which is one past the last valid index.
            "slotAtSize" to
                NbtIO.write(
                    NbtCompound().also {
                        it.putInt("Version", 1)
                        it.putInt("Size", 4)
                        it["Slots"] =
                            NbtList(
                                NbtType.COMPOUND,
                                listOf(NbtCompound().also { entry -> entry.putInt("Slot", 4) }),
                            )
                    },
                ),
            "badCount" to
                NbtIO.write(
                    NbtCompound().also {
                        it.putInt("Version", 1)
                        it.putInt("Size", 4)
                        it["Slots"] =
                            NbtList(
                                NbtType.COMPOUND,
                                listOf(
                                    NbtCompound().also { entry ->
                                        entry.putInt("Slot", 1)
                                        entry.putInt("count", 0)
                                    },
                                ),
                            )
                    },
                ),
        )
        ) {
            val message =
                try {
                    InventoryNbt.decode(bytes)
                    "accepted"
                } catch (failure: StorageFormatException) {
                    failure.message ?: "unknown"
                }
            println("ORACLE inventory$name refused=${message.replace(' ', '_')}")
        }

        // A vanilla playerdata file, built here rather than taken from a real world: nothing of
        // Mojang's ships in this repository.
        val vanilla = NbtCompound()
        vanilla["Pos"] =
            NbtList(NbtType.DOUBLE, listOf(NbtDouble(120.5), NbtDouble(71.0), NbtDouble(-340.25)))
        vanilla["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(-90.5f), NbtFloat(12.25f)))
        vanilla.putFloat("Health", 17.5f)
        vanilla.putInt("foodLevel", 14)
        vanilla.putFloat("foodSaturationLevel", 2.5f)
        vanilla.putFloat("foodExhaustionLevel", 0.75f)
        vanilla.putInt("XpLevel", 30)
        vanilla.putFloat("XpP", 0.4f)
        vanilla.putInt("XpTotal", 1395)
        vanilla.putInt("playerGameType", 1)
        vanilla["Inventory"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().also {
                        it.putByte("Slot", 0)
                        it.putString("id", "minecraft:dirt")
                        it.putInt("count", 32)
                    },
                    // Armour and the offhand, at vanilla's own out-of-range slot numbers.
                    NbtCompound().also {
                        it.putByte("Slot", 100)
                        it.putString("id", "minecraft:leather_boots")
                        it.putInt("count", 1)
                    },
                    NbtCompound().also {
                        it.putByte("Slot", 103)
                        it.putString("id", "minecraft:iron_helmet")
                        it.putInt("count", 1)
                    },
                    NbtCompound().also {
                        it.putByte("Slot", -106)
                        it.putString("id", "minecraft:shield")
                        it.putInt("count", 1)
                    },
                    // Two slots just past the ends of vanilla's own two ranges, which belong to
                    // neither and are skipped rather than folded into the armour or the offhand.
                    NbtCompound().also {
                        it.putByte("Slot", 36)
                        it.putString("id", "minecraft:bedrock")
                        it.putInt("count", 1)
                    },
                    NbtCompound().also {
                        it.putByte("Slot", 104)
                        it.putString("id", "minecraft:barrier")
                        it.putInt("count", 1)
                    },
                    // A slot vanilla never uses and an empty stack: both are skipped.
                    NbtCompound().also {
                        it.putByte("Slot", 50)
                        it.putString("id", "minecraft:stone")
                        it.putInt("count", 1)
                    },
                    NbtCompound().also {
                        it.putByte("Slot", 5)
                        it.putString("id", "minecraft:air")
                        it.putInt("count", 0)
                    },
                ),
            )
        vanilla["EnderItems"] =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().also {
                        it.putByte("Slot", 26)
                        it.putString("id", "minecraft:ender_pearl")
                        it.putInt("count", 16)
                    },
                ),
            )
        val plain = NbtIO.write(vanilla)
        val imported =
            VanillaPlayerData.read(
                uuid = "b1e0c0de-0000-4000-8000-00000000abcd",
                name = "Steve",
                gzippedBytes = plain,
                decompress = { it },
                nowEpochSeconds = 1_700_000_000L,
                worldName = "world",
            )
        println(
            "ORACLE imported pos=${imported.x}/${imported.y}/${imported.z}" +
                " rot=${imported.yaw}/${imported.pitch} health=${imported.health}" +
                " food=${imported.food}/${imported.saturation}/${imported.exhaustion}" +
                " xp=${imported.xpLevel}/${imported.xpProgress}/${imported.xpTotal}" +
                " mode=${imported.gamemode} world=${imported.world} play=${imported.playTimeTicks}" +
                " seen=${imported.lastSeenEpochSeconds}/${imported.firstSeenEpochSeconds}" +
                " inv=${describe(imported.inventory)} ender=${describe(imported.enderChest)}",
        )
        println(
            "ORACLE gamemodes ${(-1..4).joinToString(",") { StoredGamemode.byId(it).name }}",
        )
    }
}
