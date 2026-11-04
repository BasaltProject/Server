// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import java.util.function.Supplier;
import net.minecraft.util.profiling.metrics.MetricCategory;
public interface ProfilerFiller {
    String ROOT = "root";
    void startTick();
    void endTick();
    void push(String name);
    void push(Supplier<String> name);
    void pop();
    void popPush(String name);
    void popPush(Supplier<String> name);
    default void addZoneText(String text) {
    }
    default void addZoneValue(long value) {
    }
    default void setZoneColor(int color) {
    }
    default Zone zone(String name) {
        push(name);
        return new Zone(this);
    }
    default Zone zone(Supplier<String> name) {
        push(name);
        return new Zone(this);
    }
    void markForCharting(MetricCategory category);
    default void incrementCounter(String name) {
        incrementCounter(name, 1);
    }
    void incrementCounter(String name, int amount);
    default void incrementCounter(Supplier<String> name) {
        incrementCounter(name, 1);
    }
    void incrementCounter(Supplier<String> name, int amount);
    static ProfilerFiller combine(ProfilerFiller first, ProfilerFiller second) {
        return first == Profiler.kDiscard ? second : second == Profiler.kDiscard ? first : new CombinedProfileFiller(first, second);
    }
    public static class CombinedProfileFiller implements ProfilerFiller {
        private final ProfilerFiller first;
        private final ProfilerFiller second;
        public CombinedProfileFiller(ProfilerFiller first, ProfilerFiller second) {
            this.first = first;
            this.second = second;
        }
        public void startTick() {
            first.startTick();
            second.startTick();
        }
        public void endTick() {
            first.endTick();
            second.endTick();
        }
        public void push(String name) {
            first.push(name);
            second.push(name);
        }
        public void push(Supplier<String> name) {
            first.push(name);
            second.push(name);
        }
        public void markForCharting(MetricCategory category) {
            first.markForCharting(category);
            second.markForCharting(category);
        }
        public void pop() {
            first.pop();
            second.pop();
        }
        public void popPush(String name) {
            first.popPush(name);
            second.popPush(name);
        }
        public void popPush(Supplier<String> name) {
            first.popPush(name);
            second.popPush(name);
        }
        public void incrementCounter(String name, int amount) {
            first.incrementCounter(name, amount);
            second.incrementCounter(name, amount);
        }
        public void incrementCounter(Supplier<String> name, int amount) {
            first.incrementCounter(name, amount);
            second.incrementCounter(name, amount);
        }
    }
}
