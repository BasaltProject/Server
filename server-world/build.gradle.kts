plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-nbt"))
            api(project(":server-random"))
            api(project(":server-registry"))
            api(project(":wire-protocol"))
            implementation(libs.kotlinx.serialization.json)
        }
    }
}

// Temporary: release-optimised test binary for the migration benchmark.
kotlin.targets.withType<org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget>().configureEach {
    binaries.test(listOf(org.jetbrains.kotlin.gradle.plugin.mpp.NativeBuildType.RELEASE))
}
