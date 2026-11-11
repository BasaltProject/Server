// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.ReportedException;
import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.SectionPos;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.phys.Vec3;

public class PlayerSpawnFinder {
    private static final EntityDimensions PLAYER_DIMENSIONS = EntityTypes.PLAYER.getDimensions();
    private static final int MAX_CANDIDATES = 1024;
    private static final int SMALL_SET = 16;
    private static final int STRIDE = 17;
    private final ServerLevel level;
    private final BlockPos origin;
    private final int radius;
    private final int candidates;
    private final int stride;
    private final int shift;
    private int cursor;
    private final CompletableFuture<Vec3> result = new CompletableFuture<>();

    private PlayerSpawnFinder(ServerLevel level, BlockPos spawnSuggestion, int radius) {
        this.level = level;
        this.origin = spawnSuggestion;
        this.radius = radius;
        long side = radius * 2L + 1L;
        this.candidates = (int) Math.min(MAX_CANDIDATES, side * side);
        this.stride = candidates <= SMALL_SET ? candidates - 1 : STRIDE;
        this.shift = RandomSource.createThreadLocalInstance().nextInt(candidates);
    }

    public static CompletableFuture<Vec3> findSpawn(ServerLevel level, BlockPos spawnSuggestion) {
        if (level.getServer().getWorldData().getGameType() == GameType.ADVENTURE) {
            return CompletableFuture.completedFuture(settle(level, spawnSuggestion));
        }
        int radius = Math.max(0, level.getGameRules().get(GameRules.RESPAWN_RADIUS));
        int toBorder = Mth.floor(level.getWorldBorder().getDistanceToBorder(spawnSuggestion.getX(), spawnSuggestion.getZ()));
        radius = Math.min(radius, toBorder);
        if (toBorder <= 1) {
            radius = 1;
        }
        PlayerSpawnFinder finder = new PlayerSpawnFinder(level, spawnSuggestion, radius);
        finder.next();
        return finder.result;
    }

    private void next() {
        int index = cursor++;
        if (index >= candidates) {
            probe(origin.getX(), origin.getZ(), index, () -> Optional.of(settle(level, origin)));
            return;
        }
        int slot = (shift + stride * index) % candidates;
        int width = radius * 2 + 1;
        int x = origin.getX() + slot % width - radius;
        int z = origin.getZ() + slot / width - radius;
        probe(x, z, index, () -> {
            BlockPos pos = getLevelRespawnPos(level, x, z);
            return pos != null && fitsPlayer(level, pos) ? Optional.of(Vec3.atBottomCenterOf(pos)) : Optional.empty();
        });
    }

    private static Vec3 settle(CollisionGetter level, BlockPos start) {
        BlockPos.MutableBlockPos at = start.mutable();
        while (!fitsPlayer(level, at) && at.getY() < level.getMaxY()) {
            at.move(Direction.UP);
        }
        at.move(Direction.DOWN);
        while (fitsPlayer(level, at) && at.getY() > level.getMinY()) {
            at.move(Direction.DOWN);
        }
        at.move(Direction.UP);
        return Vec3.atBottomCenterOf(at);
    }

    private static boolean fitsPlayer(CollisionGetter level, BlockPos pos) {
        return level.noCollision(null, PLAYER_DIMENSIONS.makeBoundingBox(Vec3.atBottomCenterOf(pos)), true);
    }

    private void probe(int x, int z, int index, Supplier<Optional<Vec3>> check) {
        if (result.isDone()) {
            return;
        }
        ChunkPos chunk = new ChunkPos(SectionPos.blockToSectionCoord(x), SectionPos.blockToSectionCoord(z));
        level.getChunkSource().addTicketAndLoadWithRadius(TicketType.SPAWN_SEARCH, chunk, 0).whenCompleteAsync((ignored, failure) -> {
            Throwable error = failure;
            if (error == null) {
                try {
                    Optional<Vec3> found = check.get();
                    if (found.isPresent()) {
                        result.complete(found.get());
                    } else {
                        next();
                    }
                } catch (Throwable thrown) {
                    error = thrown;
                }
            }
            if (error != null) {
                CrashReport report = CrashReport.forThrowable(error, "Searching for spawn");
                CrashReportCategory details = report.addCategory("Spawn Lookup");
                details.setDetail("Origin", origin::toString);
                details.setDetail("Radius", () -> Integer.toString(radius));
                details.setDetail("Candidate", () -> "[" + x + "," + z + "]");
                details.setDetail("Progress", () -> index + " out of " + candidates);
                result.completeExceptionally(new ReportedException(report));
            }
        }, level.getServer());
    }

    protected static BlockPos getLevelRespawnPos(ServerLevel level, int x, int z) {
        LevelChunk chunk = level.getChunk(SectionPos.blockToSectionCoord(x), SectionPos.blockToSectionCoord(z));
        int top = level.dimensionType().hasCeiling() ? level.getChunkSource().getGenerator().getSpawnHeight(level)
            : chunk.getHeight(Heightmap.Types.MOTION_BLOCKING, x & 15, z & 15);
        if (top < level.getMinY()) {
            return null;
        }
        int surface = chunk.getHeight(Heightmap.Types.WORLD_SURFACE, x & 15, z & 15);
        if (surface <= top && surface > chunk.getHeight(Heightmap.Types.OCEAN_FLOOR, x & 15, z & 15)) {
            return null;
        }
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int y = top + 1; y >= level.getMinY(); y--) {
            cursor.set(x, y, z);
            BlockState state = level.getBlockState(cursor);
            if (!state.getFluidState().isEmpty()) {
                return null;
            }
            if (Block.isFaceFull(state.getCollisionShape(level, cursor), Direction.UP)) {
                return cursor.above().immutable();
            }
        }
        return null;
    }

    public static BlockPos getSpawnPosInChunk(ServerLevel level, ChunkPos chunkPos) {
        if (SharedConstants.debugVoidTerrain(chunkPos)) {
            return null;
        }
        for (int x = chunkPos.getMinBlockX(); x <= chunkPos.getMaxBlockX(); x++) {
            for (int z = chunkPos.getMinBlockZ(); z <= chunkPos.getMaxBlockZ(); z++) {
                BlockPos valid = getLevelRespawnPos(level, x, z);
                if (valid != null) {
                    return valid;
                }
            }
        }
        return null;
    }
}
