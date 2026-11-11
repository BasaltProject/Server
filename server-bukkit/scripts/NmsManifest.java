import com.sun.source.tree.*;
import com.sun.source.util.JavacTask;
import com.sun.source.util.SourcePositions;
import com.sun.source.util.Trees;
import com.sun.tools.javac.code.Flags;
import com.sun.tools.javac.tree.JCTree;
import java.io.IOException;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;
import java.util.stream.*;
import javax.lang.model.element.Modifier;
import javax.tools.*;

/**
 * Signature-only manifest of the NMS surface, and the generator of throwing stubs from it.
 * Usage: manifest SRC_ROOT OUT_DIR | stubs MANIFEST_DIR OUT_DIR SKIP_ROOT...
 */
public final class NmsManifest {
    private static final Pattern kAnnotation = Pattern.compile("@[A-Za-z_][\\w.]*");

    private Trees trees;
    private SourcePositions positions;
    private CompilationUnitTree unit;
    private String source;

    public static void main(String[] args) throws Exception {
        NmsManifest tool = new NmsManifest();
        if (args[0].equals("manifest")) {
            tool.runManifest(Path.of(args[1]), Path.of(args[2]));
        } else if (args[0].equals("coverage")) {
            tool.runCoverage(Path.of(args[1]), Path.of(args[2]), Path.of(args[3]));
        } else if (args[0].equals("complete")) {
            tool.runComplete(Path.of(args[1]), Path.of(args[2]), Path.of(args[3]));
        } else {
            List<Path> skips = new ArrayList<>();
            for (int i = 3; i < args.length; i++) {
                skips.add(Path.of(args[i]));
            }
            tool.runStubs(Path.of(args[1]), Path.of(args[2]), skips);
        }
    }

    // ---- parsing helpers

    private List<CompilationUnitTree> parse(List<Path> files) throws IOException {
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        StandardJavaFileManager fm = compiler.getStandardFileManager(null, null, java.nio.charset.StandardCharsets.UTF_8);
        JavacTask task = (JavacTask) compiler.getTask(null, fm, d -> {}, List.of("-proc:none"), null,
            fm.getJavaFileObjectsFromPaths(files));
        trees = Trees.instance(task);
        positions = trees.getSourcePositions();
        List<CompilationUnitTree> units = new ArrayList<>();
        for (CompilationUnitTree cu : task.parse()) {
            units.add(cu);
        }
        return units;
    }

    private String text(Tree t) {
        long s = positions.getStartPosition(unit, t);
        long e = positions.getEndPosition(unit, t);
        return clean(source.substring((int) s, (int) e));
    }

    private String textBetween(Tree from, Tree to) {
        long s = positions.getStartPosition(unit, from);
        long e = positions.getEndPosition(unit, to);
        return clean(source.substring((int) s, (int) e));
    }

    private static String clean(String raw) {
        StringBuilder sb = new StringBuilder();
        int i = 0;
        int n = raw.length();
        while (i < n) {
            char c = raw.charAt(i);
            if (c == '/' && i + 1 < n && raw.charAt(i + 1) == '*') {
                int end = raw.indexOf("*/", i + 2);
                i = end < 0 ? n : end + 2;
                continue;
            }
            if (c == '/' && i + 1 < n && raw.charAt(i + 1) == '/') {
                while (i < n && raw.charAt(i) != '\n') {
                    i++;
                }
                continue;
            }
            if (c == '@' && !raw.startsWith("@interface", i)) {
                Matcher m = kAnnotation.matcher(raw);
                m.region(i, n);
                if (m.lookingAt()) {
                    i = m.end();
                    int j = i;
                    while (j < n && Character.isWhitespace(raw.charAt(j))) {
                        j++;
                    }
                    if (j < n && raw.charAt(j) == '(') {
                        int depth = 0;
                        for (; j < n; j++) {
                            char d = raw.charAt(j);
                            if (d == '(') {
                                depth++;
                            } else if (d == ')' && --depth == 0) {
                                j++;
                                break;
                            }
                        }
                        i = j;
                    }
                    continue;
                }
            }
            sb.append(c);
            i++;
        }
        return sb.toString().replaceAll("\\s+", " ").replaceAll("\\s*,\\s*", ", ").replaceAll("\\s+>", ">").trim();
    }

    private static String mods(ModifiersTree m, boolean method) {
        Set<Modifier> f = m.getFlags();
        StringBuilder sb = new StringBuilder();
        for (Modifier x : List.of(Modifier.PUBLIC, Modifier.PROTECTED, Modifier.PRIVATE, Modifier.STATIC,
            Modifier.ABSTRACT, Modifier.DEFAULT, Modifier.FINAL)) {
            if (f.contains(x)) {
                sb.append(x.toString()).append(' ');
            }
        }
        return sb.toString();
    }

    private static boolean has(ModifiersTree m, Modifier x) {
        return m.getFlags().contains(x);
    }

    private static long flags(Tree t) {
        if (t instanceof JCTree.JCVariableDecl v) {
            return v.mods.flags;
        }
        if (t instanceof JCTree.JCMethodDecl d) {
            return d.mods.flags;
        }
        return 0;
    }

    // ---- manifest mode

    private void runManifest(Path root, Path out) throws IOException {
        List<Path> files;
        try (Stream<Path> s = Files.walk(root)) {
            files = s.filter(p -> p.toString().endsWith(".java") && !p.endsWith("package-info.java")
                && !p.endsWith("module-info.java")).sorted().toList();
        }
        Map<String, StringBuilder> perPackage = new TreeMap<>();
        for (CompilationUnitTree cu : parse(files)) {
            unit = cu;
            source = cu.getSourceFile().getCharContent(true).toString();
            String pkg = cu.getPackageName() == null ? "" : cu.getPackageName().toString();
            String file = Path.of(cu.getSourceFile().toUri()).getFileName().toString();
            StringBuilder body = new StringBuilder();
            for (Tree t : cu.getTypeDecls()) {
                if (t instanceof ClassTree ct) {
                    manifestClass(ct, body, "", false);
                }
            }
            if (body.isEmpty()) {
                continue;
            }
            StringBuilder sec = perPackage.computeIfAbsent(pkg, k -> new StringBuilder());
            sec.append("//// ").append(file).append('\n');
            Set<String> words = new HashSet<>();
            Matcher w = Pattern.compile("[A-Za-z_]\\w*").matcher(body);
            while (w.find()) {
                words.add(w.group());
            }
            for (ImportTree imp : cu.getImports()) {
                if (imp.isStatic()) {
                    continue;
                }
                String q = imp.getQualifiedIdentifier().toString();
                String simple = q.substring(q.lastIndexOf('.') + 1);
                if (simple.equals("*") || words.contains(simple)) {
                    sec.append("import ").append(q).append(";\n");
                }
            }
            sec.append(body);
        }
        Files.createDirectories(out);
        for (var e : perPackage.entrySet()) {
            Files.writeString(out.resolve((e.getKey().isEmpty() ? "default" : e.getKey()) + ".txt"), e.getValue());
        }
    }

    private void manifestClass(ClassTree ct, StringBuilder sb, String ind, boolean nested) {
        ModifiersTree m = ct.getModifiers();
        Tree.Kind kind = ct.getKind();
        String keyword = switch (kind) {
            case INTERFACE -> "interface";
            case ENUM -> "enum";
            case RECORD -> "record";
            case ANNOTATION_TYPE -> "@interface";
            default -> "class";
        };
        StringBuilder mm = new StringBuilder();
        for (Modifier x : List.of(Modifier.PUBLIC, Modifier.PROTECTED, Modifier.STATIC)) {
            if (has(m, x)) {
                mm.append(x).append(' ');
            }
        }
        if (kind == Tree.Kind.CLASS && has(m, Modifier.ABSTRACT)) {
            mm.append("abstract ");
        }
        if (kind == Tree.Kind.CLASS && has(m, Modifier.FINAL)) {
            mm.append("final ");
        }
        sb.append(ind).append(mm).append(keyword).append(' ').append(ct.getSimpleName());
        if (!ct.getTypeParameters().isEmpty()) {
            sb.append('<').append(ct.getTypeParameters().stream().map(this::text).collect(Collectors.joining(", ")))
                .append('>');
        }
        List<VariableTree> components = new ArrayList<>();
        if (kind == Tree.Kind.RECORD) {
            for (Tree mem : ct.getMembers()) {
                if (mem instanceof VariableTree v && (flags(v) & Flags.RECORD) != 0) {
                    components.add(v);
                }
            }
            sb.append('(').append(components.stream().map(this::param).collect(Collectors.joining(", ")))
                .append(')');
        }
        if (ct.getExtendsClause() != null) {
            sb.append(" extends ").append(text(ct.getExtendsClause()));
        }
        if (!ct.getImplementsClause().isEmpty()) {
            sb.append(kind == Tree.Kind.INTERFACE ? " extends " : " implements ")
                .append(ct.getImplementsClause().stream().map(this::text).collect(Collectors.joining(", ")));
        }
        sb.append(" {\n");
        String in = ind + "    ";
        boolean isInterface = kind == Tree.Kind.INTERFACE || kind == Tree.Kind.ANNOTATION_TYPE;
        Set<String> enumSeen = new HashSet<>();
        if (kind == Tree.Kind.ENUM) {
            List<String> names = new ArrayList<>();
            for (Tree mem : ct.getMembers()) {
                if (mem instanceof VariableTree v && v.getInitializer() instanceof NewClassTree nc
                    && (flags(v) & Flags.ENUM) != 0) {
                    names.add(v.getName().toString());
                }
            }
            sb.append(in).append(String.join(", ", names)).append(";\n");
            for (Tree mem : ct.getMembers()) {
                if (mem instanceof MethodTree mt && !isPrivate(mt.getModifiers())) {
                    enumSeen.add(mt.getName() + "/" + mt.getParameters().size());
                }
            }
        }
        for (Tree mem : ct.getMembers()) {
            if (mem instanceof ClassTree inner) {
                manifestClass(inner, sb, in, true);
            } else if (mem instanceof VariableTree v) {
                if ((flags(v) & (Flags.ENUM | Flags.RECORD)) != 0 || isPrivate(v.getModifiers())) {
                    continue;
                }
                sb.append(in).append(mods(v.getModifiers(), false)).append(text(v.getType())).append(' ')
                    .append(v.getName()).append(";\n");
            } else if (mem instanceof MethodTree mt) {
                manifestMethod(ct, mt, sb, in, isInterface, kind == Tree.Kind.RECORD);
            }
        }
        if (kind == Tree.Kind.ENUM) {
            for (Tree mem : ct.getMembers()) {
                if (mem instanceof VariableTree v && v.getInitializer() instanceof NewClassTree nc
                    && nc.getClassBody() != null) {
                    for (Tree bm : nc.getClassBody().getMembers()) {
                        if (bm instanceof MethodTree mt && mt.getReturnType() != null
                            && !isPrivate(mt.getModifiers())
                            && enumSeen.add(mt.getName() + "/" + mt.getParameters().size())) {
                            manifestMethod(ct, mt, sb, in, false, false);
                        }
                    }
                }
            }
        }
        sb.append(ind).append("}\n");
    }

