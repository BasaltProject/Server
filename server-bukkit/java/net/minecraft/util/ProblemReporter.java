package net.minecraft.util;

import org.basaltmc.bukkit.Unsupported;
public interface ProblemReporter {
    public static ProblemReporter DISCARDING = new ProblemReporter() {
        @Override
        public ProblemReporter forChild(ProblemReporter.PathElement path) {
            return this;
        }

        @Override
        public void report(ProblemReporter.Problem problem) {
        }
    };
    public default ProblemReporter forChild(ProblemReporter.PathElement path) {
        throw Unsupported.hit("net.minecraft.util.ProblemReporter.forChild");
    }
    public default void report(ProblemReporter.Problem problem) {
        throw Unsupported.hit("net.minecraft.util.ProblemReporter.report");
    }

    public static class Collector implements ProblemReporter {
        private final java.util.List<Problem> problems = new java.util.concurrent.CopyOnWriteArrayList<>();

        public Collector() {
        }
        public ProblemReporter forChild(ProblemReporter.PathElement path) {
            return this;
        }
        public String getTreeReport() {
            StringBuilder text = new StringBuilder();
            for (Problem problem : problems) {
                text.append(text.length() == 0 ? "" : "; ").append(problem);
            }
            return text.toString();
        }
        public boolean isEmpty() {
            return problems.isEmpty();
        }
        public void report(ProblemReporter.Problem problem) {
            problems.add(problem);
        }
    }

    public record FieldPathElement(String name) implements ProblemReporter.PathElement {
    }

    public record IndexedFieldPathElement(String name, int index) implements ProblemReporter.PathElement {
    }

    public interface PathElement {
    }

    public interface Problem {
    }
}
