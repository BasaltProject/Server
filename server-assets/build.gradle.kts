plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-nbt"))
            api(project(":wire-io"))
            api(project(":wire-text"))
            implementation(libs.ktor.client.core)
            implementation(libs.kotlinx.coroutines.core)
            implementation(libs.cryptography.core)
            implementation(libs.cryptography.provider.optimal)
        }
        commonTest.dependencies {
            implementation(libs.ktor.client.mock)
        }

        // Ktor needs a concrete native engine per platform family (see wire-auth's build script
        // for the fuller explanation of the guarded lookups).
        sourceSets.findByName("macosMain")?.dependencies {
            implementation(libs.ktor.client.darwin)
        }
        sourceSets.findByName("linuxMain")?.dependencies {
            implementation(libs.ktor.client.curl)
        }
        sourceSets.findByName("mingwMain")?.dependencies {
            implementation(libs.ktor.client.winhttp)
        }
    }
}
