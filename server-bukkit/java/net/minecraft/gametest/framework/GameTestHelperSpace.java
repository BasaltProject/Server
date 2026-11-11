// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.IntPredicate;
import java.util.function.Predicate;
import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import org.basaltmc.bukkit.WorldNatives;

public abstract class GameTestHelperSpace {
    protected final GameTestInfo testInfo;
    private boolean finalCheckAdded;

    protected GameTestHelperSpace(GameTestInfo testInfo) {
        this.testInfo = testInfo;
    }

    public GameTestAssertException assertionException(Component description) {
        return new GameTestAssertException(description, testInfo.getTick());
    }

    public GameTestAssertException assertionException(String descriptionId, Object... arguments) {
        return assertionException(Component.translatable(descriptionId, arguments));
    }

    public GameTestAssertPosException assertionException(BlockPos pos, Component description) {
        return new GameTestAssertPosException(description, absolutePos(pos), pos, testInfo.getTick());
    }

    public GameTestAssertPosException assertionException(BlockPos pos, String descriptionId, Object... arguments) {
        return assertionException(pos, Component.translatable(descriptionId, arguments));
    }

    public ServerLevel getLevel() {
        return testInfo.getLevel();
    }

    public BlockState getBlockState(BlockPos pos) {
        return getLevel().getBlockState(absolutePos(pos));
    }

    public <T extends BlockEntity> T getBlockEntity(BlockPos pos, Class<T> type) {
        BlockEntity entity = getLevel().getBlockEntity(absolutePos(pos));
        if (entity == null) {
            throw assertionException(pos, "test.error.missing_block_entity");
        }
        if (!type.isInstance(entity)) {
            throw assertionException(pos, "test.error.wrong_block_entity", entity.getClass().getSimpleName());
        }
        return type.cast(entity);
    }

    protected Vec3 center(BlockPos pos) {
        return Vec3.atBottomCenterOf(pos);
    }

    private static int steps(Rotation rotation) {
        return rotation.ordinal();
    }

    private static int[] turn(int x, int z, int steps) {
        return switch (Math.floorMod(steps, 4)) {
            case 1 -> new int[] {-z, x};
            case 2 -> new int[] {-x, -z};
            case 3 -> new int[] {z, -x};
            default -> new int[] {x, z};
        };
    }

    private BlockPos structureBase() {
        return testInfo.getTestInstanceBlockEntity().getStructurePos();
    }

    public BlockPos absolutePos(BlockPos relativePos) {
        int[] turned = turn(relativePos.getX(), relativePos.getZ(), steps(getTestRotation()));
        return structureBase().offset(turned[0], relativePos.getY(), turned[1]);
    }

    public BlockPos relativePos(BlockPos absolutePos) {
        BlockPos delta = absolutePos.subtract(structureBase());
        int[] turned = turn(delta.getX(), delta.getZ(), -steps(getTestRotation()));
        return new BlockPos(turned[0], delta.getY(), turned[1]);
    }

    public Vec3 absoluteVec(Vec3 relativeVec) {
        int s = Math.floorMod(steps(getTestRotation()), 4);
        double x = relativeVec.x;
        double z = relativeVec.z;
        double rx = s == 1 ? -z : s == 2 ? -x : s == 3 ? z : x;
        double rz = s == 1 ? x : s == 2 ? -z : s == 3 ? -x : z;
        BlockPos base = structureBase();
        return new Vec3(base.getX() + rx, base.getY() + relativeVec.y, base.getZ() + rz);
    }

    public Vec3 relativeVec(Vec3 absoluteVec) {
        BlockPos base = structureBase();
        double dx = absoluteVec.x - base.getX();
        double dz = absoluteVec.z - base.getZ();
        int s = Math.floorMod(-steps(getTestRotation()), 4);
        double rx = s == 1 ? -dz : s == 2 ? -dx : s == 3 ? dz : dx;
        double rz = s == 1 ? dx : s == 2 ? -dz : s == 3 ? -dx : dz;
        return new Vec3(rx, absoluteVec.y - base.getY(), rz);
    }