    private static boolean isPrivate(ModifiersTree m) {
        return m.getFlags().contains(Modifier.PRIVATE);
    }

    private String param(VariableTree v) {
        String t = text(v.getType());
        if ((flags(v) & Flags.VARARGS) != 0 && t.endsWith("[]")) {
            t = t.substring(0, t.length() - 2) + "...";
        }
        return t + " " + v.getName();
    }

    private void manifestMethod(ClassTree owner, MethodTree mt, StringBuilder sb, String in, boolean isInterface,
        boolean record) {
        ModifiersTree m = mt.getModifiers();
        boolean ctor = mt.getReturnType() == null;
        if ((flags(mt) & Flags.COMPACT_RECORD_CONSTRUCTOR) != 0) {
            return;
        }
        if (isPrivate(m) && !ctor) {
            return;
        }
        if (ctor && owner.getKind() == Tree.Kind.ENUM) {
            return;
        }
        StringBuilder line = new StringBuilder(in);
        String mm = mods(m, true);
        if (isInterface && !has(m, Modifier.DEFAULT) && !has(m, Modifier.STATIC) && mt.getBody() != null) {
            mm += "default ";
        }
        line.append(mm);
        if (!mt.getTypeParameters().isEmpty()) {
            line.append('<').append(mt.getTypeParameters().stream().map(this::text).collect(Collectors.joining(", ")))
                .append("> ");
        }
        if (!ctor) {
            line.append(text(mt.getReturnType())).append(' ').append(mt.getName());
        } else {
            line.append(owner.getSimpleName());
        }
        line.append('(').append(mt.getParameters().stream().map(this::param).collect(Collectors.joining(", ")))
            .append(')');
        if (!mt.getThrows().isEmpty()) {
            line.append(" throws ").append(mt.getThrows().stream().map(this::text).collect(Collectors.joining(", ")));
        }
        sb.append(line).append(";\n");
    }


    // ---- complete mode: add the members a hand-written class lacks

    private void runComplete(Path manifestDir, Path javaRoot, Path scratch) throws IOException {
        Map<CompilationUnitTree, String> sources = new HashMap<>();
        List<CompilationUnitTree> units = loadManifest(manifestDir, scratch, sources);
        int touched = 0;
        int added = 0;
        for (CompilationUnitTree cu : units) {
            unit = cu;
            source = sources.get(cu);
            String pkg = cu.getPackageName() == null ? "" : cu.getPackageName().toString();
            for (Tree t : cu.getTypeDecls()) {
                if (!(t instanceof ClassTree ct)) {
                    continue;
                }
                String fqn = pkg.isEmpty() ? ct.getSimpleName().toString() : pkg + "." + ct.getSimpleName();
                Path file = javaRoot.resolve(fqn.replace('.', '/') + ".java");
                if (!Files.exists(file)) {
                    continue;
                }
                NmsManifest h = new NmsManifest();
                CompilationUnitTree hu = h.parse(List.of(file)).get(0);
                h.unit = hu;
                h.source = hu.getSourceFile().getCharContent(true).toString();
                ClassTree hand = null;
                for (Tree ht : hu.getTypeDecls()) {
                    if (ht instanceof ClassTree hc && hc.getSimpleName().contentEquals(ct.getSimpleName())) {
                        hand = hc;
                    }
                }
                if (hand == null) {
                    continue;
                }
                List<Object[]> inserts = new ArrayList<>();
                forceConcrete = true;
                completeClass(ct, hand, h, fqn, pkg, "", inserts);
                if (inserts.isEmpty()) {
                    continue;
                }
                Map<String, String> handSimple = new HashMap<>();
                for (ImportTree imp : hu.getImports()) {
                    String q = imp.getQualifiedIdentifier().toString();
                    handSimple.put(q.substring(q.lastIndexOf('.') + 1), q);
                }
                StringBuilder imps = new StringBuilder();
                for (ImportTree imp : cu.getImports()) {
                    String q = imp.getQualifiedIdentifier().toString();
                    String simple = q.substring(q.lastIndexOf('.') + 1);
                    if (!handSimple.containsValue(q) && (simple.equals("*") || !handSimple.containsKey(simple))) {
                        imps.append("import ").append(q).append(";\n");
                        handSimple.put(simple, q);
                    }
                }
                int impAt;
                if (!hu.getImports().isEmpty()) {
                    impAt = (int) h.positions.getEndPosition(hu, hu.getImports().get(hu.getImports().size() - 1));
                    imps.insert(0, "\n");
                    if (imps.length() > 1) {
                        imps.setLength(imps.length() - 1);
                    }
                } else if (hu.getPackageName() != null) {
                    impAt = (int) h.positions.getEndPosition(hu, hu.getPackage());
                    imps.insert(0, "\n\n");
                    if (imps.length() > 2) {
                        imps.setLength(imps.length() - 1);
                    }
                } else {
                    impAt = 0;
                }
                if (imps.toString().isBlank()) {
                    imps.setLength(0);
                }
                inserts.add(new Object[] {impAt, imps.toString()});
                inserts.sort((x, y) -> Integer.compare((Integer) y[0], (Integer) x[0]));
                StringBuilder out = new StringBuilder(h.source);
                for (Object[] ins : inserts) {
                    String txt = (String) ins[1];
                    if (txt.startsWith("\u0000")) {
                        int e = txt.indexOf('\u0000', 1);
                        int len = Integer.parseInt(txt.substring(1, e));
                        out.replace((Integer) ins[0], (Integer) ins[0] + len, txt.substring(e + 1));
                    } else {
                        out.insert((Integer) ins[0], txt);
                    }
                    added++;
                }
                Files.writeString(file, out.toString());
                touched++;
            }
        }
        System.out.println("completed classes: " + touched + ", insertions: " + added);
    }

    private void completeClass(ClassTree ct, ClassTree hand, NmsManifest h, String fqn, String pkg, String ind,
        List<Object[]> inserts) {
        ClassInfo info = index.get(fqn);
        Tree.Kind kind = ct.getKind();
        boolean isInterface = kind == Tree.Kind.INTERFACE || kind == Tree.Kind.ANNOTATION_TYPE;
        String in = ind + "    ";
        List<String> typeVars = new ArrayList<>(info.typeParams);
        String want = headerSig(qualifyNested(headerText(ct), ct, info));
        String have = headerSig(h.headerText(hand));
        if (!want.equals(have)) {
            mergeHeader(ct, hand, h, info, inserts);
        }
        Set<String> handFields = new HashSet<>();
        Map<String, List<List<String>>> handMethods = new HashMap<>();
        List<List<String>> handCtors = new ArrayList<>();
        Map<String, ClassTree> handNested = new HashMap<>();
        for (Tree mem : hand.getMembers()) {
            if (mem instanceof VariableTree v) {
                handFields.add(v.getName().toString());
            } else if (mem instanceof ClassTree c) {
                handNested.put(c.getSimpleName().toString(), c);
            } else if (mem instanceof MethodTree mt) {
                List<String> sig = h.erasedParams(mt);
                if (mt.getReturnType() == null) {
                    handCtors.add(sig);
                } else {
                    handMethods.computeIfAbsent(mt.getName().toString(), k -> new ArrayList<>()).add(sig);
                }
            }
        }
        StringBuilder sb = new StringBuilder();
        boolean addedCtor = false;
        boolean noArgAdded = false;
        for (Tree mem : ct.getMembers()) {
            if (mem instanceof ClassTree inner) {
                ClassTree hi = handNested.get(inner.getSimpleName().toString());
                String innerFqn = fqn + "." + inner.getSimpleName();
                if (hi != null) {
                    completeClass(inner, hi, h, innerFqn, pkg, in, inserts);
                } else {
                    boolean saved = forceConcrete;
                    forceConcrete = false;
                    stubClass(inner, sb, in, innerFqn, pkg, List.of());
                    forceConcrete = saved;
                }
            } else if (mem instanceof VariableTree v) {
                if ((flags(v) & (Flags.ENUM | Flags.RECORD)) != 0 || handFields.contains(v.getName().toString())) {
                    continue;
                }
                boolean fin = isInterface || has(v.getModifiers(), Modifier.FINAL);
                String type = text(v.getType());
                sb.append(in).append(mods(v.getModifiers(), false)).append(type).append(' ').append(v.getName())
                    .append(fin ? " = " + initOf(v, type, typeVars, fqn, isInterface || has(v.getModifiers(), Modifier.STATIC),
                        kind == Tree.Kind.ENUM) : "").append(";\n");
            } else if (mem instanceof MethodTree mt) {
                List<String> sig = erasedParams(mt);
                List<List<String>> pool = mt.getReturnType() == null ? handCtors
                    : handMethods.getOrDefault(mt.getName().toString(), List.of());
                if (mt.getReturnType() == null && (kind == Tree.Kind.ENUM || hand.getKind() == Tree.Kind.RECORD)) {
                    continue;
                }
                if (pool.stream().anyMatch(x -> sigMatch(x, sig))) {
                    continue;
                }
                if ((flags(mt) & Flags.COMPACT_RECORD_CONSTRUCTOR) != 0) {
                    continue;
                }
                stubMethod(ct, mt, sb, in, fqn, info, isInterface, typeVars);
                if (mt.getReturnType() == null) {
                    addedCtor = true;
                    noArgAdded |= mt.getParameters().isEmpty();
                }
            }
        }
        if (addedCtor && !noArgAdded && handCtors.isEmpty() && hand.getKind() == Tree.Kind.CLASS) {
            sb.append(in).append(has(hand.getModifiers(), Modifier.PUBLIC) ? "public " : "")
                .append(hand.getSimpleName()).append("() {}\n");
        }
        if (sb.length() > 0) {
            int close = (int) h.positions.getEndPosition(h.unit, hand) - 1;
            boolean bare = hand.getKind() == Tree.Kind.ENUM && hand.getMembers().stream()
                .allMatch(x -> x instanceof VariableTree v && (flags(v) & Flags.ENUM) != 0);
            inserts.add(new Object[] {close, (bare ? ";\n" : "") + sb});
        }
    }

