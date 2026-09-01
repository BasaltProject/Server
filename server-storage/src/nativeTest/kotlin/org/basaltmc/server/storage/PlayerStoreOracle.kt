package org.basaltmc.server.storage

import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.AfterTest
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the player database: the same scenario, in the same order. */
class PlayerStoreOracle {
    private val path = "build/test-data/oracle_players.db"

    @AfterTest
    fun cleanUp() {
        for (suffix in listOf("", "-wal", "-shm")) FileIO.delete("$path$suffix")
    }

    private fun player(
        uuid: String,
        name: String,
        world: String = "overworld",
        gamemode: StoredGamemode = StoredGamemode.SURVIVAL,
        lastSeen: Long = 1_700_000_000L,
        playTime: Long = 72_000L,
        group: String? = null,
        inventory: StoredInventory = StoredInventory.empty(),
        tags: Set<String> = emptySet(),
        stats: StoredStats = StoredStats.EMPTY,
    ) = StoredPlayer(
        uuid = uuid,
        name = name,
        lastSeenEpochSeconds = lastSeen,
        firstSeenEpochSeconds = 1_600_000_000L,
        playTimeTicks = playTime,
        world = world,
        x = 8.5,
        y = 63.0,
        z = -12.25,
        yaw = 90f,
        pitch = -12.5f,
        health = 17.5f,
        food = 18,
        saturation = 3.5f,
        exhaustion = 1.25f,
        xpLevel = 7,
        xpProgress = 0.4f,
        xpTotal = 62,
        gamemode = gamemode,
        group = group,
        inventory = inventory,
        tags = tags,
        stats = stats,
    )