    public AABB absoluteAABB(AABB relativeAABB) {
        Vec3 a = absoluteVec(new Vec3(relativeAABB.minX, relativeAABB.minY, relativeAABB.minZ));
        Vec3 b = absoluteVec(new Vec3(relativeAABB.maxX, relativeAABB.maxY, relativeAABB.maxZ));
        return new AABB(a, b);
    }

    public AABB relativeAABB(AABB absoluteAABB) {
        Vec3 a = relativeVec(new Vec3(absoluteAABB.minX, absoluteAABB.minY, absoluteAABB.minZ));
        Vec3 b = relativeVec(new Vec3(absoluteAABB.maxX, absoluteAABB.maxY, absoluteAABB.maxZ));
        return new AABB(a, b);
    }

    public Rotation getTestRotation() {
        return testInfo.getRotation();
    }

    public Direction getTestDirection() {
        Direction direction = Direction.NORTH;
        for (int i = 0; i < steps(getTestRotation()); i++) {
            direction = direction.getClockWise();
        }
        return direction;
    }

    public Direction getAbsoluteDirection(Direction direction) {
        Direction out = direction;
        if (out.getAxis().isHorizontal()) {
            for (int i = 0; i < steps(getTestRotation()); i++) {
                out = out.getClockWise();
            }
        }
        return out;
    }

    public AABB getBounds() {
        return testInfo.getStructureBounds();
    }

    public AABB getBoundsWithPadding() {
        return getBounds().inflate(testInfo.getTest().padding());
    }

    public AABB getRelativeBounds() {
        return relativeAABB(getBounds());
    }

    public void forEveryBlockInStructure(java.util.function.Consumer<BlockPos> forBlock) {
        BoundingBox box = testInfo.getTestInstanceBlockEntity().getStructureBoundingBox();
        BlockPos.betweenClosedStream(box.minX(), box.minY(), box.minZ(), box.maxX(), box.maxY(), box.maxZ())
            .forEach(abs -> forBlock.accept(relativePos(abs)));
    }

    public long getTick() {
        return testInfo.getTick();
    }

    protected <E extends Entity> List<E> entitiesIn(EntityType<E> type, AABB absolute) {
        List<E> found = new ArrayList<>();
        for (Entity entity : getLevel().getEntitiesOfClass(Entity.class, absolute)) {
            if (entity.getType() == type) {
                found.add(type.tryCast(entity));
            }
        }
        return found;
    }

    public void succeed() {
        testInfo.succeed();
    }

    private void ensureSingleFinalCheck() {
        if (finalCheckAdded) {
            throw new IllegalStateException("This test already has a final check");
        }
        finalCheckAdded = true;
    }

    public void succeedIf(Runnable asserter) {
        ensureSingleFinalCheck();
        testInfo.createSequence().thenWaitUntil(0L, asserter).thenSucceed();
    }

    public void succeedWhen(Runnable asserter) {
        ensureSingleFinalCheck();
        testInfo.createSequence().thenWaitUntil(asserter).thenSucceed();
    }

    public void succeedOnTickWhen(int tick, Runnable asserter) {
        ensureSingleFinalCheck();
        testInfo.createSequence().thenWaitUntil(tick, asserter).thenSucceed();
    }

    public void runAtTickTime(long time, Runnable asserter) {
        testInfo.setRunAtTickTime(time, asserter);
    }

    public void runBeforeTestEnd(Runnable asserter) {
        runAtTickTime(testInfo.getTimeoutTicks() - 1, asserter);
    }

    public void runAfterDelay(long ticksToDelay, Runnable whatToRun) {
        runAtTickTime(testInfo.getTick() + ticksToDelay, whatToRun);
    }

    public GameTestSequence startSequence() {
        return testInfo.createSequence();
    }

}
