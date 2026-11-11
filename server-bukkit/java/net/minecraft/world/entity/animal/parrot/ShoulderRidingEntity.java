// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.parrot;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.level.storage.TagValueOutput;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.level.Level;
public abstract class ShoulderRidingEntity extends TamableAnimal {
    protected int rideCooldownCounter;
    protected ShoulderRidingEntity(EntityType<? extends ShoulderRidingEntity> type, Level level) {
        super(type, level);
    }
    public boolean setEntityOnShoulder(ServerPlayer player) {
        try (ProblemReporter.ScopedCollector reporter = new ProblemReporter.ScopedCollector(problemPath(), org.slf4j.LoggerFactory.getLogger(ShoulderRidingEntity.class))) {
            TagValueOutput output = TagValueOutput.createWithContext(reporter, registryAccess());
            saveWithoutId(output);
            output.putString("id", getEncodeId());
            if (player.setEntityOnShoulder(output.buildResult())) {
                discard();
                return true;
            }
        }
        return false;
    }
    public void tick() {
    }
    public boolean canSitOnShoulder() {
        return rideCooldownCounter > 100;


    }
}
