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
        CompoundTag tag = new CompoundTag();
        tag.putString("Name", bracket < 0 ? text : text.substring(0, bracket));
        if (bracket >= 0) {
            CompoundTag properties = new CompoundTag();
            for (String pair : text.substring(bracket + 1, text.length() - 1).split(",")) {
                int equals = pair.indexOf('=');
                properties.putString(pair.substring(0, equals), pair.substring(equals + 1));
            }
            tag.put("Properties", properties);
        }
        return tag;
    }
    public static Component toPrettyComponent(Tag tag) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.nbt.NbtUtils#toPrettyComponent");
    }
    public static String structureToSnbt(CompoundTag structure) {
        return new SnbtPrinterTagVisitor().visit(structure);
    }
    public static CompoundTag snbtToStructure(String snbt) throws CommandSyntaxException {
        return TagParser.parseCompoundFully(snbt);
    }
    static CompoundTag packStructureTemplate(CompoundTag snbt) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.nbt.NbtUtils#packStructureTemplate");
    }
    static CompoundTag unpackStructureTemplate(CompoundTag template) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.nbt.NbtUtils#unpackStructureTemplate");
    }
    static String packBlockState(CompoundTag compound, int version) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.nbt.NbtUtils#packBlockState");
    }
    static CompoundTag unpackBlockState(String compound, int version) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.nbt.NbtUtils#unpackBlockState");
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
