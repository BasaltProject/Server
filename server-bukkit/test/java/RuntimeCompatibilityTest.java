package compatibility;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.Statement;
import net.minecraft.resources.ResourceKey;

public final class RuntimeCompatibilityTest {
    public static final class OptionalIntegration {
        private MissingDependency optionalImplementation() { return null; }
        public static String available() { return "available"; }
    }

    public static void publicReflection() throws Exception {
        Object value = OptionalIntegration.class.getMethod("available").invoke(null);
        if (!"available".equals(value)) throw new AssertionError("Public method was not invoked");
        for (java.lang.reflect.Method method : OptionalIntegration.class.getMethods()) {
            if (!Modifier.isPublic(method.getModifiers())) throw new AssertionError("Private method leaked");
        }
    }

    public static void featureKeys() throws Exception {
        for (String name : new String[] {"AquaticFeatures", "CaveFeatures", "EndFeatures", "NetherFeatures",
                "PileFeatures", "TreeFeatures", "VegetationFeatures"}) {
            Class<?> type = Class.forName("net.minecraft.data.worldgen.features." + name);
            int count = 0;
            for (Field field : type.getFields()) {
                if (field.getType() != ResourceKey.class) continue;
                ResourceKey<?> key = (ResourceKey<?>) field.get(null);
                if (key == null || !"minecraft:worldgen/configured_feature".equals(key.registry().toString())) {
                    throw new AssertionError("Invalid configured feature key: " + field);
                }
                count++;
            }
            if (count == 0) throw new AssertionError("No configured feature keys in " + name);
        }
    }

    public static void sqlite() throws Exception {
        Class.forName("org.sqlite.JDBC");
        try (Connection connection = DriverManager.getConnection("jdbc:sqlite::memory:");
                Statement statement = connection.createStatement()) {
            statement.executeUpdate("CREATE TABLE compatibility (value INTEGER NOT NULL)");
            statement.executeUpdate("INSERT INTO compatibility VALUES (42)");
            try (ResultSet result = statement.executeQuery("SELECT value FROM compatibility")) {
                if (!result.next() || result.getInt(1) != 42) throw new AssertionError("SQLite round trip failed");
            }
        }
    }

    public static void registryTags() throws Exception {
        // Tag's static constants ask Bukkit for tags during initialization.
        // This JNI fixture has no running server; only those lookups are stubbed.
        org.bukkit.Server server = (org.bukkit.Server) java.lang.reflect.Proxy.newProxyInstance(
                org.bukkit.Server.class.getClassLoader(), new Class<?>[] {org.bukkit.Server.class},
                (proxy, method, args) -> switch (method.getName()) {
                    case "getLogger" -> java.util.logging.Logger.getLogger("compatibility");
                    case "getName", "getVersion", "getBukkitVersion" -> "compatibility";
                    default -> null;
                });
        org.bukkit.Bukkit.setServer(server);
        Class<?> tags = Class.forName("org.basaltmc.bukkit.BasaltTag");
        var all = tags.getDeclaredMethod("all", String.class, Class.class);
        var invalidate = tags.getDeclaredMethod("invalidate");
        all.setAccessible(true);
        invalidate.setAccessible(true);
        for (int pass = 0; pass < 2; ++pass) {
            java.util.Map<?, ?> snapshot = (java.util.Map<?, ?>) all.invoke(null, org.bukkit.Tag.REGISTRY_BLOCKS, org.bukkit.Material.class);
            if (snapshot != all.invoke(null, org.bukkit.Tag.REGISTRY_BLOCKS, org.bukkit.Material.class)) {
                throw new AssertionError("Tag snapshot was not cached");
            }
            for (String name : new String[] {"first", "second"}) {
                @SuppressWarnings("unchecked")
                org.bukkit.Tag<org.bukkit.Material> tag = (org.bukkit.Tag<org.bukkit.Material>) snapshot.get(org.bukkit.NamespacedKey.minecraft(name));
                if (tag == null || !tag.getValues().equals(java.util.Set.of(org.bukkit.Material.STONE, org.bukkit.Material.DIRT))) {
                    throw new AssertionError("Shared tag members were not retained");
                }
            }
            invalidate.invoke(null);
        }
    }

    public static void loggingBackend() {
        System.setProperty("log4j2.loggerContextFactory", "org.basaltmc.bukkit.log.BasaltLog4jFactory");
        org.apache.logging.log4j.Logger api = org.apache.logging.log4j.LogManager.getRootLogger();
        if (!(api instanceof org.apache.logging.log4j.core.Logger root)) {
            throw new AssertionError("Root logger has no shared Log4j backend");
        }
        int[] received = {0};
        org.apache.logging.log4j.core.Appender appender =
                new org.apache.logging.log4j.core.appender.AbstractAppender("compatibility-probe", null, null, false,
                        org.apache.logging.log4j.core.config.Property.EMPTY_ARRAY) {
                    @Override public void append(org.apache.logging.log4j.core.LogEvent event) { received[0]++; }
                };
        appender.start();
        root.addAppender(appender);
        try {
            root.error("Runtime compatibility logging probe");
            if (received[0] != 1) throw new AssertionError("Appender did not receive the logging event");
        } finally {
            root.removeAppender(appender);
            appender.stop();
        }
        org.apache.logging.log4j.LogManager.shutdown();
    }
}

// Deliberately removed after compilation: optional integration types need not be installed.
class MissingDependency {}
