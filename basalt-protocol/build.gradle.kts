plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-nbt"))
            api(project(":basalt-text"))
            api(project(":basalt-random"))
            api(project(":basalt-io"))
        }
    }
}
