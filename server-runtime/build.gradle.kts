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
        commonMain {
            kotlin.srcDir(layout.buildDirectory.dir("generated/build-info/kotlin"))
        }
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":server-world"))
            api(project(":server-storage"))
            api(project(":server-entity"))
            api(project(":server-registry"))
            api(project(":wire-protocol"))
            api(project(":wire-multiversion"))
            api(project(":server-assets"))
            api(project(":wire-auth"))
            api(project(":server-plugins"))
            implementation(libs.cryptography.core)
            implementation(libs.cryptography.provider.optimal)
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
                entryPoint = "org.basaltmc.server.runtime.main"
                // Pinned, so the module can be renamed without renaming what operators download.
                baseName = "basalt-server"
            }
        }
    }
}

/**
 * `./gradlew build` leaves one binary in `build/`, for the machine doing the building and under the
 * name it would be downloaded with. Release unless `-Pbasalt.debug`.
 */
val debugBinary: Boolean = providers.gradleProperty("basalt.debug").map { it != "false" }.getOrElse(false)
val binaryTarget: String = extra["basaltHostTarget"].toString()
val binaryLinkTask: String =
    "link" + (if (debugBinary) "Debug" else "Release") + "Executable" +
        binaryTarget.replaceFirstChar { it.uppercase() }

// `arm64`/`x64` as the Kotlin/Native target names spell them, and mingw named after what it
// produces, Windows.
val binaryPlatform: String =
    when {
        binaryTarget.startsWith("macos") -> "macOS"
        binaryTarget.startsWith("linux") -> "Linux"
        else -> "Windows"
    }
val binaryFileName: String =
    "BasaltServer-" + project.version + "-" + binaryPlatform + "-" +
        (if (binaryTarget.endsWith("Arm64")) "arm64" else "x64") +
        (if (binaryPlatform == "Windows") ".exe" else "")

val linkedBinary = tasks.named<org.jetbrains.kotlin.gradle.tasks.KotlinNativeLink>(binaryLinkTask)
val linkedFile = linkedBinary.flatMap { it.outputFile }
val copiedBinary = rootProject.layout.buildDirectory.file(binaryFileName)

val basaltBinary =
    tasks.register("basaltBinary") {
        group = "build"
        description = "Links the binary for this host and leaves it in build/, named for the platform it runs on."
        dependsOn(linkedBinary)
        inputs.files(linkedBinary)
        // The file, never the directory: declaring build/ itself an output hands it to Gradle's
        // stale-output cleanup, which then deletes everything in it that no task produced.
        outputs.file(copiedBinary)
        doLast {
            val target = copiedBinary.get().asFile
            // .kexe is Kotlin/Native's own extension; on Unix the execute bit is what makes a
            // file run, so the name loses it and the permission travels.
            linkedFile.get().copyTo(target, overwrite = true)
            target.setExecutable(true)
        }
    }

tasks.named("assemble") { dependsOn(basaltBinary) }

// Kotlin/Native hangs every link task on `assemble`. Unless the build asked for targets of its
// own, only the one that ends up in build/ belongs there.
if (!providers.gradleProperty("basalt.targets").isPresent) {
    afterEvaluate {
        tasks.named("assemble") {
            setDependsOn(
                dependsOn.filterNot {
                    it is TaskProvider<*> && it.name.contains("Executable") && it.name != binaryLinkTask
                },
            )
        }
    }
}

/**
 * Turns every `.toml` under `src/nativeMain/resources/config-defaults` into one file of constants,
 * so `tab_list.toml` becomes `TAB_LIST_DEFAULT_TOML` and adding a template is adding a file.
 *
 * (Do not write a glob with a slash and a star in this comment: Kotlin nests block comments and
 * swallows the rest of the script.)
 *
 * They are compiled in rather than read at runtime because Kotlin/Native has no resource loading,
 * and needing a folder beside the binary would cost the single self-contained binary.
 */
tasks.register("generateConfigDefaults") {
    val input = file("src/nativeMain/resources/config-defaults")
    val output = file("build/generated/config-defaults/kotlin/org/basaltmc/server/runtime/config")
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
            "package org.basaltmc.server.runtime.config\n\n// Generated from " +
                "src/nativeMain/resources/config-defaults. Edit the .toml files, not this one.\n\n"
        File(output, "ConfigDefaults.kt").writeText(header + parts.joinToString("\n\n") + "\n")
    }
}

/**
 * Writes the version into the binary instead of keeping a constant somebody has to remember to
 * bump. The build number comes from the CI, so a release is identified by the same string the
 * artifact is named with.
 */
tasks.register("generateBuildInfo") {
    val output = file("build/generated/build-info/kotlin/org/basaltmc/server/runtime")
    val version = project.version.toString()
    val minecraft = project.extra["minecraftVersion"].toString()
    inputs.property("version", version)
    inputs.property("minecraft", minecraft)
    outputs.dir(output)
    doLast {
        output.mkdirs()
        File(output, "BuildInfo.kt").writeText(
            "package org.basaltmc.server.runtime\n\n" +
                "import org.basaltmc.wire.multiversion.ProtocolVersions\n\n" +
                "// Generated by the build. Change the version in gradle.properties, not here.\n" +
                "public object BuildInfo {\n" +
                "    public const val VERSION: String = \"" + version + "\"\n" +
                "    public const val MINECRAFT_VERSION: String = \"" + minecraft + "\"\n" +
                "    public const val PROTOCOL_VERSION: Int = ProtocolVersions.NATIVE\n\n" +
                "    /** A build nobody can download: built outside the CI, so it has no build number. */\n" +
                "    public val isDevelopmentBuild: Boolean = VERSION.endsWith(\"-dev\")\n" +
                "}\n",
        )
    }
}

// Every compilation of this module, the test one included, needs the constants to exist first.
tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinNativeCompile>().configureEach {
    dependsOn("generateConfigDefaults", "generateBuildInfo")
}
