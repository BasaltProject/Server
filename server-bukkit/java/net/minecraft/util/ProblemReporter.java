// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import org.slf4j.Logger;

public interface ProblemReporter {
    ProblemReporter DISCARDING = new ProblemReporter() {
        @Override
        public ProblemReporter forChild(PathElement path) {
            return this;
        }

        @Override
        public void report(Problem problem) {}
    };

    ProblemReporter forChild(PathElement path);

    void report(Problem problem);

    class Collector implements ProblemReporter {
        public static final PathElement EMPTY_ROOT = () -> "";
        private final Collector parent;
        private final PathElement path;
        private final Map<PathElement, Collector> children = new LinkedHashMap<>();
        private final List<Problem> problems = new ArrayList<>();

        public Collector() {
            this(EMPTY_ROOT);
        }

        public Collector(PathElement root) {
            this.parent = null;
            this.path = root;
        }

        private Collector(Collector parent, PathElement path) {
            this.parent = parent;
            this.path = path;
        }

        @Override
        public synchronized ProblemReporter forChild(PathElement path) {
            return children.computeIfAbsent(path, element -> new Collector(this, element));
        }

        @Override
        public synchronized void report(Problem problem) {
            problems.add(problem);
        }

        public synchronized boolean isEmpty() {
            return problems.isEmpty() && children.values().stream().allMatch(Collector::isEmpty);
        }

        public synchronized boolean hasFatalProblems() {
            return problems.stream().anyMatch(Problem::isFatal)
                || children.values().stream().anyMatch(Collector::hasFatalProblems);
        }

        private String pathString() {
            StringBuilder out = new StringBuilder();
            List<String> parts = new ArrayList<>();
            for (Collector node = this; node != null; node = node.parent) {
                parts.add(0, node.path.get());
            }
            for (String part : parts) {
                out.append(part);
            }
            return out.toString();
        }

        public synchronized void forEach(BiConsumer<String, Problem> output) {
            for (Problem problem : problems) {
                output.accept(pathString(), problem);
            }
            for (Collector child : children.values()) {
                child.forEach(output);
            }
        }

        public String getReport() {
            StringBuilder report = new StringBuilder();
            forEach((path, problem) -> report.append(path).append(": ").append(problem.description()).append('\n'));
            return report.toString();
        }

        public String getTreeReport() {
            return getReport();
        }
    }

    record FieldPathElement(String name) implements PathElement {
        @Override
        public String get() {
            return "." + name;
        }
    }

    record IndexedFieldPathElement(String name, int index) implements PathElement {
        @Override
        public String get() {
            return "." + name + "[" + index + "]";
        }
    }

    interface PathElement {
        String get();
    }

    interface Problem {
        String description();

        default boolean isFatal() {
            return true;
        }
    }

    record RootFieldPathElement(String name) implements PathElement {
        @Override
        public String get() {
            return name;
        }
    }

    record RootElementPathElement(ResourceKey<?> id) implements PathElement {
        @Override
        public String get() {
            return "{" + id.identifier() + "@" + id.registry() + "}";
        }
    }

    record IndexedPathElement(int index) implements PathElement {
        @Override
        public String get() {
            return "[" + index + "]";
        }
    }

    record MapEntryPathElement(String name, String key) implements PathElement {
        @Override
        public String get() {
            return "." + name + "[" + key + "]";
        }
    }

    record ElementReferencePathElement(ResourceKey<?> id) implements PathElement {
        @Override
        public String get() {
            return "->{" + id.identifier() + "@" + id.registry() + "}";
        }
    }

    record CollectionReferencePathElement(TagKey<?> id) implements PathElement {
        @Override
        public String get() {
            return "->#{" + id.location() + "@" + id.registry() + "}";
        }
    }

    class ScopedCollector extends Collector implements AutoCloseable {
        private final Logger logger;

        public ScopedCollector(Logger logger) {
            this.logger = logger;
        }

        public ScopedCollector(PathElement root, Logger logger) {
            super(root);
            this.logger = logger;
        }

        @Override
        public void close() {
            if (!isEmpty()) {
                logger.warn("Encountered problems:\n{}", getReport());
            }
        }
    }
}
