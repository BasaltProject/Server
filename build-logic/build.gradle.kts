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
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.9.0")
}
