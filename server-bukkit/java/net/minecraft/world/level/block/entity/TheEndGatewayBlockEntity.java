// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.util.Mth;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.chunk.LevelChunk;
import org.basaltmc.bukkit.RestNatives;
import org.basaltmc.bukkit.facade.ValueIo;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class TheEndGatewayBlockEntity extends TheEndPortalBlockEntity {
    private static final int kSpawnTime = 200;
    private static final int kCooldownTime = 40;
    private long age;
    private int teleportCooldown;
    private BlockPos exitPortal;
    private boolean exactTeleport;
    public TheEndGatewayBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
        readState(nbt);
    }
    private void readState(CompoundTag tag) {
        age = tag.getLongOr("Age", 0L);
        exactTeleport = tag.getBooleanOr("ExactTeleport", false);
        int[] at = tag.getIntArray("exit_portal").orElse(null);
        exitPortal = at != null && at.length == 3 && Level.isInSpawnableBounds(new BlockPos(at[0], at[1], at[2])) ? new BlockPos(at[0], at[1], at[2]) : null;
    }
    private void writeState(CompoundTag tag) {
        tag.putLong("Age", age);
        if (exitPortal != null) {
            tag.putIntArray("exit_portal", new int[] {exitPortal.getX(), exitPortal.getY(), exitPortal.getZ()});
        }
        if (exactTeleport) {
            tag.putBoolean("ExactTeleport", true);
        }
    }
    private static BlockPos tallestAround(BlockGetter level, BlockPos around, int dist, boolean allowBedrock) {
        BlockPos best = null;
        for (int dx = -dist; dx <= dist; dx++) {
            for (int dz = -dist; dz <= dist; dz++) {
                if (dx == 0 && dz == 0 && !allowBedrock) {
                    continue;
                }
                int floor = best == null ? level.getMinY() : best.getY();
                for (int y = level.getMaxY(); y > floor; y--) {
                    BlockPos probe = new BlockPos(around.getX() + dx, y, around.getZ() + dz);
                    BlockState there = level.getBlockState(probe);
                    if (there.isCollisionShapeFullBlock(level, probe) && (allowBedrock || !there.is(Blocks.BEDROCK))) {
                        best = probe;
                        break;
                    }
                }
            }
        }
        return best == null ? around : best;
    }
    private static LevelChunk chunkAt(Level level, Vec3 pos) {
        return level.getChunk(Mth.floor(pos.x / 16.0), Mth.floor(pos.z / 16.0));
    }
    private static boolean emptyChunk(ServerLevel level, Vec3 pos) {
        return chunkAt(level, pos).getHighestFilledSectionIndex() == -1;
    }
    private static BlockPos validSpawnIn(LevelChunk chunk) {
        ChunkPos where = chunk.getPos();
        BlockPos from = new BlockPos(where.getMinBlockX(), 30, where.getMinBlockZ());
        BlockPos to = new BlockPos(where.getMaxBlockX(), chunk.getHighestSectionPosition() + 15, where.getMaxBlockZ());
        BlockPos best = null;
        double bestDist = 0.0;
        for (BlockPos pos : BlockPos.betweenClosed(from, to)) {
            BlockPos up1 = pos.above();
            BlockPos up2 = pos.above(2);
            if (chunk.getBlockState(pos).is(Blocks.END_STONE) && !chunk.getBlockState(up1).isCollisionShapeFullBlock(chunk, up1)
                && !chunk.getBlockState(up2).isCollisionShapeFullBlock(chunk, up2)) {
                double dist = pos.distToCenterSqr(0.0, 0.0, 0.0);
                if (best == null || dist < bestDist) {
                    best = pos.immutable();
                    bestDist = dist;
                }
            }
        }
        return best;
    }
    private static Vec3 tentativeExitXZ(ServerLevel level, BlockPos gateway) {
        Vec3 heading = new Vec3(gateway.getX(), 0.0, gateway.getZ()).normalize();
        Vec3 probe = heading.scale(1024.0);
        for (int budget = 16; !emptyChunk(level, probe) && budget-- > 0; probe = probe.add(heading.scale(-16.0))) {
            continue;
        }
        for (int budget = 16; emptyChunk(level, probe) && budget-- > 0; probe = probe.add(heading.scale(16.0))) {
            continue;
        }
        return probe;
    }
    private static BlockPos findOrCreateTeleportPos(ServerLevel level, BlockPos gateway) {
        Vec3 xz = tentativeExitXZ(level, gateway);
        BlockPos found = validSpawnIn(chunkAt(level, xz));
        if (found == null) {
            found = BlockPos.containing(xz.x + 0.5, 75.0, xz.z + 0.5);
            RestNatives.placeFeature(level.getWorld().getName(), "minecraft:end_island", found.getX(), found.getY(), found.getZ(), found.asLong(), true);
        }
        return tallestAround(level, found, 16, true);
    }
    public TheEndGatewayBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("end_gateway"), worldPosition, blockState);
    }
    protected void saveAdditional(ValueOutput output) {
        CompoundTag tag = saveWithoutMetadata((HolderLookup.Provider) null);
        writeState(tag);
        ValueIo.write(output, tag);
    }
    protected void loadAdditional(ValueInput input) {
        CompoundTag tag = ValueIo.read(input);
        readState(tag);
        load(tag);
    }
    public static void beamAnimationTick(Level level, BlockPos pos, BlockState state, TheEndGatewayBlockEntity entity) {
        entity.age++;
        if (entity.isCoolingDown()) {
            entity.teleportCooldown--;
        }
    }
    public static void portalTick(Level level, BlockPos pos, BlockState state, TheEndGatewayBlockEntity entity) {
        boolean wasSpawning = entity.isSpawning();
        boolean wasCooling = entity.isCoolingDown();
        entity.age++;
        if (wasCooling) {
            entity.teleportCooldown--;
        } else if (entity.age % 2400L == 0L) {
            triggerCooldown(level, pos, state, entity);
        }
        if (wasSpawning != entity.isSpawning() || wasCooling != entity.isCoolingDown()) {
            CompoundTag tag = entity.nbt();
            entity.writeState(tag);
            entity.load(tag);
        }
    }
    public boolean isSpawning() {
        return age < kSpawnTime;
    }
    public boolean isCoolingDown() {
        return teleportCooldown > 0;
    }
    public float getSpawnPercent(float a) {
        return Mth.clamp((age + a) / kSpawnTime, 0.0F, 1.0F);
    }
    public float getCooldownPercent(float a) {
        return 1.0F - Mth.clamp((teleportCooldown - a) / kCooldownTime, 0.0F, 1.0F);
    }
    public ClientboundBlockEntityDataPacket getUpdatePacket() {
        return ClientboundBlockEntityDataPacket.create(this);
    }
    public CompoundTag getUpdateTag(HolderLookup.Provider registries) {
        return saveCustomOnly(registries);
    }
    public static void triggerCooldown(Level level, BlockPos pos, BlockState blockState, TheEndGatewayBlockEntity entity) {
        if (!level.isClientSide()) {
            entity.teleportCooldown = kCooldownTime;
            level.blockEvent(pos, blockState.getBlock(), 1, 0);
        }
    }
    public boolean triggerEvent(int b0, int b1) {
        if (b0 == 1) {
            teleportCooldown = kCooldownTime;
            return true;
        }
        return super.triggerEvent(b0, b1);
    }
    public Vec3 getPortalPosition(ServerLevel currentLevel, BlockPos portalEntryPos) {
        if (exitPortal == null && currentLevel.dimension() == Level.END) {
            BlockPos created = findOrCreateTeleportPos(currentLevel, portalEntryPos).above(10);
            RestNatives.placeEndGateway(currentLevel.getWorld().getName(), created.getX(), created.getY(), created.getZ());
            if (currentLevel.getBlockEntity(created) instanceof TheEndGatewayBlockEntity fresh) {
                fresh.setExitPosition(portalEntryPos, false);
            }
            setExitPosition(created, exactTeleport);
        }
        if (exitPortal == null) {
            return null;
        }
        BlockPos target = exactTeleport ? exitPortal : tallestAround(currentLevel, exitPortal.offset(0, 2, 0), 5, false).above();
        return Vec3.atBottomCenterOf(target);
    }
    public boolean shouldRenderFace(Direction direction) {
        return Block.shouldRenderFace(getBlockState(), getLevel().getBlockState(getBlockPos().relative(direction)), direction);
    }
    public int getParticleAmount() {
        int visible = 0;
        for (Direction side : Direction.values()) {
            visible += shouldRenderFace(side) ? 1 : 0;
        }
        return visible;
    }
    public void setExitPosition(BlockPos exactPosition, boolean exact) {
        exactTeleport = exact;
        exitPortal = exactPosition;
        CompoundTag tag = nbt();
        writeState(tag);
        load(tag);


    }
}
