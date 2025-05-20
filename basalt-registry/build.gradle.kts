plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-nbt"))
            api(project(":basalt-protocol"))
            implementation(libs.kotlinx.serialization.json)
        }
    }
}
