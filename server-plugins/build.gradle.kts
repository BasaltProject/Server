plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-io"))
            api(project(":server-lua"))
            api(project(":server-world"))
            api(project(":server-entity"))
            api(project(":server-registry"))
            api(project(":wire-protocol"))
        }
    }
}
