// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.stream.Stream;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class LightningBolt extends Entity {
    public long seed;
    public LightningBolt(EntityType<? extends LightningBolt> type, Level level) {
        super(type, level);
    }
    public void setVisualOnly(boolean visualOnly) {
        if (live() instanceof org.bukkit.entity.LightningStrike strike) {
            return;
        }
    }
    public SoundSource getSoundSource() {
        return SoundSource.WEATHER;
    }
    public ServerPlayer getCause() {
        return null;
    }
    public void setCause(ServerPlayer cause) {
        return;
    }
    public void tick() {
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return distance < 16384.0;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public int getBlocksSetOnFire() {
        return 0;
    }
    public Stream<Entity> getHitEntities() {
        return Stream.empty();
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
}
