import io.papermc.paper.registry.RegistryKey;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Type;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.TreeMap;

/** Writes RegistryTypes: which API type each Paper registry holds, read once from the API jar's own RegistryKey constants. */
public final class GenerateRegistryTypes {
    public static void main(String[] args) throws IOException, IllegalAccessException {
        TreeMap<String, String> types = new TreeMap<>();
        for (Field field : RegistryKey.class.getFields()) {
            if (!Modifier.isStatic(field.getModifiers()) || !(field.getGenericType() instanceof ParameterizedType generic)) {
                continue;
            }
            Type argument = generic.getActualTypeArguments()[0];
            Class<?> raw = argument instanceof Class<?> type ? type
                : argument instanceof ParameterizedType nested ? (Class<?>) nested.getRawType() : null;
            if (raw != null) {
                types.put(((RegistryKey<?>) field.get(null)).key().value(), raw.getCanonicalName());
            }
        }
        StringBuilder out = new StringBuilder("package org.basaltmc.bukkit.gen;\n\n");
        out.append("import java.util.Map;\n\npublic final class RegistryTypes {\n    private RegistryTypes() {}\n\n");
        out.append("    public static final Map<String, Class<?>> kByName = Map.ofEntries(\n");
        String separator = "";
        for (var entry : types.entrySet()) {
            out.append(separator).append("        Map.entry(\"").append(entry.getKey()).append("\", ")
                .append(entry.getValue()).append(".class)");
            separator = ",\n";
        }
        out.append(");\n}\n");
        Path file = Path.of(args[0], "org/basaltmc/bukkit/gen/RegistryTypes.java");
        Files.createDirectories(file.getParent());
        Files.writeString(file, out.toString());
    }
}
