// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Dynamic;
import net.minecraft.core.HolderGetter;
import net.minecraft.network.chat.Component;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueOutput;
public final class NbtUtils {
    public static final String SNBT_DATA_TAG = "data";
    public static final String LEGACY_BLOCK_STATE_ID_TAG = "Name";
    public static final String LEGACY_BLOCKSTATE_PROPERTY_TAG = "Properties";
    private NbtUtils() {
    }
    public static boolean compareNbt(Tag expected, Tag actual, boolean partialListMatches) {
        if (expected == actual || expected == null) {
            return true;
        }
        if (actual == null || !expected.getClass().equals(actual.getClass())) {
            return false;
        }
        if (expected instanceof CompoundTag expectedCompound) {
            CompoundTag actualCompound = (CompoundTag) actual;
            if (actualCompound.size() < expectedCompound.size()) {
        return false;
            }
            for (java.util.Map.Entry<String, Tag> entry : expectedCompound.entrySet()) {
        if (!compareNbt(entry.getValue(), actualCompound.get(entry.getKey()), partialListMatches)) {
            return false;
        }
            }
            return true;
        }
        if (expected instanceof ListTag expectedList && partialListMatches) {
            ListTag actualList = (ListTag) actual;
            if (expectedList.isEmpty()) {
        return actualList.isEmpty();
            }
            if (actualList.size() < expectedList.size()) {
        return false;
            }
            for (Tag wanted : expectedList) {
        boolean found = false;
        for (Tag candidate : actualList) {
            if (compareNbt(wanted, candidate, partialListMatches)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
            }
            return true;
        }
        return expected.equals(actual);
    }
    public static BlockState readBlockState(HolderGetter<Block> blocks, CompoundTag tag) {
        StringBuilder text = new StringBuilder(tag.getStringOr("Name", "minecraft:air"));
        CompoundTag properties = tag.getCompoundOrEmpty("Properties");
        if (!properties.isEmpty()) {
            text.append('[');
            boolean first = true;
            for (String key : properties.keySet()) {
                text.append(first ? "" : ",").append(key).append('=').append(properties.getStringOr(key, ""));
                first = false;
            }
            text.append(']');
        }
        try {
            return net.minecraft.commands.arguments.blocks.BlockStateParser.parseForBlock(
                org.basaltmc.bukkit.facade.ServerAccess.registries().lookupOrThrow(net.minecraft.core.registries.Registries.BLOCK), text.toString(), false).blockState();
        } catch (com.mojang.brigadier.exceptions.CommandSyntaxException failure) {
            return net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
        }
    }
    public static CompoundTag writeBlockState(BlockState state) {
        String text = net.minecraft.commands.arguments.blocks.BlockStateParser.serialize(state);
        int bracket = text.indexOf('[');
        CompoundTag packed = new CompoundTag();
        packed.putString("Name", bracket < 0 ? text : text.substring(0, bracket));
        if (bracket >= 0) {
            CompoundTag props = new CompoundTag();
            for (String pair : text.substring(bracket + 1, text.length() - 1).split(",")) {
                int equals = pair.indexOf('=');
                props.putString(pair.substring(0, equals), pair.substring(equals + 1));
            }
            packed.put("Properties", props);
        }
        return packed;
    }
    public static Component toPrettyComponent(Tag tag) {
        return new TextComponentTagVisitor("").visit(tag);
    }
    public static String structureToSnbt(CompoundTag structure) {
        return new SnbtPrinterTagVisitor().visit(packStructureTemplate(structure));
    }
    public static CompoundTag snbtToStructure(String snbt) throws CommandSyntaxException {
        return unpackStructureTemplate(TagParser.parseCompoundFully(snbt));
    }
    private static int compareYxz(ListTag left, ListTag right, boolean decimals) {
        if (left.size() < 3 || right.size() < 3) {
            return Integer.compare(right.size(), left.size());
        }
        int[] order = {1, 0, 2};
        for (int axis : order) {
            int result = decimals ? Double.compare(left.getDoubleOr(axis, 0.0), right.getDoubleOr(axis, 0.0))
                : Integer.compare(left.getIntOr(axis, 0), right.getIntOr(axis, 0));
            if (result != 0) {
                return result;
            }
        }
        return 0;
    }
    private static java.util.List<CompoundTag> compoundsOf(ListTag list) {
        java.util.List<CompoundTag> out = new java.util.ArrayList<>();
        for (int i = 0; i < list.size(); i++) {
            CompoundTag compound = list.getCompoundOrEmpty(i);
            out.add(compound);
        }
        return out;
    }
    static CompoundTag packStructureTemplate(CompoundTag snbt) {
        int version = getDataVersion(snbt);
        ListTag palettes = snbt.contains("palettes") ? snbt.getListOrEmpty("palettes") : null;
        ListTag palette = palettes != null ? palettes.getListOrEmpty(0) : snbt.getListOrEmpty("palette");
        ListTag deflated = new ListTag();
        for (CompoundTag state : compoundsOf(palette)) {
            deflated.add(StringTag.valueOf(packBlockState(state, version)));
        }
        snbt.put("palette", deflated);
        if (palettes != null) {
            ListTag rebuilt = new ListTag();
            for (int p = 0; p < palettes.size(); p++) {
                ListTag old = palettes.getListOrEmpty(p);
                CompoundTag packed = new CompoundTag();
                for (int i = 0; i < old.size(); i++) {
                    packed.putString(deflated.getStringOr(i, ""), packBlockState(old.getCompoundOrEmpty(i), version));
                }
                rebuilt.add(packed);
            }
            snbt.put("palettes", rebuilt);
        }
        if (snbt.contains("entities")) {
            java.util.List<CompoundTag> entities = new java.util.ArrayList<>(compoundsOf(snbt.getListOrEmpty("entities")));
            entities.sort((a, b) -> compareYxz(a.getListOrEmpty("pos"), b.getListOrEmpty("pos"), true));
            ListTag sorted = new ListTag();
            entities.forEach(sorted::add);
            snbt.put("entities", sorted);
        }
        java.util.List<CompoundTag> blocks = new java.util.ArrayList<>(compoundsOf(snbt.getListOrEmpty("blocks")));
        blocks.sort((a, b) -> compareYxz(a.getListOrEmpty("pos"), b.getListOrEmpty("pos"), false));
        ListTag data = new ListTag();
        for (CompoundTag block : blocks) {
            block.putString("state", deflated.getStringOr(block.getIntOr("state", 0), ""));
            data.add(block);
        }
        snbt.put("data", data);
        snbt.remove("blocks");
        return snbt;
    }
    static CompoundTag unpackStructureTemplate(CompoundTag template) {
        int version = getDataVersion(template);
        ListTag packed = template.getListOrEmpty("palette");
        java.util.List<String> names = new java.util.ArrayList<>();
        for (int i = 0; i < packed.size(); i++) {
            names.add(packed.getStringOr(i, ""));
        }
        if (template.contains("palettes")) {
            ListTag rebuilt = new ListTag();
            ListTag old = template.getListOrEmpty("palettes");
            for (int p = 0; p < old.size(); p++) {
                ListTag palette = new ListTag();
                for (String name : names) {
                    palette.add(unpackBlockState(old.getCompoundOrEmpty(p).getStringOr(name, ""), version));
                }
                rebuilt.add(palette);
            }
            template.put("palettes", rebuilt);
            template.remove("palette");
        } else {
            ListTag palette = new ListTag();
            for (String name : names) {
                palette.add(unpackBlockState(name, version));
            }
            template.put("palette", palette);
        }
        if (template.contains("data")) {
            ListTag blocks = template.getListOrEmpty("data");
            for (int i = 0; i < blocks.size(); i++) {
                CompoundTag block = blocks.getCompoundOrEmpty(i);
                String state = block.getStringOr("state", "");
                int id = names.indexOf(state);
                if (id < 0) {
                    throw new IllegalStateException("Entry " + state + " missing from palette");
                }
                block.putInt("state", id);
            }
            template.put("blocks", blocks);
            template.remove("data");
        }
        return template;
    }
    static String packBlockState(CompoundTag compound, int version) {
        String idKey = version >= 5006 ? "id" : "Name";
        String propertiesKey = version >= 5006 ? "properties" : "Properties";
        StringBuilder out = new StringBuilder(compound.getStringOr(idKey, ""));
        if (compound.contains(propertiesKey)) {
            CompoundTag properties = compound.getCompoundOrEmpty(propertiesKey);
            out.append('{');
            boolean first = true;
            for (String key : new java.util.TreeSet<>(properties.keySet())) {
                out.append(first ? "" : ",").append(key).append(':').append(properties.getStringOr(key, ""));
                first = false;
            }
            out.append('}');
        }
        return out.toString();
    }
    static CompoundTag unpackBlockState(String compound, int version) {
        CompoundTag state = new CompoundTag();
        CompoundTag values2 = new CompoundTag();
        int open = compound.indexOf('{');
        String name = compound;
        if (open >= 0) {
            name = compound.substring(0, open);
            int close = compound.indexOf('}', open);
            String values = close > open + 1 ? compound.substring(open + 1, close) : "";
            for (String pair : values.split(",")) {
                int colon = pair.indexOf(':');
                if (colon > 0) {
                    values2.putString(pair.substring(0, colon), pair.substring(colon + 1));
                }
            }
        }
        boolean current = version >= 5006;
        state.putString(current ? "id" : "Name", name);
        if (!values2.isEmpty()) {
            state.put(current ? "properties" : "Properties", values2);
        }
        return state;
    }
    public static CompoundTag addCurrentDataVersion(CompoundTag tag) {
        return addDataVersion(tag, net.minecraft.SharedConstants.getCurrentVersion().dataVersion().version());
    }
    public static CompoundTag addDataVersion(CompoundTag tag, int version) {
        tag.putInt("DataVersion", version);
        return tag;
    }
    public static <T> Dynamic<T> addDataVersion(Dynamic<T> tag, int version) {
        return tag.set("DataVersion", tag.createInt(version));
    }
    public static void addCurrentDataVersion(ValueOutput output) {
        addDataVersion(output, net.minecraft.SharedConstants.getCurrentVersion().dataVersion().version());
    }
    public static void addDataVersion(ValueOutput output, int version) {
        output.putInt("DataVersion", version);
    }
    public static int getDataVersion(CompoundTag tag) {
        return getDataVersion(tag, -1);
    }
    public static int getDataVersion(CompoundTag tag, int _default) {
        return tag.getIntOr("DataVersion", _default);
    }
    public static int getDataVersion(Dynamic<?> dynamic) {
        return getDataVersion(dynamic, -1);
    }
    public static int getDataVersion(Dynamic<?> dynamic, int _default) {
        return dynamic.get("DataVersion").asInt(_default);
    }
}
