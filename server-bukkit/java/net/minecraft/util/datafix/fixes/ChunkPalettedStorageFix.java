// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap;
public class ChunkPalettedStorageFix extends DataFix {
    public ChunkPalettedStorageFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public static String getName(Dynamic<?> state) {
        return state.get("Name").asString("");
    }
    public static String getProperty(Dynamic<?> state, String property) {
        return state.get("Properties").get(property).asString("");
    }
    public static int idFor(CrudeIncrementalIntIdentityHashBiMap<Dynamic<?>> states, Dynamic<?> state) {
        int id = states.getId(state);
        return id == -1 ? states.add(state) : id;
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static int getSideMask(boolean west, boolean east, boolean north, boolean south) {
        return net.minecraft.util.datafix.fixes.LeavesFix.getSideMask(west, east, north, south);
    }
    static class MappingConstants {
        static final java.util.BitSet VIRTUAL = bits(54, 146, 25, 26, 51, 53, 67, 108, 109, 114, 128, 134, 135, 136, 156, 163, 164, 180, 203, 55, 85, 113, 188, 189,
            190, 191, 192, 93, 94, 101, 102, 160, 106, 107, 183, 184, 185, 186, 187, 132, 139, 199);
        static final java.util.BitSet FIX = bits(2, 3, 110, 140, 144, 25, 86, 26, 176, 177, 175, 64, 71, 193, 194, 195, 196, 197);
        static final Dynamic<?> AIR = new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, named("minecraft:air"));
        private static java.util.BitSet bits(int... ids) {
            java.util.BitSet set = new java.util.BitSet(256);
            for (int id : ids) {
                set.set(id);
            }
            return set;
        }
        private static net.minecraft.nbt.CompoundTag named(String name) {
            net.minecraft.nbt.CompoundTag tag = new net.minecraft.nbt.CompoundTag();
            tag.putString("Name", name);
            return tag;
        }
    }
    static class Section {
        private final CrudeIncrementalIntIdentityHashBiMap<Dynamic<?>> palette = CrudeIncrementalIntIdentityHashBiMap.create(32);
        private final java.util.List<Dynamic<?>> listTag = new java.util.ArrayList<>();
        private final Dynamic<?> section;
        private final boolean hasData;
        private final java.util.Map<Integer, java.util.List<Integer>> toFix = new java.util.LinkedHashMap<>();
        private final java.util.List<Integer> update = new java.util.ArrayList<>();
        private final java.util.Set<Dynamic<?>> seen = java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
        private final int[] buffer = new int[4096];
        public final int y;
        public Section(Dynamic<?> section) {
            this.section = section;
            this.y = section.get("Y").asInt(0);
            this.hasData = section.get("Blocks").result().isPresent();
        }
        public Dynamic<?> getBlock(int pos) {
            Dynamic<?> tag = pos >= 0 && pos < 4096 ? palette.byId(buffer[pos]) : null;
            return tag == null ? MappingConstants.AIR : tag;
        }
        public void setBlock(int idx, Dynamic<?> blockState) {
            if (seen.add(blockState)) {
                listTag.add("%%FILTER_ME%%".equals(getName(blockState)) ? MappingConstants.AIR : blockState);
            }
            buffer[idx] = idFor(palette, blockState);
        }
        public int upgrade(int sides) {
            if (!hasData) {
                return sides;
            }
            java.nio.ByteBuffer blocks = section.get("Blocks").asByteBufferOpt().result().get();
            DataLayer data = section.get("Data").asByteBufferOpt().map(b -> new DataLayer(com.mojang.datafixers.DataFixUtils.toArray(b))).result().orElseGet(DataLayer::new);
            DataLayer extra = section.get("Add").asByteBufferOpt().map(b -> new DataLayer(com.mojang.datafixers.DataFixUtils.toArray(b))).result().orElseGet(DataLayer::new);
            seen.add(MappingConstants.AIR);
            idFor(palette, MappingConstants.AIR);
            listTag.add(MappingConstants.AIR);
            for (int idx = 0; idx < 4096; idx++) {
                int x = idx & 15;
                int yy = idx >> 8 & 15;
                int z = idx >> 4 & 15;
                int id = extra.get(x, yy, z) << 12 | (blocks.get(idx) & 255) << 4 | data.get(x, yy, z);
                if (MappingConstants.FIX.get(id >> 4)) {
                    toFix.computeIfAbsent(id >> 4, k -> new java.util.ArrayList<>()).add(idx);
                }
                if (MappingConstants.VIRTUAL.get(id >> 4)) {
                    int edge = getSideMask(x == 0, x == 15, z == 0, z == 15);
                    if (edge == 0) {
                        update.add(idx);
                    } else {
                        sides |= edge;
                    }
                }
                setBlock(idx, BlockStateData.getTag(id));
            }
            return sides;
        }
        public Dynamic<?> write() {
            if (!hasData) {
                return section;
            }
            net.minecraft.util.datafix.PackedBitStorage storage = new net.minecraft.util.datafix.PackedBitStorage(
                Math.max(4, com.mojang.datafixers.DataFixUtils.ceillog2(seen.size())), 4096);
            for (int i = 0; i < buffer.length; i++) {
                storage.set(i, buffer[i]);
            }
            return section.set("Palette", section.createList(listTag.stream())).set("BlockStates", section.createLongList(java.util.Arrays.stream(storage.getRaw())))
                .remove("Blocks").remove("Data").remove("Add");
        }
    }
    static final class UpgradeChunk {
        private static final int kVersionKey = 14511;
        private final Dynamic<?> level;
        private Dynamic<?> fixed;
        public UpgradeChunk(Dynamic<?> level) {
            this.level = level;
        }
        public static int relative(int pos, Direction direction) {
            int step = direction.getAxisDirection().getStep();
            switch (direction.getAxis()) {
                case X: {
                    int x = (pos & 15) + step;
                    return x < 0 || x > 15 ? -1 : (pos & ~15) | x;
                }
                case Y: {
                    int y = (pos >> 8) + step;
                    return y < 0 || y > 255 ? -1 : (pos & 255) | (y << 8);
                }
                default: {
                    int z = ((pos >> 4) & 15) + step;
                    return z < 0 || z > 15 ? -1 : (pos & ~240) | (z << 4);
                }
            }
        }
        public Dynamic<?> getBlock(int pos) {
            Dynamic<?> section = write().get("Sections").asStream().filter(each -> each.get("Y").asInt(-1) == pos >> 12).findFirst().orElse(null);
            if (section == null) {
                return MappingConstants.AIR;
            }
            java.util.List<Dynamic<?>> states = section.get("Palette").asList(each -> each);
            net.minecraft.util.datafix.PackedBitStorage storage = new net.minecraft.util.datafix.PackedBitStorage(
                Math.max(4, com.mojang.datafixers.DataFixUtils.ceillog2(states.size())), 4096, section.get("BlockStates").asLongStream().toArray());
            int index = storage.get(pos & 4095);
            return index < states.size() ? states.get(index) : MappingConstants.AIR;
        }
        public Dynamic<?> write() {
            if (fixed == null) {
                net.minecraft.nbt.CompoundTag chunk = new net.minecraft.nbt.CompoundTag();
                chunk.put("Level", (net.minecraft.nbt.CompoundTag) level.convert(net.minecraft.nbt.NbtOps.INSTANCE).getValue());
                net.minecraft.nbt.CompoundTag result = org.basaltmc.bukkit.facade.NativeStep.root("chunk", "ChunkPalettedStorageFix", kVersionKey, chunk).getCompoundOrEmpty("Level");
                fixed = new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, result).convert(level.getOps());
            }
            return fixed;
        }
    }
    static class DataLayer {
        private final byte[] data;
        public DataLayer() {
            this.data = new byte[2048];
        }
        public DataLayer(byte[] data) {
            if (data.length != 2048) {
                throw new IllegalArgumentException("ChunkNibbleArrays should be 2048 bytes not: " + data.length);
            }
            this.data = data;
        }
        public int get(int x, int y, int z) {
            int index = (y << 8) | (z << 4) | x;
            int packed = data[index >> 1];
            return (index & 1) == 0 ? packed & 15 : (packed >> 4) & 15;
        }
    }
    public static enum Direction {
        DOWN(AxisDirection.NEGATIVE, Axis.Y), UP(AxisDirection.POSITIVE, Axis.Y), NORTH(AxisDirection.NEGATIVE, Axis.Z),
        SOUTH(AxisDirection.POSITIVE, Axis.Z), WEST(AxisDirection.NEGATIVE, Axis.X), EAST(AxisDirection.POSITIVE, Axis.X);
        private final AxisDirection axisDirection;
        private final Axis axis;
        Direction(AxisDirection axisDirection, Axis axis) {
            this.axisDirection = axisDirection;
            this.axis = axis;
        }
        public AxisDirection getAxisDirection() {
            return axisDirection;
        }
        public Axis getAxis() {
            return axis;
        }
        public static enum Axis {
            X, Y, Z;
        }
        public static enum AxisDirection {
            POSITIVE(1), NEGATIVE(-1);
            private final int step;
            AxisDirection(int step) {
                this.step = step;
            }
            public int getStep() {
                    return step;
            }
        }
    }
}
