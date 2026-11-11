// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import java.util.function.Consumer;
import net.minecraft.world.entity.Entity;
public class EntityTickList {
    private it.unimi.dsi.fastutil.ints.Int2ObjectMap<Entity> active = new it.unimi.dsi.fastutil.ints.Int2ObjectLinkedOpenHashMap<>();
    private it.unimi.dsi.fastutil.ints.Int2ObjectMap<Entity> spare = new it.unimi.dsi.fastutil.ints.Int2ObjectLinkedOpenHashMap<>();
    private it.unimi.dsi.fastutil.ints.Int2ObjectMap<Entity> iterating;
    private void detachFromIteration() {
        if (iterating != active) {
            return;
        }
        spare.clear();
        spare.putAll(active);
        var swap = active;
        active = spare;
        spare = swap;
    }
    public void add(Entity entity) {
        detachFromIteration();
        active.put(entity.getId(), entity);
    }
    public void remove(Entity entity) {
        detachFromIteration();
        active.remove(entity.getId());
    }
    public boolean contains(Entity entity) {
        return active.containsKey(entity.getId());
    }
    public void forEach(Consumer<Entity> output) {
        if (iterating != null) {
            throw new UnsupportedOperationException("Only one concurrent iteration supported");
        }
        iterating = active;
        try {
            for (Entity each : active.values()) {
                output.accept(each);
            }
        } finally {
            iterating = null;
        }
    }
}
