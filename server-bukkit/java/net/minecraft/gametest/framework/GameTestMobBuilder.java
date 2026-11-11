// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.phys.Vec3;

public class GameTestMobBuilder<E extends Mob> extends GameTestEntityBuilder<E> {
    private boolean noFreeWill;

    public GameTestMobBuilder(GameTestHelper testHelper, EntityType<E> entityType, Vec3 position) {
        super(testHelper, entityType, position);
    }

    public GameTestMobBuilder<E> withNoFreeWill() {
        noFreeWill = true;
        return this;
    }

    @Override
    protected void prepare(E mob) {
        super.prepare(mob);
        if (noFreeWill) {
            mob.removeFreeWill();
        }
    }

    @Override
    public E spawn() {
        return super.spawn();
    }
}
