// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import java.util.function.Supplier;
import net.minecraft.util.profiling.metrics.MetricCategory;
public final class Profiler {
    static final ProfilerFiller kDiscard = new ProfilerFiller() {
        public void startTick() {
        }
        public void endTick() {
        }
        public void push(String name) {
        }
        public void push(Supplier<String> name) {
        }
        public void pop() {
        }
        public void popPush(String name) {
        }
        public void popPush(Supplier<String> name) {
        }
        public Zone zone(String name) {
            return Zone.INACTIVE;
        }
        public Zone zone(Supplier<String> name) {
            return Zone.INACTIVE;
        }
        public void markForCharting(MetricCategory category) {
        }
        public void incrementCounter(String name, int amount) {
        }
        public void incrementCounter(Supplier<String> name, int amount) {
        }
    };
    private static final ThreadLocal<ProfilerFiller> kActive = new ThreadLocal<>();
    private Profiler() {
    }
    public static Scope use(ProfilerFiller filler) {
        ProfilerFiller previous = kActive.get();
        kActive.set(filler);
        return () -> kActive.set(previous);
    }
    public static ProfilerFiller get() {
        ProfilerFiller active = kActive.get();
        return active == null ? kDiscard : active;
    }
    public interface Scope extends AutoCloseable {
        void close();
    }
}
