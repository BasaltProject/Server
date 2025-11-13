package org.basaltmc.server.assets

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the parts of the asset layer that touch no network. */
class AssetsOracle {
    private val eulaFiles =
        listOf(
            "accepted" to "eula = true",
            "acceptedTight" to "eula=true",
            "acceptedShouty" to "EULA = TRUE",
            "acceptedSpaced" to "  eula   =   true  ",
            "acceptedAfterComments" to "# a comment\n# eula = true\neula = true\n",
            "rejected" to "eula = false",
            // Every line here says true inside a comment, which is not agreement.
            "commentedOut" to "#eula = true\n# eula=true\n   #eula = true",
            "empty" to "",
            "unrelated" to "eula-accepted = true\nseula = true\neula = truex",
            "vanillaShape" to
                "#By changing the setting below to TRUE you are indicating your agreement to our EULA\n" +
                "#Mon Jan 01 00:00:00 UTC 2024\neula=true\n",
        )

    private val metadata =
        mapOf(
            // The shape piston-meta answers with: the size is a number.
            "piston" to
                """{"downloads":{
                     "client":{"url":"https://piston-data/client.jar","sha1":"aaaa1111","size":26000000},
                     "server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":52000000}}}""",
            // The shape this project writes when it caches one: the size is a string.
            "cached" to
                """{"downloads":{
                     "client":{"url":"https://piston-data/client.jar","sha1":"aaaa1111","size":"26000000"},
                     "server":{"url":"https://piston-data/server.jar","sha1":"bbbb2222","size":"52000000"}}}""",
        )

    @Test
    fun dump() {
        for ((name, content) in eulaFiles) {
            println("ORACLE eula$name accepted=${EulaFile.isAccepted(content)}")
        }
        println(
            "ORACLE eulaPaths file=${EulaFile.PATH} legacy=${EulaFile.LEGACY_PATH} exit=${EulaFile.REJECTED_EXIT_CODE}",
        )

        for ((name, json) in metadata) {
            val parsed = VersionManifest().parseMetadata("26.2", Json.parseToJsonElement(json) as JsonObject)
            println(
                "ORACLE metadata$name id=${parsed.id}" +
                    " client=${parsed.client.url}/${parsed.client.sha1}/${parsed.client.size}" +
                    " server=${parsed.server.url}/${parsed.server.sha1}/${parsed.server.size}" +
                    " json=${parsed.toJson()}",
            )
        }

        // A warm cache: the index alone, with no jar anywhere, which is what a second boot reads.
        val dataRoot = "build/test-data/oracle_assets"
        FileIO.createDirectories("$dataRoot/worldgen/noise")
        val index =
            listOf(
                "data/minecraft/worldgen/noise/continentalness.json",
                "data/minecraft/worldgen/noise/erosion.json",
                "data/minecraft/worldgen/biome/plains.json",
                "data/minecraft/loot_table/blocks/stone.json",
                "data/basalt/worldgen/noise/custom.json",
                "data/minecraft/tags/block/logs.json",
                "assets/minecraft/lang/en_us.json",
                // Neither of these is under a namespace directory, so no id comes out of them.
                "data/pack.mcmeta",
                "data/minecraft/worldgen/noise/not_json.txt",
            )
        FileIO.writeText("$dataRoot/.index", index.joinToString("\n"))
        FileIO.writeText("$dataRoot/.unpacked", "26.2 v2-lang")
        FileIO.writeText("$dataRoot/worldgen/noise/continentalness.json", "{\"firstOctave\":-9}")
        val assets = ServerJarAssets("26.2", dataRoot) { error("a warm cache must not resolve its source") }
        println(
            "ORACLE assets entries=${assets.entriesUnder("data/minecraft/worldgen/noise/")}" +
                " ids=${assets.dataIds("worldgen/noise", ".json").map { it.toString() }.sorted()}" +
                " biomes=${assets.dataIds("worldgen/biome", ".json").map { it.toString() }}" +
                " read=${assets.readText("data/minecraft/worldgen/noise/continentalness.json")}" +
                " missing=${assets.readEntry("data/minecraft/worldgen/noise/erosion.json")}",
        )
        for (suffix in listOf("/.index", "/.unpacked", "/worldgen/noise/continentalness.json")) {
            FileIO.delete("$dataRoot$suffix")
        }
    }
}
