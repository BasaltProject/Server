// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
public abstract class Monster extends PathfinderMob implements Enemy {
    protected Monster(EntityType<? extends Monster> type, Level level) {
        super(type, level);
    }
    public SoundSource getSoundSource() {
        return SoundSource.HOSTILE;
    }
    public void aiStep() {
    }
    protected void updateNoActionTime() {
        return;
    }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected SoundEvent getSwimSplashSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return -level.getPathfindingCostFromLightLevels(pos);
    }
    public static boolean isDarkEnoughToSpawn(ServerLevelAccessor level, BlockPos pos, RandomSource random) {
        if (org.basaltmc.bukkit.facade.SpawnLight.sky(level, pos) > random.nextInt(32)) {
            return false;
        }
        int light = org.basaltmc.bukkit.facade.SpawnLight.raw(level, pos, org.basaltmc.bukkit.facade.SpawnLight.thundering(level) ? 10 : 0);
        return light <= random.nextInt(8);
    }
    public static boolean checkMonsterSpawnRules(EntityType<? extends Mob> type, ServerLevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return !org.basaltmc.bukkit.facade.SpawnLight.peaceful(level) && (EntitySpawnReason.ignoresLightRequirements(spawnReason) || isDarkEnoughToSpawn(level, pos, random))
            && Mob.checkMobSpawnRules(type, level, spawnReason, pos, random);
    }
    public static boolean checkAnyLightMonsterSpawnRules(EntityType<? extends Monster> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return !org.basaltmc.bukkit.facade.SpawnLight.peaceful(level) && Mob.checkMobSpawnRules(type, level, spawnReason, pos, random);
    }
    public static boolean checkSurfaceMonstersSpawnRules(EntityType<? extends Mob> type, ServerLevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return checkMonsterSpawnRules(type, level, spawnReason, pos, random) && (EntitySpawnReason.isSpawner(spawnReason) || org.basaltmc.bukkit.facade.SpawnLight.sky(level, pos) >= 15);
    }
    public static AttributeSupplier.Builder createMonsterAttributes() {
        return Mob.createMobAttributes().add(Attributes.ATTACK_DAMAGE);
    }
    public boolean shouldDropExperience() {
        return true;
    }
    protected boolean shouldDropLoot(ServerLevel level) {
        return true;
    }
    public boolean isPreventingPlayerRest(ServerLevel level, Player player) {
        return true;
    }
}
