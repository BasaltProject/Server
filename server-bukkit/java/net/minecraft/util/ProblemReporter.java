package net.minecraft.util;

import org.basaltmc.bukkit.Unsupported;
public interface ProblemReporter {
    public static ProblemReporter DISCARDING = null;
    public default ProblemReporter forChild(ProblemReporter.PathElement path) {
        throw Unsupported.hit("net.minecraft.util.ProblemReporter.forChild");
    }
    public default void report(ProblemReporter.Problem problem) {
        throw Unsupported.hit("net.minecraft.util.ProblemReporter.report");
    }

    public static class Collector implements ProblemReporter {
        public Collector() {
        }
        public ProblemReporter forChild(ProblemReporter.PathElement path) {
            throw Unsupported.hit("net.minecraft.util.ProblemReporter.Collector.forChild");
        }
        public String getTreeReport() {
            throw Unsupported.hit("net.minecraft.util.ProblemReporter.Collector.getTreeReport");
        }
        public boolean isEmpty() {
            throw Unsupported.hit("net.minecraft.util.ProblemReporter.Collector.isEmpty");
        }
        public void report(ProblemReporter.Problem problem) {
            throw Unsupported.hit("net.minecraft.util.ProblemReporter.Collector.report");
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
