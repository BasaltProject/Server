// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.RandomStrollGoal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
public class Silverfish extends Monster {
    public Silverfish(EntityType<? extends Silverfish> type, Level level) {
        super(type, level);
    }
    private SilverfishWakeUpFriendsGoal friendsGoal;
    static org.bukkit.Material infestedFor(org.bukkit.Material host) {
        switch (host) {
            case STONE:
            case COBBLESTONE:
            case STONE_BRICKS:
            case MOSSY_STONE_BRICKS:
            case CRACKED_STONE_BRICKS:
            case CHISELED_STONE_BRICKS:
            case DEEPSLATE:
                return org.bukkit.Material.valueOf("INFESTED_" + host.name());
            default:
                return null;
        }
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 8.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.25)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 1.0);
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.silverfish.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.silverfish.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.silverfish.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.silverfish.step"), 0.15F, 1.0F);
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void tick() {
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public static boolean checkSilverfishSpawnRules(EntityType<Silverfish> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    static class SilverfishWakeUpFriendsGoal extends Goal {
        private final Silverfish silverfish;
        private int lookForFriends;
        public SilverfishWakeUpFriendsGoal(Silverfish silverfish) {
            this.silverfish = silverfish;
        }
        public void notifyHurt() {
            if (lookForFriends == 0) {
                lookForFriends = adjustedTickDelay(20);
            }
        }
        public boolean canUse() {
            return lookForFriends > 0;
        }
        public void tick() {
            lookForFriends--;
            if (lookForFriends > 0) {
                return;
            }
            org.bukkit.entity.Entity self = (org.bukkit.entity.Entity) silverfish.getBukkitEntity();
            org.bukkit.World world = self.getWorld();
            BlockPos origin = silverfish.blockPosition();
            for (int dy = 0; dy <= 5; dy = dy > 0 ? -dy : 1 - dy) {
                for (int dx = 0; dx <= 10; dx = dx > 0 ? -dx : 1 - dx) {
                    for (int dz = 0; dz <= 10; dz = dz > 0 ? -dz : 1 - dz) {
                        org.bukkit.block.Block block = world.getBlockAt(origin.getX() + dx, origin.getY() + dy, origin.getZ() + dz);
                        if (block.getType().name().startsWith("INFESTED_")) {
                            if (Boolean.TRUE.equals(world.getGameRuleValue(org.bukkit.GameRule.MOB_GRIEFING))) {
                                block.breakNaturally();
                            } else {
                                block.setType(org.bukkit.Material.valueOf(block.getType().name().substring("INFESTED_".length())));
                            }
                            if (silverfish.getRandom().nextBoolean()) {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    static class SilverfishMergeWithStoneGoal extends RandomStrollGoal {
        private final Silverfish silverfish;
        private Direction selectedDirection;
        private boolean doMerge;
        public SilverfishMergeWithStoneGoal(Silverfish silverfish) {
            super(silverfish, 1.0, 10);
            this.silverfish = silverfish;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canUse() {
            if (silverfish.getTarget() != null || !silverfish.getNavigation().isDone()) {
                return false;
            }
            org.bukkit.World world = ((org.bukkit.entity.Entity) silverfish.getBukkitEntity()).getWorld();
            if (Boolean.TRUE.equals(world.getGameRuleValue(org.bukkit.GameRule.MOB_GRIEFING)) && silverfish.getRandom().nextInt(reducedTickDelay(10)) == 0) {
                selectedDirection = Direction.getRandom(silverfish.getRandom());
                BlockPos pos = silverfish.blockPosition().offset(selectedDirection.getStepX(), selectedDirection.getStepY(), selectedDirection.getStepZ());
                if (infestedFor(world.getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getType()) != null) {
                    doMerge = true;
                    return true;
                }
            }
            doMerge = false;
            return super.canUse();
        }
        public boolean canContinueToUse() {
            return !doMerge && super.canContinueToUse();
        }
        public void start() {
            if (!doMerge) {
                super.start();
                return;
            }
            org.bukkit.World world = ((org.bukkit.entity.Entity) silverfish.getBukkitEntity()).getWorld();
            BlockPos pos = silverfish.blockPosition().offset(selectedDirection.getStepX(), selectedDirection.getStepY(), selectedDirection.getStepZ());
            org.bukkit.block.Block block = world.getBlockAt(pos.getX(), pos.getY(), pos.getZ());
            org.bukkit.Material infested = infestedFor(block.getType());
            if (infested != null) {
                block.setType(infested);
                silverfish.discard();
            }
        }
    }
}
