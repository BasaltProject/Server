plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-nbt"))
            // For the particle option shapes: which codec a particle type uses is a protocol fact.
            api(project(":wire-protocol"))
            implementation(libs.kotlinx.serialization.json)
        }
    }
}
