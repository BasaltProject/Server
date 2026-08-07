rootProject.name = "basalt"

pluginManagement {
    includeBuild("build-logic")
    repositories {
        gradlePluginPortal()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        mavenCentral()
    }
}

include(
    ":basalt-common",
    ":basalt-nbt",
    ":basalt-text",
    ":basalt-random",
    ":basalt-io",
    ":basalt-protocol",
    ":basalt-registry",
    ":basalt-world",
    ":basalt-entity",
    ":basalt-server",
    ":basalt-assets",
    ":basalt-auth",
    ":basalt-translate",
    ":basalt-bedrock",
    ":basalt-lua",
    ":basalt-bsmod",
    ":basalt-datagen",
)
