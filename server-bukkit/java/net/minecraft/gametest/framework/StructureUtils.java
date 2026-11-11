// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Comparator;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.TestInstanceBlockEntity;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

public class StructureUtils {
    public static final int DEFAULT_Y_SEARCH_RADIUS = 10;
    public static Path testStructuresTargetDir = Paths.get("gameteststructures").toAbsolutePath();
    public static Path testStructuresSourceDir = testStructuresTargetDir;

    public static Rotation getRotationForRotationSteps(int rotationSteps) {
        return Rotation.values()[Math.floorMod(rotationSteps, 4)];
    }

    public static int getRotationStepsForRotation(Rotation rotation) {
        return rotation.ordinal();
    }

    public static TestInstanceBlockEntity createNewEmptyTest(Identifier id, BlockPos structurePos, Vec3i size, Rotation rotation, ServerLevel level) {
        BoundingBox box = getStructureBoundingBox(TestInstanceBlockEntity.getStructurePos(structurePos), size, rotation);
        clearSpaceForStructure(box, level);
        level.setBlockAndUpdate(structurePos, Blocks.TEST_INSTANCE_BLOCK.defaultBlockState());
        if (!(level.getBlockEntity(structurePos) instanceof TestInstanceBlockEntity entity)) {
            throw new IllegalStateException("No test instance block entity at " + structurePos);
        }
        entity.set(new TestInstanceBlockEntity.Data(Optional.of(net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.TEST_INSTANCE, id)), size, rotation, false, TestInstanceBlockEntity.Status.CLEARED, Optional.empty()));
        return entity;
    }

    public static void clearSpaceForStructure(BoundingBox box, ServerLevel level) {
        AABB area = AABB.of(box).inflate(1.0);
        for (Entity entity : level.getEntitiesOfClass(Entity.class, area, e -> !(e instanceof Player))) {
            entity.discard();
        }
        BlockPos.betweenClosedStream(box.minX(), box.minY(), box.minZ(), box.maxX(), box.maxY(), box.maxZ())
            .forEach(pos -> level.setBlock(pos, Blocks.AIR.defaultBlockState(), 818));
        level.getBlockTicks().clearArea(box);
        level.clearBlockEvents(box);
    }

    public static BlockPos getTransformedFarCorner(BlockPos structurePosition, Vec3i size, Rotation rotation) {
        int dx = size.getX() - 1;
        int dz = size.getZ() - 1;
        return switch (rotation) {
            case NONE -> structurePosition.offset(dx, size.getY() - 1, dz);
            case CLOCKWISE_90 -> structurePosition.offset(-dz, size.getY() - 1, dx);
            case CLOCKWISE_180 -> structurePosition.offset(-dx, size.getY() - 1, -dz);
            case COUNTERCLOCKWISE_90 -> structurePosition.offset(dz, size.getY() - 1, -dx);
        };
    }

    public static BoundingBox getStructureBoundingBox(BlockPos northWestCorner, Vec3i size, Rotation rotation) {
        return BoundingBox.fromCorners(northWestCorner, getTransformedFarCorner(northWestCorner, size, rotation));
    }

    public static Optional<BlockPos> findTestContainingPos(BlockPos pos, int searchRadius, ServerLevel level) {
        return findTestBlocks(pos, searchRadius, level).filter(blockPos -> level.getBlockEntity(blockPos) instanceof TestInstanceBlockEntity entity
            && entity.getStructureBoundingBox().isInside(pos)).findFirst();
    }

    public static Optional<BlockPos> findNearestTest(BlockPos relativeToPos, int searchRadius, ServerLevel level) {
        return findTestBlocks(relativeToPos, searchRadius, level).min(Comparator.comparingDouble(pos -> pos.distSqr(relativeToPos)));
    }

    public static Stream<BlockPos> findTestBlocks(BlockPos centerPos, int searchRadius, ServerLevel level) {
        return BlockPos.betweenClosedStream(new AABB(centerPos).inflate(searchRadius))
            .filter(pos -> level.getBlockState(pos).is(Blocks.TEST_INSTANCE_BLOCK))
            .map(BlockPos::immutable);
    }

    public static Stream<BlockPos> lookedAtTestPos(BlockPos pos, Entity camera, ServerLevel level) {
        Vec3 eye = camera.getEyePosition();
        Vec3 look = camera.getLookAngle();
        return findTestBlocks(pos, 250, level).filter(testPos -> {
            if (!(level.getBlockEntity(testPos) instanceof TestInstanceBlockEntity entity)) {
                return false;
            }
            return entity.getStructureBounds().clip(eye, eye.add(look.scale(100.0))).isPresent();
        });
    }
}
