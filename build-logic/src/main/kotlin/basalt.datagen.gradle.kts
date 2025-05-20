import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import java.io.File

// This module never ships in the server binary (see the module's own root package doc):
// it only runs `java` against a locally supplied jar at build time, so it needs no Kotlin/Native
// target of its own, just the `base` lifecycle tasks (`build`, `check`, `clean`) for consistency
// with every other subproject.
plugins {
    base
}

tasks.register("generate") {
    group = "basalt"
    description = "Regenerates basalt-registry's real vanilla ID tables from the official server jar's own data generator."

    doLast {
        val jarPath =
            project.findProperty("mc.jar") as String?
                ?: error("Missing -Pmc.jar=/path/to/server.jar (the bundler jar, e.g. server-2.jar)")
        val jarFile = File(jarPath)
        check(jarFile.isFile) { "No such file: $jarPath" }

        val workDir = project.layout.buildDirectory.dir("datagen").get().asFile
        workDir.mkdirs()

        // The bundler needs its own classpath unpacked first, so this must run against the full
        // bundler jar (e.g. `server-2.jar`), not the nested `META-INF/versions/*/server-*.jar` alone.
        val process =
            ProcessBuilder(
                "java",
                "-DbundlerMainClass=net.minecraft.data.Main",
                "-jar",
                jarFile.absolutePath,
                "--reports",
            ).directory(workDir)
                .redirectOutput(ProcessBuilder.Redirect.INHERIT)
                .redirectError(ProcessBuilder.Redirect.INHERIT)
                .start()
        val exitCode = process.waitFor()
        check(exitCode == 0) { "Data generator exited with code $exitCode" }

        val reportsDir = File(workDir, "generated/reports")
        val blockStates = parseDefaultBlockStates(File(reportsDir, "blocks.json").readText())
        val items = parseRegistryEntries(File(reportsDir, "registries.json").readText(), "minecraft:item")

        val registryDir = project.rootProject.file("basalt-registry/src/commonMain/kotlin/org/basaltmc/registry")
        writeIdTable(File(registryDir, "VanillaBlockStates.kt"), "ALL_DEFAULT_BLOCK_STATES", BLOCK_STATES_KDOC, blockStates)
        writeIdTable(File(registryDir, "VanillaItems.kt"), "ALL_ITEMS", ITEMS_KDOC, items)

        println("Regenerated ${blockStates.size} block default states and ${items.size} items from $jarPath.")
    }
}

// Each block's `states` array marks exactly one entry `"default": true`, even blocks with a single,
// property-less state: that entry's `id` is the real numeric default block state id.
fun parseDefaultBlockStates(json: String): List<Pair<Int, String>> {
    val root = Json.parseToJsonElement(json) as JsonObject
    return root.entries
        .map { (key, value) ->
            val states = (value as JsonObject).getValue("states") as JsonArray
            val default = states.map { it as JsonObject }.first { (it["default"] as? JsonPrimitive)?.content == "true" }
            default.getValue("id").jsonPrimitive.content.toInt() to key.substringAfter(':')
        }.sortedBy { it.first }
}

// `registries.json` is keyed by registry id (`minecraft:item`, `minecraft:block`...), each with an
// `entries` map of `namespace:path` to `{"protocol_id": N}`.
fun parseRegistryEntries(
    json: String,
    registryId: String,
): List<Pair<Int, String>> {
    val root = Json.parseToJsonElement(json) as JsonObject
    val entries = (root.getValue(registryId) as JsonObject).getValue("entries") as JsonObject
    return entries.entries
        .map { (key, value) ->
            (value as JsonObject).getValue("protocol_id").jsonPrimitive.content.toInt() to key.substringAfter(':')
        }.sortedBy { it.first }
}

fun writeIdTable(
    file: File,
    varName: String,
    kdoc: String,
    pairs: List<Pair<Int, String>>,
) {
    val content =
        buildString {
            appendLine("package org.basaltmc.registry")
            appendLine()
            appendLine(kdoc)
            appendLine("internal val $varName: List<Pair<Int, String>> =")
            appendLine("    listOf(")
            for ((id, path) in pairs) appendLine("        $id to \"$path\",")
            appendLine("    )")
        }
    file.writeText(content)
}

val BLOCK_STATES_KDOC =
    """
    /**
     * Every block's default state id, straight from the real 26.2 server jar's own data
     * generator (${'`'}net.minecraft.data.Main --reports${'`'}, the ${'`'}blocks.json${'`'} report): authoritative,
     * not decompiled/guessed. Non-default states (block properties: facing, waterlogged, age...)
     * aren't included, since basalt-world doesn't model block properties yet; every block here
     * is registered by its default state only, same shape [Blocks] already used for its five
     * hand-picked named constants.
     */
    """.trimIndent()

val ITEMS_KDOC =
    """
    /**
     * Every item's real protocol id, straight from the real 26.2 server jar's own data
     * generator (${'`'}net.minecraft.data.Main --reports${'`'}, the ${'`'}registries.json${'`'} report):
     * authoritative, not decompiled/guessed.
     */
    """.trimIndent()
