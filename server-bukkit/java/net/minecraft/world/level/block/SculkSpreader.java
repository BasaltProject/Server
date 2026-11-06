// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.BlockTags;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

/** Cursors live in the catalyst's native block entity; the spread itself runs in the native sculk system. */
public class SculkSpreader {
    public static final int MAX_GROWTH_RATE_RADIUS = 24;
    public static final int MAX_CHARGE = 1000;
    public static final float MAX_DECAY_FACTOR = 0.5F;
    public static final int SHRIEKER_PLACEMENT_RATE = 11;
    public static final int MAX_CURSOR_DISTANCE = 1024;
    private static final int kMaxCursors = 32;
    private final boolean isWorldGeneration;
    private final TagKey<Block> replaceableBlocks;
    private final int growthSpawnCost;
    private final int noGrowthRadius;
    private final int chargeDecayRate;
    private final int additionalDecayRate;
    private List<ChargeCursor> cursors = new ArrayList<>();

    public SculkSpreader(boolean isWorldGeneration, TagKey<Block> replaceableBlocks, int growthSpawnCost, int noGrowthRadius, int chargeDecayRate, int additionalDecayRate) {
        this.isWorldGeneration = isWorldGeneration;
        this.replaceableBlocks = replaceableBlocks;
        this.growthSpawnCost = growthSpawnCost;
        this.noGrowthRadius = noGrowthRadius;
        this.chargeDecayRate = chargeDecayRate;
        this.additionalDecayRate = additionalDecayRate;
    }

    public static SculkSpreader createLevelSpreader() {
        TagKey<Block> replaceable = BlockTags.SCULK_REPLACEABLE;
        return new SculkSpreader(false, replaceable, 10, 4, 10, 5);
    }

    public static SculkSpreader createWorldGenSpreader() {
        TagKey<Block> replaceable = BlockTags.SCULK_REPLACEABLE_WORLD_GEN;
        return new SculkSpreader(true, replaceable, 50, 1, 5, 10);
    }

    public TagKey<Block> replaceableBlocks() {
        return replaceableBlocks;
    }

    public int growthSpawnCost() {
        return growthSpawnCost;
    }

    public int noGrowthRadius() {
        return noGrowthRadius;
    }

    public int chargeDecayRate() {
        return chargeDecayRate;
    }

    public int additionalDecayRate() {
        return additionalDecayRate;
    }

    public boolean isWorldGeneration() {
        return isWorldGeneration;
    }

    public List<ChargeCursor> getCursors() {
        return cursors;
    }

    public void clear() {
        cursors.clear();
    }

    public void load(ValueInput input) {
        cursors = new ArrayList<>(input.read("cursors", ChargeCursor.CODEC.listOf()).orElse(List.of()));
        if (cursors.size() > kMaxCursors) {
            cursors.clear();
        }
    }

    public void save(ValueOutput output) {
        output.store("cursors", ChargeCursor.CODEC.listOf(), cursors);
    }

    public void addCursors(BlockPos startPos, int charge) {
        while (charge > 0) {
            int current = Math.min(charge, MAX_CHARGE);
            if (cursors.size() < kMaxCursors) {
                cursors.add(new ChargeCursor(startPos, current));
            }
            charge -= current;
        }
    }

    public void updateCursors(LevelAccessor level, BlockPos originPos, RandomSource random, boolean spreadVeins) {
        if (cursors.isEmpty() || !(level instanceof ServerLevel server)) {
            return;
        }
        BlockEntity catalyst = level.getBlockEntity(originPos);
        if (catalyst == null) {
            return;
        }
        CompoundTag data = catalyst.nbt();
        ListTag written = new ListTag();
        for (ChargeCursor cursor : cursors) {
            written.add(written.size(), cursor.toTag());
        }
        data.put("cursors", written);
        catalyst.load(data);
        org.basaltmc.bukkit.WorldEnvNatives.sculk(server.getWorld().getName(), "update", originPos.getX(), originPos.getY(), originPos.getZ(), 0, 0, 0, 0, null);
        List<ChargeCursor> after = new ArrayList<>();
        for (Tag tag : catalyst.nbt().getListOrEmpty("cursors")) {
            if (tag instanceof CompoundTag entry) {
                ChargeCursor cursor = ChargeCursor.fromTag(entry);
                if (cursor != null) {
                    after.add(cursor);
                }
            }
        }
        cursors = after;
    }

