import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Generates one class per spawnable entity type, plus the stub layers below them, from the ops traits. */
public final class GenerateEntities {
    private static final Pattern kDefault = Pattern.compile("\\bdefault\\s+(?![-:])");
    private static final Pattern kTypeVariable = Pattern.compile("^[A-Z][A-Z0-9]?$");

    private static Path opsDirectory;
    private static final Map<Class<?>, Map<String, Method>> baseMethods = new HashMap<>();
    private static final Map<Class<?>, Set<String>> baseStubbed = new HashMap<>();
    private static final Set<String> warned = new LinkedHashSet<>();
    private static final Map<String, Class<?>> apiBySimple = new TreeMap<>();
    private static final Map<String, Set<String>> sourceKeys = new HashMap<>();

    private GenerateEntities() {}

    static String simpleKey(Method method) {
        StringBuilder key = new StringBuilder(method.getName()).append('(');
        for (Class<?> parameter : method.getParameterTypes()) {
            key.append(parameter.getSimpleName()).append(',');
        }
        return key.append(')').toString();
    }

    static void collect(Set<Class<?>> into, Class<?> type) {
        if (into.add(type)) {
            for (Class<?> parent : type.getInterfaces()) {
                collect(into, parent);
            }
        }
    }

    private static Set<String> keysOf(String opsName) throws Exception {
        Set<String> cached = sourceKeys.get(opsName);
        if (cached != null) {
            return cached;
        }
        Path file = opsDirectory.resolve(opsName + ".java");
        Set<String> keys = new LinkedHashSet<>();
        String text = Files.readString(file);
        Matcher matcher = kDefault.matcher(text);
        while (matcher.find()) {
            int open = text.indexOf('(', matcher.end());
            int closeAt = open;
            int depth = 0;
            for (; closeAt < text.length(); closeAt++) {
                char c = text.charAt(closeAt);
                if (c == '(') {
                    depth++;
                } else if (c == ')' && --depth == 0) {
                    break;
                }
            }
            String head = text.substring(matcher.end(), open).strip();
            String name = head.substring(head.lastIndexOf(' ') + 1);
            keys.add(normalise(name, text.substring(open + 1, closeAt)));
        }
        sourceKeys.put(opsName, keys);
        return keys;
    }

    private static String normalise(String name, String parameters) {
        StringBuilder key = new StringBuilder(name).append('(');
        for (String parameter : splitTopLevel(parameters)) {
            String text = parameter.replaceAll("@\\w+(\\([^)]*\\))?", " ").replaceAll("\\bfinal\\b", " ").strip();
            if (text.isEmpty()) {
                continue;
            }
            text = stripGenerics(text);
            int split = text.lastIndexOf(' ');
            String type = split < 0 ? text : text.substring(0, split).strip();
            boolean varargs = type.endsWith("...");
            if (varargs) {
                type = type.substring(0, type.length() - 3).strip() + "[]";
            }
            type = type.substring(type.lastIndexOf('.') + 1);
            key.append(type).append(',');
        }
        return key.append(')').toString();
    }

    private static String stripGenerics(String text) {
        StringBuilder out = new StringBuilder();
        int depth = 0;
        for (char c : text.toCharArray()) {
            if (c == '<') {
                depth++;
            } else if (c == '>') {
                depth--;
            } else if (depth == 0) {
                out.append(c);
            }
        }
        return out.toString().replaceAll("\\s+", " ");
    }

    private static List<String> splitTopLevel(String text) {
        List<String> parts = new ArrayList<>();
        int depth = 0;
        int start = 0;
        for (int at = 0; at < text.length(); at++) {
            char c = text.charAt(at);
            if (c == '<' || c == '(') {
                depth++;
            } else if (c == '>' || c == ')') {
                depth--;
            } else if (c == ',' && depth == 0) {
                parts.add(text.substring(start, at));
                start = at + 1;
            }
        }
        parts.add(text.substring(start));
        return parts;
    }

