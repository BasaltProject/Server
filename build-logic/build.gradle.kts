plugins {
    `kotlin-dsl`
}

repositories {
    gradlePluginPortal()
    mavenCentral()
}

dependencies {
    implementation("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.10")
    implementation("org.jlleitschuh.gradle:ktlint-gradle:13.0.0")
    // On build-logic's classpath, not applied through the version catalog: the SQLDelight plugin
    // brings its own Kotlin Gradle Plugin, and resolving it in a second classloader clashes with
    // the one this included build already provides.
    implementation("app.cash.sqldelight:gradle-plugin:2.3.2")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.9.0")
}