    public static class ChargeCursor {
        public static final int MAX_CURSOR_DECAY_DELAY = 1;
        public static final Codec<ChargeCursor> CODEC = CompoundTag.CODEC.xmap(ChargeCursor::fromTag, ChargeCursor::toTag);
        private final BlockPos pos;
        private int charge;
        private int decayDelay;
        private int updateDelay;
        private Optional<Set<Direction>> facings;

        private ChargeCursor(BlockPos pos, int charge, int decayDelay, int updateDelay, Optional<Set<Direction>> facings) {
            this.pos = pos;
            this.charge = charge;
            this.decayDelay = decayDelay;
            this.updateDelay = updateDelay;
            this.facings = facings;
        }

        public ChargeCursor(BlockPos pos, int charge) {
            this(pos, charge, MAX_CURSOR_DECAY_DELAY, 0, Optional.empty());
        }

        public BlockPos getPos() {
            return pos;
        }

        public int getCharge() {
            return charge;
        }

        public int getDecayDelay() {
            return decayDelay;
        }

        public Set<Direction> getFacingData() {
            return facings.orElse(Set.of());
        }

        public void update(LevelAccessor level, BlockPos originPos, RandomSource random, SculkSpreader spreader, boolean spreadVeins) {
            List<ChargeCursor> single = new ArrayList<>(spreader.cursors);
            spreader.cursors = new ArrayList<>(List.of(this));
            spreader.updateCursors(level, originPos, random, spreadVeins);
            ChargeCursor result = spreader.cursors.isEmpty() ? null : spreader.cursors.get(0);
            if (result != null) {
                charge = result.charge;
                decayDelay = result.decayDelay;
                updateDelay = result.updateDelay;
                facings = result.facings;
            }
            spreader.cursors = single;
        }

        private static ChargeCursor fromTag(CompoundTag entry) {
            int[] at = entry.getIntArray("pos").orElse(null);
            if (at == null || at.length != 3) {
                return null;
            }
            Optional<Set<Direction>> directions = Optional.empty();
            if (entry.contains("facings")) {
                Set<Direction> found = EnumSet.noneOf(Direction.class);
                for (Tag tag : entry.getListOrEmpty("facings")) {
                    Direction direction = Direction.byName(tag.asString().orElse(""));
                    if (direction != null) {
                        found.add(direction);
                    }
                }
                directions = Optional.of(found);
            }
            return new ChargeCursor(new BlockPos(at[0], at[1], at[2]), Math.max(0, Math.min(MAX_CHARGE, entry.getIntOr("charge", 0))),
                Math.max(0, Math.min(MAX_CURSOR_DECAY_DELAY, entry.getIntOr("decay_delay", 1))), Math.max(0, entry.getIntOr("update_delay", 0)), directions);
        }

        private CompoundTag toTag() {
            CompoundTag entry = new CompoundTag();
            entry.putIntArray("pos", new int[] {pos.getX(), pos.getY(), pos.getZ()});
            entry.putInt("charge", charge);
            entry.putInt("decay_delay", decayDelay);
            entry.putInt("update_delay", updateDelay);
            facings.ifPresent(set -> {
                ListTag list = new ListTag();
                for (Direction direction : set) {
                    list.add(list.size(), StringTag.valueOf(direction.getName()));
                }
                entry.put("facings", list);
            });
            return entry;
        }
    }
}
