// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import java.util.stream.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import com.google.common.collect.Iterables;
import com.google.common.collect.LinkedHashMultiset;
import com.google.common.collect.Multisets;
import net.minecraft.world.level.saveddata.maps.*;
import net.minecraft.world.level.material.MapColor;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.item.component.MapPostProcessing;
public class MapItem extends Item {
    public static final int IMAGE_WIDTH = 128;
    public static final int IMAGE_HEIGHT = 128;

    public MapItem(Item.Properties properties) {
        super(properties);
    }

    public static ItemStack create(ServerLevel level, int originX, int originZ, byte scale, boolean trackPosition, boolean unlimitedTracking) {
        ItemStack map = new ItemStack(Items.FILLED_MAP);
        applyNewSavedData(level, map, originX, originZ, scale, trackPosition, unlimitedTracking);
        return map;
    }

    public static void applyNewSavedData(ServerLevel level, ItemStack map, int originX, int originZ, byte scale, boolean trackPosition, boolean unlimitedTracking) {
        MapItemSavedData data = MapItemSavedData.createFresh(originX, originZ, scale, trackPosition, unlimitedTracking, level.dimension());
        MapId id = level.getFreeMapId();
        level.setMapData(id, data);
        map.set(DataComponents.MAP_ID, id);
    }

    public static MapItemSavedData getSavedData(MapId id, Level level) {
        return id == null ? null : level.getMapData(id);
    }

    public static MapItemSavedData getSavedData(ItemStack itemStack, Level level) {
        return getSavedData(itemStack.get(DataComponents.MAP_ID), level);
    }

    private static final class CellSample {
        final LinkedHashMultiset<MapColor> colors = LinkedHashMultiset.create();
        double height;
        int waterDepth;
    }

    public void update(Level level, Entity player, MapItemSavedData data) {
        if (level.dimension() != data.dimension || !(player instanceof Player holder)) {
            return;
        }
        final int scale = 1 << data.scale;
        final int centerX = data.centerX;
        final int centerZ = data.centerZ;
        final int playerX = Mth.floor(player.getX() - centerX) / scale + 64;
        final int playerY = Mth.floor(player.getZ() - centerZ) / scale + 64;
        final boolean ceiling = level.dimensionType().hasCeiling();
        final int radius = (ceiling ? 128 / 2 : 128) / scale;
        final int radiusSq = radius * radius;
        final int ditherSq = (radius - 2) * (radius - 2);
        MapItemSavedData.HoldingPlayer holding = data.getHoldingPlayer(holder);
        holding.step++;
        boolean carry = false;
        for (int x = playerX - radius + 1; x < playerX + radius; x++) {
            if (!carry && (x & 15) != (holding.step & 15)) {
                continue;
            }
            carry = false;
            double previousHeight = 0.0;
            for (int y = playerY - radius - 1; y < playerY + radius; y++) {
                boolean outside = x < 0 || x >= 128 || y < -1 || y >= 128;
                if (outside) {
                    continue;
                }
                int distanceSq = Mth.square(x - playerX) + Mth.square(y - playerY);
                int minX = (centerX / scale + x - 64) * scale;
                int minZ = (centerZ / scale + y - 64) * scale;
                LevelChunk chunk = level.getChunk(SectionPos.blockToSectionCoord(minX), SectionPos.blockToSectionCoord(minZ));
                if (chunk.isEmpty()) {
                    continue;
                }
                CellSample cell = new CellSample();
                if (ceiling) {
                    sampleCeiling(level, cell, minX, minZ);
                } else {
                    sampleSurface(level, data, chunk, cell, minX, minZ, scale);
                }
                MapColor color = Iterables.getFirst(Multisets.copyHighestCountFirst(cell.colors), MapColor.NONE);
                int parity = x + y & 1;
                MapColor.Brightness brightness = color == MapColor.WATER
                    ? waterBrightness(cell.waterDepth / (scale * scale), parity)
                    : landBrightness(cell.height - previousHeight, scale, parity);
                previousHeight = cell.height;
                boolean dither = distanceSq > ditherSq;
                if (y >= 0 && distanceSq < radiusSq && (!dither || parity != 0)) {
                    carry |= data.updateColor(x, y, color.getPackedId(brightness));
                }
            }
        }
    }

    private static void sampleCeiling(Level level, CellSample cell, int minX, int minZ) {
        int noise = minX + minZ * 231871;
        noise = noise * noise * 31287121 + noise * 11;
        boolean dirt = (noise >> 20 & 1) == 0;
        BlockState base = dirt ? Blocks.DIRT.defaultBlockState() : Blocks.STONE.defaultBlockState();
        cell.colors.add(base.getMapColor(level, BlockPos.ZERO), dirt ? 10 : 100);
        cell.height = 100.0;
    }

