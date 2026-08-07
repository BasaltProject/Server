plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-nbt"))
            api(project(":basalt-random"))
            api(project(":basalt-registry"))
            api(project(":basalt-protocol"))
        }
    }
}
