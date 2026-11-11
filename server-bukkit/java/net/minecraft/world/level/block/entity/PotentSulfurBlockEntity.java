// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import java.util.List;
import java.util.function.Predicate;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.core.HolderLookup;
import net.minecraft.tags.EntityTypeTags;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.PotentSulfurBlock;
import net.minecraft.world.level.block.state.properties.PotentSulfurState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.HitResult;
import org.basaltmc.bukkit.facade.ValueIo;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class PotentSulfurBlockEntity extends BlockEntity {
    private static final Predicate<Entity> kAffected = e -> !e.isSpectator() && e.isAlive();
    public PotentSulfurBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
        waitingCountdown = nbt.getIntOr("countdown", -1);
    }
    private static boolean passable(BlockState state, Level level, BlockPos pos) {
        return state.isAir() || state.is(Blocks.WATER) || state.getCollisionShape(level, pos).isEmpty();
    }
    private static BlockPos gasSource(Level level, BlockPos origin) {
        BlockPos.MutableBlockPos cursor = origin.above(1).mutable();
        int top = origin.getY() + 5;
        while (cursor.getY() <= top) {
            BlockState state = level.getBlockState(cursor);
            boolean wet = level.getFluidState(cursor).isSourceOfType(Fluids.WATER);
            if (wet && (state.is(Blocks.WATER) || passable(state, level, cursor))) {
                cursor.move(Direction.UP);
            } else {
                return state.isAir() || passable(state, level, cursor) ? cursor.immutable() : null;
            }
        }
        return null;
    }
    private static int clearHeight(Level level, BlockPos base, int waterBlocks) {
        int limit = 6 * waterBlocks;
        for (int i = 0; i < limit; i++) {
            BlockPos at = base.above(i);
            if (!passable(level.getBlockState(at), level, at)) {
                return i;
            }
        }
        return limit;
    }
    private static boolean isWaterAt(Level level, Vec3 pos) {
        return level.getFluidState(BlockPos.containing(pos)).isSourceOfType(Fluids.WATER);
    }
    private static final BlockEntityTicker<PotentSulfurBlockEntity> kNoop = (level, pos, state, entity) -> {};
    static {
        SERVER_NAUSEA_EFFECT_TICKER = (level, pos, state, entity) -> {
            if (level.getGameTime() % 10L != 0L) {
                return;
            }
            BlockPos source = gasSource(level, pos);
            if (source == null) {
                return;
            }
            AABB zone = new AABB(source).inflate(2.5, 0.0, 2.5);
            for (LivingEntity living : level.getEntitiesOfClass(LivingEntity.class, zone, kAffected)) {
                if (canBeReachedByNoxiousGas(level, source, living.getEyePosition())) {
                    living.addEffect(new MobEffectInstance(MobEffects.NAUSEA, 80, 0, true, true));
                }
            }
        };
        CLIENT_NOXIOUS_GAS_TICKER = kNoop;
        CLIENT_GEYSER_PLUME_TICKER = sound -> kNoop;
        SERVER_WAITING_COUNTDOWN_TICKER = (level, pos, state, entity) -> {
            if (level.getGameTime() % 20L != 0L) {
                return;
            }
            BlockPos source = gasSource(level, pos);
            if (source == null) {
                return;
            }
            boolean dormant = state.getValue(PotentSulfurBlock.STATE) == PotentSulfurState.DORMANT;
            if (entity.waitingCountdown <= 0) {
                int water = source.getY() - pos.getY() - 1;
                RandomSource rng = geyserPositional((ServerLevel) level, pos);
                if (dormant) {
                    entity.waitingCountdown = 10 * (water - 1) + rng.nextIntBetweenInclusive(15, 30);
                } else {
                    rng.nextInt();
                    entity.waitingCountdown = water - 1 + rng.nextIntBetweenInclusive(1, 2);
                }
            }
            if (entity.waitingCountdown > 0) {
                entity.waitingCountdown--;
            }
            if (entity.waitingCountdown == 0) {
                PotentSulfurState next = dormant ? PotentSulfurState.ERUPTING : PotentSulfurState.DORMANT;
                level.setBlockAndUpdate(pos, state.setValue(PotentSulfurBlock.STATE, next));
                if (next == PotentSulfurState.DORMANT) {
                    level.gameEvent(GameEvent.BLOCK_DEACTIVATE, pos, GameEvent.Context.of(state));
                }
            }
        };
        LAUNCH_ENTITY_TICKER = (level, pos, state, entity) -> {
            BlockPos source = gasSource(level, pos);
            if (source == null) {
                return;
            }
            int water = source.getY() - pos.getY() - 1;
            int height = clearHeight(level, pos.above(), water);
            AABB column = new AABB(pos.above()).expandTowards(0.0, height - 1, 0.0);
            for (Entity launched : level.getEntitiesOfClass(Entity.class, column, kAffected)) {
                Vec3 motion = launched.getDeltaMovement();
                launched.checkFallDistanceAccumulation();
                if (!launched.canSimulateMovement() || launched instanceof Player player && player.getAbilities().flying) {
                    continue;
                }
                if (!launched.isPassenger() && !launched.is(EntityTypeTags.NOT_AFFECTED_BY_GEYSERS) && motion.y < 0.3 + water * 0.1) {
                    launched.addDeltaMovement(new Vec3(0.0, 0.2, 0.0));
                }
            }
        };
    }
    public static final float EFFECT_RANGE = 3.0F;
    public static final int PARTICLE_FREQUENCY_TICKS = 20;
    public static final int SOUND_FREQUENCY_TICKS = 40;
    public int waitingCountdown = -1;
    public long eruptionTick = -1L;
    public static BlockEntityTicker<PotentSulfurBlockEntity> SERVER_NAUSEA_EFFECT_TICKER;
    public static BlockEntityTicker<PotentSulfurBlockEntity> CLIENT_NOXIOUS_GAS_TICKER;
    public static Function<SoundEvent, BlockEntityTicker<PotentSulfurBlockEntity>> CLIENT_GEYSER_PLUME_TICKER;
    public static BlockEntityTicker<PotentSulfurBlockEntity> SERVER_WAITING_COUNTDOWN_TICKER;
    public static final long GEYSER_SALT = -904011478L;
    public static BlockEntityTicker<PotentSulfurBlockEntity> LAUNCH_ENTITY_TICKER;
    public PotentSulfurBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("potent_sulfur"), worldPosition, blockState);
    }
    protected void saveAdditional(ValueOutput output) {
        CompoundTag tag = saveWithoutMetadata((HolderLookup.Provider) null);
        tag.putInt("countdown", waitingCountdown);
        ValueIo.write(output, tag);
    }
    protected void loadAdditional(ValueInput input) {
        CompoundTag tag = ValueIo.read(input);
        waitingCountdown = tag.getIntOr("countdown", waitingCountdown);
        load(tag);
    }
    public void setLevel(Level level) {
        super.setLevel(level);
        if (eruptionTick == -1L) {
            eruptionTick = level.getGameTime();
        }
    }
    public void resetCountdown() {
        waitingCountdown = -1;
    }
    public static RandomSource geyserPositional(ServerLevel level, BlockPos pos) {
        return new XoroshiroRandomSource(level.getSeed() ^ GEYSER_SALT).forkPositional().at(pos);
    }
    public static boolean canBeReachedByNoxiousGas(Level level, BlockPos sourceBlock, Vec3 pos) {
        BlockPos at = BlockPos.containing(pos);
        if (!passable(level.getBlockState(at), level, at) || pos.distanceToSqr(Vec3.atCenterOf(sourceBlock)) > 9.0) {
            return false;
        }
        Vec3 belowSource = Vec3.atCenterOf(sourceBlock.below());
        Vec3 belowPos = pos.with(Direction.Axis.Y, pos.y - 1.0);
        if (!isWaterAt(level, belowPos)) {
            return false;
        }
        HitResult hit = level.clip(new ClipContext(belowSource, belowPos, ClipContext.Block.COLLIDER, ClipContext.Fluid.NONE, (Entity) null));
        return hit.getType() != HitResult.Type.BLOCK;


    }
}
