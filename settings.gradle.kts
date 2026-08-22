rootProject.name = "basalt"

pluginManagement {
    // Wire owns every build convention, this repository included: see its build-logic for why
    // there is only one of these.
    includeBuild("wire/build-logic")
    repositories {
        gradlePluginPortal()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        mavenCentral()
        // Luak, the Lua runtime the plugin API runs on. Public, no authentication.
        maven("https://repo.blueva.net/releases")
    }
    versionCatalogs {
        // One catalog for every repository that consumes Wire, so versions cannot drift apart.
        create("libs") {
            from(files("wire/gradle/libs.versions.toml"))
        }
    }
}

include(
    ":wire-common",
    ":wire-nbt",
    ":wire-text",
    ":server-random",
    ":wire-io",
    ":wire-protocol",
    ":server-registry",
    ":server-world",
    ":server-storage",
    ":server-entity",
    ":server-runtime",
    ":server-assets",
    ":wire-auth",
    ":wire-multiversion",
    ":wire-bedrock",
    ":server-lua",
    ":server-plugins",
    ":server-datagen",
)

// The network layer is shared with the proxy and lives in the Wire submodule. It is composed in
// rather than depended on as a published artifact: both repositories move together for now.
for (name in listOf(
    "wire-common",
    "wire-nbt",
    "wire-text",
    "wire-io",
    "wire-protocol",
    "wire-auth",
    "wire-multiversion",
    "wire-bedrock",
)) {
    project(":$name").projectDir = file("wire/$name")
}
