plugins {
    id("basalt.sqldelight")
    id("basalt.sqlite-native")
}

sqldelight {
    databases {
        create("PlayerDatabase") {
            packageName.set("org.basaltmc.storage.db")
            srcDirs.setFrom("src/commonMain/sqldelight")
            // The default dialect is SQLite 3.18, which predates UPSERT. The SQLite actually linked
            // into the binary is the 3.53 amalgamation, so the generator should be told as much.
            dialect("app.cash.sqldelight:sqlite-3-38-dialect:2.3.2")
        }
    }
    // The SQLite the driver talks to is the one linked into this binary, not the operating
    // system's: see basalt-storage's own README note and `SqliteAmalgamation`.
    linkSqlite.set(false)
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-nbt"))
        }
        nativeMain.dependencies {
            implementation(libs.sqldelight.native.driver)
        }
        nativeTest.dependencies {
            // Only the tests need it: gzip is the transport vanilla's player files come wrapped in,
            // and this module deliberately takes the decompressor as a parameter instead of linking one.
            implementation(project(":basalt-io"))
        }
    }
}
