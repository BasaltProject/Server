plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-lua"))
            api(project(":basalt-world"))
            api(project(":basalt-entity"))
            api(project(":basalt-registry"))
            api(project(":basalt-protocol"))
        }
    }
}