    private static boolean matches(String source, String api) {
        int sourceOpen = source.indexOf('(');
        int apiOpen = api.indexOf('(');
        if (!source.substring(0, sourceOpen).equals(api.substring(0, apiOpen))) {
            return false;
        }
        String[] left = source.substring(sourceOpen + 1, source.length() - 1).split(",", -1);
        String[] right = api.substring(apiOpen + 1, api.length() - 1).split(",", -1);
        if (left.length != right.length) {
            return false;
        }
        for (int at = 0; at < left.length; at++) {
            if (!left[at].equals(right[at]) && !kTypeVariable.matcher(left[at]).matches()
                && !right[at].equals("Object")) {
                return false;
            }
        }
        return true;
    }

    static Class<?> apiOf(String opsName) {
        String simple = opsName.substring(0, opsName.length() - "Ops".length());
        Class<?> api = apiBySimple.get(simple);
        if (api == null) {
            throw new IllegalStateException("No entity API interface named " + simple + " for " + opsName);
        }
        return api;
    }

    /** The API method keys the named ops provide as default methods, across everything their interface inherits. */
    private static Set<String> coverage(String opsName) throws Exception {
        Set<String> out = new LinkedHashSet<>();
        Set<String> source = keysOf(opsName);
        for (Method method : GenerateBases.chosen(apiOf(opsName)).values()) {
            String key = simpleKey(method);
            for (String candidate : source) {
                if (matches(candidate, key)) {
                    out.add(key);
                    break;
                }
            }
        }
        return out;
    }

    static Set<String> coverOf(Path directory, String names) throws Exception {
        init(directory);
        Set<String> out = new LinkedHashSet<>();
        for (String name : names.split(",")) {
            out.addAll(coverage(name));
        }
        return out;
    }