    private void sampleSurface(Level level, MapItemSavedData data, LevelChunk chunk, CellSample cell, int minX, int minZ, int scale) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        final int floor = level.getMinY();
        for (int dx = 0; dx < scale; dx++) {
            for (int dz = 0; dz < scale; dz++) {
                pos.set(minX + dx, 0, minZ + dz);
                int top = chunk.getHeight(Heightmap.Types.WORLD_SURFACE, pos.getX(), pos.getZ()) + 1;
                BlockState state;
                if (top <= floor) {
                    state = Blocks.BEDROCK.defaultBlockState();
                } else {
                    do {
                        top--;
                        pos.setY(top);
                        state = chunk.getBlockState(pos);
                    } while (top > floor && state.getMapColor(level, pos) == MapColor.NONE);
                    if (top > floor && !state.getFluidState().isEmpty()) {
                        probe.set(pos);
                        int scan = top - 1;
                        BlockState under;
                        do {
                            probe.setY(scan--);
                            under = chunk.getBlockState(probe);
                            cell.waterDepth++;
                        } while (scan > floor && !under.getFluidState().isEmpty());
                        state = fluidSurface(level, state, pos);
                    }
                }
                data.checkBanners(level, pos.getX(), pos.getZ());
                cell.height += (double) top / (scale * scale);
                cell.colors.add(state.getMapColor(level, pos));
            }
        }
    }

    private static MapColor.Brightness waterBrightness(int depth, int parity) {
        double shade = depth * 0.1 + parity * 0.2;
        if (shade < 0.5) {
            return MapColor.Brightness.HIGH;
        }
        return shade > 0.9 ? MapColor.Brightness.LOW : MapColor.Brightness.NORMAL;
    }

    private static MapColor.Brightness landBrightness(double slope, int scale, int parity) {
        double shade = slope * 4.0 / (scale + 4) + (parity - 0.5) * 0.4;
        if (shade > 0.6) {
            return MapColor.Brightness.HIGH;
        }
        return shade < -0.6 ? MapColor.Brightness.LOW : MapColor.Brightness.NORMAL;
    }

    private BlockState fluidSurface(Level level, BlockState state, BlockPos pos) {
        FluidState fluid = state.getFluidState();
        return !fluid.isEmpty() && !state.isFaceSturdy(level, pos, Direction.UP) ? fluid.createLegacyBlock() : state;
    }

    public static void renderBiomePreviewMap(ServerLevel level, ItemStack mapItemStack) {
        MapItemSavedData data = getSavedData(mapItemStack, level);
        if (data == null || level.dimension() != data.dimension) {
            return;
        }
        int scale = 1 << data.scale;
        int originX = data.centerX / scale - 64;
        int originZ = data.centerZ / scale - 64;
        boolean[] watery = new boolean[IMAGE_WIDTH * IMAGE_HEIGHT];
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int index = 0; index < watery.length; index++) {
            int column = index % IMAGE_WIDTH;
            int row = index / IMAGE_WIDTH;
            cursor.set((originX + column) * scale, level.getSeaLevel(), (originZ + row) * scale);
            watery[index] = level.getBiome(cursor).is(BiomeTags.WATER_ON_MAP_OUTLINES);
        }
        for (int mx = 1; mx < 127; mx++) {
            for (int mz = 1; mz < 127; mz++) {
                int neighbours = 0;
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if ((dx != 0 || dz != 0) && watery[(mz + dz) * IMAGE_WIDTH + mx + dx]) {
                            neighbours++;
                        }
                    }
                }
                boolean self = watery[mz * IMAGE_WIDTH + mx];
                if (!self && neighbours == 0) {
                    continue;
                }
                MapColor color;
                MapColor.Brightness brightness = MapColor.Brightness.LOWEST;
                if (!self) {
                    color = MapColor.COLOR_BROWN;
                    if (neighbours > 3) {
                        brightness = MapColor.Brightness.NORMAL;
                    }
                } else if (neighbours > 7 && mz % 2 != 0) {
                    continue;
                } else {
                    color = MapColor.COLOR_ORANGE;
                    if (neighbours > 7) {
                        brightness = stripeBrightness(mx, mz);
                    } else if (neighbours > 5) {
                        brightness = MapColor.Brightness.NORMAL;
                    } else if (neighbours > 1) {
                        brightness = MapColor.Brightness.LOW;
                    }
                }
                data.setColor(mx, mz, color.getPackedId(brightness));
            }
        }
    }

    private static MapColor.Brightness stripeBrightness(int mx, int mz) {
        int band = (mx + (int) (Mth.sin((float) mz) * 7.0F)) / 8 % 5;
        return switch (band) {
            case 0, 4 -> MapColor.Brightness.LOW;
            case 1, 3 -> MapColor.Brightness.NORMAL;
            case 2 -> MapColor.Brightness.HIGH;
            default -> MapColor.Brightness.LOWEST;
        };
    }

    @Override
    public void inventoryTick(ItemStack itemStack, ServerLevel level, Entity owner, EquipmentSlot slot) {
        MapItemSavedData data = getSavedData(itemStack, level);
        if (data == null) {
            return;
        }
        if (owner instanceof Player player) {
            data.tickCarriedBy(player, itemStack, null);
        }
        if (!data.locked && slot != null && slot.getType() == EquipmentSlot.Type.HAND) {
            update(level, owner, data);
        }
    }

    @Override
    public void onCraftedPostProcess(ItemStack itemStack, Level level) {
        MapPostProcessing post = itemStack.remove(DataComponents.MAP_POST_PROCESSING);
        if (post == null || !(level instanceof ServerLevel serverLevel)) {
            return;
        }
        MapItemSavedData source = getSavedData(itemStack, serverLevel);
        if (source == null) {
            return;
        }
        MapId id = serverLevel.getFreeMapId();
        serverLevel.setMapData(id, post == MapPostProcessing.LOCK ? source.locked() : source.scaled());
        itemStack.set(DataComponents.MAP_ID, id);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        if (!context.getLevel().getBlockState(context.getClickedPos()).is(BlockTags.BANNERS)) {
            return super.useOn(context);
        }
        if (!context.getLevel().isClientSide()) {
            MapItemSavedData data = getSavedData(context.getItemInHand(), context.getLevel());
            if (data != null && !data.toggleBanner(context.getLevel(), context.getClickedPos())) {
                return InteractionResult.FAIL;
            }
        }
        return InteractionResult.SUCCESS;
    }
}
