package org.basaltmc.server.plugins

import kotlin.test.Test

/** The frozen Kotlin as the oracle for what loads, in what order, and what does not. */
class PluginOrderOracle {
    private fun manifest(
        id: String,
        version: String = "1.0.0",
        api: Int = PluginManifest.API,
        depends: List<Pair<String, String>> = emptyList(),
        soft: List<Pair<String, String>> = emptyList(),
    ) = PluginManifest(
        id = id,
        name = id,
        version = version,
        authors = emptyList(),
        api = api,
        main = "src/main.lua",
        dependencies =
            depends.map { PluginDependency(it.first, it.second, true) } +
                soft.map { PluginDependency(it.first, it.second, false) },
        capabilities = PluginCapabilities.NONE,
        privateResources = emptyList(),
    )

    private fun describe(orderings: List<PluginOrdering>): String =
        orderings.joinToString(";") {
            when (it) {
                is PluginOrdering.Load -> "+${it.manifest.id}"
                is PluginOrdering.Refused -> "-${it.manifest.id}(${it.reason})"
            }
        }

    private fun report(
        name: String,
        candidates: List<PluginManifest>,
    ) {
        println("ORACLE $name ${describe(PluginOrder.resolve(candidates))}")
    }

    @Test
    fun dump() {
        // Nothing depends on anything: alphabetical, whatever order they were found in.
        report("plain", listOf(manifest("zebra"), manifest("apple"), manifest("mango")))

        // A chain: the order is what the dependencies force, not the alphabet.
        report(
            "chain",
            listOf(
                manifest("app", depends = listOf("core" to ">=1.0")),
                manifest("core", depends = listOf("base" to "*")),
                manifest("base"),
            ),
        )

        // A soft dependency does not decide whether something loads, but it does decide when.
        report("soft", listOf(manifest("addon", soft = listOf("zbase" to "*")), manifest("zbase")))
        // The same soft dependency, with the other one absent: it loads anyway.
        report("softMissing", listOf(manifest("addon", soft = listOf("gone" to "*"))))

        report("missing", listOf(manifest("app", depends = listOf("core" to "*"))))
        report(
            "tooOld",
            listOf(manifest("app", depends = listOf("core" to ">=2.0")), manifest("core", version = "1.5")),
        )
        report("wrongApi", listOf(manifest("old", api = 0), manifest("future", api = 2), manifest("fine")))
        // An API refusal is the reason given, not the dependency it never got to check.
        report(
            "wrongApiWithDeps",
            listOf(manifest("old", api = 0, depends = listOf("gone" to "*"))),
        )
        // Two broken dependencies at once: which one the message names is worth pinning.
        report(
            "twoBroken",
            listOf(manifest("app", depends = listOf("aaa" to "*", "zzz" to "*"))),
        )

        // The refusal spreads down the chain, however deep.
        report(
            "spread",
            // Deliberately in the order that defeats a single pass: 'far' is looked at before the
            // plugin it depends on has been refused, so the spread has to go round again.
            listOf(
                manifest("far", depends = listOf("leaf" to "*")),
                manifest("leaf", depends = listOf("mid" to "*")),
                manifest("mid", depends = listOf("root" to "*")),
            ),
        )

        report("selfCycle", listOf(manifest("loop", depends = listOf("loop" to "*"))))
        report(
            "cycle",
            listOf(
                manifest("alpha", depends = listOf("beta" to "*")),
                manifest("beta", depends = listOf("gamma" to "*")),
                manifest("gamma", depends = listOf("alpha" to "*")),
                manifest("free"),
            ),
        )
        report("empty", emptyList())

        val loaded =
            listOf(
                manifest("base"),
                manifest("core", depends = listOf("base" to "*")),
                manifest("app", depends = listOf("core" to "*")),
                manifest("ui", depends = listOf("core" to "*")),
                manifest("unrelated"),
                manifest("softOnly", soft = listOf("base" to "*")),
            )
        println("ORACLE dependentsBase ${PluginOrder.dependents("base", loaded)}")
        println("ORACLE dependentsCore ${PluginOrder.dependents("core", loaded)}")
        println("ORACLE dependentsLeaf ${PluginOrder.dependents("app", loaded)}")
        println("ORACLE dependentsUnknown ${PluginOrder.dependents("nobody", loaded)}")
    }
}