    private List<String> erasedParams(MethodTree mt) {
        List<String> typeVars = mt.getTypeParameters().stream().map(tp -> tp.getName().toString()).toList();
        List<String> out = new ArrayList<>();
        for (VariableTree p : mt.getParameters()) {
            out.add(erasedType(text(p.getType()), typeVars));
        }
        return out;
    }

    private static String erasedType(String type, List<String> typeVars) {
        String t = type.replace("...", "[]");
        StringBuilder e = new StringBuilder();
        int depth = 0;
        for (char c : t.toCharArray()) {
            if (c == '<') {
                depth++;
            } else if (c == '>') {
                depth--;
            } else if (depth == 0) {
                e.append(c);
            }
        }
        String er = e.toString().replace(" ", "");
        String base = er.replace("[]", "");
        String dims = er.substring(base.length());
        base = base.substring(base.lastIndexOf('.') + 1);
        if (base.length() <= 2 && Character.isUpperCase(base.charAt(0)) || typeVars.contains(base)) {
            base = "*";
        }
        return base + dims;
    }

    private static boolean sigMatch(List<String> a, List<String> b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (int i = 0; i < a.size(); i++) {
            String x = a.get(i);
            String y = b.get(i);
            if (!x.equals(y) && !x.startsWith("*") && !y.startsWith("*")) {
                return false;
            }
        }
        return true;
    }


    // ---- coverage: how much of the manifest a compiled jar provides

    private void runCoverage(Path manifestDir, Path scratch, Path jar) throws Exception {
        Map<CompilationUnitTree, String> sources = new HashMap<>();
        List<CompilationUnitTree> units = loadManifest(manifestDir, scratch, sources);
        java.net.URLClassLoader loader = new java.net.URLClassLoader(new java.net.URL[] {jar.toUri().toURL()},
            NmsManifest.class.getClassLoader());
        int[] totals = new int[6];
        Map<String, int[]> perPackage = new TreeMap<>();
        for (CompilationUnitTree cu : units) {
            unit = cu;
            source = sources.get(cu);
            String pkg = cu.getPackageName() == null ? "" : cu.getPackageName().toString();
            for (Tree t : cu.getTypeDecls()) {
                if (t instanceof ClassTree ct) {
                    coverClass(ct, pkg + "." + ct.getSimpleName(), loader, perPackage.computeIfAbsent(pkg,
                        k -> new int[6]), totals);
                }
            }
        }
        System.out.printf("classes %d/%d  members %d/%d (fields %d/%d, methods+ctors %d/%d)%n", totals[0], totals[1],
            totals[2] + totals[4], totals[3] + totals[5], totals[2], totals[3], totals[4], totals[5]);
        if (System.getProperty("perPackage") != null) {
            perPackage.forEach((k, v) -> System.out.printf("%-70s classes %4d/%4d members %5d/%5d%n", k, v[0], v[1],
                v[2] + v[4], v[3] + v[5]));
        }
    }

    private void coverClass(ClassTree ct, String binary, ClassLoader loader, int[] pkg, int[] totals) {
        Class<?> k = null;
        try {
            k = Class.forName(binary, false, loader);
        } catch (ClassNotFoundException | LinkageError e) {
            // missing
        }
        pkg[1]++;
        totals[1]++;
        if (k != null) {
            pkg[0]++;
            totals[0]++;
        }
        Set<String> fields = new HashSet<>();
        Set<String> methods = new HashSet<>();
        if (k != null) {
            try {
                for (var f : k.getDeclaredFields()) {
                    fields.add(f.getName());
                }
                for (var m : k.getDeclaredMethods()) {
                    methods.add(m.getName() + "/" + m.getParameterCount());
                }
                for (var c : k.getDeclaredConstructors()) {
                    methods.add("<init>/" + c.getParameterCount());
                }
            } catch (LinkageError e) {
                // unresolvable member types count as missing
            }
        }
        for (Tree mem : ct.getMembers()) {
            if (mem instanceof ClassTree inner) {
                coverClass(inner, binary + "$" + inner.getSimpleName(), loader, pkg, totals);
            } else if (mem instanceof VariableTree v) {
                if ((flags(v) & Flags.RECORD) != 0) {
                    continue;
                }
                pkg[3]++;
                totals[3]++;
                if (fields.contains(v.getName().toString())) {
                    pkg[2]++;
                    totals[2]++;
                } else if (System.getProperty("missing") != null) {
                    System.out.println("MISSING " + binary + "." + v.getName());
                }
            } else if (mem instanceof MethodTree mt) {
                String key = (mt.getReturnType() == null ? "<init>" : mt.getName().toString()) + "/"
                    + mt.getParameters().size();
                pkg[5]++;
                totals[5]++;
                if (methods.contains(key)) {
                    pkg[4]++;
                    totals[4]++;
                } else if (System.getProperty("missing") != null) {
                    System.out.println("MISSING " + binary + "#" + key);
                }
            }
        }
    }

    // ---- stubs mode

    private record ClassInfo(String fqn, String superName, String pkg, List<String> typeParams, List<String> ctors,
        List<String> imports, Map<String, String> nested, List<String> privCtors) {}

    private final Map<String, ClassInfo> index = new HashMap<>();
    private boolean forceConcrete;
    private final Set<String> blockHooks = new HashSet<>();

    private void loadBlockHooks(Path root) throws IOException {
        Pattern method = Pattern.compile("^    (?:protected|public)(?: final)? (?!static)([\\w<>\\[\\], .?]+?) (\\w+)\\((.*)\\) (?:throws [\\w, ]+)?\\{$");
        for (String file : List.of("net/minecraft/world/level/block/state/BlockBehaviour.java", "net/minecraft/world/level/block/Block.java")) {
            Path path = root.resolve(file);
            if (!Files.exists(path)) {
                continue;
            }
            for (String line : Files.readAllLines(path)) {
                Matcher m = method.matcher(line);
                if (m.matches()) {
                    blockHooks.add(m.group(2) + "/" + paramTypes(m.group(3)) + "/" + m.group(1).trim());
                }
            }
        }
    }

    private static String paramTypes(String params) {
        List<String> types = new ArrayList<>();
        int depth = 0;
        StringBuilder current = new StringBuilder();
        for (char c : (params + ",").toCharArray()) {
            if (c == '<') {
                depth++;
            } else if (c == '>') {
                depth--;
            }
            if (c == ',' && depth == 0) {
                String p = current.toString().trim().replaceAll("\\bfinal ", "");
                if (!p.isEmpty()) {
                    types.add(p.substring(0, p.lastIndexOf(' ') < 0 ? p.length() : p.lastIndexOf(' ')).trim());
                }
                current.setLength(0);
            } else {
                current.append(c);
            }
        }
        return String.join(",", types);
    }

