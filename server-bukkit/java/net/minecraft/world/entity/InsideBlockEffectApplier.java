// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.function.Consumer;
public interface InsideBlockEffectApplier {
    InsideBlockEffectApplier NOOP = new InsideBlockEffectApplier() {
        public void apply(InsideBlockEffectType type) {
        }
        public void runBefore(InsideBlockEffectType type, Consumer<Entity> effect) {
        }
        public void runAfter(InsideBlockEffectType type, Consumer<Entity> effect) {
        }
    };
    void apply(InsideBlockEffectType type);
    void runBefore(InsideBlockEffectType type, Consumer<Entity> effect);
    void runAfter(InsideBlockEffectType type, Consumer<Entity> effect);
    public static class StepBasedCollector implements InsideBlockEffectApplier {
        private static final InsideBlockEffectType[] kOrder = InsideBlockEffectType.values();
        private final java.util.Set<InsideBlockEffectType> pending = java.util.EnumSet.noneOf(InsideBlockEffectType.class);
        private final java.util.Map<InsideBlockEffectType, java.util.List<Consumer<Entity>>> before = new java.util.EnumMap<>(InsideBlockEffectType.class);
        private final java.util.Map<InsideBlockEffectType, java.util.List<Consumer<Entity>>> after = new java.util.EnumMap<>(InsideBlockEffectType.class);
        private final java.util.List<Consumer<Entity>> queue = new java.util.ArrayList<>();
        private int currentStep = -1;
        private void flush() {
            for (InsideBlockEffectType type : kOrder) {
                java.util.List<Consumer<Entity>> head = before.remove(type);
                if (head != null) {
                    queue.addAll(head);
                }
                if (pending.remove(type)) {
                    queue.add(type.effect());
                }
                java.util.List<Consumer<Entity>> tail = after.remove(type);
                if (tail != null) {
                    queue.addAll(tail);
                }
            }
        }

        public void advanceStep(int step) {
            if (currentStep != step) {
                currentStep = step;
                flush();
            }
        }
        public void applyAndClear(Entity entity) {
            flush();
            for (Consumer<Entity> action : queue) {
                if (!entity.isAlive()) {
                    break;
                }
                action.accept(entity);
            }
            queue.clear();
            currentStep = -1;
        }
        public void apply(InsideBlockEffectType type) {
            pending.add(type);
        }
        public void runBefore(InsideBlockEffectType type, Consumer<Entity> effect) {
            before.computeIfAbsent(type, key -> new java.util.ArrayList<>()).add(effect);
        }
        public void runAfter(InsideBlockEffectType type, Consumer<Entity> effect) {
            after.computeIfAbsent(type, key -> new java.util.ArrayList<>()).add(effect);
        }
    }
}
