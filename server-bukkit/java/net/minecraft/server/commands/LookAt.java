// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;
public interface LookAt {
    void perform(CommandSourceStack source, Entity target);
    public static record LookAtEntity(Entity entity, EntityAnchorArgument.Anchor anchor) implements LookAt {
        public void perform(CommandSourceStack source, Entity target) {
            if (target instanceof net.minecraft.server.level.ServerPlayer player) {
                player.lookAt(source.getAnchor(), entity, anchor);
            } else {
                target.lookAt(source.getAnchor(), anchor.apply(entity));
            }
        }
    }
    public static record LookAtPosition(Vec3 position) implements LookAt {
        public void perform(CommandSourceStack source, Entity target) {
            target.lookAt(source.getAnchor(), position);
        }
    }
}
