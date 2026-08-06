plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            // TODO: depend on BlueLuaK (net.blueva.luak) once it publishes Kotlin/Native
            // artifacts to a resolvable repository.
        }
    }
}
