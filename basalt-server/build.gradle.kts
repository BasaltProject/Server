import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget

plugins {
    id("basalt.native-module")
}

/** Three quotes, built rather than written, so this script never contains a raw-string delimiter. */
val tripleQuote = "\"".repeat(3)

kotlin {
    sourceSets {
        nativeMain {
            kotlin.srcDir(layout.buildDirectory.dir("generated/config-defaults/kotlin"))
        }
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-world"))
            api(project(":basalt-storage"))
            api(project(":basalt-entity"))
            api(project(":basalt-registry"))
            api(project(":basalt-protocol"))
            api(project(":basalt-assets"))
            api(project(":basalt-auth"))
            api(project(":basalt-bsmod"))
        }
        nativeTest.dependencies {
            // Only to play the client side of the online-mode handshake in ServerIntegrationTest.
            implementation(libs.cryptography.core)
            implementation(libs.cryptography.provider.optimal)
        }
    }

    targets.withType<KotlinNativeTarget>().configureEach {
        binaries {
            executable {
                entryPoint = "org.basaltmc.server.main"
            }
        }
    }
}

/**
 * Turns every `.toml` under `src/nativeMain/resources/config-defaults` into one file of constants.
 *
 * (Do not write a glob with a slash and a star in this comment: Kotlin nests block comments, so it
 * opens one and silently swallows the rest of this script. That cost an hour.)
 *
 * The templates are real TOML files: edited as TOML, highlighted as TOML, with no Kotlin escaping to
 * remember. They are compiled *in* rather than read from disk at runtime because Kotlin/Native has
 * no resource loading at all, and a server that needed a folder beside it to write its own first
 * configuration would stop being the single self-contained binary this project ships.
 *
 * `tab_list.toml` becomes `TAB_LIST_DEFAULT_TOML`, so adding a template is adding a file.
 */
tasks.register("generateConfigDefaults") {
    val input = file("src/nativeMain/resources/config-defaults")
    val output = file("build/generated/config-defaults/kotlin/org/basaltmc/server/config")
    inputs.dir(input)
    outputs.dir(output)
    doLast {
        output.mkdirs()
        val templates =
            input
                .listFiles()
                .orEmpty()
                .filter { it.name.endsWith(".toml") }
                .sortedBy { it.name }
        val parts = mutableListOf<String>()
        for (f in templates) {
            val name = f.name.removeSuffix(".toml").uppercase() + "_DEFAULT_TOML"
            val text = f.readText()
            // A raw string cannot contain three quotes, and would read a dollar sign as a template
            // expression. Neither appears in any template today; failing here the day one does beats
            // compiling in something quietly different from the file on disk.
            require(!text.contains(tripleQuote)) { f.name + " has a triple quote, which a raw string cannot hold" }
            require(!text.contains('$')) { f.name + " has a dollar sign, which a raw string would expand" }
            parts.add("internal const val " + name + ": String =\n    " + tripleQuote + text + tripleQuote)
        }
        val header =
            "package org.basaltmc.server.config\n\n// Generated from " +
                "src/nativeMain/resources/config-defaults. Edit the .toml files, not this one.\n\n"
        File(output, "ConfigDefaults.kt").writeText(header + parts.joinToString("\n\n") + "\n")
    }
}

// Every compilation of this module, the test one included, needs the constants to exist first.
tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinNativeCompile>().configureEach {
    dependsOn("generateConfigDefaults")
}
