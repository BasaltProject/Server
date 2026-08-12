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
