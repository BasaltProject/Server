package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Samples the stack of every Java thread while the server's profiler runs, and attributes each stack to the plugin
 * whose code is in it, the way spark does. The interpreter stops the world for each stack, so the interval is a
 * trade between detail and the pause it costs.
 */
final class Sampler {
    private static final long kIntervalMillis = 10;
    private static final long kIdlePollMillis = 50;
    private static final long kWindowMillis = 10_000;
    private static final int kMaxWindows = 180;
    private static final int kMaxNodes = 60_000;
    private static final Set<String> kBlocking = Set.of("wait", "sleep", "park", "parkNanos", "park0", "epollWait",
        "kevent", "poll", "poll0", "select", "accept", "accept0", "socketAccept", "socketRead0", "readBytes", "read0",
        "waitFor", "yield", "join0", "await");

    private static final Object lock = new Object();
    private static Map<String, Node> roots = new HashMap<>();
    private static int nodes;
    private static long startedAt;
    private static long rounds;
    private static long lastRoundAt;
    private static boolean running;

    private Sampler() {}

    private static final class Node {
        final String name;
        long hits;
        long self;
        long[] windows = new long[1];
        final Map<String, Node> children = new HashMap<>();

        Node(String name) {
            this.name = name;
        }

        void hit(int window) {
            hits++;
            if (window >= windows.length) {
                windows = java.util.Arrays.copyOf(windows, Math.max(window + 1, windows.length * 2));
            }
            windows[window]++;
        }
    }

    static void start() {
        Thread thread = new Thread(Sampler::loop, "basalt-sampler");
        thread.setDaemon(true);
        thread.start();
    }

    private static void loop() {
        while (true) {
            try {
                if (!Natives.profiling()) {
                    synchronized (lock) {
                        running = false;
                    }
                    Thread.sleep(kIdlePollMillis);
                    continue;
                }
                long now = System.currentTimeMillis();
                synchronized (lock) {
                    if (!running) {
                        running = true;
                        roots = new HashMap<>();
                        nodes = 0;
                        rounds = 0;
                        startedAt = now;
                    }
                    lastRoundAt = now;
                    rounds++;
                }
                sample((int) Math.min((now - startedAt) / kWindowMillis, kMaxWindows - 1));
                Thread.sleep(kIntervalMillis);
            } catch (InterruptedException e) {
                return;
            } catch (Throwable t) {
                // A failed round costs one sample; the profiler must never disturb the server it measures.
            }
        }
    }

    private static boolean idle(StackTraceElement[] trace) {
        StackTraceElement top = trace[0];
        return top.isNativeMethod() && kBlocking.contains(top.getMethodName());
    }

    private static void sample(int window) {
        Thread[] all = new Thread[Thread.activeCount() + 8];
        int count = Thread.enumerate(all);
        Thread self = Thread.currentThread();
        for (int i = 0; i < count; i++) {
            Thread thread = all[i];
            if (thread == self || thread == null) {
                continue;
            }
            StackTraceElement[] trace = thread.getStackTrace();
            if (trace.length == 0 || idle(trace)) {
                continue;
            }
            record("Java " + thread.getName(), trace, window);
        }
    }

    private static void record(String thread, StackTraceElement[] trace, int window) {
        synchronized (lock) {
            Node node = roots.get(thread);
            if (node == null) {
                node = new Node(thread);
                roots.put(thread, node);
                nodes++;
            }
            node.hit(window);
            boolean attributed = false;
            for (int i = trace.length - 1; i >= 0; i--) {
                String owner = attributed ? null : BasaltPluginClassLoader.pluginOfClass(trace[i].getClassName());
                if (owner != null) {
                    attributed = true;
                    node = child(node, "plugin.bukkit." + owner.toLowerCase(java.util.Locale.ROOT), window);
                    if (node == null) {
                        return;
                    }
                }
                node = child(node, trace[i].getClassName() + "." + trace[i].getMethodName(), window);
                if (node == null) {
                    return;
                }
            }
            node.self++;
        }
    }

    private static Node child(Node parent, String name, int window) {
        Node found = parent.children.get(name);
        if (found == null) {
            if (nodes >= kMaxNodes) {
                return null;
            }
            found = new Node(name);
            parent.children.put(name, found);
            nodes++;
        }
        found.hit(window);
        return found;
    }

    /** One JSON object per thread, in the shape of the profiler report's own threads. */
    static String[] report() {
        synchronized (lock) {
            if (rounds == 0) {
                return new String[0];
            }
            double perSample = Math.max(lastRoundAt - startedAt, 1) / (double) rounds;
            List<String> out = new ArrayList<>();
            for (Node thread : roots.values()) {
                StringBuilder json = new StringBuilder();
                json.append("{\"name\":");
                quote(json, thread.name);
                json.append(",\"total_millis\":").append(millis(thread.hits, perSample)).append(",\"root\":");
                json.append("{\"name\":\"root\",\"calls\":0,\"total_millis\":")
                    .append(millis(thread.hits, perSample)).append(",\"self_millis\":0,\"max_millis\":0,")
                    .append("\"window_calls\":[],\"window_millis\":[],\"children\":[");
                node(json, thread, perSample, true);
                json.append("]}}");
                out.add(json.toString());
            }
            return out.toArray(new String[0]);
        }
    }

    private static void node(StringBuilder json, Node parent, double perSample, boolean first) {
        boolean comma = !first;
        for (Node node : parent.children.values()) {
            if (comma) {
                json.append(',');
            }
            comma = true;
            json.append("{\"name\":");
            quote(json, node.name);
            json.append(",\"calls\":").append(node.hits).append(",\"total_millis\":")
                .append(millis(node.hits, perSample)).append(",\"self_millis\":")
                .append(millis(node.self, perSample)).append(",\"max_millis\":0,\"window_calls\":");
            series(json, node.windows, 1);
            json.append(",\"window_millis\":");
            series(json, node.windows, perSample);
            json.append(",\"children\":[");
            node(json, node, perSample, true);
            json.append("]}");
        }
    }

    private static void series(StringBuilder json, long[] windows, double scale) {
        json.append('[');
        for (int i = 0; i < windows.length; i++) {
            if (i > 0) {
                json.append(',');
            }
            if (scale == 1) {
                json.append(windows[i]);
            } else {
                json.append(millis(windows[i], scale));
            }
        }
        json.append(']');
    }

    private static String millis(long hits, double perSample) {
        return String.format(java.util.Locale.ROOT, "%.3f", hits * perSample);
    }

    private static void quote(StringBuilder out, String text) {
        out.append('"');
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == '"' || c == '\\') {
                out.append('\\').append(c);
            } else if (c < 0x20) {
                out.append(' ');
            } else {
                out.append(c);
            }
        }
        out.append('"');
    }
}
