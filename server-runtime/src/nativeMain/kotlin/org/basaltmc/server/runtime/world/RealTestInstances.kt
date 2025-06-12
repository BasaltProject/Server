package org.basaltmc.server.runtime.world

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger

/**
 * One gametest instance, as `data/<ns>/test_instance/<path>.json` declares it.
 *
 * Only the shape this server can run is modelled: a `minecraft:function` test, which is a function
 * that fails by failing. That is the one kind the vanilla jar itself ships, and the one a datapack
 * can write without the block-assertion API.
 */
public data class TestInstance(
    public val id: Identifier,
    public val type: Identifier,
    public val function: Identifier?,
    public val maxTicks: Int,
    public val required: Boolean,
) {
    public val isFunctionTest: Boolean get() = type == Identifier.vanilla("function") && function != null
}

/** Test instances from the operator's own `server.jar`, plus whatever datapacks add later. */
public class RealTestInstances(
    serverJar: ServerJarAssets?,
    private val logger: Logger = Logger("Tests"),
) {
    public val all: List<TestInstance> = serverJar?.let(::load).orEmpty()

    public operator fun get(id: Identifier): TestInstance? = all.firstOrNull { it.id == id }

    private fun load(serverJar: ServerJarAssets): List<TestInstance> {
        val loaded = mutableListOf<TestInstance>()
        for (entry in serverJar.entriesUnder(TEST_PREFIX).sorted()) {
            if (!entry.endsWith(".json")) continue
            val path = entry.removePrefix(TEST_PREFIX).removeSuffix(".json")
            runCatching {
                val root = Json.parseToJsonElement(serverJar.readText(entry)) as JsonObject
                TestInstance(
                    id = Identifier.vanilla(path),
                    type = Identifier.parse((root["type"] as JsonPrimitive).content),
                    function = (root["function"] as? JsonPrimitive)?.content?.let(Identifier::parse),
                    maxTicks = (root["max_ticks"] as? JsonPrimitive)?.content?.toIntOrNull() ?: DEFAULT_MAX_TICKS,
                    required = (root["required"] as? JsonPrimitive)?.content?.toBooleanStrictOrNull() ?: true,
                )
            }.onSuccess { loaded += it }
                .onFailure { logger.debug { "Skipped test instance $entry: ${it.message}" } }
        }
        if (loaded.isNotEmpty()) logger.info { "Loaded ${loaded.size} test instance(s)." }
        return loaded
    }

    private companion object {
        const val TEST_PREFIX = "data/minecraft/test_instance/"
        const val DEFAULT_MAX_TICKS = 100
    }
}
