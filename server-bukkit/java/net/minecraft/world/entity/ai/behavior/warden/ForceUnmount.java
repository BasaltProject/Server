// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.warden;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.Behavior;

public class ForceUnmount extends Behavior<LivingEntity> {
    public ForceUnmount() {
        super(Map.of());
    }

    protected boolean checkExtraStartConditions(ServerLevel level, LivingEntity body) {
        return body.isPassenger();
    }

    protected void start(ServerLevel level, LivingEntity body, long timestamp) {
        body.unRide();
    }
}
