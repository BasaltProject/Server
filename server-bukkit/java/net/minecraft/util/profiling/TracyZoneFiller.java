// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import java.util.function.Supplier;
import net.minecraft.util.profiling.metrics.MetricCategory;
public class TracyZoneFiller implements ProfilerFiller {
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
    public void markForCharting(MetricCategory category) {
    }
    public void incrementCounter(String name, int amount) {
    }
    public void incrementCounter(Supplier<String> name, int amount) {
    }
    public void addZoneText(String text) {
    }
    public void addZoneValue(long value) {
    }
    public void setZoneColor(int color) {
    }
    static final class PlotAndValue {
        private PlotAndValue(String name) {
            
        }
        public void set(int value) {
        }
        public void add(int amount) {
        }
    }
}
