import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget

plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-world"))
            api(project(":basalt-storage"))
            api(project(":basalt-entity"))
            api(project(":basalt-registry"))
            api(project(":basalt-protocol"))
            api(project(":basalt-assets"))
            api(project(":basalt-auth"))
            api(project(":basalt-bsmod"))
        }
        nativeTest.dependencies {
            // Only to play the client side of the online-mode handshake in ServerIntegrationTest.
            implementation(libs.cryptography.core)
            implementation(libs.cryptography.provider.optimal)
        }
    }

    targets.withType<KotlinNativeTarget>().configureEach {
        binaries {
            executable {
                entryPoint = "org.basaltmc.server.main"
            }
        }
    }
}
