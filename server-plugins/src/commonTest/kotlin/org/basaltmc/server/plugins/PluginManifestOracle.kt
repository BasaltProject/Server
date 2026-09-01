package org.basaltmc.server.plugins

import kotlin.test.Test

/** The frozen Kotlin as the oracle for `plugin.toml` and the version comparison behind it. */
class PluginManifestOracle {
    private val manifests =
        mapOf(
            // Everything a manifest can say, in one file.
            "full" to
                """
                id = "arena"
                name = "Arena"
                version = "2.1.0"
                authors = ["Whiron", "Ana"]
                api = 1
                main = "src/arena.lua"

                [depends]
                economy = ">=1.4"
                worldedit = "*"
                # A requirement that is not a string at all, which means any version rather than none.
                permissions = 3

                [soft-depends]
                chat = "<2.0"

                [capabilities]
                storage = true
                players_data = true
                commands = true
                packets = true
                files = ["maps/", "schematics/"]
                network = ["https://arena.example/"]

                [exports]
                modules = ["api", "util"]
                services = ["arena"]

                [resources]
                private = ["templates/", "seed.json"]
                """.trimIndent(),
            // The smallest file that loads: everything else takes its default.
            "minimal" to
                """
                id = "hello"
                version = "1.0.0"
                api = 1
                """.trimIndent(),
            // A module list that opens everything.
            "open" to
                """
                id = "toolkit"
                version = "0.1"
                api = 1

                [exports]
                modules = ["*"]
                """.trimIndent(),
        )

    private fun describe(manifest: PluginManifest): String =
        "${manifest.id}/${manifest.name}/${manifest.version}/${manifest.api}/${manifest.main}" +
            "/authors=${manifest.authors}" +
            "/deps=${manifest.dependencies.map { "${it.id}${it.requirement}${if (it.hard) "!" else "?"}" }}" +
            "/hard=${manifest.hardDependencies.map { it.id }}" +
            "/caps=${manifest.capabilities.storage},${manifest.capabilities.playersData}," +
            "${manifest.capabilities.commands},${manifest.capabilities.packets}," +
            "${manifest.capabilities.files},${manifest.capabilities.network}" +
            "/asks=${manifest.capabilities.asks().map { it.permissionName }.sorted()}" +
            "/exports=${manifest.exports.modules},${manifest.exports.services}," +
            "${manifest.exports.opensEverything}" +
            "/private=${manifest.privateResources}"

    @Test
    fun dump() {
        for ((name, text) in manifests) {
            println("ORACLE manifest$name ${describe(PluginManifest.parse(text))}")
        }

        for (
        (name, text) in
        listOf(
            "noId" to "version = \"1\"\napi = 1",
            "badId" to "id = \"Arena\"\nversion = \"1\"\napi = 1",
            "digitId" to "id = \"1arena\"\nversion = \"1\"\napi = 1",
            "dashId" to "id = \"my-plugin\"\nversion = \"1\"\napi = 1",
            "noVersion" to "id = \"arena\"\napi = 1",
            "noApi" to "id = \"arena\"\nversion = \"1\"",
            "notToml" to "id = \nversion",
        )
        ) {
            val message =
                try {
                    PluginManifest.parse(text)
                    "accepted"
                } catch (failure: ManifestException) {
                    failure.message ?: "unknown"
                }
            println("ORACLE refused$name ${message.replace(' ', '_')}")
        }

        val versions =
            listOf(
                "1.0.0" to "1.0.0",
                "1.0" to "1.0.0",
                "1.2.3" to "1.2.4",
                "1.10.0" to "1.9.0",
                "2.0" to "1.99.99",
                "1.0.0-beta" to "1.0.0",
                "1.0.0-2" to "1.0.0-10",
                "" to "0",
                "1.0.0" to "1.0.0.0",
            )
        println(
            "ORACLE versions ${versions.joinToString(",") { (a, b) -> "$a<=>$b=${compareVersions(a, b)}" }}",
        )

        val requirements =
            listOf("*", "1.2.3", ">=1.2", "> 1.2", "<=2.0", "<2.0", "  1.2.3  ")
        val candidates = listOf("1.2.3", "1.2.0", "2.0.0", "0.9")
        println(
            "ORACLE requirements " +
                requirements.joinToString(";") { requirement ->
                    val dependency = PluginDependency("x", requirement, true)
                    "$requirement=" + candidates.joinToString(",") { dependency.accepts(it).toString() }
                },
        )

        val exports = PluginExports(modules = listOf("api", "util"))
        println(
            "ORACLE exports api=${exports.exports("api")} other=${exports.exports("secret")}" +
                " open=${PluginExports(modules = listOf("*")).exports("anything")}" +
                " none=${PluginExports.NONE.exports("api")}",
        )
        println(
            "ORACLE permissions ${PluginPermission.entries.map { it.permissionName }}" +
                " of=${PluginPermission.of("packets")} unknown=${PluginPermission.of("nope")}",
        )
        println("ORACLE constants api=${PluginManifest.API} file=${PluginManifest.FILE_NAME}")
    }
}
