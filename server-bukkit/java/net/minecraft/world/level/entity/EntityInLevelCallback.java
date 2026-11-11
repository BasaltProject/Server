// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import net.minecraft.world.entity.Entity;
public interface EntityInLevelCallback {
    EntityInLevelCallback NULL = new EntityInLevelCallback() {
        public void onMove() {
        }
        public void onRemove(Entity.RemovalReason reason) {
        }
    };
    void onMove();
    void onRemove(Entity.RemovalReason reason);
}
