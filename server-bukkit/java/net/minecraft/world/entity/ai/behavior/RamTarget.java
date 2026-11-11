// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import java.util.function.Function;
import java.util.function.ToDoubleFunction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.goat.Goat;
public class RamTarget extends Behavior<Goat> {
    private final Function<Goat, UniformInt> getTimeBetweenRams;
    public static final int TIME_OUT_DURATION = 200;
    public static final float RAM_SPEED_FORCE_FACTOR = 1.65F;
    public RamTarget(Function<Goat, UniformInt> getTimeBetweenRams, TargetingConditions ramTargeting, float speed, ToDoubleFunction<Goat> getKnockbackForce, Function<Goat, SoundEvent> getImpactSound, Function<Goat, SoundEvent> getHornBreakSound) {
        super((java.util.Map) null);
        this.getTimeBetweenRams = getTimeBetweenRams;
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Goat body) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, Goat body, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, Goat body, long timestamp) {
        
    }
    protected void tick(ServerLevel level, Goat body, long timestamp) {
        
    }
    protected void finishRam(ServerLevel level, Goat body) {
        level.broadcastEntityEvent(body, (byte) 59);
        body.getBrain().setMemory(MemoryModuleType.RAM_COOLDOWN_TICKS, getTimeBetweenRams.apply(body).sample(level.getRandom()));
        body.getBrain().eraseMemory(MemoryModuleType.RAM_TARGET);
    }
}