    private void runStubs(Path manifestDir, Path out, List<Path> skips) throws IOException {
        Set<String> skipTop = new HashSet<>();
        for (Path root : skips) {
            if (!Files.isDirectory(root)) {
                continue;
            }
            loadBlockHooks(root);
            Path registriesFile = root.resolve("net/minecraft/core/registries/Registries.java");
            if (Files.exists(registriesFile)) {
                Matcher keys = Pattern.compile("ResourceKey<Registry<(.+?)>> (\\w+) =\\s*ResourceKey\\.createRegistryKey")
                    .matcher(Files.readString(registriesFile));
                while (keys.find()) {
                    kRegistryByType.putIfAbsent(keys.group(1).replaceAll("\\s+", ""), keys.group(2));
                }
            }
            Path soundFile = root.resolve("net/minecraft/sounds/SoundEvents.java");
            if (Files.exists(soundFile)) {
                Matcher fields = Pattern.compile("SoundEvent (\\w+) = of\\(").matcher(Files.readString(soundFile));
                while (fields.find()) {
                    kSoundFields.add(fields.group(1));
                }
            }
            try (Stream<Path> s = Files.walk(root)) {
                s.filter(p -> p.toString().endsWith(".java")).forEach(p -> {
                    String rel = root.relativize(p).toString().replace('\\', '/');
                    skipTop.add(rel.substring(0, rel.length() - 5).replace('/', '.'));
                });
            }
        }
        Path work = out.resolveSibling(out.getFileName() + "-split");
        Map<CompilationUnitTree, String> sources = new HashMap<>();
        List<CompilationUnitTree> units = loadManifest(manifestDir, work, sources);
        Set<String> excluded = new HashSet<>();
        Path exFile = manifestDir.resolve("exclude.list");
        if (Files.exists(exFile)) {
            for (String l : Files.readAllLines(exFile)) {
                if (!l.isBlank() && !l.startsWith("#")) {
                    excluded.add(l.trim());
                }
            }
        }
        for (Path root : skips) {
            if (!Files.isDirectory(root)) {
                continue;
            }
            List<Path> handFiles;
            try (Stream<Path> w = Files.walk(root)) {
                handFiles = w.filter(p -> p.toString().endsWith(".java")).filter(p -> {
                    String rel = root.relativize(p).toString().replace('\\', '/');
                    return index.containsKey(rel.substring(0, rel.length() - 5).replace('/', '.'));
                }).toList();
            }
            NmsManifest h = new NmsManifest();
            for (CompilationUnitTree hu : h.parse(handFiles)) {
                h.unit = hu;
                h.source = hu.getSourceFile().getCharContent(true).toString();
                String hp = hu.getPackageName() == null ? "" : hu.getPackageName().toString();
                List<String> himports = hu.getImports().stream().map(i -> i.getQualifiedIdentifier().toString()).toList();
                for (Tree t : hu.getTypeDecls()) {
                    if (t instanceof ClassTree hc) {
                        ClassInfo before = index.get(hp.isEmpty() ? hc.getSimpleName().toString()
                            : hp + "." + hc.getSimpleName());
                        h.indexClass(hc, hp, before.fqn, himports);
                        index.putAll(h.index);
                    }
                }
            }
        }
        int written = 0;
        for (CompilationUnitTree cu : units) {
            unit = cu;
            source = sources.get(cu);
            String pkg = cu.getPackageName() == null ? "" : cu.getPackageName().toString();
            for (Tree t : cu.getTypeDecls()) {
                if (!(t instanceof ClassTree ct)) {
                    continue;
                }
                String fqn = pkg.isEmpty() ? ct.getSimpleName().toString() : pkg + "." + ct.getSimpleName();
                if (skipTop.contains(fqn) || excluded.contains(fqn)) {
                    continue;
                }
                StringBuilder sb = new StringBuilder();
                if (!pkg.isEmpty()) {
                    sb.append("package ").append(pkg).append(";\n");
                }
                for (ImportTree imp : cu.getImports()) {
                    sb.append("import ").append(imp.getQualifiedIdentifier()).append(";\n");
                }
                stubClass(ct, sb, "", fqn, pkg, cu.getImports().stream()
                    .map(i -> i.getQualifiedIdentifier().toString()).toList());
                Path dest = out.resolve(fqn.replace('.', '/') + ".java");
                Files.createDirectories(dest.getParent());
                Files.writeString(dest, sb.toString());
                written++;
            }
        }
        writeInertReport();
        System.out.println("stubs written: " + written + ", skipped hand-written: " + skipTop.size());
    }

    private List<CompilationUnitTree> loadManifest(Path manifestDir, Path work,
        Map<CompilationUnitTree, String> sources) throws IOException {
        List<Path> tmpFiles = new ArrayList<>();
        deleteTree(work);
        Files.createDirectories(work);
        List<String[]> sections = new ArrayList<>();
        try (Stream<Path> s = Files.list(manifestDir)) {
            for (Path f : s.filter(p -> p.toString().endsWith(".txt")).sorted().toList()) {
                String pkg = f.getFileName().toString().replaceAll("\\.txt$", "");
                String[] parts = Files.readString(f).split("(?m)^//// ");
                for (String part : parts) {
                    if (part.isBlank()) {
                        continue;
                    }
                    int nl = part.indexOf('\n');
                    String name = part.substring(0, nl).trim().replace(".java", "");
                    Path p = work.resolve(pkg.replace('.', '/')).resolve(name + ".java");
                    Files.createDirectories(p.getParent());
                    String header = pkg.startsWith("net.minecraft") || pkg.startsWith("com.mojang")
                        ? "// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md\n" : "";
                    Files.writeString(p, header + (pkg.equals("default") ? "" : "package " + pkg + ";\n") + part.substring(nl + 1));
                    tmpFiles.add(p);
                }
            }
        }
        List<CompilationUnitTree> units = parse(tmpFiles);
        for (CompilationUnitTree cu : units) {
            unit = cu;
            source = cu.getSourceFile().getCharContent(true).toString();
            sources.put(cu, source);
            String pkg = cu.getPackageName() == null ? "" : cu.getPackageName().toString();
            List<String> imports = cu.getImports().stream().map(i -> i.getQualifiedIdentifier().toString()).toList();
            for (Tree t : cu.getTypeDecls()) {
                if (t instanceof ClassTree ct) {
                    indexClass(ct, pkg, pkg.isEmpty() ? ct.getSimpleName().toString() : pkg + "." + ct.getSimpleName(),
                        imports);
                }
            }
        }
        return units;
    }

    private static void deleteTree(Path p) throws IOException {
        if (Files.exists(p)) {
            try (Stream<Path> s = Files.walk(p)) {
                for (Path q : s.sorted(Comparator.reverseOrder()).toList()) {
                    Files.delete(q);
                }
            }
        }
    }

    private void indexClass(ClassTree ct, String pkg, String fqn, List<String> imports) {
        List<String> typeParams = ct.getTypeParameters().stream().map(tp -> tp.getName().toString()).toList();
        List<String> ctors = new ArrayList<>();
        List<String> privCtors = new ArrayList<>();
        Map<String, String> nested = new HashMap<>();
        for (Tree mem : ct.getMembers()) {
            if (mem instanceof MethodTree mt && mt.getReturnType() == null) {
                (isPrivate(mt.getModifiers()) ? privCtors : ctors).add(mt.getParameters().stream()
                    .map(p -> text(p.getType())).collect(Collectors.joining("\u0001")));
            } else if (mem instanceof ClassTree inner) {
                nested.put(inner.getSimpleName().toString(), fqn + "." + inner.getSimpleName());
                indexClass(inner, pkg, fqn + "." + inner.getSimpleName(), imports);
            }
        }
        boolean explicit = ct.getMembers().stream().anyMatch(x -> x instanceof MethodTree mt && mt.getReturnType() == null);
        if (!explicit) {
            ctors.add("");
        }
        String sup = ct.getExtendsClause() == null || ct.getKind() == Tree.Kind.INTERFACE ? null
            : text(ct.getExtendsClause());
        index.put(fqn, new ClassInfo(fqn, sup, pkg, typeParams, ctors, imports, nested, privCtors));
    }

    private ClassInfo resolve(String simple, ClassInfo from) {
        simple = simple.replaceAll("<.*", "").trim();
        if (simple.contains(".")) {
            ClassInfo direct = index.get(simple);
            if (direct != null) {
                return direct;
            }
            String head = simple.substring(0, simple.indexOf('.'));
            ClassInfo h = resolve(head, from);
            if (h != null) {
                return index.get(h.fqn + simple.substring(simple.indexOf('.')));
            }
            return null;
        }
        for (String imp : from.imports) {
            if (imp.endsWith("." + simple)) {
                return index.get(imp);
            }
        }
        String outer = from.fqn;
        while (outer.contains(".")) {
            outer = outer.substring(0, outer.lastIndexOf('.'));
            ClassInfo o = index.get(outer);
            if (o != null && o.nested.containsKey(simple)) {
                return index.get(o.nested.get(simple));
            }
        }
        ClassInfo own = index.get(from.fqn + "." + simple);
        if (own != null) {
            return own;
        }
        return index.get(from.pkg.isEmpty() ? simple : from.pkg + "." + simple);
    }

    private static final Set<String> kPrimitives = Set.of("int", "long", "short", "byte", "char", "boolean", "float",
        "double");

    private String qualify(String base, ClassInfo ctx) {
        if (ctx == null || base.startsWith("?")) {
            return base;
        }
        ClassInfo r = resolve(base, ctx);
        if (r != null) {
            return r.fqn;
        }
        String head = base.contains(".") ? base.substring(0, base.indexOf('.')) : base;
        for (String imp : ctx.imports) {
            if (imp.endsWith("." + head)) {
                return imp + base.substring(head.length());
            }
        }
        return base;
    }

    private String nullArg(String type, List<String> typeVars, ClassInfo ctx) {
        String t = type.trim();
        boolean varargs = t.endsWith("...");
        if (varargs) {
            t = t.substring(0, t.length() - 3) + "[]";
        }
        StringBuilder erased = new StringBuilder();
        int depth = 0;
        for (char c : t.toCharArray()) {
            if (c == '<') {
                depth++;
            } else if (c == '>') {
                depth--;
            } else if (depth == 0) {
                erased.append(c);
            }
        }
        String e = erased.toString().replace(" ", "");
        if (kPrimitives.contains(e)) {
            return e.equals("boolean") ? "false" : "(" + e + ") 0";
        }
        String base = e.replace("[]", "");
        if (typeVars.contains(base) || base.startsWith("?")) {
            return e.endsWith("[]") ? "null" : "org.basaltmc.bukkit.facade.NmsStub.nul()";
        }
        return "(" + qualify(base, ctx) + e.substring(base.length()) + ") null";
    }

