// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import it.unimi.dsi.fastutil.ints.IntOpenHashSet;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Mob;

public class Sensing {
    private final Mob mob;
    private final IntOpenHashSet seen = new IntOpenHashSet();
    private final IntOpenHashSet unseen = new IntOpenHashSet();

    public Sensing(Mob mob) {
        this.mob = mob;
    }

    public void tick() {
        seen.clear();
        unseen.clear();
    }

    public boolean hasLineOfSight(Entity target) {
        int id = target.getId();
        if (seen.contains(id)) {
            return true;
        }
        if (unseen.contains(id)) {
            return false;
        }
        boolean visible = mob.hasLineOfSight(target);
        (visible ? seen : unseen).add(id);
        return visible;
    }
}