    private static void init(Path directory) throws Exception {
        if (opsDirectory != null) {
            return;
        }
        opsDirectory = directory;
        Class<?> entityType = Class.forName("org.bukkit.entity.EntityType");
        Set<Class<?>> all = new LinkedHashSet<>();
        for (Object value : entityType.getEnumConstants()) {
            Class<?> api = (Class<?>) entityType.getMethod("getEntityClass").invoke(value);
            if (api != null) {
                collect(all, api);
            }
        }
        for (Class<?> type : all) {
            String name = type.getSimpleName();
            Class<?> clash = apiBySimple.get(name);
            if (clash != null && clash != type) {
                boolean paper = type.getName().startsWith("io.papermc");
                Class<?> loser = paper ? type : clash;
                if (!paper) {
                    apiBySimple.put(name, type);
                }
                apiBySimple.put("Paper" + name, loser);
            } else {
                apiBySimple.put(name, type);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        init(Path.of(args[0]));
        Path out = Path.of(args[1], "org", "bukkit", "craftbukkit", "entity");
        Path layers = Path.of(args[1], "org", "basaltmc", "bukkit", "gen");
        Files.createDirectories(out);
        Files.createDirectories(layers);

        List<String> allOps = new ArrayList<>();
        try (var listing = Files.list(opsDirectory)) {
            listing.map(path -> path.getFileName().toString()).filter(name -> name.endsWith("Ops.java"))
                .map(name -> name.substring(0, name.length() - ".java".length())).sorted().forEach(allOps::add);
        }

        Class<?> entityApi = Class.forName("org.bukkit.entity.Entity");
        Class<?> livingApi = Class.forName("org.bukkit.entity.LivingEntity");
        Map<Class<?>, Set<String>> baseStubKeys = new HashMap<>();
        Map<Class<?>, List<String>> baseOps = new HashMap<>();
        for (Class<?> base : List.of(entityApi, livingApi)) {
            Set<Class<?>> closure = new LinkedHashSet<>();
            collect(closure, base);
            List<String> mine = new ArrayList<>();
            Set<String> covered = new LinkedHashSet<>();
            for (String ops : allOps) {
                if (closure.contains(apiBySimple.get(ops.substring(0, ops.length() - 3)))) {
                    mine.add(ops);
                    covered.addAll(coverage(ops));
                }
            }
            baseOps.put(base, mine);
            Set<String> handled = new LinkedHashSet<>();
            StringBuilder source = new StringBuilder("package org.basaltmc.bukkit.gen;\n\n");
            source.append("import org.basaltmc.bukkit.Unsupported;\n\n");
            source.append("/** Generated by server-bukkit/scripts/GenerateEntities.java: do not edit. */\n");
            source.append("@SuppressWarnings({\"rawtypes\", \"unchecked\", \"deprecation\", \"removal\"})\n");
            String superclass = base == livingApi ? "org.basaltmc.bukkit.entity.BasaltLivingEntity"
                : "org.basaltmc.bukkit.entity.BasaltEntity";
            String layer = base == livingApi ? "LivingLeaf" : "EntityLeaf";
            String top = base == livingApi ? "LivingEntityOps" : "EntityOps";
            source.append("public abstract class ").append(layer).append(" extends ").append(superclass)
                .append(" implements ").append(base.getCanonicalName()).append(", org.basaltmc.bukkit.entity.ops.")
                .append(top).append(" {\n    protected ").append(layer)
                .append("(org.basaltmc.bukkit.entity.EntityCore core) {\n        super(core);\n    }\n\n");
            for (Method method : GenerateBases.chosen(base).values()) {
                String key = simpleKey(method);
                handled.add(key);
                baseMethods.computeIfAbsent(base, ignored -> new HashMap<>()).put(key, method);
                if (!covered.contains(key)) {
                    baseStubbed.computeIfAbsent(base, ignored -> new LinkedHashSet<>()).add(key);
                }
                source.append(covered.contains(key) ? bridge(method, top) : GenerateBases.stub(method, base));
            }
            source.append("}\n");
            baseStubKeys.put(base, handled);
            Files.writeString(layers.resolve(layer + ".java"), source.toString());
        }

        Class<?> entityType = Class.forName("org.bukkit.entity.EntityType");
        Map<String, String> makers = new LinkedHashMap<>();
        for (Object value : entityType.getEnumConstants()) {
            Class<?> api = (Class<?>) entityType.getMethod("getEntityClass").invoke(value);
            String name = value.toString();
            if (api == null || name.equals("PLAYER")) {
                continue;
            }
            String className = "Craft" + api.getSimpleName();
            if (!makers.containsValue(className)) {
                Files.writeString(out.resolve(className + ".java"), leaf(api, className, allOps, baseStubKeys, baseOps));
            }
            makers.put(name, className);
        }
        StringBuilder factory = new StringBuilder("package org.basaltmc.bukkit.gen;\n\n");
        factory.append("import org.basaltmc.bukkit.entity.EntityCore;\n\n");
        factory.append("/** Generated by server-bukkit/scripts/GenerateEntities.java: do not edit. */\n");
        factory.append("public final class EntityLeaves implements org.basaltmc.bukkit.entity.EntityMaker {\n");
        factory.append("    @Override\n    public org.bukkit.entity.Entity make(String type, EntityCore core) {\n");
        factory.append("        switch (type) {\n");
        for (Map.Entry<String, String> entry : makers.entrySet()) {
            factory.append("            case \"").append(entry.getKey()).append("\":\n                return new org.bukkit")
                .append(".craftbukkit.entity.").append(entry.getValue()).append("(core);\n");
        }
        factory.append("            default:\n                return null;\n        }\n    }\n}\n");
        Files.writeString(layers.resolve("EntityLeaves.java"), factory.toString());
    }

    private static String leaf(Class<?> api, String className, List<String> allOps,
            Map<Class<?>, Set<String>> baseStubKeys, Map<Class<?>, List<String>> baseOps) throws Exception {
        boolean living = org.bukkit.entity.LivingEntity.class.isAssignableFrom(api);
        Class<?> base = living ? org.bukkit.entity.LivingEntity.class : org.bukkit.entity.Entity.class;
        Set<Class<?>> closure = new LinkedHashSet<>();
        collect(closure, api);
        List<String> mine = new ArrayList<>();
        Map<String, String> provider = new LinkedHashMap<>();
        for (String ops : allOps) {
            if (!closure.contains(apiBySimple.get(ops.substring(0, ops.length() - 3)))
                || baseOps.get(base).contains(ops)) {
                continue;
            }
            mine.add(ops);
            for (String key : coverage(ops)) {
                if (baseStubbed.get(base).contains(key)) {
                    throw new IllegalStateException(ops + " covers " + key + " which the base stub layer stubs");
                }
                if (baseStubKeys.get(base).contains(key)) {
                    if (warned.add(ops + key)) {
                        System.err.println(ops + " is shadowed by the base layer for " + key);
                    }
                } else {
                    provider.putIfAbsent(key, ops);
                }
            }
        }
        StringBuilder body = new StringBuilder();
        Set<String> bridged = new LinkedHashSet<>();
        Set<String> inherited = baseStubKeys.get(base);
        for (Method method : GenerateBases.chosen(api).values()) {
            String key = simpleKey(method);
            Method held = baseMethods.get(base).get(key);
            boolean narrower = held != null && held.getReturnType() != method.getReturnType()
                && held.getReturnType().isAssignableFrom(method.getReturnType());
            if (inherited.contains(key) && !narrower) {
                continue;
            }
            if (narrower) {
                System.err.println("covariant override in " + className + ": " + key);
            }
            String ops = provider.get(key);
            if (ops == null) {
                body.append(GenerateBases.stub(method, api));
            } else if (redeclared(closure, apiBySimple.get(ops.substring(0, ops.length() - 3)), key)) {
                bridged.add(ops);
                body.append(bridge(method, ops));
            }
        }
        for (String ops : bridged) {
            if (!mine.contains(ops)) {
                mine.add(ops);
            }
        }
        StringBuilder source = new StringBuilder("package org.bukkit.craftbukkit.entity;\n\n");
        source.append("import org.basaltmc.bukkit.Unsupported;\n\n");
        source.append("/** Generated by server-bukkit/scripts/GenerateEntities.java: do not edit. */\n");
        source.append("@SuppressWarnings({\"rawtypes\", \"unchecked\", \"deprecation\", \"removal\"})\n");
        source.append("public class ").append(className).append(" extends org.basaltmc.bukkit.gen.")
            .append(living ? "LivingLeaf" : "EntityLeaf").append(" implements ").append(api.getCanonicalName());
        for (String ops : mine) {
            source.append(", org.basaltmc.bukkit.entity.ops.").append(ops);
        }
        source.append(" {\n    public ").append(className).append("(org.basaltmc.bukkit.entity.EntityCore core) {\n")
            .append("        super(core);\n    }\n\n").append(body);
        return source.append("}\n").toString();
    }

    static boolean redeclared(Set<Class<?>> closure, Class<?> opsApi, String key) {
        for (Class<?> type : closure) {
            if (type.isAssignableFrom(opsApi)) {
                continue;
            }
            for (Method method : type.getDeclaredMethods()) {
                if (Modifier.isAbstract(method.getModifiers()) && simpleKey(method).equals(key)) {
                    return true;
                }
            }
        }
        return false;
    }

    static String bridge(Method method, String ops) {
        String stub = GenerateBases.stub(method, method.getDeclaringClass());
        int bodyAt = stub.indexOf(" {\n");
        StringBuilder call = new StringBuilder();
        int parameters = method.getParameterCount();
        for (int at = 0; at < parameters; at++) {
            call.append(at > 0 ? ", " : "").append('a').append(at);
        }
        String head = stub.substring(0, bodyAt);
        boolean returns = method.getReturnType() != void.class;
        Class<?> type = method.getReturnType();
        String cast = returns && !type.isPrimitive() && type != Object.class && Modifier.isPublic(type.getModifiers())
            && method.getTypeParameters().length == 0 && method.getGenericReturnType() instanceof Class<?>
            ? "(" + type.getCanonicalName() + ") " : "";
        return head + " {\n        " + (returns ? "return " + cast : "") + "org.basaltmc.bukkit.entity.ops." + ops
            + ".super." + method.getName() + "(" + call + ");\n    }\n\n";
    }
}