    private fun describe(player: StoredPlayer?): String {
        if (player == null) return "-"
        val slots =
            player.inventory.slots
                .withIndex()
                .filter { it.value != null }
                .joinToString("|") { "${it.index}:${it.value!!.id}x${it.value!!.count}" }
        return "${player.uuid}/${player.name}/${player.world}/${player.gamemode}/${player.x}/${player.yaw}/" +
            "${player.health}/${player.food}/${player.xpLevel}/${player.playTimeTicks}/${player.group}/" +
            "${player.tags.sorted()}/${player.stats.counters.entries.sortedBy { it.key }
                .joinToString(",") { "${it.key}=${it.value}" }}/$slots/" +
            "${player.enderChest.slots.size}"
    }

    private fun summaries(found: List<PlayerSummary>): String = found.joinToString(",") { "${it.name}=${it.value}" }

    @Test
    fun dump() {
        for (suffix in listOf("", "-wal", "-shm")) FileIO.delete("$path$suffix")
        PlayerStore.open(path).use { store ->
            val players = store.players
            val sword =
                StoredItemStack(
                    "minecraft:diamond_sword",
                    1,
                    NbtCompound().also { it.putString("minecraft:custom_name", "Biter") },
                )
            val inventory =
                StoredInventory.empty().let {
                    val slots = it.slots.toMutableList()
                    slots[0] = StoredItemStack("minecraft:stone", 64)
                    slots[8] = sword
                    StoredInventory(slots)
                }

            players.save(
                player(
                    "11111111-2222-3333-4444-555555555555",
                    "Whiron",
                    inventory = inventory,
                    tags = setOf("builder", "alpha"),
                    stats = StoredStats.EMPTY.with(StoredStats.DEATHS, 4).with(StoredStats.JUMP, 900),
                ),
            )
            players.save(player("22222222-0000-0000-0000-000000000000", "Ana", world = "nether", playTime = 10L))
            players.save(
                player(
                    "33333333-0000-0000-0000-000000000000",
                    "Bo",
                    gamemode = StoredGamemode.CREATIVE,
                    lastSeen = 1_500_000_000L,
                    playTime = 500_000L,
                ),
            )
            // The same name under a second UUID, seen more recently: the name lookup takes this one.
            players.save(
                player(
                    "44444444-0000-0000-0000-000000000000",
                    "whiron",
                    world = "the_end",
                    lastSeen = 1_800_000_000L,
                ),
            )

            println("ORACLE load ${describe(players.load("11111111-2222-3333-4444-555555555555"))}")
            println("ORACLE loadByName ${describe(players.loadByName("WHIRON"))}")
            println("ORACLE loadMissing ${describe(players.load("nobody"))}")
            // The one player who is not in survival, so the mode really comes off the row.
            println("ORACLE loadCreative ${describe(players.load("33333333-0000-0000-0000-000000000000"))}")
            println(
                "ORACLE counts all=${players.count()} exists=${players.exists("nobody")}" +
                    " modes=${players.countByGamemode().entries.sortedBy { it.key.name }
                        .joinToString(",") { "${it.key}=${it.value}" }}",
            )
            println(
                "ORACLE queries stale=${summaries(players.findLastSeenBefore(1_700_000_000L))}" +
                    " world=${summaries(players.findInWorld("nether"))}" +
                    " played=${summaries(players.findWithPlayTimeAtLeast(72_000L))}",
            )

            println(
                "ORACLE groups set=${players.setGroup("11111111-2222-3333-4444-555555555555", "admin")}" +
                    " missing=${players.setGroup("nobody", "admin")}" +
                    " members=${summaries(players.findInGroup("admin"))}",
            )
            players.setLastIp("11111111-2222-3333-4444-555555555555", "203.0.113.7")
            players.setLastIp("22222222-0000-0000-0000-000000000000", "203.0.113.7")
            println(
                "ORACLE address known=${players.findNameByAddress("203.0.113.7")}" +
                    " unknown=${players.findNameByAddress("198.51.100.1")}",
            )

            // A save that overwrites: the group survives it, because the upsert carries it.
            players.saveAll(
                listOf(
                    player("22222222-0000-0000-0000-000000000000", "Ana", world = "overworld", playTime = 99L),
                    player("55555555-0000-0000-0000-000000000000", "Cy"),
                ),
            )
            println(
                "ORACLE afterBatch all=${players.count()} ana=${describe(
                    players.load("22222222-0000-0000-0000-000000000000"),
                )}",
            )
            players.delete("55555555-0000-0000-0000-000000000000")
            println("ORACLE afterDelete all=${players.count()}")

            val access = store.access
            access.ban(
                BanRecord(
                    "11111111-2222-3333-4444-555555555555",
                    BanKind.PLAYER,
                    "Whiron",
                    "griefing",
                    "console",
                    1_700_000_100L,
                    null,
                ),
            )
            access.ban(BanRecord("203.0.113.9", BanKind.IP, null, "spam", "Ana", 1_700_000_200L, 1_800_000_000L))
            // Created later but sorting earlier by subject, so the order is the created one.
            access.ban(BanRecord("198.51.100.4", BanKind.IP, null, "bot", "Bo", 1_700_000_300L, null))
            val ban = access.banOf("11111111-2222-3333-4444-555555555555", BanKind.PLAYER)
            println(
                "ORACLE bans player=${ban?.reason}/${ban?.source}/${ban?.name}/${ban?.expiresAtEpochSeconds}" +
                    " byKind=${access.bans(
                        BanKind.IP,
                    ).joinToString(",") { "${it.subject}:${it.expiresAtEpochSeconds}" }}" +
                    " wrongKind=${access.banOf("11111111-2222-3333-4444-555555555555", BanKind.IP)}" +
                    " pardon=${access.pardon("203.0.113.9", BanKind.IP)}" +
                    " pardonAgain=${access.pardon("203.0.113.9", BanKind.IP)}" +
                    " storedKinds=${BanKind.PLAYER.storedName}/${BanKind.IP.storedName}",
            )

            access.addToWhitelist(WhitelistEntry("11111111-2222-3333-4444-555555555555", "Whiron", 1_700_000_300L))
            access.addToWhitelist(WhitelistEntry("22222222-0000-0000-0000-000000000000", "Ana", 1_700_000_400L))
            println(
                "ORACLE whitelist list=${access.whitelist().joinToString(
                    ",",
                ) { "${it.name}@${it.addedAtEpochSeconds}" }}" +
                    " is=${access.isWhitelisted("11111111-2222-3333-4444-555555555555")}" +
                    " isNot=${access.isWhitelisted("nobody")}" +
                    " byName=${access.whitelistedByName("ANA")?.uuid}" +
                    " remove=${access.removeFromWhitelist("22222222-0000-0000-0000-000000000000")}" +
                    " removeAgain=${access.removeFromWhitelist("22222222-0000-0000-0000-000000000000")}",
            )

            access.setSpawnPoint(
                StoredSpawnPoint("11111111-2222-3333-4444-555555555555", "overworld", 100.5, 64.0, -200.25, 45f),
            )
            val spawn = access.spawnPointOf("11111111-2222-3333-4444-555555555555")
            println(
                "ORACLE spawn ${spawn?.world}/${spawn?.x}/${spawn?.y}/${spawn?.z}/${spawn?.yaw}" +
                    " missing=${access.spawnPointOf("nobody")}",
            )
            access.clearSpawnPoint("11111111-2222-3333-4444-555555555555")
            println("ORACLE spawnCleared ${access.spawnPointOf("11111111-2222-3333-4444-555555555555")}")

            val progress = store.progress
            progress.grantCriterion("uuid-a", "minecraft:story/root", "crafting_table")
            progress.grantCriterion("uuid-a", "minecraft:story/root", "planks")
            // Granting twice is one row, not two.
            progress.grantCriterion("uuid-a", "minecraft:story/root", "planks")
            progress.grantCriterion("uuid-a", "minecraft:story/mine_stone", "get_stone")
            progress.grantCriterion("uuid-b", "minecraft:story/root", "crafting_table")
            progress.learnRecipe("uuid-a", "minecraft:torch")
            progress.learnRecipe("uuid-a", "minecraft:stick")
            progress.learnRecipe("uuid-a", "minecraft:stick")
            val loaded = progress.load("uuid-a")
            println(
                "ORACLE progress criteria=${loaded.criteria.entries.sortedBy { it.key }
                    .joinToString(";") { "${it.key}=${it.value.sorted()}" }}" +
                    " recipes=${loaded.recipes.sorted()}",
            )
            progress.revokeCriterion("uuid-a", "minecraft:story/root", "planks")
            progress.revokeAdvancement("uuid-a", "minecraft:story/mine_stone")
            progress.forgetRecipe("uuid-a", "minecraft:stick")
            val after = progress.load("uuid-a")
            println(
                "ORACLE progressAfter criteria=${after.criteria.entries.sortedBy { it.key }
                    .joinToString(";") { "${it.key}=${it.value.sorted()}" }}" +
                    " recipes=${after.recipes.sorted()} other=${progress.load("uuid-b").criteria.keys.sorted()}",
            )

            val data = store.pluginData
            data.set("shop", "uuid-a", "balance", "125.5")
            data.set("shop", "uuid-b", "balance", "980")
            data.set("shop", "uuid-c", "balance", "7")
            data.set("shop", "uuid-a", "title", "Merchant")
            // A different plugin, same key and player: neither can see the other's row.
            data.set("duels", "uuid-a", "balance", "3")
            println(
                "ORACLE pluginData get=${data.get("shop", "uuid-a", "balance")}" +
                    " other=${data.get("duels", "uuid-a", "balance")}" +
                    " missing=${data.get("shop", "uuid-a", "nothing")}" +
                    " all=${data.all("shop", "uuid-a").entries.sortedBy { it.key }
                        .joinToString(",") { "${it.key}=${it.value}" }}" +
                    " top=${data.top("shop", "balance", 2).joinToString(",") { "${it.first}=${it.second}" }}" +
                    " namespaces=${data.namespaces().entries.sortedBy { it.key }
                        .joinToString(",") { "${it.key}=${it.value}" }}",
            )
            data.set("shop", "uuid-a", "title", null)
            data.purge("duels")
            println(
                "ORACLE pluginDataAfter title=${data.get("shop", "uuid-a", "title")}" +
                    " namespaces=${data.namespaces().entries.sortedBy { it.key }
                        .joinToString(",") { "${it.key}=${it.value}" }}",
            )
        }

        // Reopening reads back what the checkpoint folded in.
        PlayerStore.open(path).use { store ->
            println(
                "ORACLE reopened all=${store.players.count()}" +
                    " whiron=${describe(store.players.load("11111111-2222-3333-4444-555555555555"))}",
            )
        }
    }
}
