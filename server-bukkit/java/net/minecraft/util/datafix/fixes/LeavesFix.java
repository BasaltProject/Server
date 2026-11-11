// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.OpticFinder;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Dynamic;
import java.util.List;
import net.minecraft.util.datafix.PackedBitStorage;
public class LeavesFix extends DataFix {
    public LeavesFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static int getIndex(int x, int y, int z) {
        return (y << 8) | (z << 4) | x;
    }
    public static int getSideMask(boolean west, boolean east, boolean north, boolean south) {
        if (north) {
            return east ? 2 : west ? 128 : 1;
        }
        if (south) {
            return west ? 32 : east ? 8 : 16;
        }
        return east ? 4 : west ? 64 : 0;
    }
    private static final java.util.Map<String, Integer> kLeaves = java.util.Map.of("minecraft:acacia_leaves", 0, "minecraft:birch_leaves", 1,
        "minecraft:dark_oak_leaves", 2, "minecraft:jungle_leaves", 3, "minecraft:oak_leaves", 4, "minecraft:spruce_leaves", 5);
    private static final java.util.Set<String> kLogs = java.util.Set.of("minecraft:acacia_bark", "minecraft:birch_bark", "minecraft:dark_oak_bark",
        "minecraft:jungle_bark", "minecraft:oak_bark", "minecraft:spruce_bark", "minecraft:acacia_log", "minecraft:birch_log", "minecraft:dark_oak_log",
        "minecraft:jungle_log", "minecraft:oak_log", "minecraft:spruce_log", "minecraft:stripped_acacia_log", "minecraft:stripped_birch_log",
        "minecraft:stripped_dark_oak_log", "minecraft:stripped_jungle_log", "minecraft:stripped_oak_log", "minecraft:stripped_spruce_log");
    public static abstract class Section {
        protected static final String BLOCK_STATES_TAG = "BlockStates";
        protected static final String NAME_TAG = "Name";
        protected static final String PROPERTIES_TAG = "Properties";
        protected final OpticFinder<List<Pair<String, Dynamic<?>>>> paletteFinder = com.mojang.datafixers.DSL.fieldFinder("Palette",
            com.mojang.datafixers.DSL.list(com.mojang.datafixers.DSL.named(References.BLOCK_STATE.typeName(), com.mojang.datafixers.DSL.remainderType())));
        protected final List<Dynamic<?>> palette;
        protected final int index;
        protected PackedBitStorage storage;
        public Section(Typed<?> section, Schema inputSchema) {
            if (!java.util.Objects.equals(inputSchema.getType(References.BLOCK_STATE),
                com.mojang.datafixers.DSL.named(References.BLOCK_STATE.typeName(), com.mojang.datafixers.DSL.remainderType()))) {
                throw new IllegalStateException("Block state type is not what was expected.");
            }
            palette = section.getOptional(paletteFinder).map(entries -> entries.stream().<Dynamic<?>>map(Pair::getSecond)
                .collect(java.util.stream.Collectors.toCollection(java.util.ArrayList::new))).orElseGet(java.util.ArrayList::new);
            Dynamic<?> tag = section.get(com.mojang.datafixers.DSL.remainderFinder());
            index = tag.get("Y").asInt(0);
            readStorage(tag);
        }
        protected void readStorage(Dynamic<?> tag) {
            if (skippable()) {
                storage = null;
                return;
            }
            storage = new PackedBitStorage(Math.max(4, com.mojang.datafixers.DataFixUtils.ceillog2(palette.size())), 4096, tag.get(BLOCK_STATES_TAG).asLongStream().toArray());
        }
        public Typed<?> write(Typed<?> section) {
            if (isSkippable()) {
                return section;
            }
            Typed<?> updated = section.update(com.mojang.datafixers.DSL.remainderFinder(),
                tag -> tag.set(BLOCK_STATES_TAG, tag.createLongList(java.util.Arrays.stream(storage.getRaw()))));
            return updated.set(paletteFinder, palette.stream().map(state -> Pair.<String, Dynamic<?>>of(References.BLOCK_STATE.typeName(), state)).toList());
        }
        public boolean isSkippable() {
            return storage == null;
        }
        public int getBlock(int pos) {
            return storage.get(pos);
        }
        protected static int getStateId(String blockName, boolean persistent, int distance) {
            return kLeaves.get(blockName) << 5 | (persistent ? 16 : 0) | distance;
        }
        public int getIndex() {
            return index;
        }
        protected abstract boolean skippable();
    }
    public static final class LeavesSection extends Section {
        private java.util.Set<Integer> leafIds;
        private java.util.Set<Integer> logIds;
        private java.util.Map<Integer, Integer> stateToPalette;
        public LeavesSection(Typed<?> section, Schema inputSchema) {
            super(section, inputSchema);
        }
        protected boolean skippable() {
            leafIds = new java.util.HashSet<>();
            logIds = new java.util.HashSet<>();
            stateToPalette = new java.util.HashMap<>();
            for (int i = 0; i < palette.size(); i++) {
                Dynamic<?> entry = palette.get(i);
                String name = entry.get(NAME_TAG).asString("");
                if (kLeaves.containsKey(name)) {
                    boolean persistent = "false".equals(entry.get(PROPERTIES_TAG).get("decayable").asString(""));
                    leafIds.add(i);
                    stateToPalette.put(getStateId(name, persistent, 7), i);
                    palette.set(i, leafTag(entry, name, persistent, 7));
                }
                if (kLogs.contains(name)) {
                    logIds.add(i);
                }
            }
            return leafIds.isEmpty() && logIds.isEmpty();
        }
        private Dynamic<?> leafTag(Dynamic<?> input, String name, boolean persistent, int distance) {
            Dynamic<?> properties = input.emptyMap().set("persistent", input.createString(Boolean.toString(persistent)))
                .set("distance", input.createString(Integer.toString(distance)));
            return input.emptyMap().set(PROPERTIES_TAG, properties).set(NAME_TAG, input.createString(name));
        }
        public boolean isLog(int block) {
            return logIds.contains(block);
        }
        public boolean isLeaf(int block) {
            return leafIds.contains(block);
        }
        int getDistance(int block) {
            return isLog(block) ? 0 : Integer.parseInt(palette.get(block).get(PROPERTIES_TAG).get("distance").asString(""));
        }
        void setDistance(int pos, int block, int distance) {
            Dynamic<?> base = palette.get(block);
            String name = base.get(NAME_TAG).asString("");
            boolean persistent = "true".equals(base.get(PROPERTIES_TAG).get("persistent").asString(""));
            int state = getStateId(name, persistent, distance);
            if (!stateToPalette.containsKey(state)) {
                leafIds.add(palette.size());
                stateToPalette.put(state, palette.size());
                palette.add(leafTag(base, name, persistent, distance));
            }
            int id = stateToPalette.get(state);
            if (1 << storage.getBits() <= id) {
                PackedBitStorage wider = new PackedBitStorage(storage.getBits() + 1, 4096);
                for (int i = 0; i < 4096; i++) {
                    wider.set(i, storage.get(i));
                }
                storage = wider;
            }
            storage.set(pos, id);
        }
    }
}