    private void stubClass(ClassTree ct, StringBuilder sb, String ind, String fqn, String pkg, List<String> imports) {
        ClassInfo info = index.get(fqn);
        Tree.Kind kind = ct.getKind();
        boolean isInterface = kind == Tree.Kind.INTERFACE || kind == Tree.Kind.ANNOTATION_TYPE;
        sb.append(ind).append(qualifyNested(headerText(ct), ct, info)).append(" {\n");
        String in = ind + "    ";
        List<String> typeVars = new ArrayList<>(info.typeParams);
        if (kind == Tree.Kind.ENUM) {
            List<String> names = new ArrayList<>();
            for (Tree mem : ct.getMembers()) {
                if (mem instanceof VariableTree v && (flags(v) & Flags.ENUM) != 0) {
                    names.add(v.getName().toString());
                }
            }
            sb.append(in).append(String.join(", ", names)).append(";\n");
        }
        boolean explicitCtor = ct.getMembers().stream().anyMatch(x -> x instanceof MethodTree mt
            && mt.getReturnType() == null);
        if (kind == Tree.Kind.CLASS && !explicitCtor) {
            String call = superCall(info);
            if (!call.isEmpty()) {
                sb.append(in).append("protected ").append(ct.getSimpleName()).append("() { ").append(call).append("}\n");
            }
        }
        for (Tree mem : ct.getMembers()) {
            if (mem instanceof ClassTree inner) {
                stubClass(inner, sb, in, fqn + "." + inner.getSimpleName(), pkg, imports);
            } else if (mem instanceof VariableTree v) {
                if ((flags(v) & (Flags.ENUM | Flags.RECORD)) != 0) {
                    continue;
                }
                boolean fin = isInterface || has(v.getModifiers(), Modifier.FINAL);
                String type = text(v.getType());
                String init = "";
                if (fin) {
                    init = " = " + initOf(v, type, typeVars, fqn, isInterface || has(v.getModifiers(), Modifier.STATIC),
                        kind == Tree.Kind.ENUM);
                }
                sb.append(in).append(mods(v.getModifiers(), false)).append(type).append(' ').append(v.getName())
                    .append(init).append(";\n");
            } else if (mem instanceof MethodTree mt) {
                stubMethod(ct, mt, sb, in, fqn, info, isInterface, typeVars);
            }
        }
        sb.append(ind).append("}\n");
    }

    private String qualifyNested(String header, ClassTree ct, ClassInfo info) {
        int at = header.indexOf(" extends ");
        int imp = header.indexOf(" implements ");
        int cut = at < 0 ? imp : imp < 0 ? at : Math.min(at, imp);
        if (cut < 0) {
            return header;
        }
        String head = header.substring(0, cut);
        String tail = header.substring(cut);
        for (var e : info.nested.entrySet()) {
            tail = tail.replaceAll("(?<![\\w.])" + e.getKey() + "(?![\\w])", Matcher.quoteReplacement(
                ct.getSimpleName() + "." + e.getKey()));
        }
        return head + tail;
    }

    private static List<String> splitTop(String s) {
        List<String> out = new ArrayList<>();
        int depth = 0;
        int last = 0;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '<' || c == '(') {
                depth++;
            } else if (c == '>' || c == ')') {
                depth--;
            } else if (c == ',' && depth == 0) {
                out.add(s.substring(last, i).trim());
                last = i + 1;
            }
        }
        if (!s.substring(last).isBlank()) {
            out.add(s.substring(last).trim());
        }
        return out;
    }

    private static String simpleOf(String type) {
        String t = type.replaceAll("<.*", "").trim();
        return t.substring(t.lastIndexOf('.') + 1);
    }

    /** Adds missing supertypes and adopts type arguments; replaces the clause text of the hand-written header. */
    private void mergeHeader(ClassTree ct, ClassTree hand, NmsManifest h, ClassInfo info, List<Object[]> inserts) {
        if (hand.getKind() != ct.getKind() || ct.getKind() == Tree.Kind.ENUM) {
            if (ct.getKind() != Tree.Kind.ENUM || hand.getKind() != Tree.Kind.ENUM) {
                return;
            }
        }
        boolean iface = ct.getKind() == Tree.Kind.INTERFACE;
        List<String> wantExt = new ArrayList<>();
        List<String> wantImpl = new ArrayList<>();
        if (ct.getExtendsClause() != null && !iface) {
            wantExt.add(qualifyNestedType(text(ct.getExtendsClause()), ct, info));
        }
        for (Tree t : ct.getImplementsClause()) {
            wantImpl.add(qualifyNestedType(text(t), ct, info));
        }
        List<String> haveExt = new ArrayList<>();
        List<String> haveImpl = new ArrayList<>();
        if (hand.getExtendsClause() != null && !iface) {
            haveExt.add(h.text(hand.getExtendsClause()));
        }
        for (Tree t : hand.getImplementsClause()) {
            haveImpl.add(h.text(t));
        }
        List<String> newExt = merge(haveExt, wantExt);
        List<String> newImpl = merge(haveImpl, wantImpl);
        String wantTp = ct.getTypeParameters().isEmpty() ? "" : "<" + ct.getTypeParameters().stream()
            .map(this::text).collect(Collectors.joining(", ")) + ">";
        String haveTp = hand.getTypeParameters().isEmpty() ? "" : "<" + hand.getTypeParameters().stream()
            .map(h::text).collect(Collectors.joining(", ")) + ">";
        List<String> wantNames = ct.getTypeParameters().stream().map(x -> x.getName().toString()).toList();
        List<String> haveNames = hand.getTypeParameters().stream().map(x -> x.getName().toString()).toList();
        String newTp = wantNames.equals(haveNames) ? wantTp : haveTp;
        if (newExt.equals(haveExt) && newImpl.equals(haveImpl) && newTp.equals(haveTp)) {
            return;
        }
        int from = h.afterHeaderStart(hand);
        if (from < 0) {
            return;
        }
        int brace = h.source.indexOf('{', from);
        String mid = h.source.substring(from, brace);
        String comps = "";
        if (hand.getKind() == Tree.Kind.RECORD) {
            int open = mid.indexOf('(');
            int depth = 0;
            int end = open;
            for (int i = open; i < mid.length(); i++) {
                if (mid.charAt(i) == '(') {
                    depth++;
                } else if (mid.charAt(i) == ')' && --depth == 0) {
                    end = i;
                    break;
                }
            }
            comps = mid.substring(open, end + 1);
        }
        StringBuilder nh = new StringBuilder(newTp).append(comps);
        if (!newExt.isEmpty()) {
            nh.append(" extends ").append(String.join(", ", newExt));
        }
        if (!newImpl.isEmpty()) {
            nh.append(iface ? " extends " : " implements ").append(String.join(", ", newImpl));
        }
        nh.append(' ');
        inserts.add(new Object[] {from, "\u0000" + (brace - from) + "\u0000" + nh});
    }

    private static List<String> merge(List<String> have, List<String> want) {
        List<String> out = new ArrayList<>(have);
        for (String w : want) {
            boolean done = false;
            for (int i = 0; i < out.size(); i++) {
                if (simpleOf(out.get(i)).equals(simpleOf(w))) {
                    if (!out.get(i).contains("<") || w.contains("<")) {
                        out.set(i, w);
                    }
                    done = true;
                    break;
                }
            }
            if (!done) {
                out.add(w);
            }
        }
        return out;
    }

    private String qualifyNestedType(String type, ClassTree ct, ClassInfo info) {
        String t = type;
        for (var e : info.nested.entrySet()) {
            t = t.replaceAll("(?<![\\w.])" + e.getKey() + "(?![\\w])", Matcher.quoteReplacement(
                ct.getSimpleName() + "." + e.getKey()));
        }
        return t;
    }

    private int afterHeaderStart(ClassTree hand) {
        int start = (int) positions.getStartPosition(unit, hand);
        Matcher m = Pattern.compile("\\b(class|interface|enum|record)\\s+" + hand.getSimpleName()).matcher(source);
        if (m.find(start)) {
            return m.end();
        }
        return -1;
    }

    private static String headerSig(String header) {
        String h = header.replaceAll("\\b(public|protected|private|static|abstract|final|sealed|non-sealed) ", "");
        return h.replaceAll("\\s+", " ").trim();
    }

    private String initOf(VariableTree v, String type, List<String> typeVars, String fqn, boolean isStatic,
        boolean ownerIsEnum) {
        if (isStatic && v.getInitializer() == null && ownerIsEnum && type.matches("(StringRepresentable\\.)?EnumCodec<.*>")) {
            return "net.minecraft.util.StringRepresentable.fromEnum(" + fqn + "::values)";
        }
        if (isStatic && type.startsWith("ResourceKey<") && type.endsWith(">") && fqn.startsWith("net.minecraft.world.entity.")) {
            String registry = kRegistryByType.get(type.substring(12, type.length() - 1).replaceAll("\\s+", ""));
            if (registry != null) {
                return "net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries." + registry
                    + ", net.minecraft.resources.Identifier.withDefaultNamespace(\"" + v.getName().toString().toLowerCase(Locale.ROOT)
                    + "\"))";
            }
        }
        if (isStatic && v.getInitializer() == null) {
            String lazy = type.startsWith("MapCodec<") ? "mapCodec" : type.startsWith("Codec<") ? "codec"
                : type.startsWith("StreamCodec<") ? "streamCodec" : null;
            if (lazy != null) {
                return "org.basaltmc.bukkit.facade.LazyCodecs." + lazy + "(" + fqn + ".class, \"" + v.getName() + "\")";
            }
        }
        if (kPrimitives.contains(type) && v.getInitializer() != null) {
            String text = text(v.getInitializer()).trim();
            if (text.matches("-?[0-9][0-9a-fA-FxX_.]*[fFdDlL]?|true|false")) {
                return type.equals("byte") || type.equals("short") ? "(" + type + ") " + text : text;
            }
        }
        if (type.equals("String") && v.getInitializer() != null) {
            String text = text(v.getInitializer()).trim();
            if (text.matches("\"[^\"\\\\]*\"")) {
                return text;
            }
        }
        return defaultOf(type, typeVars);
    }

    private String defaultOf(String type, List<String> typeVars) {
        if (type.equals("boolean")) {
            return "false";
        }
        if (kPrimitives.contains(type)) {
            return "0";
        }
        return "null";
    }

    private String headerText(ClassTree ct) {
        long s = positions.getStartPosition(unit, ct);
        int brace = -1;
        int depth = 0;
        String src = source;
        int start = (int) s;
        for (int i = start; i < src.length(); i++) {
            char c = src.charAt(i);
            if (c == '<') {
                depth++;
            } else if (c == '>') {
                depth--;
            } else if (c == '{' && depth == 0) {
                brace = i;
                break;
            }
        }
        String h = clean(src.substring(start, brace));
        if (ct.getKind() == Tree.Kind.ENUM || ct.getKind() == Tree.Kind.RECORD) {
            return h;
        }
        return h;
    }

    private void stubMethod(ClassTree owner, MethodTree mt, StringBuilder sb, String in, String fqn, ClassInfo info,
        boolean isInterface, List<String> typeVars) {
        ModifiersTree m = mt.getModifiers();
        boolean ctor = mt.getReturnType() == null;
        boolean isEnum = owner.getKind() == Tree.Kind.ENUM;
        boolean isRecord = owner.getKind() == Tree.Kind.RECORD;
        boolean isAnnotation = owner.getKind() == Tree.Kind.ANNOTATION_TYPE;
        String header = clean(source.substring((int) positions.getStartPosition(unit, mt),
            (int) positions.getEndPosition(unit, mt)));
        if (header.endsWith(";")) {
            header = header.substring(0, header.length() - 1);
        }
        boolean abstractMember = has(m, Modifier.ABSTRACT) || (isInterface && !has(m, Modifier.DEFAULT)
            && !has(m, Modifier.STATIC));
        if (isInterface && !ctor && List.of("toString/0", "hashCode/0", "equals/1")
            .contains(mt.getName() + "/" + mt.getParameters().size())) {
            return;
        }
        if (forceConcrete && !ctor) {
            int paren = header.indexOf('(');
            header = header.substring(0, paren).replaceAll("\\bfinal ", "") + header.substring(paren);
        }
        if (isEnum || isRecord || forceConcrete && !isAnnotation) {
            header = header.replaceFirst("\\babstract ", "");
            if (forceConcrete && isInterface && abstractMember && !has(m, Modifier.DEFAULT)) {
                header = "default " + header;
            }
            abstractMember = false;
        }
        if (isAnnotation) {
            sb.append(in).append(header).append(";\n");
            return;
        }
        if (abstractMember) {
            sb.append(in).append(header).append(";\n");
            return;
        }
        String name = mt.getName().toString();
        String where = fqn + "#" + (ctor ? "<init>" : name);
        if (!ctor && !isInterface && !isEnum && !forceConcrete && List.of("toString/0", "hashCode/0", "equals/1")
            .contains(name + "/" + mt.getParameters().size())) {
            if (!isRecord) {
                String arg = mt.getParameters().isEmpty() ? "" : mt.getParameters().get(0).getName().toString();
                sb.append(in).append(header).append(" {\n").append(in).append("    return super.").append(name)
                    .append("(").append(arg).append(");\n").append(in).append("}\n");
            }
            return;
        }
        if (!ctor && isEnum && mt.getParameters().isEmpty() && name.equals("getSerializedName")) {
            sb.append(in).append(header).append(" {\n").append(in).append("    return name().toLowerCase(java.util.Locale.ROOT);\n")
                .append(in).append("}\n");
            return;
        }
        if (!ctor && isEnum && fqn.startsWith("net.minecraft.world.entity.") && mt.getParameters().isEmpty()
            && (name.equals("getId") || name.equals("id")) && text(mt.getReturnType()).equals("int")) {
            sb.append(in).append(header).append(" {\n").append(in).append("    return ordinal();\n").append(in).append("}\n");
            return;
        }
        if (!ctor && isEnum && fqn.startsWith("net.minecraft.world.entity.") && name.equals("byId") && has(mt.getModifiers(), Modifier.STATIC)
            && mt.getParameters().size() == 1 && text(mt.getParameters().get(0).getType()).equals("int")
            && text(mt.getReturnType()).equals(owner.getSimpleName().toString())) {
            String id = mt.getParameters().get(0).getName().toString();
            sb.append(in).append(header).append(" {\n").append(in).append("    return values()[Math.floorMod(").append(id)
                .append(", values().length)];\n").append(in).append("}\n");
            return;
        }
        if (!ctor && !isInterface && !isEnum && !isRecord && fqn.startsWith("net.minecraft.world.entity.")
            && !has(mt.getModifiers(), Modifier.STATIC)
            && Set.of("start", "stop", "tick", "canUse", "canContinueToUse", "canStillUse", "checkExtraStartConditions",
                "requiresUpdateEveryTick", "doTick", "requires").contains(name) && inertBody(mt, mt.getReturnType() == null ? "" : text(mt.getReturnType()), name) != null
            && !isEntityLike(info)) {
            String ret0 = mt.getReturnType() == null ? "" : text(mt.getReturnType());
            inertNote("nested-goal-inert", fqn, name);
            sb.append(in).append(header).append(" {\n").append(in).append("    ").append(inertBody(mt, ret0, name)).append("\n").append(in)
                .append("}\n");
            return;
        }
        if (!ctor && !isInterface && !isEnum && !isRecord && isEntityLike(info)) {
            String ret = mt.getReturnType() == null ? "" : text(mt.getReturnType());
            if (ret.equals("void") && kEntityHooks.contains(name)) {
                inertNote("entity-hook-noop", fqn, name);
                sb.append(in).append(header).append(" {\n").append(in).append("}\n");
                return;
            }
            if (name.equals("canMate") && ret.equals("boolean") && mt.getParameters().size() == 1
                && text(mt.getParameters().get(0).getType()).equals("Animal")) {
                String other = mt.getParameters().get(0).getName().toString();
                inertNote("can-mate-love-mode", fqn, name);
                if (owner.getSimpleName().contentEquals("Parrot")) {
                    sb.append(in).append(header).append(" {\n").append(in).append("    return false;\n").append(in).append("}\n");
                    return;
                }
                sb.append(in).append(header).append(" {\n").append(in).append("    return ").append(other).append(" != this && ")
                    .append(other).append(".getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && ")
                    .append("selfApi.isLoveMode() && ").append(other).append(".live() instanceof org.bukkit.entity.Animals otherApi && ")
                    .append("otherApi.isLoveMode();\n").append(in).append("}\n");
                return;
            }
            String bridged = bukkitBridge(owner.getSimpleName().toString(), mt, ret, name);
            if (bridged != null) {
                sb.append(in).append(header).append(" {\n").append(in).append("    ").append(bridged).append("\n").append(in)
                    .append("}\n");
                return;
            }
            String inert = inertBody(mt, ret, name);
            if (inert != null) {
                inertNote(kSuperFallback.contains(name) ? "super-fallback" : "inert-or-default:" + name, fqn, name);
                sb.append(in).append(header).append(" {\n").append(in).append("    ").append(inert).append("\n").append(in).append("}\n");
                return;
            }
            if (has(mt.getModifiers(), Modifier.PROTECTED) && !has(mt.getModifiers(), Modifier.ABSTRACT)) {
                String body = ret.equals("void") ? "" : ret.equals("boolean") ? "    return false;\n"
                    : kPrimitives.contains(ret) ? "    return 0;\n" : null;
                if (body != null) {
                    inertNote("protected-internal-default", fqn, name);
                    sb.append(in).append(header).append(" {\n").append(in).append(body).append(in).append("}\n");
                    return;
                }
            }
            if (ret.equals("InteractionResult") && (name.equals("mobInteract") || name.equals("interact"))) {
                inertNote("interaction-pass", fqn, name);
                sb.append(in).append(header).append(" {\n").append(in)
                    .append("    return net.minecraft.world.InteractionResult.PASS;\n").append(in).append("}\n");
                return;
            }
            if (ret.equals("boolean") && name.matches("check\\w*SpawnRules|isBrightEnoughToSpawn|isDarkEnoughToSpawn")) {
                inertNote("spawn-rules-true", fqn, name);
                sb.append(in).append(header).append(" {\n").append(in).append("    return true;\n").append(in).append("}\n");
                return;
            }
            if (ret.equals("SoundEvent") && name.matches("get\\w*Sound")) {
                String constant = soundConstant(owner.getSimpleName().toString(), name);
                if (constant != null) {
                    sb.append(in).append(header).append(" {\n").append(in).append("    return net.minecraft.sounds.SoundEvents.")
                        .append(constant).append(";\n").append(in).append("}\n");
                    return;
                }
                inertNote("sound-null", fqn, name);
                sb.append(in).append(header).append(" {\n").append(in).append("    return null;\n").append(in).append("}\n");
                return;
            }
        }
        if (!ctor && name.equals("create") && has(mt.getModifiers(), Modifier.STATIC) && fqn.startsWith("net.minecraft.world.entity.")
            && mt.getReturnType() != null && text(mt.getReturnType()).matches("(BehaviorControl|OneShot)<.*>")) {
            inertNote("behavior-factory-inert", fqn, name);
            sb.append(in).append(header).append(" {\n").append(in)
                .append("    return new net.minecraft.world.entity.ai.behavior.InertBehavior<>();\n").append(in).append("}\n");
            return;
        }
        if (!ctor && !isInterface && !isEnum && !isRecord && !has(mt.getModifiers(), Modifier.STATIC)
            && fqn.matches("net\\.minecraft\\.world\\.level\\.block\\.[A-Z]\\w*(\\.\\w+)*")) {
            String ret = text(mt.getReturnType());
            List<String> types = new ArrayList<>();
            List<String> names = new ArrayList<>();
            for (VariableTree p : mt.getParameters()) {
                types.add(text(p.getType()));
                names.add(p.getName().toString());
            }
            if (blockHooks.contains(name + "/" + String.join(",", types) + "/" + ret)) {
                inertNote("block-hook-super", fqn, name);
                sb.append(in).append(header).append(" {\n").append(in).append("    ").append(ret.equals("void") ? "" : "return ")
                    .append("super.").append(name).append("(").append(String.join(", ", names)).append(");\n").append(in).append("}\n");
                return;
            }
        }
        if (!ctor && name.equals("codec") && mt.getParameters().isEmpty() && !has(mt.getModifiers(), Modifier.STATIC)
            && text(mt.getReturnType()).startsWith("MapCodec<")) {
            for (Tree member : owner.getMembers()) {
                if (member instanceof VariableTree v && has(v.getModifiers(), Modifier.STATIC)
                    && (v.getName().contentEquals("MAP_CODEC") || v.getName().contentEquals("CODEC"))
                    && text(v.getType()).equals(text(mt.getReturnType()))) {
                    inertNote("codec-field", fqn, name);
                    sb.append(in).append(header).append(" {\n").append(in).append("    return ").append(v.getName()).append(";\n")
                        .append(in).append("}\n");
                    return;
                }
            }
        }
        if (!ctor && name.equals("validate") && mt.getParameters().size() == 1
            && text(mt.getParameters().get(0).getType()).equals("ValidationContext") && text(mt.getReturnType()).equals("void")
            && fqn.startsWith("net.minecraft.world.level.storage.loot.")) {
            inertNote("loot-validate-noop", fqn, name);
            sb.append(in).append(header).append(" {\n").append(in).append("}\n");
            return;
        }
        if (!ctor && name.equals("performBonemeal") && fqn.startsWith("net.minecraft.world.level.block.") && mt.getParameters().size() == 5
            && text(mt.getParameters().get(0).getType()).equals("ServerLevel") && text(mt.getParameters().get(2).getType()).equals("BlockPos")) {
            String level = mt.getParameters().get(0).getName().toString();
            String pos = mt.getParameters().get(2).getName().toString();
            inertNote("bonemeal-native", fqn, name);
            sb.append(in).append(header).append(" {\n").append(in).append("    org.basaltmc.bukkit.WorldEnvNatives.boneMeal(").append(level)
                .append(".getWorld().getName(), ").append(pos).append(".getX(), ").append(pos).append(".getY(), ").append(pos)
                .append(".getZ());\n").append(in).append("}\n");
            return;
        }
        if (!ctor && name.equals("getReferencedContextParams") && mt.getParameters().isEmpty() && !has(mt.getModifiers(), Modifier.STATIC)
            && fqn.startsWith("net.minecraft.world.level.storage.loot.")) {
            sb.append(in).append(header).append(" {\n").append(in).append("    return java.util.Set.of();\n").append(in).append("}\n");
            return;
        }
        if (!ctor) {
            sb.append(in).append(header).append(" {\n").append(in)
                .append("    throw org.basaltmc.bukkit.facade.NmsStub.unsupported(\"").append(where).append("\");\n")
                .append(in).append("}\n");
            return;
        }
        String call = "";
        if (isRecord) {
            List<String> comps = owner.getMembers().stream().filter(x -> x instanceof VariableTree v
                && (flags(v) & Flags.RECORD) != 0).map(x -> text(((VariableTree) x).getType())).toList();
            List<String> declared = erasedParams(mt);
            List<String> wanted = new ArrayList<>();
            for (String c : comps) {
                wanted.add(erasedType(c, List.of()));
            }
            call = sigMatch(declared, wanted) ? "" : "this(" + recordDefaults(owner, typeVars) + "); ";
        } else if (!isEnum) {
            call = superCall(info);
            String pass = isEntityLike(info) ? passThroughSuper(info, mt) : null;
            if (pass != null) {
                sb.append(in).append(header).append(" {\n").append(in).append("    ").append(pass).append("\n").append(in)
                    .append("}\n");
                return;
            }
        }
        if (ctor && !isRecord && !isEnum) {
            sb.append(in).append(header).append(" {\n").append(in).append("    ").append(call.trim()).append("\n").append(in)
                .append("}\n");
            return;
        }
        sb.append(in).append(header).append(" {\n").append(in).append("    ").append(call).append("throw ")
            .append("org.basaltmc.bukkit.facade.NmsStub.unsupported(\"").append(where).append("\");\n")
            .append(in).append("}\n");
    }

    private String codecConstant(ClassTree owner, String returnType) {
        boolean map = returnType.startsWith("MapCodec");
        for (Tree member : owner.getMembers()) {
            if (member instanceof VariableTree v && has(v.getModifiers(), Modifier.STATIC)
                && (v.getName().contentEquals("CODEC") || v.getName().contentEquals("MAP_CODEC"))
                && text(v.getType()).startsWith("MapCodec") == map && text(v.getType()).contains("Codec")) {
                return v.getName().toString();
            }
        }
        return null;
    }

    private String topOf(ClassInfo c) {
        String rest = c.pkg.isEmpty() ? c.fqn : c.fqn.substring(c.pkg.length() + 1);
        return c.pkg + "/" + rest.split("\\.")[0];
    }

    private static final Set<String> kPassThroughRoots = Set.of("net.minecraft.world.entity.Entity",
        "net.minecraft.world.entity.ai.navigation.PathNavigation", "net.minecraft.world.entity.ai.goal.Goal",
        "net.minecraft.world.entity.ai.control.LookControl", "net.minecraft.world.entity.ai.control.MoveControl",
        "net.minecraft.world.entity.ai.control.JumpControl", "net.minecraft.world.effect.MobEffect",
        "net.minecraft.world.entity.ai.sensing.Sensor");

    private static final Set<String> kBridgeTypes = Set.of("boolean", "int", "float", "double", "long");

    private String bukkitBridge(String simple, MethodTree mt, String ret, String name) {
        Class<?> api;
        try {
            api = Class.forName("org.bukkit.entity." + simple);
        } catch (ReflectiveOperationException | LinkageError e) {
            return null;
        }
        if (!api.isInterface() || mt.getParameters().size() > 1) {
            return null;
        }
        boolean setter = name.startsWith("set") && ret.equals("void") && mt.getParameters().size() == 1;
        boolean getter = !setter && mt.getParameters().isEmpty() && kBridgeTypes.contains(ret);
        if (!setter && !getter) {
            return null;
        }
        String ptype = setter ? text(mt.getParameters().get(0).getType()).trim() : "";
        if (setter && !kBridgeTypes.contains(ptype)) {
            return null;
        }
        String base = name.replaceFirst("^(is|has|can|get|set)", "");
        List<String> names = setter ? List.of(name) : List.of(name, "is" + base, "has" + base, "get" + base);
        for (String candidate : names) {
            for (java.lang.reflect.Method m : api.getMethods()) {
                if (!m.getName().equals(candidate) || java.lang.reflect.Modifier.isStatic(m.getModifiers())) {
                    continue;
                }
                if (setter && m.getParameterCount() == 1 && m.getParameterTypes()[0].getName().equals(ptype)) {
                    String arg = mt.getParameters().get(0).getName().toString();
                    return "if (live() instanceof org.bukkit.entity." + simple + " api) { api." + candidate + "(" + arg + "); }";
                }
                if (getter && m.getParameterCount() == 0 && m.getReturnType().isPrimitive() && !m.getReturnType().equals(void.class)
                    && (m.getReturnType().getName().equals(ret) || (!ret.equals("boolean") && !m.getReturnType().equals(boolean.class)))) {
                    String zero = ret.equals("boolean") ? "false" : "0";
                    return "return live() instanceof org.bukkit.entity." + simple + " api ? (" + ret + ") api." + candidate + "() : " + zero + ";";
                }
            }
        }
        return null;
    }

    private static final java.util.TreeSet<String> kInertLog = new java.util.TreeSet<>();

    private static void inertNote(String rule, String fqn, String name) {
        kInertLog.add(rule + "\t" + fqn + "#" + name);
    }

    static void writeInertReport() throws java.io.IOException {
        String target = System.getProperty("inertReport");
        if (target != null) {
            java.nio.file.Files.write(java.nio.file.Path.of(target), kInertLog);
        }
    }

    private static final Set<String> kSuperFallback = Set.of("removeWhenFarAway", "canBeLeashed", "doHurtTarget",
        "getMovementEmission", "getSoundSource", "shouldRenderAtSqrDistance", "getMaxHeadYRot", "getMaxHeadXRot", "canUseSlot",
        "recreateFromPacket", "thunderHit", "isPickable", "getAmbientSoundInterval", "getWalkTargetValue",
        "hurtServer", "getLootTable", "getSlot", "getMainArm", "isAlliedTo", "getFallSounds", "createNavigation",
        "getType", "checkSpawnObstruction", "getSpawnAnimation", "isInvulnerableTo", "ignoreExplosion", "getPistonPushReaction", "dampensVibrations",
        "isIgnoringBlockTriggers", "getDimensionChangingDelay", "getMoveSimulationType", "getBaseExperienceReward", "isAlwaysExperienceDropper",
        "playAmbientSound", "removeAfterChangingDimensions", "isSensitiveToWater",
        "canBeAffected", "canBreatheUnderwater", "isFlapping", "getBlockSpeedFactor", "getDefaultGravity", "getEffectiveGravity");
    private static final Set<String> kInertGoal = Set.of("start", "stop", "tick");

    private final Set<String> kSoundFields = new HashSet<>();
    private final Map<String, String> kRegistryByType = new HashMap<>();

    private String soundConstant(String owner, String name) {
        String kind = switch (name) {
            case "getAmbientSound" -> "AMBIENT";
            case "getHurtSound" -> "HURT";
            case "getDeathSound" -> "DEATH";
            case "getStepSound" -> "STEP";
            default -> null;
        };
        if (kind == null) {
            return null;
        }
        String constant = owner.replaceAll("([a-z0-9])([A-Z])", "$1_$2").toUpperCase(Locale.ROOT) + "_" + kind;
        return kSoundFields.contains(constant) ? constant : null;
    }

    private String inertBody(MethodTree mt, String ret, String name) {
        if (kInertGoal.contains(name) && ret.equals("void")) {
            return "";
        }
        if (ret.equals("boolean") && List.of("canUse", "canContinueToUse", "canStillUse", "checkExtraStartConditions",
            "requiresUpdateEveryTick", "canBeeUse", "canBeeContinueToUse").contains(name)) {
            return "return false;";
        }
        if (name.equals("doTick") && ret.equals("void")) {
            return "";
        }
        if (name.equals("requires") && ret.startsWith("Set<")) {
            return "return java.util.Set.of();";
        }
        if (name.equals("finalizeSpawn") && ret.equals("SpawnGroupData") && mt.getParameters().size() == 4) {
            return "return " + mt.getParameters().get(3).getName() + ";";
        }
        if (name.equals("createAttributes") && ret.equals("AttributeSupplier.Builder")) {
            return "return net.minecraft.world.entity.Mob.createMobAttributes();";
        }
        if (name.equals("setInitialPos") && mt.getParameters().size() == 3) {
            return "setPos(" + mt.getParameters().get(0).getName() + ", " + mt.getParameters().get(1).getName() + ", "
                + mt.getParameters().get(2).getName() + ");";
        }
        if (name.equals("getMotionDirection") && mt.getParameters().isEmpty()) {
            return "return getDirection();";
        }
        if ((name.equals("push") || name.equals("onPassengerTurned") || name.equals("setInput") || name.equals("setPaddleState")
            || name.equals("setOnRails") || name.equals("setFlipped") || name.equals("setDisplayOffset")) && ret.equals("void")) {
            return "";
        }
        if (name.equals("canBeCollidedWith") || name.equals("isUnderWater") || name.equals("isOnRails") || name.equals("isFlipped")
            || name.equals("isRideable") || name.equals("isFurnace") || name.equals("isFirstTick")) {
            return ret.equals("boolean") ? "return false;" : null;
        }
        if ((name.equals("getBrain") || name.equals("makeBrain")) && ret.startsWith("Brain<")) {
            String args = mt.getParameters().stream().map(p -> p.getName().toString()).collect(Collectors.joining(", "));
            return "@SuppressWarnings(\"unchecked\") " + ret + " made = (" + ret + ") (Brain<?>) super." + name + "(" + args + "); return made;";
        }
        if (name.equals("performRangedAttack") && ret.equals("void") && mt.getParameters().size() == 2) {
            String target = mt.getParameters().get(0).getName().toString();
            String power = mt.getParameters().get(1).getName().toString();
            return "if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && " + target + ".live() instanceof "
                + "org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, " + power + "); }";
        }
        if (name.equals("isFood") && ret.equals("boolean") && mt.getParameters().size() == 1) {
            String stack = mt.getParameters().get(0).getName().toString();
            return "return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem("
                + "org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(" + stack + "));";
        }
        if (name.equals("getBreedOffspring")) {
            return "return null;";
        }
        if (name.equals("getDefaultDimensions") && ret.equals("EntityDimensions")) {
            return "return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());";
        }
        if (kSuperFallback.contains(name)) {
            String args = mt.getParameters().stream().map(p -> p.getName().toString()).collect(Collectors.joining(", "));
            return (ret.equals("void") ? "" : "return ") + "super." + name + "(" + args + ");";
        }
        return null;
    }

    private static final Set<String> kEntityHooks = Set.of("registerGoals", "defineSynchedData", "addAdditionalSaveData",
        "readAdditionalSaveData", "customServerAiStep", "aiStep", "tick", "baseTick", "handleEntityEvent", "playAmbientSound",
        "playHurtSound", "playStepSound", "onSyncedDataUpdated", "updateControlFlags", "tickDeath", "serverAiStep", "spawnAnim",
        "dropCustomDeathLoot", "dropEquipment", "onEffectAdded", "checkDespawn", "playSwimSound", "tickHeadTurn", "rideTick",
        "positionRider", "refreshDimensions", "fixupDimensions", "markHurt", "pushEntities", "doPush", "playAttackSound",
        "onBelowWorld", "handleDamageEvent", "animateHurt", "updateSwimming", "doWaterSplashEffect", "waterSwimSound");

    private boolean isEntityLike(ClassInfo info) {
        ClassInfo cur = info;
        for (int i = 0; i < 32 && cur != null && cur.superName != null; i++) {
            ClassInfo sup = resolve(cur.superName, cur);
            if (sup == null) {
                return false;
            }
            if (kPassThroughRoots.contains(sup.fqn)) {
                return true;
            }
            cur = sup;
        }
        return false;
    }

    private String passThroughSuper(ClassInfo info, MethodTree mt) {
        ClassInfo sup = resolve(info.superName, info);
        List<String> own = erasedParams(mt);
        List<String> names = mt.getParameters().stream().map(p -> p.getName().toString()).toList();
        List<String> pool = new ArrayList<>(sup.ctors);
        if (topOf(sup).equals(topOf(info))) {
            pool.addAll(sup.privCtors);
        }
        String bestCall = null;
        int bestArity = -1;
        for (String ctor : pool) {
            String[] types = ctor.isEmpty() ? new String[0] : ctor.split("\u0001");
            boolean[] used = new boolean[own.size()];
            List<String> args = new ArrayList<>();
            boolean ok = true;
            for (String t : types) {
                String want = erasedType(t, List.of());
                int hit = -1;
                for (int i = 0; i < own.size() && hit < 0; i++) {
                    if (!used[i] && own.get(i).equals(want)) {
                        hit = i;
                    }
                }
                if (hit < 0) {
                    ok = false;
                    break;
                }
                used[hit] = true;
                args.add(names.get(hit));
            }
            if (ok && types.length > bestArity) {
                bestArity = types.length;
                bestCall = "super(" + String.join(", ", args) + ");";
            }
        }
        return bestCall;
    }

    private String superCall(ClassInfo info) {
        if (info.superName == null) {
            return "";
        }
        ClassInfo sup = resolve(info.superName, info);
        if (sup == null) {
            return externalSuperCall(info);
        }
        List<String> pool = new ArrayList<>(sup.ctors);
        if (topOf(sup).equals(topOf(info))) {
            pool.addAll(sup.privCtors);
        }
        if (pool.isEmpty() || pool.contains("")) {
            return "";
        }
        String best = pool.stream().min(Comparator.comparingInt(c -> c.split("\u0001").length)).orElse("");
        List<String> supVars = new ArrayList<>(sup.typeParams);
        String args = Arrays.stream(best.split("\u0001")).map(t -> nullArg(t, supVars, sup))
            .collect(Collectors.joining(", "));
        return "super(" + args + "); ";
    }

    private String externalSuperCall(ClassInfo info) {
        String simple = info.superName.replaceAll("<.*", "").trim();
        List<String> candidates = new ArrayList<>();
        if (simple.contains(".")) {
            candidates.add(simple);
        }
        String head = simple.contains(".") ? simple.substring(0, simple.indexOf('.')) : simple;
        for (String imp : info.imports) {
            if (imp.endsWith("." + head)) {
                candidates.add(imp + simple.substring(head.length()).replace('.', '$'));
            }
        }
        candidates.add("java.lang." + simple);
        for (String c : candidates) {
            try {
                Class<?> k = Class.forName(c, false, NmsManifest.class.getClassLoader());
                java.lang.reflect.Constructor<?> best = null;
                for (var ctor : k.getDeclaredConstructors()) {
                    int m = ctor.getModifiers();
                    if (!java.lang.reflect.Modifier.isPublic(m) && !java.lang.reflect.Modifier.isProtected(m)) {
                        continue;
                    }
                    if (ctor.getParameterCount() == 0) {
                        return "";
                    }
                    if (ctor.getExceptionTypes().length > 0) {
                        continue;
                    }
                    if (best == null || ctor.getParameterCount() < best.getParameterCount()) {
                        best = ctor;
                    }
                }
                if (best == null) {
                    return "";
                }
                StringBuilder args = new StringBuilder();
                for (Class<?> pt : best.getParameterTypes()) {
                    if (args.length() > 0) {
                        args.append(", ");
                    }
                    args.append(pt.isPrimitive() ? (pt == boolean.class ? "false" : "(" + pt.getName() + ") 0")
                        : "(" + pt.getCanonicalName() + ") null");
                }
                return "super(" + args + "); ";
            } catch (ClassNotFoundException | LinkageError e) {
                // try next
            }
        }
        return "";
    }

    private String recordDefaults(ClassTree owner, List<String> typeVars) {
        return owner.getMembers().stream().filter(x -> x instanceof VariableTree v && (flags(v) & Flags.RECORD) != 0)
            .map(x -> nullArg(text(((VariableTree) x).getType()), typeVars, null)).collect(Collectors.joining(", "));
    }
}
